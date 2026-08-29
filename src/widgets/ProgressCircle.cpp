#include "ProgressCircle.h"
#include <QPainter>
#include <QPixmapCache>
#include <QtMath>
#include <QEasingCurve>

ProgressCircle::ProgressCircle(QWidget *parent)
    : QWidget(parent), mInfinilyMode(true), mVisibleText(true), mValue(0), mMaximum(100), mInnerRadius(0.6), mOuterRadius(1.0), mColor(76, 194, 255), mVisibleValue(0), mValueAnimation(this, "visibleValue"), mInfiniteAnimation(this, "infiniteAnimationValue"), mInfiniteAnimationValue(0.0)
{
    mValueAnimation.setEasingCurve(QEasingCurve::OutCubic);
    mInfiniteAnimation.setLoopCount(-1); // infinite
    mInfiniteAnimation.setDuration(1200);
    mInfiniteAnimation.setStartValue(0.0);
    mInfiniteAnimation.setEndValue(1.0);
    mInfiniteAnimation.start();
}

int ProgressCircle::value() const
{
    return mValue;
}

bool ProgressCircle::infinilyMode() const
{
    return mInfinilyMode;
}

int ProgressCircle::maximum() const
{
    return mMaximum;
}

qreal ProgressCircle::innerRadius() const
{
    return mInnerRadius;
}

qreal ProgressCircle::outerRadius() const
{
    return mOuterRadius;
}

QColor ProgressCircle::color() const
{
    return mColor;
}

bool ProgressCircle::getVisibleText() const
{
    return mVisibleText;
}

void ProgressCircle::setValue(int value)
{
    if(value < 0)
        value = 0;

    if(mValue != value)
    {
        mValueAnimation.stop();
        mValueAnimation.setEndValue(value);
        mValueAnimation.setDuration(300);
        mValueAnimation.start();

        mValue = value;
        emit valueChanged(value);
    }
}

void ProgressCircle::setInfinilyMode(bool value)
{
    mInfinilyMode = value;
    update();
    if(value)
    {
        mInfiniteAnimation.start();
    }
    else
    {
        mInfiniteAnimation.stop();
    }
}

void ProgressCircle::setVisibleText(bool value)
{
    if(mVisibleText != value)
    {
        mVisibleText = value;
        update();
    }
}

void ProgressCircle::setMaximum(int maximum)
{
    if(maximum < 0)
        maximum = 0;

    if(mMaximum != maximum)
    {
        mMaximum = maximum;
        update();
        emit maximumChanged(maximum);
    }
}

void ProgressCircle::setInnerRadius(qreal innerRadius)
{
    if(innerRadius > 1.0)
        innerRadius = 1.0;
    if(innerRadius < 0.0)
        innerRadius = 0.0;

    if(mInnerRadius != innerRadius)
    {
        mInnerRadius = innerRadius;
        update();
    }
}

void ProgressCircle::setOuterRadius(qreal outerRadius)
{
    if(outerRadius > 1.0)
        outerRadius = 1.0;
    if(outerRadius < 0.0)
        outerRadius = 0.0;

    if(mOuterRadius != outerRadius)
    {
        mOuterRadius = outerRadius;
        update();
    }
}

void ProgressCircle::setColor(QColor color)
{
    if(color != mColor)
    {
        mColor = color;
        update();
    }
}

static QRectF squared(QRectF rect)
{
    if(rect.width() > rect.height())
    {
        qreal diff = rect.width() - rect.height();
        return rect.adjusted(diff / 2, 0, -diff / 2, 0);
    }
    else
    {
        qreal diff = rect.height() - rect.width();
        return rect.adjusted(0, diff / 2, 0, -diff / 2);
    }
}

void ProgressCircle::paintEvent(QPaintEvent *)
{
    QPixmap pixmap;
    if(!QPixmapCache::find(key(), &pixmap))
    {
        pixmap = generatePixmap();
        QPixmapCache::insert(key(), pixmap);
    }

    // Draw pixmap at center of item
    QPainter painter(this);
    painter.drawPixmap(0.5 * (width() - pixmap.width()), 0.5 * (height() - pixmap.height()), pixmap);
}

void ProgressCircle::setInfiniteAnimationValue(qreal value)
{
    mInfiniteAnimationValue = value;
    update();
}

void ProgressCircle::setVisibleValue(int value)
{
    if(mVisibleValue != value)
    {
        mVisibleValue = value;
        update();
    }
}

QString ProgressCircle::key() const
{
    return QString("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10").arg(mInfiniteAnimationValue).arg(mVisibleValue).arg(mMaximum).arg(mInnerRadius).arg(mOuterRadius).arg(width()).arg(height()).arg(mColor.rgb()).arg(mInfinilyMode).arg(mVisibleText);
}

