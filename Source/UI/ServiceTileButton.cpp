#include "ServiceTileButton.h"
#include "MainWindow.h"
#include "ThemeManager.h"

#include <QCursor>
#include <QEasingCurve>
#include <QEnterEvent>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPolygon>
#include <QPainterPath>
#include <QResizeEvent>
#include <QShowEvent>

// ServiceTileButton Implementation (Metro UI Tile Card with "NEW" Red Ribbon)
// ============================================================================

bool ServiceTileButton::s_isAnyLaunching = false;

ServiceTileButton::ServiceTileButton(
    const QIcon &icon, const QString &title, const QString &badgeText, Tier tier, QWidget *parent)
    : ServiceTileButton(icon, title, badgeText, tier, ServiceFlag::None, parent)
{}

ServiceTileButton::ServiceTileButton(
    const QIcon &icon, const QString &title, const QString &badgeText, Tier tier, ServiceFlag flag, QWidget *parent)
    : ServiceTileButton(
          icon,
          title,
          badgeText,
          tier,
          flag != ServiceFlag::None,
          (flag == ServiceFlag::Beta ? QString::fromUtf8("BETA") : QString::fromUtf8("NEW")),
          parent)
{
    m_flag = flag;
    m_details.flag = flag;
}

ServiceTileButton::ServiceTileButton(
    const QIcon &icon,
    const QString &title,
    const QString &badgeText,
    Tier tier,
    bool showRibbon,
    const QString &ribbonText,
    QWidget *parent)
    : QPushButton(parent)
    , m_title(title)
    , m_badgeText(badgeText)
    , m_tier(tier)
    , m_showRibbon(showRibbon)
    , m_ribbonText(ribbonText)
{
    if (!showRibbon)
        m_flag = ServiceFlag::None;
    else if (ribbonText.contains("BETA", Qt::CaseInsensitive) && ribbonText.contains("NEW", Qt::CaseInsensitive))
        m_flag = ServiceFlag::NewBeta;
    else if (ribbonText.compare("BETA", Qt::CaseInsensitive) == 0)
        m_flag = ServiceFlag::Beta;
    else
        m_flag = ServiceFlag::New;

    m_details.title = title;
    m_details.badgeText = badgeText;
    m_details.tier = tier;
    m_details.icon = icon;
    m_details.flag = m_flag;

    setIcon(icon);
    setAttribute(Qt::WA_Hover, true);
    setMouseTracking(true);
    setFixedHeight(70);
    setMinimumWidth(260);
    setMaximumWidth(360);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setCursor(tier == Tier::Disabled ? Qt::ForbiddenCursor : Qt::PointingHandCursor);
    setStyleSheet(
        "ServiceTileButton { background: transparent; border: none; outline: none; padding: 0px; margin: 0px; }");

    m_btnHelp = new QPushButton(QString::fromUtf8("(?) Что это?"), this);
    m_btnHelp->setObjectName(QStringLiteral("serviceHelpBtn"));
    m_btnHelp->setCursor(Qt::PointingHandCursor);
    m_btnHelp->setFocusPolicy(Qt::NoFocus);
    m_btnHelp->setStyleSheet(QStringLiteral(
        "QPushButton#serviceHelpBtn {"
        "    background-color: transparent;"
        "    color: #4DA3FF;"
        "    border: none;"
        "    border-radius: 8px;"
        "    font-size: 11px;"
        "    font-weight: 600;"
        "    padding: 1px 6px;"
        "    min-width: 76px;"
        "    min-height: 20px;"
        "    max-height: 22px;"
        "}"
        "QPushButton#serviceHelpBtn:hover {"
        "    background-color: rgba(0, 113, 227, 0.12);"
        "}"
        "QPushButton#serviceHelpBtn:pressed {"
        "    background-color: rgba(0, 113, 227, 0.2);"
        "}"));
    m_btnHelp->setToolTip(QString::fromUtf8("Подробная информация о сервисе и условиях"));
    connect(m_btnHelp, &QPushButton::clicked, this, &ServiceTileButton::showInfoDialog);

    const int initW = 280;
    const int btnW = 86;
    const int btnH = 22;
    m_btnHelp->setGeometry(initW - btnW - 8, 70 - btnH - 6, btnW, btnH);
    m_btnHelp->setVisible(true);
    m_btnHelp->show();
    m_btnHelp->raise();

    m_rippleAnim = new QVariantAnimation(this);
    m_rippleAnim->setDuration(380);
    m_rippleAnim->setStartValue(0.0);
    m_rippleAnim->setEndValue(1.0);
    m_rippleAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_rippleAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &val) {
        m_rippleProgress = val.toReal();
        const qreal maxR = qMax(width(), height()) * 1.35;
        m_rippleRadius = m_rippleProgress * maxR;
        m_rippleOpacity = 0.40 * (1.0 - m_rippleProgress);
        update();
    });
    connect(m_rippleAnim, &QVariantAnimation::finished, this, [this]() {
        m_rippleRadius = 0.0;
        m_rippleOpacity = 0.0;
        update();
    });

    m_clickAnim = new QVariantAnimation(this);
    m_clickAnim->setDuration(1000);
    m_clickAnim->setStartValue(0.0);
    m_clickAnim->setEndValue(1.0);
    m_clickAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_clickAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &val) {
        m_clickProgress = val.toReal();
        update();
    });
    connect(m_clickAnim, &QVariantAnimation::finished, this, [this]() {
        m_clickProgress = 0.0;
        update();
    });
}

