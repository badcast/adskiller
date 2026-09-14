#include "AdbDeviceVisualizer.h"

#include <QWidget>
#include <QTimer>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QFont>
#include <QPen>
#include <QColor>
#include <QRadialGradient>
#include <QConicalGradient>
#include <QLinearGradient>
#include <cmath>

#include "adbfront.h"
#include "applefront.h"


AdbDeviceVisualizer::AdbDeviceVisualizer(QWidget *parent) : QWidget(parent), m_status(UNKNOWN), m_time(0.0f), m_connectedTime(0.0f)
{
    s_adbVisualizer = this;
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setMinimumSize(280, 420);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_animTimer = new QTimer(this);
    connect(
        m_animTimer,
        &QTimer::timeout,
        this,
        [this]()
        {
            m_time += 0.035f;
            if(m_status == DEVICE)
                m_connectedTime += 0.035f;
            update();
        });
    m_animTimer->start(25); // ~40 FPS
}

AdbDeviceVisualizer::~AdbDeviceVisualizer()
{
    if(s_adbVisualizer == this)
        s_adbVisualizer = nullptr;
}

void AdbDeviceVisualizer::setIsApple(bool apple)
{
    if(m_isApple != apple)
    {
        m_isApple = apple;
        update();
    }
}

bool AdbDeviceVisualizer::isApple() const
{
    return m_isApple;
}

void AdbDeviceVisualizer::setStatus(AdbConStatus s, const QString &name, const QString &sub)
{
    if(m_status != s || m_devName != name || m_devSub != sub)
    {
        if(m_status != DEVICE && s == DEVICE)
            m_connectedTime = 0.0f;
        m_status = s;
        m_devName = name;
        m_devSub = sub;
        update();
    }
}

void AdbDeviceVisualizer::setStatus(AppleConStatus s, const QString &name, const QString &sub)
{
    m_isApple = true;
    AdbConStatus adbStatus = UNKNOWN;
    if(s == APPLE_DEVICE || s == APPLE_RECOVERY || s == APPLE_DFU)
        adbStatus = DEVICE;
    else if(s == APPLE_UNAUTH)
        adbStatus = UNAUTH;
    setStatus(adbStatus, name, sub);
}

AdbConStatus AdbDeviceVisualizer::status() const
{
    return m_status;
}

void AdbDeviceVisualizer::startAnimation()
{
    if(!m_animTimer->isActive())
        m_animTimer->start(25);
}

void AdbDeviceVisualizer::stopAnimation()
{
    m_animTimer->stop();
}

