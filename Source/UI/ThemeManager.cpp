#include "ThemeManager.h"

#include <QApplication>
#include <QColor>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QEvent>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QAbstractScrollArea>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QBuffer>
#include <QGraphicsDropShadowEffect>
#include <QIconEngine>
#include <QImage>
#include <QImageReader>
#include <QMap>
#include <QPaintEvent>
#include <QPainter>
#include <QPixmap>
#include <QHash>
#include <QPalette>
#include <QRegularExpression>
#include <QWidget>
#include <QStyleFactory>

namespace
{
constexpr auto PropSource = "_themeSrc";
constexpr auto PropApplied = "_themeApplied";
constexpr auto PropTextSrc = "_themeTextSrc";
constexpr auto PropTextApplied = "_themeTextApplied";

enum class Group
{
    Surface,
    Border,
    Text,
    Accent
};

struct Token
{
    const char *dark;
    const char *light;
    Group group;
};

// Legacy (slate/cyan) colors -> Apple design tokens.
const QHash<QString, Token> &tokenTable()
{
    static const QHash<QString, Token> t = [] {
        QHash<QString, Token> m;
        auto add = [&m](std::initializer_list<const char *> keys, Token tk) {
            for (const char *k : keys)
                m.insert(QString::fromLatin1(k), tk);
        };
        add({"#0a0e1a",
             "#0b0f19",
             "#0b101d",
             "#070a12",
             "#070b14",
             "#0b1120",
             "#080c14",
             "#050811",
             "#121214",
             "#141416",
             "#18181b",
             "#27272a",
             "#0a1120",
             "#0a0f1d",
             "#080e1a",
             "#050b14",
             "#08111d"},
            {"#1c1c1e", "#f5f5f7", Group::Surface});
        add({"#0f172a", "#131e35", "#161f33", "#141b2d", "#0e1526", "#0d1527", "#141c2e", "#141e33", "#111827"},
            {"#2c2c2e", "#ffffff", Group::Surface});
        add({"#1e293b"}, {"#38383a", "#e5e5ea", Group::Border});
        add({"#334155"}, {"#48484a", "#d1d1d6", Group::Border});
        add({"#38bdf8", "#0284c7", "#00e5ff", "#4cc2ff", "#0078d4", "#0ea5e9", "#7dd3fc", "#0891b2", "#005fb8"},
            {"#0071e3", "#0071e3", Group::Accent});
        add({"#0369a1"}, {"#0077ed", "#0062c4", Group::Accent});
        add({"#075985"}, {"#0062c4", "#0058b0", Group::Accent});
        add({"#f8fafc", "#f1f5f9"}, {"#f5f5f7", "#1d1d1f", Group::Text});
        add({"#e2e8f0", "#e3e5e8", "#e5e7eb", "#e0e0e0"}, {"#e5e5ea", "#3a3a3c", Group::Text});
        add({"#cbd5e1", "#d1d5db", "#bac0cb"}, {"#d1d1d6", "#3a3a3c", Group::Text});
        add({"#94a3b8", "#9ca3af", "#a1a1aa", "#8e9297", "#a0aabf"}, {"#98989d", "#6e6e73", Group::Text});
        add({"#64748b", "#71717a", "#6b7280", "#7e8590", "#72767d"}, {"#8e8e93", "#8e8e93", Group::Text});
        add({"#475569", "#4b5563"}, {"#636366", "#aeaeb2", Group::Text});
        return m;
    }();
    return t;
}

// A saturated, not-too-light fill (accent, green "buy" buttons, ...): text on it stays white.
bool isStrongFill(const QString &c)
{
    const QColor q(c);
    if (!q.isValid())
        return false;
    float h, sat, l, a;
    q.getHslF(&h, &sat, &l, &a);
    return sat > 0.4f && l < 0.62f;
}

bool isAccent(const QString &c)
{
    return c == QLatin1String("#0071e3") || c == QLatin1String("#0077ed") || c == QLatin1String("#0062c4")
           || c == QLatin1String("#0058b0");
}

enum class Kind
{
    Auto,
    Background,
    Text,
    Border
};

// Light/bright legacy color used as a fill or border (e.g. slider handle) -> light-theme surface.
QString lightFill(float l, Kind kind)
{
    if (kind == Kind::Border)
        return QStringLiteral("#c7c7cc");
    if (l > 0.85f)
        return QStringLiteral("#ffffff");
    return l > 0.5f ? QStringLiteral("#c7c7cc") : QStringLiteral("#aeaeb2");
}

QString mapColor(const QString &hex, Kind kind, ThemeManager::Theme theme)
{
    const bool dark = theme == ThemeManager::Theme::Dark;
    const QString key = hex.toLower();
    QColor c(hex);
    if (!c.isValid())
        return hex;
    float h, s, l, a;
    c.getHslF(&h, &s, &l, &a);

    const auto it = tokenTable().constFind(key);
    if (kind == Kind::Auto && it != tokenTable().cend())
        kind = it->group == Group::Text ? Kind::Text : Kind::Background;
    if (it != tokenTable().cend())
    {
        // A surface/border color used as TEXT is dark text on a bright fill: keep it dark.
        if (kind == Kind::Text && (it->group == Group::Surface || it->group == Group::Border))
            return hex;
        // A text color used as a fill/border (handles, bright pills).
        if (kind != Kind::Text && it->group == Group::Text)
            return dark ? QString::fromLatin1(it->dark) : lightFill(l, kind);
        return QString::fromLatin1(dark ? it->dark : it->light);
    }

    // Slate / blue-grey or neutral colors: bucket by lightness into the Apple greys.
    const float hue = h < 0 ? 0 : h * 360.0f;
    const bool neutral = s < 0.12f || (hue >= 190.0f && hue <= 270.0f && s < 0.65f);
    if (kind == Kind::Auto)
        kind = (neutral && l >= 0.5f) ? Kind::Text : Kind::Background;
    if (neutral)
    {
        auto pick = [dark](const char *d, const char *li) {
            return QString::fromLatin1(dark ? d : li);
        };
        if (kind == Kind::Text)
        {
            if (l > 0.9f)
                return pick("#f5f5f7", "#1d1d1f");
            if (l > 0.75f)
                return pick("#d1d1d6", "#3a3a3c");
            if (l > 0.55f)
                return pick("#98989d", "#6e6e73");
            if (l > 0.35f)
                return pick("#8e8e93", "#8e8e93");
            return hex; // dark text on a bright fill
        }
        if (l >= 0.5f)
            return dark ? hex : lightFill(l, kind);
        if (kind == Kind::Border)
        {
            if (l < 0.2f)
                return pick("#38383a", "#e5e5ea");
            if (l < 0.32f)
                return pick("#48484a", "#d1d1d6");
            return pick("#636366", "#c7c7cc");
        }
        if (l < 0.07f)
            return pick("#1c1c1e", "#f5f5f7");
        if (l < 0.13f)
            return pick("#2c2c2e", "#ffffff");
        if (l < 0.2f)
            return pick("#3a3a3c", "#e8e8ed");
        if (l < 0.32f)
            return pick("#48484a", "#d1d1d6");
        return pick("#636366", "#aeaeb2");
    }

    if (dark)
        return hex;

    // Saturated one-off colors (tinted badges etc.): keep the hue, flip the lightness.
    if (kind == Kind::Text)
    {
        if (l > 0.5f)
            l = 0.35f;
        else
            return hex;
    }
    else if (l < 0.4f)
    {
        l = 1.0f - l;
    }
    else
    {
        return hex;
    }
    return QColor::fromHslF(h, s, l, a).name();
}

QString remapBlock(const QString &selector, const QString &body, ThemeManager::Theme theme, bool roundable = false)
{
    // Dark "tinted" semantic controls (saturated dark fill + bright text of the same family, e.g. green
    // "start" / amber "stop" buttons): in the light theme turn them into Apple's tinted style, i.e. a
    // translucent fill of the text colour with a darker text, instead of flipping the fill's lightness.
    if (theme == ThemeManager::Theme::Light)
    {
        static const QRegularExpression tintBgRe(
            QStringLiteral(R"((?<![-\w])(background(?:-color)?\s*:\s*)(#[0-9a-fA-F]{6})(\s*[;}]|\s*$))"));
        static const QRegularExpression tintFgRe(QStringLiteral(R"((?<![-\w])(color\s*:\s*)(#[0-9a-fA-F]{6}))"));
        const auto bgM = tintBgRe.match(body);
        const auto fgM = tintFgRe.match(body);
        if (bgM.hasMatch() && fgM.hasMatch())
        {
            float bh, bs, bl, ba, fh, fs, fl, fa;
            QColor(bgM.captured(2)).getHslF(&bh, &bs, &bl, &ba);
            QColor(fgM.captured(2)).getHslF(&fh, &fs, &fl, &fa);
            const float bgHue = bh < 0 ? 0 : bh * 360.0f;
            const bool slateBg = bgHue >= 190.0f && bgHue <= 270.0f;
            if (!slateBg && bs > 0.35f && bl < 0.4f && fs > 0.35f && fl > 0.45f)
            {
                QColor text = QColor::fromHslF(fh, fs, 0.33f);
                QColor tint = text;
                tint.setAlphaF(0.15);
                QString nb = body;
                nb.replace(fgM.capturedStart(2), fgM.capturedLength(2), text.name());
                // The bg offset may have shifted if the fg comes first: recompute on the updated string.
                const auto bg2 = tintBgRe.match(nb);
                nb.replace(
                    bg2.capturedStart(2),
                    bg2.capturedLength(2),
                    QStringLiteral("rgba(%1, %2, %3, %4)")
                        .arg(tint.red())
                        .arg(tint.green())
                        .arg(tint.blue())
                        .arg(0.15));
                return remapBlock(selector, nb, theme, roundable);
            }
        }
    }

    static const QRegularExpression propRe(QStringLiteral(
        R"((?<![-\w])(background(?:-color)?|color|border(?:-top|-bottom|-left|-right)?(?:-color)?|selection-background-color|selection-color|gridline-color|outline)\s*:\s*([^;}]*))"));
    static const QRegularExpression colorRe(QStringLiteral(R"(#[0-9a-fA-F]{6}\b|\bwhite\b)"));
    const bool light = theme == ThemeManager::Theme::Light;

    // Pass 1: does this block paint an accent background? Then white text on it must stay white.
    bool accentBg = false;
    for (auto m = propRe.globalMatch(body); m.hasNext();)
    {
        const auto mm = m.next();
        if (!mm.captured(1).startsWith(QLatin1String("background")))
            continue;
        for (auto cm = colorRe.globalMatch(mm.captured(2)); cm.hasNext();)
            accentBg |= isStrongFill(mapColor(cm.next().captured(0), Kind::Background, theme).toLower());
    }

    QString out;
    qsizetype last = 0;
    for (auto m = propRe.globalMatch(body); m.hasNext();)
    {
        const auto mm = m.next();
        const QString prop = mm.captured(1);
        const Kind kind
            = prop.startsWith(QLatin1String("background")) || prop == QLatin1String("selection-background-color")
                  ? Kind::Background
                  : (prop.startsWith(QLatin1String("border")) || prop == QLatin1String("gridline-color")
                             || prop == QLatin1String("outline")
                         ? Kind::Border
                         : Kind::Text);
        QString value = mm.captured(2);
        QString nv;
        qsizetype vl = 0;
        for (auto cm = colorRe.globalMatch(value); cm.hasNext();)
        {
            const auto c = cm.next();
            nv += value.mid(vl, c.capturedStart() - vl);
            vl = c.capturedEnd();
            const QString col = c.captured(0);
            if (col == QLatin1String("white"))
                nv += (light && kind == Kind::Text && !accentBg) ? QStringLiteral("#1d1d1f") : col;
            else if (light && accentBg && kind == Kind::Text && col.toLower() == QLatin1String("#ffffff"))
                nv += col;
            else
                nv += mapColor(col, kind, theme);
        }
        nv += value.mid(vl);
        out += body.mid(last, mm.capturedStart(2) - last) + nv;
        last = mm.capturedEnd(2);
    }
    out += body.mid(last);

    // Typography: proportional UI fonts hard-coded in widget styles follow the app font (Inter).
    static const QRegularExpression fontRe(QStringLiteral(R"((?<![-\w])font-family\s*:\s*([^;}]*))"));
    {
        QString fixedFonts;
        qsizetype pos = 0;
        for (auto m = fontRe.globalMatch(out); m.hasNext();)
        {
            const auto r = m.next();
            fixedFonts += out.mid(pos, r.capturedStart() - pos);
            pos = r.capturedEnd();
            const QString v = r.captured(1).toLower();
            const bool mono = v.contains(QLatin1String("mono")) || v.contains(QLatin1String("consolas"))
                              || v.contains(QLatin1String("courier"));
            fixedFonts += mono ? r.captured(0) : QStringLiteral("font-family: \"Inter\"");
        }
        out = fixedFonts + out.mid(pos);
    }

    // Apple look: no colored "accent stripe" on the left edge of panels.
    static const QRegularExpression stripeRe(
        QStringLiteral(R"((?<![-\w])border-left\s*:\s*[2-9](?:\.\d+)?px\s+solid\s+[^;}]*;?)"));
    out.remove(stripeRe);

    // Apple look: no square corners. Sub-controls (::indicator, ::drop-down, ...) keep their own shape.
    if (!selector.contains(QLatin1String("::")))
    {
        static const QRegularExpression radiusRe(QStringLiteral(R"((?<![-\w])border-radius\s*:\s*(\d+(?:\.\d+)?)px)"));
        // Panels/cards get a larger corner than small controls.
        static const QRegularExpression panelRe(QStringLiteral(R"((?:^|[\s,>])(?:QFrame|QWidget|QGroupBox)(?![\w]))"));
        static const QRegularExpression smallCtlRe(QStringLiteral(
            R"((?:^|[\s,>])(?:QLabel|QPushButton|QToolButton|QLineEdit|QComboBox|QSpinBox|QDateEdit)(?![\w]))"));
        const int squareFix = (selector.contains(panelRe) && !selector.contains(smallCtlRe)) ? 12 : 8;
        QString fixed;
        qsizetype pos = 0;
        for (auto m = radiusRe.globalMatch(out); m.hasNext();)
        {
            const auto r = m.next();
            fixed += out.mid(pos, r.capturedStart() - pos);
            pos = r.capturedEnd();
            fixed += r.captured(1).toDouble() <= 6.0 ? QStringLiteral("border-radius: %1px").arg(squareFix)
                                                     : r.captured(0);
        }
        out = fixed + out.mid(pos);

        // Small controls with a filled background but no radius would be square: round them.
        static const QRegularExpression ctlRe(QStringLiteral(
            R"((?:^|[\s,>])(?:QLabel|QPushButton|QToolButton|QLineEdit|QComboBox|QSpinBox|QDateEdit)(?![\w]))"));
        static const QRegularExpression bgRe(QStringLiteral(
            R"((?<![-\w])background(?:-color)?\s*:\s*(?!transparent|none)[^;}]*(?:#[0-9a-fA-F]{3,8}|rgba?\(|qlinear|qradial|white|black))"));
        const bool target = selector.isEmpty() ? roundable : selector.contains(ctlRe);
        if (target && out.contains(bgRe))
        {
            // A tinted fill with a coloured outline of the same hue looks heavy: keep only the fill.
            static const QRegularExpression outlineRe(
                QStringLiteral(R"((?<![-\w])border\s*:\s*\d+(?:\.\d+)?px\s+solid\s+(#[0-9a-fA-F]{6})\s*;?)"));
            QString cleaned;
            qsizetype from = 0;
            for (auto m = outlineRe.globalMatch(out); m.hasNext();)
            {
                const auto mm = m.next();
                const QColor oc(mm.captured(1));
                float oh, os, ol, oa;
                oc.getHslF(&oh, &os, &ol, &oa);
                cleaned += out.mid(from, mm.capturedStart() - from);
                from = mm.capturedEnd();
                // Neutral hairlines and accent outlines stay; saturated non-accent outlines go.
                if (os < 0.35f || isAccent(mm.captured(1).toLower()))
                    cleaned += mm.captured(0);
                else
                    cleaned += QStringLiteral("border: none;");
            }
            out = cleaned + out.mid(from);
        }
        if (target && !out.contains(QLatin1String("border-radius")) && out.contains(bgRe)
            && !out.contains(QLatin1String("border-image")))
        {
            if (!out.trimmed().isEmpty() && !out.trimmed().endsWith(QLatin1Char(';')))
                out += QLatin1Char(';');
            out += QStringLiteral(" border-radius: 8px;");
        }
    }
    return out;
}
} // namespace