void ServiceTileButton::setTier(Tier tier)
{
    m_tier = tier;
    m_details.tier = tier;
    setCursor((!m_serviceActive || tier == Tier::Disabled) ? Qt::ForbiddenCursor : Qt::PointingHandCursor);
    update();
}

void ServiceTileButton::setTitle(const QString &title)
{
    m_title = title;
    m_details.title = title;
    update();
}

void ServiceTileButton::setBadgeText(const QString &text)
{
    m_badgeText = text;
    m_details.badgeText = text;
    update();
}

void ServiceTileButton::setServiceActive(bool active)
{
    m_serviceActive = active;
    m_details.isActive = active;
    setCursor((!m_serviceActive || m_tier == Tier::Disabled) ? Qt::ForbiddenCursor : Qt::PointingHandCursor);
    update();
}

void ServiceTileButton::setServiceDetails(const Details &details)
{
    m_details = details;
    m_serviceActive = details.isActive;
    m_sortScore = details.sortScore;
    if (details.flag != ServiceFlag::None)
        setFlag(details.flag);
    setCursor((!m_serviceActive || m_tier == Tier::Disabled) ? Qt::ForbiddenCursor : Qt::PointingHandCursor);
    update();
}

void ServiceTileButton::setFlag(ServiceFlag flag)
{
    m_flag = flag;
    switch (flag)
    {
        case ServiceFlag::None:
            m_showRibbon = false;
            m_ribbonText.clear();
            break;
        case ServiceFlag::New:
            m_showRibbon = true;
            m_ribbonText = QString::fromUtf8("NEW");
            break;
        case ServiceFlag::Beta:
            m_showRibbon = true;
            m_ribbonText = QString::fromUtf8("BETA");
            break;
        case ServiceFlag::NewBeta:
            m_showRibbon = true;
            m_ribbonText = QString::fromUtf8("NEW");
            break;
    }
    m_details.flag = flag;
    update();
}

void ServiceTileButton::showInfoDialog()
{
    ServiceInfoDialog dlg(m_details, this);
    dlg.exec();
}

void ServiceTileButton::showEvent(QShowEvent *event)
{
    QPushButton::showEvent(event);
    if (m_btnHelp)
    {
        const int btnW = 86;
        const int btnH = 22;
        const int btnX = width() - btnW - 8;
        const int btnY = height() - btnH - 6;
        m_btnHelp->setGeometry(btnX, btnY, btnW, btnH);
        m_btnHelp->setVisible(true);
        m_btnHelp->show();
        m_btnHelp->raise();
    }
}

