#include "CyberReactorLoader.h"

#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>
#include <QRadialGradient>
#include <QLinearGradient>
#include <QConicalGradient>
#include <QFont>
#include <QFontMetrics>
#include <cmath>

static const qreal PI = 3.14159265358979323846;

CyberReactorLoader::CyberReactorLoader(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_Hover, false);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setStyleSheet("background: transparent; border: none;");

    setMinimumSize(280, 240);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Тексты теперь связаны с телефонами и блокировкой рекламы
    m_telemetryLines << QString::fromUtf8("МОБИЛЬНЫЙ ЩИТ: АНАЛИЗ ТРАФИКА")
                     << QString::fromUtf8("АНТИ-РЕКЛАМА: ПЕРЕХВАТ БАННЕРОВ")
                     << QString::fromUtf8("ОЧИСТКА ЭКРАНА: УДАЛЕНИЕ СКРИПТОВ")
                     << QString::fromUtf8("БЛОКИРОВКА ТРЕКЕРОВ: АКТИВНА")
                     << QString::fromUtf8("ADSKILLER: ОПТИМИЗАЦИЯ УСТРОЙСТВА");

    initParticles();

    m_timer = new QTimer(this);
    m_timer->setInterval(20);
    connect(m_timer, &QTimer::timeout, this, &CyberReactorLoader::onAnimationTick);

    m_elapsed.start();
}

CyberReactorLoader::~CyberReactorLoader()
{
    if(m_timer)
        m_timer->stop();
}

void CyberReactorLoader::initParticles()
{
    m_particles.clear();
    const qreal baseRadii[] = {45.0, 52.0, 68.0, 75.0, 88.0, 50.0, 62.0, 78.0, 85.0, 60.0};
    const qreal speeds[] = {0.035, -0.045, 0.025, -0.030, 0.040, -0.020, 0.050, -0.035, 0.028, -0.042};
    const qreal sizes[] = {1.5, 2.0, 1.2, 2.5, 1.5, 1.8, 2.2, 1.2, 2.0, 1.4};

    for(int i = 0; i < 10; ++i)
    {
        Particle p;
        p.orbitRadius = baseRadii[i];
        p.angle = (i * 36.0) * (PI / 180.0);
        p.speed = speeds[i];
        p.size = sizes[i];

        // Красные частицы символизируют заблокированную рекламу и трекеры
        if(i % 3 == 0)
            p.color = QColor(255, 60, 60, 210); // Угроза/Реклама (Красный)
        else if(i % 2 == 0)
            p.color = QColor(0, 191, 255, 180); // Чистые данные (Глубокий синий)
        else
            p.color = QColor(0, 245, 255, 200); // Чистые данные (Голубой)

        m_particles.append(p);
    }
}

void CyberReactorLoader::setStatusText(const QString &text)
{
    m_statusText = text;
    update();
}

QSize CyberReactorLoader::sizeHint() const
{
    return QSize(360, 260);
}

QSize CyberReactorLoader::minimumSizeHint() const
{
    return QSize(280, 220);
}

void CyberReactorLoader::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if(m_timer && !m_timer->isActive())
    {
        m_elapsed.restart();
        m_timer->start();
    }
}

void CyberReactorLoader::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    if(m_timer && m_timer->isActive())
        m_timer->stop();
}

void CyberReactorLoader::onAnimationTick()
{
    m_outerAngle += 1.2;
    if(m_outerAngle >= 360.0)
        m_outerAngle -= 360.0;

    m_middleAngle -= 1.8;
    if(m_middleAngle <= -360.0)
        m_middleAngle += 360.0;

    m_radarAngle += 3.0;
    if(m_radarAngle >= 360.0)
        m_radarAngle -= 360.0;

    m_pulsePhase += 0.05;
    if(m_pulsePhase >= 2.0 * PI)
        m_pulsePhase -= 2.0 * PI;

    // Волна "очистки" расходится от телефона
    m_shockwaveRadius += 1.5;
    if(m_shockwaveRadius > 130.0)
        m_shockwaveRadius = 15.0;

    m_streamProgress += 0.012;
    if(m_streamProgress > 1.0)
        m_streamProgress = 0.0;

    for(auto &p : m_particles)
    {
        p.angle += p.speed;
        if(p.angle >= 2.0 * PI)
            p.angle -= 2.0 * PI;
        else if(p.angle < 0)
            p.angle += 2.0 * PI;
    }

    qint64 now = m_elapsed.elapsed();
    if(now - m_lastTelemetrySwitch > 1800)
    {
        m_lastTelemetrySwitch = now;
        if(!m_telemetryLines.isEmpty())
            m_telemetryIndex = (m_telemetryIndex + 1) % m_telemetryLines.size();
    }

    update();
}

