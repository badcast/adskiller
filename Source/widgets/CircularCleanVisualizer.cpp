#include <QPainter>
#include <QPainterPath>

#include "CircularCleanVisualizer.h"

CircularCleanVisualizer::CircularCleanVisualizer(QWidget *parent) : QWidget(parent), m_tickerText("* ОЧИСТКА МУСОРА * КЭШ ПРИЛОЖЕНИЙ * THUMBNAILS * ФОНОВЫЕ ПРОЦЕССЫ * ADB CLEANER ")
{
    setFixedSize(270, 270);
    setAttribute(Qt::WA_OpaquePaintEvent, false);

    m_animTimer = new QTimer(this);
    m_animTimer->setInterval(30); // ~33 FPS
    connect(m_animTimer, &QTimer::timeout, this, &CircularCleanVisualizer::onAnimationTick);

    reset();
}

CircularCleanVisualizer::~CircularCleanVisualizer()
{
    m_animTimer->stop();
}

void CircularCleanVisualizer::reset()
{
    m_progress = 0;
    m_tickerAngle = 0.0;
    m_particleAngle = 0.0;
    m_iconScale = 1.0;
    m_pulsePhase = 0.0;
    m_appName = "Ожидание запуска";
    m_pkgName.clear();
    m_statusText = "Готово к быстрой очистке";
    m_isRunning = false;
    m_isCompleted = false;

    // Default icon: storage clean icon from resources
    QPixmap icon(":/svg/services/storage-clean");
    if(icon.isNull())
        icon = QPixmap(":/svg/trash");
    m_appIcon = icon.scaled(56, 56, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    update();
}

void CircularCleanVisualizer::setProgress(int percent)
{
    m_progress = qBound(0, percent, 100);
    update();
}

void CircularCleanVisualizer::setCurrentApp(const QString &appName, const QString &pkgName, const QPixmap &icon, const QString &statusText)
{
    m_appName = appName;
    m_pkgName = pkgName;
    m_statusText = statusText;

    if(!icon.isNull())
    {
        // Round corners for the app icon
        QPixmap rounded(56, 56);
        rounded.fill(Qt::transparent);
        QPainter rp(&rounded);
        rp.setRenderHint(QPainter::Antialiasing);
        QPainterPath path;
        path.addRoundedRect(QRectF(0, 0, 56, 56), 12, 12);
        rp.setClipPath(path);
        rp.drawPixmap(0, 0, icon.scaled(56, 56, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        rp.end();
        m_appIcon = rounded;
    }

    m_iconScale = 0.85; // trigger pulse scale
    update();
}

void CircularCleanVisualizer::setRunning(bool running)
{
    m_isRunning = running;
    m_isCompleted = false;
    if(running)
    {
        if(!m_animTimer->isActive())
            m_animTimer->start();
    }
    else
    {
        m_animTimer->stop();
    }
    update();
}

void CircularCleanVisualizer::setCompleted(bool completed, const QString &summaryText)
{
    m_isCompleted = completed;
    m_isRunning = false;
    m_progress = 100;
    m_appName = "Очистка завершена";
    m_statusText = summaryText.isEmpty() ? "Все выбранные элементы очищены" : summaryText;

    QPixmap checkPix(":/svg/check-circle");
    if(checkPix.isNull())
        checkPix = QPixmap(":/svg/check");
    if(!checkPix.isNull())
    {
        m_appIcon = checkPix.scaled(56, 56, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    m_animTimer->stop();
    update();
}

void CircularCleanVisualizer::onAnimationTick()
{
    if(m_isRunning)
    {
        // Advance circular ticker angle
        m_tickerAngle += 0.75;
        if(m_tickerAngle >= 360.0)
            m_tickerAngle -= 360.0;

        // Counter-rotating particle/accent ring
        m_particleAngle -= 1.4;
        if(m_particleAngle <= -360.0)
            m_particleAngle += 360.0;

        // Icon pulse animation
        m_pulsePhase += 0.08;
        if(m_iconScale < 1.0)
        {
            m_iconScale = qMin<qreal>(1.0, m_iconScale + 0.03);
        }

        update();
    }
}

void CircularCleanVisualizer::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
}

void CircularCleanVisualizer::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    const qreal cx = width() / 2.0;
    const qreal cy = height() / 2.0;
    const qreal outerR = qMin(width(), height()) / 2.0 - 6.0;

    // 1. Background radial glow
    QRadialGradient bgGlow(QPointF(cx, cy), outerR);
    if(m_isCompleted)
    {
        bgGlow.setColorAt(0.0, QColor(6, 78, 59, 120));
        bgGlow.setColorAt(0.7, QColor(10, 14, 26, 80));
        bgGlow.setColorAt(1.0, QColor(10, 14, 26, 0));
    }
    else if(m_isRunning)
    {
        bgGlow.setColorAt(0.0, QColor(2, 132, 199, 130));
        bgGlow.setColorAt(0.65, QColor(10, 14, 26, 90));
        bgGlow.setColorAt(1.0, QColor(10, 14, 26, 0));
    }
    else
    {
        bgGlow.setColorAt(0.0, QColor(30, 41, 59, 80));
        bgGlow.setColorAt(0.7, QColor(10, 14, 26, 50));
        bgGlow.setColorAt(1.0, QColor(10, 14, 26, 0));
    }
    p.fillRect(rect(), bgGlow);

    // 2. Circular Outer Track Line
    QPen trackPen(QColor(30, 41, 59, 140), 2.0);
    p.setPen(trackPen);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(cx, cy), outerR, outerR);

    // 3. Circular Running Ticker Text (Бегущая строка по кругу)
    const qreal textR = outerR - 10.0;
    QFont tickerFont("Segoe UI", 8, QFont::Bold);
    tickerFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
    p.setFont(tickerFont);

    int totalChars = m_tickerText.length();
    if(totalChars > 0)
    {
        const qreal stepDeg = 360.0 / totalChars;
        for(int i = 0; i < totalChars; ++i)
        {
            qreal charAngle = m_tickerAngle + (i * stepDeg);
            qreal rad = qDegreesToRadians(charAngle);

            p.save();
            p.translate(cx + textR * std::cos(rad), cy + textR * std::sin(rad));
            p.rotate(charAngle + 90.0);

            QChar ch = m_tickerText.at(i);
            if(ch == QChar(0x2726) || ch == QChar('*')) // symbol *
            {
                p.setPen(m_isCompleted ? QColor(52, 211, 153) : QColor(56, 189, 248));
            }
            else
            {
                p.setPen(m_isCompleted ? QColor(167, 243, 208, 200) : (m_isRunning ? QColor(224, 242, 254, 220) : QColor(148, 163, 184, 150)));
            }

            p.drawText(QRectF(-8, -8, 16, 16), Qt::AlignCenter, QString(ch));
            p.restore();
        }
    }

    // 4. Progress Arc (Electric Cyan / Emerald Green)
    const qreal arcR = outerR - 24.0;
    QRectF arcRect(cx - arcR, cy - arcR, arcR * 2.0, arcR * 2.0);

    // Subtle track background for arc
    p.setPen(QPen(QColor(15, 23, 42, 180), 5.0, Qt::SolidLine, Qt::RoundCap));
    p.drawEllipse(arcRect);

    if(m_progress > 0)
    {
        QConicalGradient arcGrad(QPointF(cx, cy), -90);
        if(m_isCompleted)
        {
            arcGrad.setColorAt(0.0, QColor(52, 211, 153));
            arcGrad.setColorAt(1.0, QColor(16, 185, 129));
        }
        else
        {
            arcGrad.setColorAt(0.0, QColor(56, 189, 248));
            arcGrad.setColorAt(0.5, QColor(0, 229, 255));
            arcGrad.setColorAt(1.0, QColor(52, 211, 153));
        }

        QPen progPen(QBrush(arcGrad), 5.5, Qt::SolidLine, Qt::RoundCap);
        p.setPen(progPen);
        int spanAngle = static_cast<int>(-m_progress * 3.6 * 16.0);
        p.drawArc(arcRect, 90 * 16, spanAngle);
    }

    // 5. Rotating accent dashed orbit (counter-direction)
    if(m_isRunning)
    {
        const qreal dashR = outerR - 33.0;
        p.save();
        p.translate(cx, cy);
        p.rotate(m_particleAngle);
        QPen dashPen(QColor(56, 189, 248, 80), 1.5, Qt::DashLine);
        p.setPen(dashPen);
        p.drawEllipse(QPointF(0, 0), dashR, dashR);
        p.restore();
    }

    // 6. Central Circle (frosted dark disc)
    const qreal centerR = 64.0;
    QRectF centerRect(cx - centerR, cy - centerR, centerR * 2.0, centerR * 2.0);

    QRadialGradient centerGrad(QPointF(cx, cy), centerR);
    centerGrad.setColorAt(0.0, QColor(15, 23, 42, 240));
    centerGrad.setColorAt(0.85, QColor(7, 10, 18, 250));
    centerGrad.setColorAt(1.0, QColor(3, 7, 18, 255));

    p.setBrush(centerGrad);
    QPen centerBorder(m_isCompleted ? QColor(52, 211, 153, 180) : (m_isRunning ? QColor(56, 189, 248, 160) : QColor(30, 41, 59, 200)), 2.0);
    p.setPen(centerBorder);
    p.drawEllipse(centerRect);

    // 7. Center App Icon with scale / pulse
    if(!m_appIcon.isNull())
    {
        p.save();
        p.translate(cx, cy - 8.0);
        qreal s = m_iconScale;
        if(m_isRunning)
        {
            s *= (1.0 + 0.03 * std::sin(m_pulsePhase));
        }
        p.scale(s, s);

        int iconW = m_appIcon.width();
        int iconH = m_appIcon.height();
        p.drawPixmap(-iconW / 2, -iconH / 2, m_appIcon);
        p.restore();
    }

    // 8. Progress % badge inside center circle at bottom
    QRectF percentRect(cx - 40, cy + 26, 80, 20);
    QFont pctFont("Segoe UI", 11, QFont::Bold);
    p.setFont(pctFont);
    p.setPen(m_isCompleted ? QColor(52, 211, 153) : (m_isRunning ? QColor(56, 189, 248) : QColor(148, 163, 184)));
    p.drawText(percentRect, Qt::AlignCenter, QString::number(m_progress) + "%");
}