void ServiceTileButton::resizeEvent(QResizeEvent *event)
{
    QPushButton::resizeEvent(event);
    if (m_btnHelp)
    {
        const int btnW = 86;
        const int btnH = 22;
        const int btnX = width() - btnW - 8;
        const int btnY = height() - btnH - 6;
        m_btnHelp->setGeometry(btnX, btnY, btnW, btnH);
        m_btnHelp->setVisible(true);
        m_btnHelp->show();
        m_btnHelp->raise();
    }
}

void ServiceTileButton::setShowRibbon(bool show)
{
    m_showRibbon = show;
    update();
}

void ServiceTileButton::setRibbonText(const QString &text)
{
    m_ribbonText = text;
    update();
}

void ServiceTileButton::triggerClickAnimation()
{
    if (m_clickAnim)
    {
        m_clickAnim->stop();
        m_clickProgress = 0.0;
        m_clickAnim->start();
    }
}

QSize ServiceTileButton::sizeHint() const
{
    return QSize(280, 70);
}

QSize ServiceTileButton::minimumSizeHint() const
{
    return QSize(260, 70);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void ServiceTileButton::enterEvent(QEnterEvent *event)
{
    QPushButton::enterEvent(event);
    m_isHovered = true;
    m_mousePos = event->position().toPoint();
    update();
}
#else
void ServiceTileButton::enterEvent(QEvent *event)
{
    QPushButton::enterEvent(event);
    m_isHovered = true;
    m_mousePos = mapFromGlobal(QCursor::pos());
    update();
}
#endif

void ServiceTileButton::leaveEvent(QEvent *event)
{
    QPushButton::leaveEvent(event);
    m_isHovered = false;
    update();
}

void ServiceTileButton::mouseMoveEvent(QMouseEvent *event)
{
    QPushButton::mouseMoveEvent(event);
    m_mousePos = event->pos();
    m_isHovered = true;
    update();
}

void ServiceTileButton::mousePressEvent(QMouseEvent *event)
{
    if (m_btnHelp && m_btnHelp->geometry().contains(event->pos()))
    {
        showInfoDialog();
        event->accept();
        return;
    }
    if (m_isLaunching || s_isAnyLaunching || !m_serviceActive || m_tier == Tier::Disabled)
        return;
    m_pressPos = event->pos();
    if (isEnabled())
    {
        m_rippleAnim->stop();
        m_rippleProgress = 0.0;
        m_rippleRadius = 15.0;
        m_rippleOpacity = 0.45;
        m_rippleAnim->start();
    }
    QPushButton::mousePressEvent(event);
}

void ServiceTileButton::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_btnHelp && m_btnHelp->geometry().contains(event->pos()))
    {
        event->accept();
        return;
    }
    if (m_isLaunching || s_isAnyLaunching || !m_serviceActive || m_tier == Tier::Disabled)
        return;
    if (rect().contains(event->pos()) && isEnabled())
    {
        triggerClickAnimation();
    }
    QPushButton::mouseReleaseEvent(event);
}

void ServiceTileButton::keyPressEvent(QKeyEvent *event)
{
    if (m_isLaunching || s_isAnyLaunching || !m_serviceActive || m_tier == Tier::Disabled)
    {
        event->accept();
        return;
    }
    QPushButton::keyPressEvent(event);
}

void ServiceTileButton::keyReleaseEvent(QKeyEvent *event)
{
    if (m_isLaunching || s_isAnyLaunching || !m_serviceActive || m_tier == Tier::Disabled)
    {
        event->accept();
        return;
    }
    QPushButton::keyReleaseEvent(event);
}