QString ThemeManager::remapStyleSheet(const QString &css, Theme theme, bool roundable)
{
    if (!css.contains(QLatin1Char('{')))
        return remapBlock(QString(), css, theme, roundable);

    static const QRegularExpression blockRe(QStringLiteral(R"(([^{}]*)\{([^{}]*)\})"));
    QString out;
    qsizetype last = 0;
    for (auto m = blockRe.globalMatch(css); m.hasNext();)
    {
        const auto mm = m.next();
        out += css.mid(last, mm.capturedStart(2) - last) + remapBlock(mm.captured(1), mm.captured(2), theme);
        last = mm.capturedEnd(2);
    }
    out += css.mid(last);
    return out;
}

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
{
    qApp->installEventFilter(this);
}

bool ThemeManager::eventFilter(QObject *watched, QEvent *event)
{
    if (!m_applied || !watched->isWidgetType())
        return QObject::eventFilter(watched, event);

    switch (event->type())
    {
        case QEvent::StyleChange:
            restyleWidget(static_cast<QWidget *>(watched));
            break;
        case QEvent::Show:
        case QEvent::Polish:
        case QEvent::Paint:
            // Widgets styled before they were shown never get a StyleChange: catch them here.
            if (event->type() != QEvent::Paint)
                restyleWidget(static_cast<QWidget *>(watched));
            // Rich text carries hard-coded colors that QSS cannot reach.
            if (auto *label = qobject_cast<QLabel *>(watched))
            {
                const QString text = label->text();
                if (text.contains(QLatin1String("color"), Qt::CaseInsensitive)
                    && label->property(PropTextApplied).toString() != text)
                {
                    // Never call setText from inside the paint event itself.
                    QMetaObject::invokeMethod(label, [this, label]() { restyleLabel(label); }, Qt::QueuedConnection);
                }
            }
            break;
        default:
            break;
    }
    return QObject::eventFilter(watched, event);
}