QPixmap ProgressCircle::generatePixmap() const
{
    QSize size = squared(rect()).size().toSize();
    if(size.width() < 10 || size.height() < 10)
        return QPixmap();

    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // Padding around bounds to allow neon glow effects
    qreal padding = 8.0;
    QRectF bounds = pixmap.rect().adjusted(padding, padding, -padding, -padding);

    qreal margin = bounds.width() * (1.0 - mOuterRadius) / 2.0;
    QRectF circleRect = bounds.adjusted(margin, margin, -margin, -margin);

    QPointF center = circleRect.center();
    qreal radius = circleRect.width() / 2.0;
    if(radius <= 4.0)
        return pixmap;

    qreal trackThickness = qMax(6.0, radius * 0.13);
    qreal arcRadius = radius - (trackThickness / 2.0);
    QRectF arcRect(center.x() - arcRadius, center.y() - arcRadius, arcRadius * 2.0, arcRadius * 2.0);

    // 1. Ambient Glass Halo
    QRadialGradient ambientGlow(center, radius + 4.0);
    ambientGlow.setColorAt(0.0, QColor(25, 27, 32, 160));
    ambientGlow.setColorAt(0.75, QColor(20, 22, 26, 110));
    ambientGlow.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.setPen(Qt::NoPen);
    painter.setBrush(ambientGlow);
    painter.drawEllipse(center, radius + 4.0, radius + 4.0);

    // 2. Track Ring (Modern frosted glass groove)
    QPen trackPen(QColor(255, 255, 255, 22), trackThickness, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(trackPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(center, arcRadius, arcRadius);

    QPen trackInnerShadow(QColor(0, 0, 0, 70), trackThickness - 4.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(trackInnerShadow);
    painter.drawEllipse(center, arcRadius, arcRadius);

    int value = qMin(mVisibleValue, mMaximum);

    // 3. Progress Arc / Infinite Spinner
    if(mInfinilyMode)
    {
        // Smooth rotating cyber beam
        int startAngle = qRound((-mInfiniteAnimationValue * 360.0 + 90.0) * 16.0);
        int spanAngle = -qRound(90.0 * 16.0);

        // Neon glow arc
        QColor glowColor(mColor.red(), mColor.green(), mColor.blue(), 55);
        QPen glowPen(glowColor, trackThickness + 6.0, Qt::SolidLine, Qt::RoundCap);
        painter.setPen(glowPen);
        painter.drawArc(arcRect, startAngle, spanAngle);

        // Core bright arc with gradient
        QConicalGradient conical(center, -mInfiniteAnimationValue * 360.0 + 90.0);
        conical.setColorAt(0.0, mColor.lighter(140));
        conical.setColorAt(0.25, mColor);
        conical.setColorAt(0.5, mColor.darker(130));
        conical.setColorAt(1.0, mColor.lighter(140));

        QPen progressPen(QBrush(conical), trackThickness, Qt::SolidLine, Qt::RoundCap);
        painter.setPen(progressPen);
        painter.drawArc(arcRect, startAngle, spanAngle);

        // High-tech tip spark
        qreal tipAngleDeg = -mInfiniteAnimationValue * 360.0 + 90.0 - 90.0;
        qreal tipRad = qDegreesToRadians(tipAngleDeg);
        QPointF tipPos(center.x() + arcRadius * std::cos(tipRad), center.y() - arcRadius * std::sin(tipRad));

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, 230));
        painter.drawEllipse(tipPos, trackThickness * 0.4, trackThickness * 0.4);

        // Secondary counter subtle arc
        int startAngle2 = qRound((mInfiniteAnimationValue * 180.0 - 90.0) * 16.0);
        int spanAngle2 = qRound(45.0 * 16.0);
        QPen counterPen(QColor(mColor.red(), mColor.green(), mColor.blue(), 65), trackThickness * 0.5, Qt::SolidLine, Qt::RoundCap);
        painter.setPen(counterPen);
        painter.drawArc(arcRect, startAngle2, spanAngle2);
    }
    else if(value > 0)
    {
        qreal percent = qBound(0.0, qreal(value) / qreal(mMaximum), 1.0);
        int startAngle = 90 * 16;
        int spanAngle = -qRound(percent * 360.0 * 16.0);

        // Neon Aura Glow
        QColor glowColor(mColor.red(), mColor.green(), mColor.blue(), 55);
        QPen glowPen(glowColor, trackThickness + 6.0, Qt::SolidLine, Qt::RoundCap);
        painter.setPen(glowPen);
        painter.drawArc(arcRect, startAngle, spanAngle);

        // Main Gradient Arc
        QLinearGradient arcGrad(arcRect.topLeft(), arcRect.bottomRight());
        arcGrad.setColorAt(0.0, mColor.lighter(135));
        arcGrad.setColorAt(0.5, mColor);
        arcGrad.setColorAt(1.0, mColor.darker(115));

        QPen progressPen(QBrush(arcGrad), trackThickness, Qt::SolidLine, Qt::RoundCap);
        painter.setPen(progressPen);
        painter.drawArc(arcRect, startAngle, spanAngle);

        // Inner highlight stroke
        QPen innerHighlight(QColor(255, 255, 255, 80), trackThickness * 0.28, Qt::SolidLine, Qt::RoundCap);
        painter.setPen(innerHighlight);
        painter.drawArc(arcRect, startAngle, spanAngle);

        // Glowing Spark Tip
        qreal tipAngleDeg = 90.0 - (percent * 360.0);
        qreal tipRad = qDegreesToRadians(tipAngleDeg);
        QPointF tipPos(center.x() + arcRadius * std::cos(tipRad), center.y() - arcRadius * std::sin(tipRad));

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, 240));
        painter.drawEllipse(tipPos, trackThickness * 0.42, trackThickness * 0.42);

        painter.setBrush(QColor(mColor.lighter(150).red(), mColor.lighter(150).green(), mColor.lighter(150).blue(), 100));
        painter.drawEllipse(tipPos, trackThickness * 0.75, trackThickness * 0.75);
    }

    // 4. Center Disc / Core
    qreal innerRadiusPx = mInnerRadius * radius;
    if(innerRadiusPx > 4.0)
    {
        qreal coreR = innerRadiusPx - (trackThickness * 0.45);
        if(coreR > 2.0)
        {
            // Frosted Center Disc Gradient
            QLinearGradient coreGrad(center.x(), center.y() - coreR, center.x(), center.y() + coreR);
            coreGrad.setColorAt(0.0, QColor(36, 38, 44, 235));
            coreGrad.setColorAt(0.6, QColor(26, 28, 32, 245));
            coreGrad.setColorAt(1.0, QColor(18, 19, 23, 255));

            painter.setPen(QPen(QColor(255, 255, 255, 30), 1.2));
            painter.setBrush(coreGrad);
            painter.drawEllipse(center, coreR, coreR);

            // Subtle inner rim shadow
            painter.setPen(QPen(QColor(0, 0, 0, 70), 1.0));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(center, coreR - 1.0, coreR - 1.0);
        }
    }
    else
    {
        // When innerRadius is 0 (completed full disk state)
        QRadialGradient fullDiskGrad(center, radius);
        fullDiskGrad.setColorAt(0.0, mColor.lighter(125));
        fullDiskGrad.setColorAt(0.7, mColor);
        fullDiskGrad.setColorAt(1.0, mColor.darker(120));

        painter.setPen(QPen(QColor(255, 255, 255, 80), 2.0));
        painter.setBrush(fullDiskGrad);
        painter.drawEllipse(center, radius - 2.0, radius - 2.0);
    }

    // 5. Center Typography / Icons
    if(mVisibleText && !mInfinilyMode)
    {
        QString numStr = QString::number(value);
        QString pctStr = "%";

        QFont numFont = this->font();
        numFont.setPixelSize(qMax(14, static_cast<int>(radius * 0.46)));
        numFont.setWeight(QFont::Bold);

        QFont pctFont = this->font();
        pctFont.setPixelSize(qMax(9, static_cast<int>(radius * 0.22)));
        pctFont.setWeight(QFont::DemiBold);

        QFontMetrics fmNum(numFont);
        QFontMetrics fmPct(pctFont);

        int numW = fmNum.horizontalAdvance(numStr);
        int pctW = fmPct.horizontalAdvance(pctStr);
        int totalW = numW + pctW + 2;

        qreal startX = center.x() - (totalW / 2.0);
        qreal numY = center.y() + (fmNum.capHeight() / 2.0);

        // Draw shadow for crisp contrast
        painter.setFont(numFont);
        painter.setPen(QColor(0, 0, 0, 160));
        painter.drawText(QPointF(startX + 1, numY + 1), numStr);

        // Draw number
        painter.setPen(QColor(255, 255, 255));
        painter.drawText(QPointF(startX, numY), numStr);

        // Draw % symbol with color accent
        painter.setFont(pctFont);
        painter.setPen(QColor(0, 0, 0, 140));
        painter.drawText(QPointF(startX + numW + 2 + 1, numY + 1), pctStr);

        painter.setPen(mColor.lighter(130));
        painter.drawText(QPointF(startX + numW + 2, numY), pctStr);
    }
    else if(mInfinilyMode && innerRadiusPx > 10.0)
    {
        // Modern center loading icon
        QFont iconFont = this->font();
        iconFont.setPixelSize(qMax(12, static_cast<int>(radius * 0.36)));
        painter.setFont(iconFont);
        painter.setPen(mColor.lighter(130));
        QRectF centerRect(center.x() - radius * 0.4, center.y() - radius * 0.4, radius * 0.8, radius * 0.8);
        painter.drawText(centerRect, Qt::AlignCenter, "⚡");
    }

    return pixmap;
}

qreal ProgressCircle::infiniteAnimationValue() const
{
    return mInfiniteAnimationValue;
}

int ProgressCircle::visibleValue() const
{
    return mVisibleValue;
}