namespace
{
struct TierStyle
{
    QColor text;
    QColor tint;
};

// Apple system colors, tuned per theme for contrast.
TierStyle tierStyle(ServiceTileButton::Tier tier, bool enabled, bool dark)
{
    auto make = [](const QColor &text, qreal tintAlpha) {
        QColor tint = text;
        tint.setAlphaF(tintAlpha);
        return TierStyle {text, tint};
    };
    if (!enabled || tier == ServiceTileButton::Tier::Disabled)
        return make(dark ? QColor(0x98, 0x98, 0x9d) : QColor(0x6e, 0x6e, 0x73), 0.16);
    switch (tier)
    {
        case ServiceTileButton::Tier::Vip:
            return make(dark ? QColor(0xff, 0xb3, 0x40) : QColor(0xb2, 0x50, 0x00), 0.16);
        case ServiceTileButton::Tier::Free:
            return make(dark ? QColor(0x30, 0xd1, 0x58) : QColor(0x24, 0x8a, 0x3d), 0.16);
        case ServiceTileButton::Tier::Dynamic:
            return make(dark ? QColor(0xbf, 0x5a, 0xf2) : QColor(0x89, 0x44, 0xab), 0.16);
        case ServiceTileButton::Tier::Credit:
        default:
            return make(dark ? QColor(0x3d, 0x9b, 0xff) : QColor(0x00, 0x71, 0xe3), 0.16);
    }
}
} // namespace