namespace
{
/// Renders a resource SVG with its colors mapped to the current theme.
class ThemedSvgIconEngine : public QIconEngine
{
public:
    explicit ThemedSvgIconEngine(const QString &path)
        : m_path(path)
    {}

    QIconEngine *clone() const override
    {
        return new ThemedSvgIconEngine(m_path);
    }
    QString key() const override
    {
        return QStringLiteral("themedsvg");
    }

    QSize actualSize(const QSize &size, QIcon::Mode, QIcon::State) override
    {
        return size;
    }

    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
    {
        return pixmapForRatio(size, 1.0, mode, state);
    }

    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state) override
    {
        const qreal dpr = painter->device() ? painter->device()->devicePixelRatio() : 1.0;
        painter->drawPixmap(rect, pixmapForRatio(rect.size(), dpr, mode, state));
    }

private:
    QPixmap pixmapForRatio(const QSize &size, qreal dpr, QIcon::Mode mode, QIcon::State)
    {
        if (size.isEmpty())
            return {};
        const auto theme = ThemeManager::instance()->theme();
        const QString key = QStringLiteral("%1|%2x%3|%4|%5")
                                .arg(int(theme))
                                .arg(size.width())
                                .arg(size.height())
                                .arg(dpr)
                                .arg(int(mode));
        auto it = m_cache.constFind(key);
        if (it != m_cache.cend())
            return *it;

        QFile f(m_path);
        if (!f.open(QFile::ReadOnly))
            return {};
        const QString base = QFileInfo(m_path).baseName();
        const bool keepWhite = base == QLatin1String("play") || base == QLatin1String("pause");
        QString svg = QString::fromUtf8(f.readAll());
        static const QRegularExpression colorRe(QStringLiteral(R"(#[0-9a-fA-F]{6}\b)"));
        QString out;
        qsizetype last = 0;
        for (auto m = colorRe.globalMatch(svg); m.hasNext();)
        {
            const auto c = m.next();
            out += svg.mid(last, c.capturedStart() - last);
            last = c.capturedEnd();
            // play/pause sit on colored buttons and must stay white; other white icons follow the theme.
            out += (c.captured(0).toLower() == QLatin1String("#ffffff") && keepWhite)
                       ? c.captured(0)
                       : mapColor(c.captured(0), Kind::Text, theme);
        }
        out += svg.mid(last);

        QByteArray data = out.toUtf8();
        QBuffer buf(&data);
        buf.open(QIODevice::ReadOnly);
        QImageReader reader(&buf, "svg");
        reader.setScaledSize(size * dpr);
        QImage img = reader.read();
        if (img.isNull())
            return {};
        QPixmap pm = QPixmap::fromImage(img);
        pm.setDevicePixelRatio(dpr);
        if (mode == QIcon::Disabled)
        {
            QPixmap faded(pm.size());
            faded.setDevicePixelRatio(dpr);
            faded.fill(Qt::transparent);
            QPainter p(&faded);
            p.setOpacity(0.4);
            p.drawPixmap(0, 0, pm);
            p.end();
            pm = faded;
        }
        m_cache.insert(key, pm);
        return pm;
    }