void CyberReactorLoader::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const int w = width();
    const int h = height();

    const qreal cx = w / 2.0;
    const qreal cy = qMax<qreal>(85.0, (h - 60.0) / 2.0);
    const qreal baseR = qMin(cx, cy) - 14.0;
    const qreal r = qBound<qreal>(52.0, baseR, 92.0);

    // ========================================================================
    // 1. Свечение защитного поля (Синее)
    // ========================================================================
    QRadialGradient bgGlow(QPointF(cx, cy), r * 1.5);
    bgGlow.setColorAt(0.0, QColor(0, 191, 255, 40));
    bgGlow.setColorAt(0.6, QColor(0, 245, 255, 5));
    bgGlow.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.setBrush(bgGlow);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(cx, cy), r * 1.5, r * 1.5);

    // ========================================================================
    // 2. Волны сканера "Анти-реклама", расходящиеся от телефона
    // ========================================================================
    const qreal maxSwR = r * 1.3;
    if(m_shockwaveRadius >= 15.0 && m_shockwaveRadius <= maxSwR)
    {
        qreal swFade = 1.0 - ((m_shockwaveRadius - 15.0) / (maxSwR - 15.0));
        int swAlpha = qBound(0, static_cast<int>(swFade * 150), 255);
        QPen swPen(QColor(0, 245, 255, swAlpha), 1.2);
        painter.setPen(swPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(QPointF(cx, cy), m_shockwaveRadius, m_shockwaveRadius);
    }

    // ========================================================================
    // 3. Кольцо фильтрации (внешнее кольцо захвата)
    // ========================================================================
    const qreal statorR = r * 1.1;
    painter.setPen(QPen(QColor(0, 245, 255, 60), 1.0, Qt::DashLine));
    painter.drawEllipse(QPointF(cx, cy), statorR, statorR);

    // Прицел-крестовина (символизирует "поиск и уничтожение" рекламы)
    painter.setPen(QPen(QColor(0, 191, 255, 120), 1.0));
    painter.drawLine(QPointF(cx, cy - statorR - 5), QPointF(cx, cy - statorR + 5));
    painter.drawLine(QPointF(cx, cy + statorR - 5), QPointF(cx, cy + statorR + 5));
    painter.drawLine(QPointF(cx - statorR - 5, cy), QPointF(cx - statorR + 5, cy));
    painter.drawLine(QPointF(cx + statorR - 5, cy), QPointF(cx + statorR + 5, cy));

    // ========================================================================
    // 4. Орбитальные кольца анализа (Вращающиеся)
    // ========================================================================
    const qreal rotorR = r * 0.90;
    QRectF rRect(cx - rotorR, cy - rotorR, rotorR * 2.0, rotorR * 2.0);

    for(int arcIdx = 0; arcIdx < 4; ++arcIdx)
    {
        const qreal startA = m_outerAngle + arcIdx * 90.0;
        const qreal spanA = 35.0;

        // Тонкие дуги считывания
        painter.setPen(QPen(QColor(0, 245, 255, 200), 1.5, Qt::SolidLine, Qt::FlatCap));
        painter.drawArc(rRect, static_cast<int>(startA * 16.0), static_cast<int>(spanA * 16.0));
    }

    // ========================================================================
    // 5. Рекламные трекеры (Частицы вокруг щита)
    // ========================================================================
    for(const auto &p : m_particles)
    {
        const qreal actualR = r * (p.orbitRadius / 95.0);
        const qreal px = cx + std::cos(p.angle) * actualR;
        const qreal py = cy + std::sin(p.angle) * actualR;

        // Если частица красная, добавим ей легкое свечение "угрозы"
        if(p.color.red() > 200)
        {
            painter.setPen(Qt::NoPen);
            QColor glow = p.color;
            glow.setAlpha(40);
            painter.setBrush(glow);
            painter.drawEllipse(QPointF(px, py), p.size * 2.5, p.size * 2.5);
        }

        painter.setPen(Qt::NoPen);
        painter.setBrush(p.color);
        painter.drawEllipse(QPointF(px, py), p.size, p.size);
    }

    // ========================================================================
    // 6. ЦЕНТРАЛЬНЫЙ ЭЛЕМЕНТ: ЗАЩИЩЕННЫЙ СМАРТФОН С ЛАЗЕРОМ
    // ========================================================================
    const qreal phoneW = r * 0.40;
    const qreal phoneH = r * 0.75;
    QRectF phoneRect(cx - phoneW / 2.0, cy - phoneH / 2.0, phoneW, phoneH);

    // Фоновая заливка телефона (стекло)
    painter.setBrush(QColor(0, 20, 35, 180));
    // Рамка телефона
    painter.setPen(QPen(QColor(0, 245, 255, 220), 1.5));
    painter.drawRoundedRect(phoneRect, 6.0, 6.0);

    // Динамик смартфона (верхняя полоска)
    painter.setPen(QPen(QColor(0, 245, 255, 150), 1.5, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(cx - phoneW * 0.15, cy - phoneH / 2.0 + 5.0), QPointF(cx + phoneW * 0.15, cy - phoneH / 2.0 + 5.0));

    // Кнопка / Индикатор "Домой" (кружок внизу)
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 245, 255, 150));
    painter.drawEllipse(QPointF(cx, cy + phoneH / 2.0 - 6.0), 1.5, 1.5);

    // ========================================================================
    // 7. СКАНЕР РЕКЛАМЫ (Лазер, бегающий по экрану смартфона)
    // ========================================================================
    // Вычисляем позицию лазера с помощью синуса (вверх-вниз)
    const qreal scanOffset = std::sin(m_pulsePhase) * (phoneH / 2.0 - 10.0);
    const qreal scanY = cy + scanOffset;

    // Горизонтальная линия сканера
    QLinearGradient scanGrad(cx - phoneW / 2.0, scanY, cx + phoneW / 2.0, scanY);
    scanGrad.setColorAt(0.0, QColor(0, 245, 255, 0));
    scanGrad.setColorAt(0.5, QColor(255, 255, 255, 255)); // Яркий центр
    scanGrad.setColorAt(1.0, QColor(0, 245, 255, 0));

    painter.setPen(QPen(QBrush(scanGrad), 1.5));
    painter.drawLine(QPointF(cx - phoneW / 2.0 + 2.0, scanY), QPointF(cx + phoneW / 2.0 - 2.0, scanY));

    // Свечение под лазером (эффект очистки экрана)
    QLinearGradient scanArea(cx, scanY, cx, scanY + 15.0);
    scanArea.setColorAt(0.0, QColor(0, 245, 255, 80));
    scanArea.setColorAt(1.0, QColor(0, 245, 255, 0));
    painter.fillRect(QRectF(cx - phoneW / 2.0 + 2.0, scanY, phoneW - 4.0, 15.0), scanArea);

    // ========================================================================
    // 8. Полоса прогресса удаления (Очистка)
    // ========================================================================
    const qreal barW = qMin<qreal>(280.0, w - 36.0);
    const qreal barH = 2.5;
    const qreal barX = cx - (barW / 2.0);
    const qreal barY = cy + r + 25.0;
    const int segments = 28; // Мелкие блоки данных
    const qreal segW = (barW - (segments - 1) * 2.0) / segments;
    const qreal shimmerPos = m_streamProgress * barW;

    for(int s = 0; s < segments; ++s)
    {
        const qreal sx = barX + s * (segW + 2.0);
        const qreal segCenter = sx + segW / 2.0;
        const qreal dist = std::abs(segCenter - (barX + shimmerPos));
        const qreal lightIntensity = qMax<qreal>(0.0, 1.0 - (dist / 40.0));

        QColor segBg;
        if(lightIntensity > 0.1)
            segBg = QColor(0, 245, 255, static_cast<int>(lightIntensity * 255));
        else
            segBg = QColor(0, 245, 255, 30); // Заблокированный/пустой слот

        painter.fillRect(QRectF(sx, barY, segW, barH), segBg);
    }

    // ========================================================================
    // 9. Строка состояния мобильной защиты
    // ========================================================================
    const qreal teleY = barY + barH + 16.0;
    if(teleY < h)
    {
        QFont teleFont("Consolas", 8, QFont::Bold);
        teleFont.setStyleHint(QFont::Monospace);
        teleFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
        painter.setFont(teleFont);

        QString teleText = m_telemetryLines.value(m_telemetryIndex);
        if(teleText.isEmpty())
            teleText = QString::fromUtf8("• СИСТЕМА: СКАНИРОВАНИЕ ТРАФИКА •");

        // Эффект плавного мигания текста
        int textAlpha = 150 + static_cast<int>(80.0 * std::sin(m_pulsePhase * 2.0));
        painter.setPen(QColor(0, 245, 255, qBound(100, textAlpha, 255)));

        QRectF textRect(10.0, teleY - 4.0, w - 20.0, 20.0);
        painter.drawText(textRect, Qt::AlignCenter, teleText);
    }
}