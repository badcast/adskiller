#include "ServiceTileButton.h"
#include "MainWindow.h"

#include <QCursor>
#include <QEasingCurve>
#include <QEnterEvent>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPolygon>
#include <QResizeEvent>
#include <QShowEvent>

// ServiceTileButton Implementation (Metro UI Tile Card with "NEW" Red Ribbon)
// ============================================================================

bool ServiceTileButton::s_isAnyLaunching = false;

ServiceTileButton::ServiceTileButton(const QIcon &icon, const QString &title, const QString &badgeText, Tier tier, QWidget *parent) : ServiceTileButton(icon, title, badgeText, tier, ServiceFlag::None, parent)
{
}

ServiceTileButton::ServiceTileButton(const QIcon &icon, const QString &title, const QString &badgeText, Tier tier, ServiceFlag flag, QWidget *parent)
    : ServiceTileButton(icon, title, badgeText, tier, flag != ServiceFlag::None, (flag == ServiceFlag::Beta ? QString::fromUtf8("BETA") : QString::fromUtf8("NEW")), parent)
{
    m_flag = flag;
    m_details.flag = flag;
}

ServiceTileButton::ServiceTileButton(const QIcon &icon, const QString &title, const QString &badgeText, Tier tier, bool showRibbon, const QString &ribbonText, QWidget *parent)
    : QPushButton(parent), m_title(title), m_badgeText(badgeText), m_tier(tier), m_showRibbon(showRibbon), m_ribbonText(ribbonText)
{
    if(!showRibbon)
        m_flag = ServiceFlag::None;
    else if(ribbonText.contains("BETA", Qt::CaseInsensitive) && ribbonText.contains("NEW", Qt::CaseInsensitive))
        m_flag = ServiceFlag::NewBeta;
    else if(ribbonText.compare("BETA", Qt::CaseInsensitive) == 0)
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
    setStyleSheet("ServiceTileButton { background: transparent; border: none; outline: none; padding: 0px; margin: 0px; }");

    m_btnHelp = new QPushButton(QString::fromUtf8("(?) Что это?"), this);
    m_btnHelp->setObjectName(QStringLiteral("serviceHelpBtn"));
    m_btnHelp->setCursor(Qt::PointingHandCursor);
    m_btnHelp->setFocusPolicy(Qt::NoFocus);
    m_btnHelp->setStyleSheet(QStringLiteral(
        "QPushButton#serviceHelpBtn {"
        "    background-color: transparent;"
        "    color: #38BDF8;"
        "    font-family: 'Segoe UI', -apple-system, sans-serif;"
        "    font-size: 11px;"
        "    font-weight: bold;"
        "    padding: 1px 6px;"
        "    min-width: 76px;"
        "    min-height: 20px;"
        "    max-height: 22px;"
        "}"
        "QPushButton#serviceHelpBtn:hover {"
        "    color: #FFFFFF;"
        "    border: 1px solid #38BDF8;"
        "}"
        "QPushButton#serviceHelpBtn:pressed {"
        "    color: #E0F2FE;"
        "    border: 1px solid #0284C7;"
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
    connect(
        m_rippleAnim,
        &QVariantAnimation::valueChanged,
        this,
        [this](const QVariant &val)
        {
            m_rippleProgress = val.toReal();
            const qreal maxR = qMax(width(), height()) * 1.35;
            m_rippleRadius = m_rippleProgress * maxR;
            m_rippleOpacity = 0.40 * (1.0 - m_rippleProgress);
            update();
        });
    connect(
        m_rippleAnim,
        &QVariantAnimation::finished,
        this,
        [this]()
        {
            m_rippleRadius = 0.0;
            m_rippleOpacity = 0.0;
            update();
        });

    m_clickAnim = new QVariantAnimation(this);
    m_clickAnim->setDuration(1000);
    m_clickAnim->setStartValue(0.0);
    m_clickAnim->setEndValue(1.0);
    m_clickAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(
        m_clickAnim,
        &QVariantAnimation::valueChanged,
        this,
        [this](const QVariant &val)
        {
            m_clickProgress = val.toReal();
            update();
        });
    connect(
        m_clickAnim,
        &QVariantAnimation::finished,
        this,
        [this]()
        {
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
    if(details.flag != ServiceFlag::None)
        setFlag(details.flag);
    setCursor((!m_serviceActive || m_tier == Tier::Disabled) ? Qt::ForbiddenCursor : Qt::PointingHandCursor);
    update();
}

void ServiceTileButton::setFlag(ServiceFlag flag)
{
    m_flag = flag;
    switch(flag)
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
    if(m_btnHelp)
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
    if(m_btnHelp)
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
    if(m_clickAnim)
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
    if(m_btnHelp && m_btnHelp->geometry().contains(event->pos()))
    {
        showInfoDialog();
        event->accept();
        return;
    }
    if(m_isLaunching || s_isAnyLaunching || !m_serviceActive || m_tier == Tier::Disabled)
        return;
    m_pressPos = event->pos();
    if(isEnabled())
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
    if(m_btnHelp && m_btnHelp->geometry().contains(event->pos()))
    {
        event->accept();
        return;
    }
    if(m_isLaunching || s_isAnyLaunching || !m_serviceActive || m_tier == Tier::Disabled)
        return;
    if(rect().contains(event->pos()) && isEnabled())
    {
        triggerClickAnimation();
    }
    QPushButton::mouseReleaseEvent(event);
}

void ServiceTileButton::keyPressEvent(QKeyEvent *event)
{
    if(m_isLaunching || s_isAnyLaunching || !m_serviceActive || m_tier == Tier::Disabled)
    {
        event->accept();
        return;
    }
    QPushButton::keyPressEvent(event);
}

void ServiceTileButton::keyReleaseEvent(QKeyEvent *event)
{
    if(m_isLaunching || s_isAnyLaunching || !m_serviceActive || m_tier == Tier::Disabled)
    {
        event->accept();
        return;
    }
    QPushButton::keyReleaseEvent(event);
}

void ServiceTileButton::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const int w = width();
    const int h = height();
    const bool enabled = isEnabled() && m_serviceActive && (m_tier != Tier::Disabled);
    const bool hovered = enabled && (underMouse() || m_isHovered);
    const bool pressed = enabled && isDown();

    // 1. Metro Card Shell: Base Dark Obsidian Background
    QColor bgColor;
    QColor borderColor;
    if(!enabled)
    {
        bgColor = QColor(11, 15, 25);     // #0B0F19
        borderColor = QColor(30, 41, 59); // #1E293B
    }
    else if(pressed)
    {
        bgColor = QColor(8, 14, 26);       // #080E1A
        borderColor = QColor(2, 132, 199); // #0284C7
    }
    else if(hovered)
    {
        bgColor = QColor(19, 31, 55);       // #131F37
        borderColor = QColor(56, 189, 248); // #38BDF8
    }
    else
    {
        bgColor = QColor(15, 23, 42);     // #0F172A
        borderColor = QColor(30, 41, 59); // #1E293B
    }

    painter.fillRect(QRect(0, 0, w, h), bgColor);

    // 1b. Windows 10 Reveal Highlight: cursor flashlight gradient
    if(enabled && hovered)
    {
        QRadialGradient revealLight(m_mousePos, 160);
        revealLight.setColorAt(0.0, QColor(0, 229, 255, 38)); // Electric cyan soft spotlight
        revealLight.setColorAt(0.4, QColor(2, 132, 199, 18)); // Deep azure falloff
        revealLight.setColorAt(1.0, QColor(2, 132, 199, 0));  // Transparent
        painter.fillRect(QRect(0, 0, w, h), revealLight);
    }

    // 1c. Windows 10 Bubble Press Ripple Effect
    if(m_rippleOpacity > 0.01)
    {
        painter.save();
        painter.setClipRect(QRect(0, 0, w, h));
        QRadialGradient bubble(m_pressPos, qMax<qreal>(10.0, m_rippleRadius));
        int alpha = qBound(0, static_cast<int>(m_rippleOpacity * 255), 255);
        bubble.setColorAt(0.0, QColor(0, 229, 255, alpha));
        bubble.setColorAt(0.5, QColor(2, 132, 199, alpha * 2 / 3));
        bubble.setColorAt(0.85, QColor(14, 165, 233, alpha / 3));
        bubble.setColorAt(1.0, QColor(2, 132, 199, 0));
        painter.setBrush(bubble);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(m_pressPos, static_cast<int>(m_rippleRadius), static_cast<int>(m_rippleRadius));
        painter.restore();
    }

    // 1d. Border: Reveal Border glow near mouse cursor
    if(enabled && hovered)
    {
        QRadialGradient borderGlow(m_mousePos, 140);
        borderGlow.setColorAt(0.0, QColor(0, 229, 255, 230));  // Hot glowing edge near mouse
        borderGlow.setColorAt(0.5, QColor(56, 189, 248, 120)); // Soft cyan
        borderGlow.setColorAt(1.0, borderColor);               // Base border color
        painter.setPen(QPen(QBrush(borderGlow), 1));
        painter.drawRect(QRect(0, 0, w - 1, h - 1));
    }
    else
    {
        painter.setPen(QPen(borderColor, 1));
        painter.drawRect(QRect(0, 0, w - 1, h - 1));
    }

    // 2. Left Status Accent Stripe (4px width)
    QColor accentColor;
    if(!enabled || m_tier == Tier::Disabled)
    {
        accentColor = QColor(71, 85, 105); // #475569 Slate
    }
    else
    {
        switch(m_tier)
        {
            case Tier::Vip:
                accentColor = hovered ? QColor(251, 191, 36) : QColor(245, 158, 11); // Gold #F59E0B / #FBBF24
                break;
            case Tier::Free:
                accentColor = hovered ? QColor(52, 211, 153) : QColor(16, 185, 129); // Emerald #10B981 / #34D399
                break;
            case Tier::Dynamic:
                accentColor = hovered ? QColor(129, 140, 248) : QColor(99, 102, 241); // Indigo #818CF8
                break;
            case Tier::Credit:
            default:
                accentColor = hovered ? QColor(56, 189, 248) : QColor(14, 165, 233); // Sky Cyan #38BDF8
                break;
        }
    }
    painter.fillRect(QRect(0, 0, 4, h), accentColor);

    // 3. Recessed Icon Plate Sub-Panel
    const int plateX = 12;
    const int plateY = (h - 54) / 2;
    const int plateSize = 54;
    const QRect plateRect(plateX, plateY, plateSize, plateSize);

    const QColor plateBg = hovered ? QColor(11, 17, 30) : QColor(7, 10, 18);
    const QColor plateBorder = hovered ? QColor(51, 65, 85) : QColor(26, 36, 54);
    painter.fillRect(plateRect, plateBg);
    painter.setPen(QPen(plateBorder, 1));
    painter.drawRect(plateRect);

    // Center icon inside plate (42x42)
    const QRect iconRect(plateX + 6, plateY + 6, plateSize - 12, plateSize - 12);
    if(!enabled)
    {
        painter.setOpacity(0.35);
        icon().paint(&painter, iconRect);
        painter.setOpacity(1.0);
    }
    else
    {
        icon().paint(&painter, iconRect);
    }

    // 4. Service Title Typography
    const int textX = plateRect.right() + 12;
    const int textTop = plateY + 1;
    // Reserved width for top-right ribbon
    const int ribbonReservation = (m_showRibbon && enabled) ? 68 : 12;
    const int maxTitleW = qMax(40, w - textX - ribbonReservation);

    QFont titleFont("Segoe UI", 10, QFont::Bold);
    painter.setFont(titleFont);
    QFontMetrics tfm(titleFont);
    const QString elidedTitle = tfm.elidedText(m_title, Qt::ElideRight, maxTitleW);

    QColor titleColor;
    if(!enabled)
        titleColor = QColor(100, 116, 139); // #64748B
    else if(hovered)
        titleColor = QColor(56, 189, 248); // #38BDF8
    else
        titleColor = QColor(248, 250, 252); // #F8FAFC

    painter.setPen(titleColor);
    painter.drawText(QRect(textX, textTop, maxTitleW, 22), Qt::AlignLeft | Qt::AlignVCenter, elidedTitle);

    // 5. Pricing / Status Badge Pill
    if(!m_badgeText.isEmpty())
    {
        QFont badgeFont("Segoe UI", 8, QFont::Bold);
        painter.setFont(badgeFont);
        QFontMetrics bfm(badgeFont);

        QColor pillBg;
        QColor pillBorder;
        QColor pillText;

        if(!enabled || m_tier == Tier::Disabled)
        {
            pillBg = QColor(24, 30, 41);
            pillBorder = QColor(51, 65, 85);
            pillText = QColor(148, 163, 184);
        }
        else
        {
            switch(m_tier)
            {
                case Tier::Vip:
                    pillBg = QColor(36, 24, 6);
                    pillBorder = QColor(146, 64, 14);
                    pillText = QColor(251, 191, 36);
                    break;
                case Tier::Free:
                    pillBg = QColor(6, 37, 22);
                    pillBorder = QColor(5, 150, 105);
                    pillText = QColor(52, 211, 153);
                    break;
                case Tier::Dynamic:
                    pillBg = QColor(14, 27, 56);
                    pillBorder = QColor(37, 99, 235);
                    pillText = QColor(96, 165, 250);
                    break;
                case Tier::Credit:
                default:
                    pillBg = QColor(12, 32, 56);
                    pillBorder = QColor(2, 132, 199);
                    pillText = QColor(56, 189, 248);
                    break;
            }
        }

        const int badgeH = 22;
        const int badgeY = plateRect.bottom() - badgeH;
        const int maxPillW = qMax(40, w - textX - 98);
        const int textW = bfm.horizontalAdvance(m_badgeText);
        const int pillW = qBound(50, textW + 16, maxPillW);
        const QRect pillRect(textX, badgeY, pillW, badgeH);

        painter.fillRect(pillRect, pillBg);
        painter.setPen(QPen(pillBorder, 1));
        painter.drawRect(pillRect);

        painter.setPen(pillText);
        const QString elidedBadge = bfm.elidedText(m_badgeText, Qt::ElideRight, pillRect.width() - 8);
        painter.drawText(pillRect, Qt::AlignCenter, elidedBadge);
    }

    // 6. Top-Right Ribbons (Vertical list if multiple flags are present)
    if(m_showRibbon)
    {
        struct RibbonEntry
        {
            QString text;
            bool isBeta;
        };
        QVector<RibbonEntry> ribbons;

        const bool hasNew = (static_cast<int>(m_flag) & static_cast<int>(ServiceFlag::New)) != 0;
        const bool hasBeta = (static_cast<int>(m_flag) & static_cast<int>(ServiceFlag::Beta)) != 0;

        if(hasNew && hasBeta)
        {
            ribbons.append({QString::fromUtf8("NEW"), false});
            ribbons.append({QString::fromUtf8("BETA"), true});
        }
        else if(hasBeta)
        {
            ribbons.append({QString::fromUtf8("BETA"), true});
        }
        else if(hasNew)
        {
            ribbons.append({QString::fromUtf8("NEW"), false});
        }
        else if(!m_ribbonText.isEmpty())
        {
            if(m_ribbonText.contains("BETA", Qt::CaseInsensitive) && m_ribbonText.contains("NEW", Qt::CaseInsensitive))
            {
                ribbons.append({QString::fromUtf8("NEW"), false});
                ribbons.append({QString::fromUtf8("BETA"), true});
            }
            else if(m_ribbonText.compare("BETA", Qt::CaseInsensitive) == 0)
            {
                ribbons.append({QString::fromUtf8("BETA"), true});
            }
            else
            {
                ribbons.append({m_ribbonText, false});
            }
        }

        const bool isMulti = ribbons.size() > 1;
        const int ribbonW = isMulti ? 52 : 56;
        const int ribbonH = isMulti ? 18 : 20;
        const int notch = isMulti ? 6 : 7;
        const int gap = 3;
        const int rx = w - ribbonW;

        QFont ribbonFont("Segoe UI", isMulti ? 7 : 8, QFont::Black);
        ribbonFont.setBold(true);
        ribbonFont.setLetterSpacing(QFont::AbsoluteSpacing, isMulti ? 1.0 : 1.2);

        for(int i = 0; i < ribbons.size(); ++i)
        {
            const auto &entry = ribbons.at(i);
            const int ry = i * (ribbonH + gap);

            // Physical drop shadow behind ribbon
            QPolygon shadowPoly;
            shadowPoly << QPoint(w, ry + 2) << QPoint(rx + 1, ry + 2) << QPoint(rx + notch + 1, ry + ribbonH / 2 + 1) << QPoint(rx + 1, ry + ribbonH + 2) << QPoint(w, ry + ribbonH + 2);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(0, 0, 0, 110));
            painter.drawPolygon(shadowPoly);

            // Ribbon polygon with decorative swallowtail notch cut on the left
            QPolygon ribbonPoly;
            ribbonPoly << QPoint(w, ry) << QPoint(rx, ry) << QPoint(rx + notch, ry + ribbonH / 2) << QPoint(rx, ry + ribbonH) << QPoint(w, ry + ribbonH);

            QLinearGradient ribbonGrad(rx, ry, w, ry + ribbonH);
            QColor strokeColor;
            QColor shadowTextColor;

            if(entry.isBeta)
            {
                if(hovered)
                {
                    ribbonGrad.setColorAt(0.0, QColor(253, 224, 71)); // #FDE047 Amber highlight
                    ribbonGrad.setColorAt(0.3, QColor(251, 191, 36)); // #FBBF24 Amber
                    ribbonGrad.setColorAt(0.8, QColor(217, 119, 6));  // #D97706 Deep amber
                    ribbonGrad.setColorAt(1.0, QColor(180, 83, 9));   // #B45309 Amber shadow
                }
                else
                {
                    ribbonGrad.setColorAt(0.0, QColor(251, 191, 36));  // #FBBF24 Amber
                    ribbonGrad.setColorAt(0.35, QColor(245, 158, 11)); // #F59E0B Gold
                    ribbonGrad.setColorAt(0.8, QColor(217, 119, 6));   // #D97706
                    ribbonGrad.setColorAt(1.0, QColor(146, 64, 14));   // #92400E
                }
                strokeColor = QColor(180, 83, 9);
                shadowTextColor = QColor(120, 53, 15);
            }
            else
            {
                if(hovered)
                {
                    ribbonGrad.setColorAt(0.0, QColor(254, 120, 120));
                    ribbonGrad.setColorAt(0.3, QColor(248, 113, 113));
                    ribbonGrad.setColorAt(0.8, QColor(220, 38, 38));
                    ribbonGrad.setColorAt(1.0, QColor(185, 28, 28));
                }
                else
                {
                    ribbonGrad.setColorAt(0.0, QColor(248, 113, 113)); // #F87171 Ruby highlight
                    ribbonGrad.setColorAt(0.35, QColor(239, 68, 68));  // #EF4444 Vivid red
                    ribbonGrad.setColorAt(0.8, QColor(220, 38, 38));   // #DC2626 Deep red
                    ribbonGrad.setColorAt(1.0, QColor(153, 27, 27));   // #991B1B Crimson shadow
                }
                strokeColor = QColor(185, 28, 28);
                shadowTextColor = QColor(127, 29, 29);
            }

            painter.setPen(QPen(strokeColor, 1));
            painter.setBrush(ribbonGrad);
            painter.drawPolygon(ribbonPoly);

            // Fine glossy highlight line along top edge
            painter.setPen(QPen(QColor(255, 255, 255, 90), 1));
            painter.drawLine(rx + 1, ry + 1, w - 1, ry + 1);

            // Ribbon Inscription
            painter.setFont(ribbonFont);
            const QRect textRect(rx + notch / 2, ry, ribbonW - notch / 2, ribbonH);

            // Embossed 3D shadow for text
            painter.setPen(shadowTextColor);
            painter.drawText(textRect.translated(0, 1), Qt::AlignCenter, entry.text);

            // Pure white foreground
            painter.setPen(Qt::white);
            painter.drawText(textRect, Qt::AlignCenter, entry.text);
        }
    }

    // 7. Click / Activation Pulse Animation
    if(m_clickProgress > 0.0)
    {
        painter.save();
        painter.setClipRect(QRect(0, 0, w, h));

        const QColor accent = accentColor;

        // Luminous light sweep beam across the card
        const int beamW = 100;
        const int sweepX = -beamW + static_cast<int>(m_clickProgress * (w + beamW * 2));
        QLinearGradient sweepGrad(sweepX, 0, sweepX + beamW, 0);
        const int sweepAlpha = static_cast<int>((1.0 - m_clickProgress) * 110);
        sweepGrad.setColorAt(0.0, QColor(accent.red(), accent.green(), accent.blue(), 0));
        sweepGrad.setColorAt(0.5, QColor(accent.red(), accent.green(), accent.blue(), sweepAlpha));
        sweepGrad.setColorAt(1.0, QColor(accent.red(), accent.green(), accent.blue(), 0));
        painter.fillRect(QRect(0, 0, w, h), sweepGrad);

        // Expanding energy shockwave ring from the icon plate center
        const QPoint pCenter = plateRect.center();
        const qreal ringR = 26.0 + m_clickProgress * 55.0;
        const int ringAlpha = static_cast<int>((1.0 - m_clickProgress) * 220);
        painter.setPen(QPen(QColor(accent.red(), accent.green(), accent.blue(), ringAlpha), 2.5));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(pCenter, static_cast<int>(ringR), static_cast<int>(ringR));

        // Neon border flash
        const int borderPulseAlpha = static_cast<int>((1.0 - m_clickProgress) * 240);
        painter.setPen(QPen(QColor(accent.red(), accent.green(), accent.blue(), borderPulseAlpha), 1.5));
        painter.drawRect(QRect(0, 0, w - 1, h - 1));

        painter.restore();
    }
}

QColor ServiceTileButton::accentColor() const
{
    if(!isEnabled() || !m_serviceActive || m_tier == Tier::Disabled)
        return QColor(71, 85, 105);

    switch(m_tier)
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