    QString m_path;
    QMap<QString, QPixmap> m_cache;
};
} // namespace

QIcon ThemeManager::icon(const QString &resourcePath)
{
    return QIcon(new ThemedSvgIconEngine(resourcePath));
}

QFont ThemeManager::uiFont(int pointSize, int weight)
{
    QFont f = qApp->font();
    f.setPointSize(pointSize);
    if (weight >= 0)
        f.setWeight(static_cast<QFont::Weight>(weight));
    return f;
}

QColor ThemeManager::autoColor(const QColor &legacy)
{
    static QHash<quint64, QColor> cache;
    const Theme theme = instance()->m_theme;
    const quint64 key = (quint64(legacy.rgba()) << 8) | (theme == Theme::Dark ? 1u : 0u);
    auto it = cache.constFind(key);
    if (it != cache.cend())
        return *it;
    QColor c(mapColor(legacy.name(), Kind::Auto, theme));
    c.setAlpha(legacy.alpha());
    cache.insert(key, c);
    return c;
}

QColor ThemeManager::themed(const QColor &legacy, bool isText)
{
    static QHash<quint64, QColor> cache;
    const Theme theme = instance()->m_theme;
    const quint64 key = (quint64(legacy.rgba()) << 8) | (isText ? 2u : 0u) | (theme == Theme::Dark ? 1u : 0u);
    auto it = cache.constFind(key);
    if (it != cache.cend())
        return *it;
    QColor c(mapColor(legacy.name(), isText ? Kind::Text : Kind::Background, theme));
    c.setAlpha(legacy.alpha());
    cache.insert(key, c);
    return c;
}