void ServiceTileButton::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const bool dark = ThemeManager::instance()->isDark();
    const QColor accent(0x00, 0x71, 0xe3);
    const int w = width();
    const int h = height();
    const bool enabled = isEnabled() && m_serviceActive && (m_tier != Tier::Disabled);
    const bool hovered = enabled && (underMouse() || m_isHovered);
    const bool pressed = enabled && isDown();

    QPainterPath cardPath;
    cardPath.addRoundedRect(QRectF(0.5, 0.5, w - 1, h - 1), 16, 16);

    // 1. Card: flat surface with a hairline border
    QColor fill = dark ? QColor(0x2c, 0x2c, 0x2e) : QColor(0xff, 0xff, 0xff);
    QColor border = dark ? QColor(255, 255, 255, 18) : QColor(0, 0, 0, 20);
    if (pressed)
        fill = dark ? QColor(0x26, 0x26, 0x28) : QColor(0xf2, 0xf2, 0xf5);
    else if (hovered)
    {
        fill = dark ? QColor(0x34, 0x34, 0x36) : QColor(0xfc, 0xfc, 0xfd);
        border = QColor(accent.red(), accent.green(), accent.blue(), 110);
    }
    painter.fillPath(cardPath, fill);

    // Soft press feedback, clipped to the card
    painter.save();
    painter.setClipPath(cardPath);
    if (m_rippleOpacity > 0.01)
    {
        QColor c = accent;
        c.setAlphaF(qBound<qreal>(0.0, m_rippleOpacity * 0.28, 0.2));
        painter.setPen(Qt::NoPen);
        painter.setBrush(c);
        painter.drawEllipse(m_pressPos, int(m_rippleRadius), int(m_rippleRadius));
    }
    if (m_clickProgress > 0.0)
    {
        QColor c = accent;
        c.setAlphaF((1.0 - m_clickProgress) * 0.12);
        painter.fillPath(cardPath, c);
    }
    painter.restore();

    painter.setPen(QPen(border, 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(cardPath);

    // 2. Icon on a rounded plate
    const int plateSize = 48;
    const QRect plateRect(14, (h - plateSize) / 2, plateSize, plateSize);
    painter.setPen(Qt::NoPen);
    painter.setBrush(dark ? QColor(0x1c, 0x1c, 0x1e) : QColor(0xf5, 0xf5, 0xf7));
    painter.drawRoundedRect(plateRect, 12, 12);
    painter.setBrush(Qt::NoBrush);

    const QRect iconRect = plateRect.adjusted(8, 8, -8, -8);
    painter.setOpacity(enabled ? 1.0 : 0.4);
    icon().paint(&painter, iconRect);
    painter.setOpacity(1.0);

    // 3. Flags (NEW / BETA) as small flat capsules in the top-right corner
    struct Flag
    {
        QString text;
        QColor color;
    };
    QVector<Flag> flags;
    if (m_showRibbon && enabled)
    {
        const bool hasNew = (static_cast<int>(m_flag) & static_cast<int>(ServiceFlag::New)) != 0;
        const bool hasBeta = (static_cast<int>(m_flag) & static_cast<int>(ServiceFlag::Beta)) != 0;
        const QColor red(0xff, 0x3b, 0x30);
        const QColor orange(0xff, 0x95, 0x00);
        if (hasNew)
            flags.append({QStringLiteral("NEW"), red});
        if (hasBeta)
            flags.append({QStringLiteral("BETA"), orange});
        if (flags.isEmpty() && !m_ribbonText.isEmpty())
        {
            const bool beta = m_ribbonText.contains(QLatin1String("BETA"), Qt::CaseInsensitive);
            flags.append({m_ribbonText, beta ? orange : red});
        }
    }
    QFont flagFont = font();
    flagFont.setPixelSize(9);
    flagFont.setWeight(QFont::Bold);
    flagFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.5);
    const QFontMetrics ffm(flagFont);
    int flagsRight = w - 12;
    int flagsWidth = 0;
    for (int i = flags.size() - 1; i >= 0; --i)
    {
        const int fw = ffm.horizontalAdvance(flags[i].text) + 14;
        const QRect r(flagsRight - fw, 10, fw, 16);
        painter.setPen(Qt::NoPen);
        painter.setBrush(flags[i].color);
        painter.drawRoundedRect(r, 8, 8);
        painter.setFont(flagFont);
        painter.setPen(Qt::white);
        painter.drawText(r, Qt::AlignCenter, flags[i].text);
        flagsRight -= fw + 4;
        flagsWidth += fw + 4;
    }

    // 4. Title
    const int textX = plateRect.right() + 14;
    const int maxTitleW = qMax(40, w - textX - 12 - (flagsWidth ? flagsWidth + 4 : 0));
    QFont titleFont = font();
    titleFont.setPixelSize(14);
    titleFont.setWeight(QFont::DemiBold);
    painter.setFont(titleFont);
    painter.setPen(
        enabled ? (dark ? QColor(0xf5, 0xf5, 0xf7) : QColor(0x1d, 0x1d, 0x1f))
                : (dark ? QColor(0x63, 0x63, 0x66) : QColor(0xae, 0xae, 0xb2)));
    painter.drawText(
        QRect(textX, 11, maxTitleW, 22),
        Qt::AlignLeft | Qt::AlignVCenter,
        QFontMetrics(titleFont).elidedText(m_title, Qt::ElideRight, maxTitleW));

    // 5. Tier badge: tinted capsule, no border
    if (!m_badgeText.isEmpty())
    {
        const TierStyle ts = tierStyle(m_tier, enabled, dark);
        QFont badgeFont = font();
        badgeFont.setPixelSize(11);
        badgeFont.setWeight(QFont::DemiBold);
        const QFontMetrics bfm(badgeFont);
        const int badgeH = 20;
        const int maxPillW = qMax(40, w - textX - 104);
        const int pillW = qBound(44, bfm.horizontalAdvance(m_badgeText) + 18, maxPillW);
        const QRect pillRect(textX, h - badgeH - 12, pillW, badgeH);
        painter.setPen(Qt::NoPen);
        painter.setBrush(ts.tint);
        painter.drawRoundedRect(pillRect, 10, 10);
        painter.setBrush(Qt::NoBrush);
        painter.setFont(badgeFont);
        painter.setPen(ts.text);
        painter.drawText(pillRect, Qt::AlignCenter, bfm.elidedText(m_badgeText, Qt::ElideRight, pillRect.width() - 10));
    }
}

QColor ServiceTileButton::accentColor() const
{
    if (!isEnabled() || !m_serviceActive || m_tier == Tier::Disabled)
        return QColor(71, 85, 105);

    switch (m_tier)
    {
        case Tier::Vip:
            return QColor(245, 158, 11); // Gold #F59E0B
        case Tier::Free:
            return QColor(16, 185, 129); // Emerald #10B981
        case Tier::Dynamic:
            return QColor(129, 140, 248); // Indigo #818CF8
        case Tier::Credit:
        default:
            return QColor(0, 229, 255); // Cyan #00E5FF
    }
}

// ============================================================================