void AdbDeviceVisualizer::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const int w = width();
    const int h = height();
    const float cx = w * 0.5f;
    const float cy = h * 0.44f;

    // Palette per status
    QColor primaryColor;
    QColor glowColor;
    QColor accentColor;

    if(m_status == DEVICE)
    {
        primaryColor = QColor(16, 185, 129); // Emerald #10B981
        glowColor = QColor(52, 211, 153, 90);
        accentColor = QColor(110, 231, 183);
    }
    else if(m_status == UNAUTH)
    {
        primaryColor = QColor(245, 158, 11); // Amber #F59E0B
        glowColor = QColor(251, 191, 36, 100);
        accentColor = QColor(253, 230, 138);
    }
    else
    {
        primaryColor = QColor(56, 189, 248); // Sky Cyan #38BDF8
        glowColor = QColor(14, 165, 233, 70);
        accentColor = QColor(186, 230, 253);
    }

    // 1. Ambient Background Glow
    QRadialGradient ambientGlow(cx, cy, qMax(w, h) * 0.55);
    ambientGlow.setColorAt(0.0, QColor(glowColor.red(), glowColor.green(), glowColor.blue(), 30));
    ambientGlow.setColorAt(0.65, QColor(glowColor.red(), glowColor.green(), glowColor.blue(), 5));
    ambientGlow.setColorAt(1.0, QColor(11, 15, 25, 0));
    p.fillRect(rect(), ambientGlow);

    // 2. Tech Orbit Ring & Crosshairs
    const float orbitR = qMin(w, h) * 0.46f;
    p.setPen(QPen(QColor(primaryColor.red(), primaryColor.green(), primaryColor.blue(), 30), 1.0f));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(cx, cy), orbitR, orbitR);

    // Rotating tech tick marks on orbit
    p.save();
    p.translate(cx, cy);
    p.rotate(std::fmod(m_time * 25.0f, 360.0f));
    p.setPen(QPen(QColor(accentColor.red(), accentColor.green(), accentColor.blue(), 80), 2.0f));
    for(int a = 0; a < 4; ++a)
    {
        p.drawLine(QPointF(orbitR - 6.0f, 0), QPointF(orbitR + 6.0f, 0));
        p.rotate(90.0);
    }
    p.restore();

    // 3. Animated Concentric Radar Waves (Expanding)
    const float maxRadarR = qMin(w, h) * 0.44f;
    const int ringCount = 3;
    for(int i = 0; i < ringCount; ++i)
    {
        float waveT = std::fmod(m_time * 0.75f + (float) i / (float) ringCount, 1.0f);
        float ringR = waveT * maxRadarR;
        int ringAlpha = static_cast<int>((1.0f - waveT) * (m_status == DEVICE ? 160 : 110));
        if(ringAlpha > 0)
        {
            QPen wavePen(QColor(primaryColor.red(), primaryColor.green(), primaryColor.blue(), ringAlpha));
            wavePen.setWidthF(1.2f);
            if(i % 2 == 1)
                wavePen.setStyle(Qt::DashLine);
            p.setPen(wavePen);
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(QPointF(cx, cy), ringR, ringR);
        }
    }

    // 4. Rotating Scan Beam (when searching)
    if(m_status == UNKNOWN)
    {
        p.save();
        p.translate(cx, cy);
        p.rotate(std::fmod(m_time * 100.0f, 360.0f));
        QConicalGradient sweep(0, 0, 0);
        sweep.setColorAt(0.0, QColor(56, 189, 248, 50));
        sweep.setColorAt(0.15, QColor(56, 189, 248, 10));
        sweep.setColorAt(0.3, QColor(56, 189, 248, 0));
        sweep.setColorAt(1.0, QColor(56, 189, 248, 0));
        p.setBrush(sweep);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(0, 0), maxRadarR * 0.9f, maxRadarR * 0.9f);
        p.restore();
    }

    // 5. Phone Dimensions & Floating Motion
    const float phoneW = 184.0f;
    const float phoneH = 326.0f;
    float floatY = (m_status == DEVICE) ? 0.0f : (std::sin(m_time * 2.2f) * 5.0f);
    const float px = cx - phoneW * 0.5f;
    const float py = cy - phoneH * 0.5f + floatY;

    // 6. USB Cable & Data Stream
    const float cableStartX = cx;
    const float cableStartY = h;
    const float cableEndY = py + phoneH;

    // Cable Base line
    p.setPen(QPen(QColor(30, 41, 59, 220), 6.0f, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(cableStartX, cableStartY), QPointF(cableStartX, cableEndY));

    // Cable Core Glow line
    p.setPen(QPen(QColor(primaryColor.red(), primaryColor.green(), primaryColor.blue(), 180), 2.5f, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(cableStartX, cableStartY), QPointF(cableStartX, cableEndY));

    // Animated light pulses traveling upward into USB port
    const int pulseCount = 4;
    for(int i = 0; i < pulseCount; ++i)
    {
        float speedMult = (m_status == DEVICE) ? 2.5f : 1.2f;
        float pulseT = std::fmod(m_time * speedMult + (float) i / (float) pulseCount, 1.0f);
        float pulseY = cableStartY - pulseT * (cableStartY - cableEndY);
        float pulseAlpha = (pulseT < 0.15f) ? (pulseT / 0.15f) : (pulseT > 0.85f ? (1.0f - pulseT) / 0.15f : 1.0f);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(accentColor.red(), accentColor.green(), accentColor.blue(), static_cast<int>(pulseAlpha * 255)));
        p.drawEllipse(QPointF(cableStartX, pulseY), 3.5f, 6.0f);
    }

    // USB-C Plug Head
    QRectF usbPlugRect(cx - 11.0f, cableEndY - 1.0f, 22.0f, 15.0f);
    QLinearGradient plugGrad(usbPlugRect.topLeft(), usbPlugRect.bottomRight());
    plugGrad.setColorAt(0.0, QColor(71, 85, 105));
    plugGrad.setColorAt(1.0, QColor(30, 41, 59));
    p.setPen(QPen(primaryColor, 1.2f));
    p.setBrush(plugGrad);
    p.drawRoundedRect(usbPlugRect, 3.0f, 3.0f);

    // 7. Outer Phone Chassis (Metallic bevel & breathing neon glow)
    QRectF phoneRect(px, py, phoneW, phoneH);
    float glowBreathing = 0.7f + 0.3f * std::sin(m_time * 3.0f);
    QPen neonPen(QColor(primaryColor.red(), primaryColor.green(), primaryColor.blue(), static_cast<int>(160 * glowBreathing)));
    neonPen.setWidthF(2.5f);

    QLinearGradient chassisGrad(phoneRect.topLeft(), phoneRect.bottomRight());
    chassisGrad.setColorAt(0.0, QColor(30, 41, 59));
    chassisGrad.setColorAt(0.5, QColor(15, 23, 42));
    chassisGrad.setColorAt(1.0, QColor(2, 6, 23));

    p.setPen(neonPen);
    p.setBrush(chassisGrad);
    p.drawRoundedRect(phoneRect, 26.0f, 26.0f);

    // 8. Phone Screen (AMOLED Glass)
    const float screenMargin = 7.0f;
    QRectF screenRect(px + screenMargin, py + screenMargin, phoneW - screenMargin * 2.0f, phoneH - screenMargin * 2.0f);
    p.setPen(QPen(QColor(51, 65, 85, 160), 1.0f));
    p.setBrush(QColor(8, 12, 20));
    p.drawRoundedRect(screenRect, 20.0f, 20.0f);

    // Speaker slit & front camera punch-hole / Dynamic Island
    if(m_isApple)
    {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(10, 10, 14));
        p.drawRoundedRect(QRectF(cx - 20.0f, py + 10.5f, 40.0f, 9.0f), 4.5f, 4.5f);
    }
    else
    {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(51, 65, 85));
        p.drawRoundedRect(QRectF(cx - 16.0f, py + 12.0f, 32.0f, 3.5f), 1.5f, 1.5f);
        p.setBrush(QColor(30, 41, 59));
        p.drawEllipse(QPointF(cx + 25.0f, py + 13.5f), 3.0f, 3.0f);
    }

    // Mini Status Bar inside Phone
    p.setPen(QColor(148, 163, 184));
    QFont statusFont = p.font();
    statusFont.setPointSize(8);
    statusFont.setBold(true);
    p.setFont(statusFont);
    p.drawText(QRectF(screenRect.left() + 10.0f, screenRect.top() + 8.0f, 50.0f, 14.0f), Qt::AlignLeft | Qt::AlignVCenter, m_isApple ? "iOS" : "Android");

    // Battery icon
    QRectF battRect(screenRect.right() - 24.0f, screenRect.top() + 10.0f, 14.0f, 8.0f);
    p.setPen(QPen(QColor(148, 163, 184), 1.0f));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(battRect, 1.5f, 1.5f);
    p.fillRect(QRectF(battRect.left() + 2.0f, battRect.top() + 2.0f, 7.0f, 4.0f), primaryColor);

    // 9. Screen Contents by State
    p.save();
    p.setClipRect(screenRect);

    if(m_status == DEVICE)
    {
        // --- CONNECTED STATE ---
        float successPulse = qMin(1.0f, m_connectedTime * 2.0f);
        float checkCenterY = py + phoneH * 0.38f;
        float checkR = 34.0f * successPulse;

        QRadialGradient succGrad(cx, checkCenterY, checkR * 1.5f);
        succGrad.setColorAt(0.0, QColor(16, 185, 129, 70));
        succGrad.setColorAt(1.0, QColor(16, 185, 129, 0));
        p.setBrush(succGrad);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(cx, checkCenterY), checkR * 1.5f, checkR * 1.5f);

        p.setPen(QPen(QColor(16, 185, 129), 2.5f));
        p.setBrush(QColor(6, 78, 59, 180));
        p.drawEllipse(QPointF(cx, checkCenterY), checkR, checkR);

        // Checkmark path
        QPainterPath checkPath;
        checkPath.moveTo(cx - 12.0f, checkCenterY);
        checkPath.lineTo(cx - 3.0f, checkCenterY + 9.0f);
        checkPath.lineTo(cx + 14.0f, checkCenterY - 8.0f);
        QPen checkPen(QColor(240, 253, 244), 3.0f, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.strokePath(checkPath, checkPen);

        // Device Title
        QFont titleFont = p.font();
        titleFont.setPointSize(11);
        titleFont.setBold(true);
        p.setFont(titleFont);
        p.setPen(QColor(240, 253, 244));
        QString displayName = m_devName.isEmpty() ? (m_isApple ? QString::fromUtf8("Apple Устройство") : QString::fromUtf8("Android Устройство")) : m_devName;
        p.drawText(QRectF(screenRect.left() + 6.0f, checkCenterY + 44.0f, screenRect.width() - 12.0f, 22.0f), Qt::AlignCenter | Qt::AlignVCenter, displayName);

        // Subtitle (Model / Vendor)
        QFont subFont = p.font();
        subFont.setPointSize(8);
        subFont.setBold(false);
        p.setFont(subFont);
        p.setPen(QColor(110, 231, 183));
        QString subText = m_devSub.isEmpty() ? QString::fromUtf8("USB Подключен") : m_devSub;
        p.drawText(QRectF(screenRect.left() + 6.0f, checkCenterY + 66.0f, screenRect.width() - 12.0f, 18.0f), Qt::AlignCenter | Qt::AlignVCenter, subText);

        // Ready Badge
        QRectF badgeRect(cx - 58.0f, checkCenterY + 92.0f, 116.0f, 22.0f);
        p.setPen(QPen(QColor(16, 185, 129), 1.0f));
        p.setBrush(QColor(16, 185, 129, 45));
        p.drawRoundedRect(badgeRect, 11.0f, 11.0f);

        QFont badgeFont = p.font();
        badgeFont.setPointSize(8);
        badgeFont.setBold(true);
        p.setFont(badgeFont);
        p.setPen(QColor(52, 211, 153));
        QString badgeText = QString::fromUtf8("АВТОРИЗОВАНО");
        if(m_isApple && (m_devSub.contains("Recovery", Qt::CaseInsensitive) || m_devSub.contains("DFU", Qt::CaseInsensitive)))
        {
            badgeText = QString::fromUtf8("ГОТОВО К ПРОШИВКЕ");
        }
        p.drawText(badgeRect, Qt::AlignCenter, badgeText);
    }
    else if(m_status == UNAUTH)
    {
        // --- UNAUTHORIZED / PERMISSION REQUIRED ---
        float alertY = py + phoneH * 0.32f;

        // Warning Icon
        p.setPen(QPen(QColor(245, 158, 11), 2.2f));
        p.setBrush(QColor(120, 53, 15, 160));
        p.drawEllipse(QPointF(cx, alertY), 22.0f, 22.0f);

        QFont warnIconFont = p.font();
        warnIconFont.setPointSize(13);
        warnIconFont.setBold(true);
        p.setFont(warnIconFont);
        p.setPen(QColor(253, 230, 138));
        p.drawText(QRectF(cx - 15.0f, alertY - 15.0f, 30.0f, 30.0f), Qt::AlignCenter, "!");

        // Simulated Prompt Dialog
        QRectF promptCard(screenRect.left() + 8.0f, alertY + 30.0f, screenRect.width() - 16.0f, 116.0f);
        p.setPen(QPen(QColor(245, 158, 11, 160), 1.0f));
        p.setBrush(QColor(30, 25, 18, 230));
        p.drawRoundedRect(promptCard, 8.0f, 8.0f);

        QFont pTitle = p.font();
        pTitle.setPointSize(8);
        pTitle.setBold(true);
        p.setFont(pTitle);
        p.setPen(QColor(251, 191, 36));
        if(m_isApple)
        {
            p.drawText(QRectF(promptCard.left() + 4.0f, promptCard.top() + 6.0f, promptCard.width() - 8.0f, 28.0f), Qt::AlignCenter | Qt::TextWordWrap, QString::fromUtf8("Доверять этому\nкомпьютеру?"));
        }
        else
        {
            p.drawText(QRectF(promptCard.left() + 4.0f, promptCard.top() + 6.0f, promptCard.width() - 8.0f, 28.0f), Qt::AlignCenter | Qt::TextWordWrap, QString::fromUtf8("Разрешить отладку\nпо USB?"));
        }

        QFont pDesc = p.font();
        pDesc.setPointSize(7);
        pDesc.setBold(false);
        p.setFont(pDesc);
        p.setPen(QColor(209, 213, 219));
        if(m_isApple)
        {
            p.drawText(QRectF(promptCard.left() + 6.0f, promptCard.top() + 38.0f, promptCard.width() - 12.0f, 28.0f), Qt::AlignCenter | Qt::TextWordWrap, QString::fromUtf8("Разблокируйте экран и подтвердите"));
        }
        else
        {
            p.drawText(QRectF(promptCard.left() + 6.0f, promptCard.top() + 38.0f, promptCard.width() - 12.0f, 28.0f), Qt::AlignCenter | Qt::TextWordWrap, QString::fromUtf8("Всегда разрешать с этого компьютера"));
        }

        // Pulsing button with touch ripple
        float btnPulse = 0.8f + 0.2f * std::sin(m_time * 4.0f);
        QRectF okBtnRect(cx - 46.0f, promptCard.bottom() - 32.0f, 92.0f, 24.0f);

        // Touch ripple expanding from button
        float ripT = std::fmod(m_time * 1.5f, 1.0f);
        p.setPen(QPen(QColor(245, 158, 11, static_cast<int>((1.0f - ripT) * 160)), 1.5f));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(okBtnRect.adjusted(-ripT * 8.0f, -ripT * 5.0f, ripT * 8.0f, ripT * 5.0f), 6.0f, 6.0f);

        p.setPen(QPen(QColor(245, 158, 11), 1.2f));
        p.setBrush(QColor(217, 119, 6, static_cast<int>(200 * btnPulse)));
        p.drawRoundedRect(okBtnRect, 5.0f, 5.0f);

        QFont btnFont = p.font();
        btnFont.setPointSize(8);
        btnFont.setBold(true);
        p.setFont(btnFont);
        p.setPen(QColor(255, 255, 255));
        p.drawText(okBtnRect, Qt::AlignCenter, m_isApple ? QString::fromUtf8("ДОВЕРЯТЬ") : QString::fromUtf8("РАЗРЕШИТЬ"));
    }
    else
    {
        // --- SEARCHING / SCANNING STATE ---
        float iconCenterY = py + phoneH * 0.40f;

        // Radar scan circles inside screen
        float inRadius = 45.0f;
        p.setPen(QPen(QColor(56, 189, 248, 60), 1.0f, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(cx, iconCenterY), inRadius, inRadius);

        float innerPulseR = 24.0f + 12.0f * std::sin(m_time * 2.5f);
        p.setPen(QPen(QColor(56, 189, 248, 90), 1.2f));
        p.drawEllipse(QPointF(cx, iconCenterY), innerPulseR, innerPulseR);

        // Center Icon Glyph
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(14, 165, 233, 50));
        p.drawEllipse(QPointF(cx, iconCenterY), 20.0f, 20.0f);

        QPixmap zapPix(":/svg/zap");
        if(!zapPix.isNull())
        {
            p.drawPixmap(QRect(static_cast<int>(cx - 10.0f), static_cast<int>(iconCenterY - 10.0f), 20, 20), zapPix.scaled(20, 20, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }

        // Text: ПОИСК УСТРОЙСТВА / ПОИСК IPHONE / IPAD
        QFont sTitle = p.font();
        sTitle.setPointSize(9);
        sTitle.setBold(true);
        p.setFont(sTitle);
        p.setPen(QColor(240, 249, 255));
        p.drawText(QRectF(screenRect.left() + 6.0f, iconCenterY + 42.0f, screenRect.width() - 12.0f, 20.0f), Qt::AlignCenter, m_isApple ? QString::fromUtf8("ПОИСК IPHONE / IPAD") : QString::fromUtf8("ПОИСК УСТРОЙСТВА"));

        // Subtitle
        QFont sDesc = p.font();
        sDesc.setPointSize(8);
        sDesc.setBold(false);
        p.setFont(sDesc);
        p.setPen(QColor(148, 163, 184));
        p.drawText(QRectF(screenRect.left() + 6.0f, iconCenterY + 62.0f, screenRect.width() - 12.0f, 18.0f), Qt::AlignCenter, m_isApple ? QString::fromUtf8("Подключите кабель к ПК") : QString::fromUtf8("Подключите USB-кабель"));

        // Animated Dots: ● ● ○
        int dotIdx = static_cast<int>(m_time * 2.5f) % 4;
        QString dots;
        for(int d = 0; d < 3; ++d)
        {
            if(d < dotIdx)
                dots += "● ";
            else
                dots += "○ ";
        }
        QFont dotsFont = p.font();
        dotsFont.setPointSize(9);
        p.setFont(dotsFont);
        p.setPen(QColor(56, 189, 248));
        p.drawText(QRectF(screenRect.left() + 6.0f, iconCenterY + 84.0f, screenRect.width() - 12.0f, 18.0f), Qt::AlignCenter, dots.trimmed());
    }

    p.restore();
}