namespace
{
/// Empty rounded plate placed right under a card. The drop shadow effect lives on this plate, not on
/// the card itself, so the card keeps rendering normally (an effect on a container re-renders all of
/// its children into an offscreen pixmap, which is slow and breaks nested effects).
class CardShadowPlate : public QWidget
{
public:
    CardShadowPlate(QWidget *card)
        : QWidget(card->parentWidget())
        , m_card(card)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        card->installEventFilter(this);
        auto *shadow = new QGraphicsDropShadowEffect(this);
        shadow->setBlurRadius(28);
        shadow->setOffset(0, 4);
        shadow->setColor(QColor(0, 0, 0, 36));
        setGraphicsEffect(shadow);
        sync();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        p.drawRoundedRect(rect(), 16, 16);
    }

    bool eventFilter(QObject *obj, QEvent *e) override
    {
        if (obj == m_card)
        {
            switch (e->type())
            {
                case QEvent::Move:
                case QEvent::Resize:
                case QEvent::Show:
                case QEvent::ParentChange:
                    sync();
                    break;
                case QEvent::Hide:
                    hide();
                    break;
                default:
                    break;
            }
        }
        return QWidget::eventFilter(obj, e);
    }

private:
    void sync()
    {
        if (!m_card->parentWidget())
            return;
        if (parentWidget() != m_card->parentWidget())
            setParent(m_card->parentWidget());
        setGeometry(m_card->geometry());
        stackUnder(m_card);
        // The shadow only makes sense on a light background.
        setVisible(m_card->isVisible() && !ThemeManager::instance()->isDark());
    }

    QWidget *m_card;
};
} // namespace

void ThemeManager::addCardShadow(QWidget *card)
{
    if (!card || !card->parentWidget())
        return;
    auto *plate = new CardShadowPlate(card);
    connect(this, &ThemeManager::themeChanged, plate, [plate, card](Theme t) {
        plate->setVisible(t == Theme::Light && card->isVisible());
    });
}

static bool isRoundable(QWidget *w)
{
    return qobject_cast<QLabel *>(w) || qobject_cast<QPushButton *>(w) || qobject_cast<QToolButton *>(w)
           || qobject_cast<QLineEdit *>(w) || qobject_cast<QComboBox *>(w) || qobject_cast<QAbstractSpinBox *>(w);
}

// A QLabel with its own plain style (no selector) and no background gets the palette Window color
// painted behind its text: a visible rectangle on any card. Make it transparent.
static QString finalizeSheet(QWidget *w, QString css)
{
    // A selector-less box style on a container ("background: ..; border: 1px solid ..") is inherited by every
    // descendant, so each child label/checkbox gets its own box. Scope it to the container itself.
    {
        const bool isContainer
            = (qobject_cast<QFrame *>(w) && !qobject_cast<QLabel *>(w) && !qobject_cast<QAbstractScrollArea *>(w))
              || QLatin1String(w->metaObject()->className()) == QLatin1String("QWidget");
        static const QRegularExpression boxRe(
            QStringLiteral(R"((?<![-\w])(?:border(?:-radius)?|background(?:-color)?)\s*:)"));
        if (isContainer && !css.contains(QLatin1Char('{')) && css.contains(boxRe))
        {
            if (w->objectName().isEmpty())
            {
                static int counter = 0;
                w->setObjectName(QStringLiteral("themeBox%1").arg(++counter));
            }
            css = QStringLiteral("QWidget#%1 { %2 }").arg(w->objectName(), css);
        }
    }

    // A bare "QFrame { border... }" on a container also hits every child QLabel (QLabel is a QFrame),
    // which draws a box around each text. Bind such rules to the frame itself.
    if (qobject_cast<QFrame *>(w) && !qobject_cast<QLabel *>(w))
    {
        static const QRegularExpression bareFrameRe(QStringLiteral(R"((^|\})(\s*)QFrame(\s*(?::[\w-]+)?\s*\{))"));
        if (css.contains(bareFrameRe))
        {
            if (w->objectName().isEmpty())
            {
                static int counter = 0;
                w->setObjectName(QStringLiteral("themeFrame%1").arg(++counter));
            }
            css.replace(bareFrameRe, QStringLiteral("\\1\\2QFrame#%1\\3").arg(w->objectName()));
        }
    }

    if (qobject_cast<QLabel *>(w) && !css.contains(QLatin1Char('{'))
        && !css.contains(QLatin1String("background"), Qt::CaseInsensitive))
    {
        if (!css.trimmed().endsWith(QLatin1Char(';')))
            css += QLatin1Char(';');
        css += QStringLiteral(" background: transparent;");
    }
    return css;
}

void ThemeManager::restyleWidget(QWidget *w)
{
    const QString current = w->styleSheet();
    if (current.isEmpty())
        return;
    // Already the result of our own remap: nothing to do.
    if (w->property(PropApplied).toString() == current)
        return;

    const QString mapped = finalizeSheet(w, remapStyleSheet(current, m_theme, isRoundable(w)));
    w->setProperty(PropSource, current);
    w->setProperty(PropApplied, mapped);
    if (mapped != current)
        w->setStyleSheet(mapped);
}

void ThemeManager::restyleLabel(QLabel *label)
{
    const QString current = label->text();
    if (label->property(PropTextApplied).toString() == current)
        return;
    const QString mapped = remapBlock(QString(), current, m_theme);
    label->setProperty(PropTextSrc, current);
    label->setProperty(PropTextApplied, mapped);
    if (mapped != current)
        label->setText(mapped);
}

void ThemeManager::restyleAllWidgets()
{
    const auto widgets = qApp->allWidgets();
    for (QWidget *w : widgets)
    {
        if (auto *label = qobject_cast<QLabel *>(w))
        {
            const QVariant txt = label->property(PropTextSrc);
            if (txt.isValid())
            {
                const QString mapped = remapBlock(QString(), txt.toString(), m_theme);
                label->setProperty(PropTextApplied, mapped);
                if (label->text() != mapped)
                    label->setText(mapped);
            }
        }
        const QVariant src = w->property(PropSource);
        if (!src.isValid())
            continue;
        const QString mapped = finalizeSheet(w, remapStyleSheet(src.toString(), m_theme, isRoundable(w)));
        w->setProperty(PropApplied, mapped);
        if (w->styleSheet() != mapped)
            w->setStyleSheet(mapped);
    }
}

ThemeManager *ThemeManager::instance()
{
    static ThemeManager *inst = new ThemeManager(qApp);
    return inst;
}

void ThemeManager::loadFonts()
{
    // Inter (SIL OFL 1.1) is bundled; if it fails to load we keep the system font.
    static const char *files[]
        = {":/resources/font-Inter-Regular",
           ":/resources/font-Inter-Medium",
           ":/resources/font-Inter-SemiBold",
           ":/resources/font-Inter-Bold"};
    bool loaded = false;
    for (const char *f : files)
        loaded |= QFontDatabase::addApplicationFont(QString::fromLatin1(f)) >= 0;

    if (loaded)
    {
        QFont font = qApp->font();
        // Inter first, then the platform's own UI font as a fallback.
        font.setFamilies({QStringLiteral("Inter"), font.family()});
        qApp->setFont(font);
    }
}

QString ThemeManager::loadStyleSheet(Theme theme)
{
    const QString path
        = theme == Theme::Dark ? QStringLiteral(":/themes/dark.qss") : QStringLiteral(":/themes/light.qss");
    QFile file(path);
    if (!file.open(QFile::ReadOnly | QFile::Text))
        return {};
    return QString::fromUtf8(file.readAll());
}

QPalette ThemeManager::buildPalette(Theme theme)
{
    QPalette p;
    if (theme == Theme::Dark)
    {
        p.setColor(QPalette::Window, QColor(10, 14, 26));
        p.setColor(QPalette::WindowText, QColor(248, 250, 252));
        p.setColor(QPalette::Base, QColor(7, 10, 18));
        p.setColor(QPalette::AlternateBase, QColor(15, 23, 42));
        p.setColor(QPalette::ToolTipBase, QColor(15, 23, 42));
        p.setColor(QPalette::ToolTipText, QColor(248, 250, 252));
        p.setColor(QPalette::Text, QColor(248, 250, 252));
        p.setColor(QPalette::Button, QColor(15, 23, 42));
        p.setColor(QPalette::ButtonText, QColor(248, 250, 252));
        p.setColor(QPalette::BrightText, QColor(56, 189, 248));
        p.setColor(QPalette::Link, QColor(56, 189, 248));
        p.setColor(QPalette::Highlight, QColor(2, 132, 199));
        p.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
        p.setColor(QPalette::PlaceholderText, QColor(100, 116, 139));

        p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(71, 85, 105));
        p.setColor(QPalette::Disabled, QPalette::Text, QColor(71, 85, 105));
        p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(71, 85, 105));
        p.setColor(QPalette::Disabled, QPalette::Highlight, QColor(30, 41, 59));
        p.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(71, 85, 105));
    }
    else
    {
        const QColor text(0x1d, 0x1d, 0x1f);
        const QColor muted(0x8e, 0x8e, 0x93);
        const QColor accent(0x00, 0x71, 0xe3);
        p.setColor(QPalette::Window, QColor(0xf5, 0xf5, 0xf7));
        p.setColor(QPalette::WindowText, text);
        p.setColor(QPalette::Base, Qt::white);
        p.setColor(QPalette::AlternateBase, QColor(0xf5, 0xf5, 0xf7));
        p.setColor(QPalette::ToolTipBase, Qt::white);
        p.setColor(QPalette::ToolTipText, text);
        p.setColor(QPalette::Text, text);
        p.setColor(QPalette::Button, Qt::white);
        p.setColor(QPalette::ButtonText, text);
        p.setColor(QPalette::BrightText, accent);
        p.setColor(QPalette::Link, accent);
        p.setColor(QPalette::Highlight, accent);
        p.setColor(QPalette::HighlightedText, Qt::white);
        p.setColor(QPalette::PlaceholderText, muted);

        p.setColor(QPalette::Disabled, QPalette::WindowText, muted);
        p.setColor(QPalette::Disabled, QPalette::Text, muted);
        p.setColor(QPalette::Disabled, QPalette::ButtonText, muted);
        p.setColor(QPalette::Disabled, QPalette::Highlight, QColor(0xd1, 0xd1, 0xd6));
        p.setColor(QPalette::Disabled, QPalette::HighlightedText, muted);
    }
    return p;
}

void ThemeManager::apply(Theme theme)
{
    if (m_applied && m_theme == theme)
        return;

    if (!m_applied)
        loadFonts();

    m_theme = theme;
    m_applied = true;

    // Fusion renders identically on Windows, Linux and macOS and respects the palette.
    qApp->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    qApp->setPalette(buildPalette(theme));
    qApp->setStyleSheet(loadStyleSheet(theme));
    restyleAllWidgets();

    emit themeChanged(theme);
}
