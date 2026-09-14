#include "ProgressCircle.h"
#include <QPainter>
#include <QPixmapCache>
#include <QtMath>
#include <QEasingCurve>
#include <QPainterPath>

ProgressCircle::ProgressCircle(QWidget *parent)
    : QWidget(parent), mInfinilyMode(true), mVisibleText(true), mValue(0), mMaximum(100), mInnerRadius(0.6), mOuterRadius(1.0), mColor(76, 194, 255), mVisibleValue(0), mValueAnimation(this, "visibleValue"), mInfiniteAnimation(this, "infiniteAnimationValue"), mInfiniteAnimationValue(0.0)
{
    mValueAnimation.setEasingCurve(QEasingCurve::OutCubic);
    mInfiniteAnimation.setLoopCount(-1);
    mInfiniteAnimation.setDuration(1500);
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
        mValueAnimation.setDuration(400);
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

    const qreal padding = 8.0;
    const QRectF bounds = pixmap.rect().adjusted(padding, padding, -padding, -padding);
    const qreal margin = bounds.width() * (1.0 - mOuterRadius) / 2.0;
    const QRectF circleRect = bounds.adjusted(margin, margin, -margin, -margin);

    const QPointF center = circleRect.center();
    const qreal radius = circleRect.width() / 2.0;
    if(radius <= 6.0)
        return pixmap;

    const int totalSegments = 36;
    const qreal outerR = radius;
    const qreal innerR = radius * 0.76;

    int value = qMin(mVisibleValue, mMaximum);
    qreal fillRatio = mInfinilyMode ? 0.0 : qBound(0.0, qreal(value) / qreal(mMaximum), 1.0);
    int activeSegments = qRound(fillRatio * totalSegments);

    QPen borderPen(QColor(mColor.red(), mColor.green(), mColor.blue(), 50), 1.0);
    painter.setPen(borderPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(center, outerR + 3.0, outerR + 3.0);

    for(int i = 0; i < totalSegments; ++i)
    {
        qreal angleDeg = -90.0 + (i * (360.0 / totalSegments));
        qreal rad = qDegreesToRadians(angleDeg);

        qreal cosA = std::cos(rad);
        qreal sinA = std::sin(rad);

        QPointF pInner(center.x() + cosA * innerR, center.y() + sinA * innerR);
        QPointF pOuter(center.x() + cosA * outerR, center.y() + sinA * outerR);

        bool isActive = false;
        qreal intensity = 0.0;

        if(mInfinilyMode)
        {
            qreal headPos = mInfiniteAnimationValue * totalSegments;
            qreal dist = std::fmod(std::abs(i - headPos), totalSegments);
            if(dist > totalSegments / 2.0)
                dist = totalSegments - dist;

            if(dist < 7.0)
            {
                isActive = true;
                intensity = 1.0 - (dist / 7.0);
            }
        }
        else
        {
            isActive = (i < activeSegments);
            intensity = isActive ? 1.0 : 0.0;
        }

        if(isActive)
        {
            QColor segColor = mColor.lighter(110 + static_cast<int>(intensity * 40));
            segColor.setAlpha(qBound(40, static_cast<int>(intensity * 255), 255));
            painter.setPen(QPen(segColor, 2.5, Qt::SolidLine, Qt::RoundCap));
        }
        else
        {
            painter.setPen(QPen(QColor(255, 255, 255, 25), 1.5, Qt::SolidLine, Qt::FlatCap));
        }

        painter.drawLine(pInner, pOuter);
    }

    qreal coreR = innerR - 6.0;
    if(coreR > 10.0)
    {
        painter.setPen(QPen(QColor(mColor.red(), mColor.green(), mColor.blue(), 70), 1.0, Qt::DashLine));
        painter.setBrush(QColor(10, 14, 20, 180));
        painter.drawEllipse(center, coreR, coreR);

        painter.setPen(QPen(mColor, 1.5));
        painter.drawLine(QPointF(center.x(), center.y() - coreR), QPointF(center.x(), center.y() - coreR + 4.0));
        painter.drawLine(QPointF(center.x(), center.y() + coreR), QPointF(center.x(), center.y() + coreR - 4.0));
        painter.drawLine(QPointF(center.x() - coreR, center.y()), QPointF(center.x() - coreR + 4.0, center.y()));
        painter.drawLine(QPointF(center.x() + coreR, center.y()), QPointF(center.x() + coreR - 4.0, center.y()));
    }

    if(mInfinilyMode && coreR > 12.0)
    {
        painter.save();
        QPainterPath clipPath;
        clipPath.addEllipse(center, coreR - 2.0, coreR - 2.0);
        painter.setClipPath(clipPath);

        const QString binaryStream = QStringLiteral("1010101010101010101010101010101010101010");
        const int fontSize = qMax(6, static_cast<int>(coreR * 0.17));
        QFont binFont("Consolas", fontSize, QFont::Bold);
        binFont.setStyleHint(QFont::Monospace);
        painter.setFont(binFont);

        QFontMetrics fmBin(binFont);
        const qreal lineHeight = fmBin.height() * 0.95;
        const qreal colSpacing = fmBin.horizontalAdvance(QLatin1Char('0')) * 1.55;
        const qreal streamShift = mInfiniteAnimationValue * lineHeight * 6.0;

        const int numCols = 5;
        const qreal startColX = center.x() - ((numCols - 1) * colSpacing) / 2.0;

        for(int col = 0; col < numCols; ++col)
        {
            qreal colX = startColX + col * colSpacing;
            qreal colCenterDist = std::abs(col - (numCols - 1) / 2.0);
            qreal colAlphaScale = 1.0 - (colCenterDist * 0.22);
            int colSeed = col * 7;

            for(qreal y = center.y() + coreR + lineHeight; y >= center.y() - coreR - lineHeight; y -= lineHeight)
            {
                qreal streamY = y - streamShift;
                while(streamY < center.y() - coreR)
                    streamY += (coreR * 2.0 + lineHeight * 2.0);

                qreal distFromCenter = std::sqrt(std::pow(colX - center.x(), 2) + std::pow(streamY - center.y(), 2));
                if(distFromCenter >= coreR - 2.0)
                    continue;

                int charIndex = std::abs(static_cast<int>((center.y() + coreR - streamY) / lineHeight) + colSeed) % binaryStream.length();
                QChar ch = binaryStream.at(charIndex);

                qreal edgeFade = qBound(0.0, 1.0 - (distFromCenter / (coreR - 2.0)), 1.0);

                QColor charColor;
                if((col + charIndex) % 11 == 0)
                    charColor = QColor(255, 60, 70);
                else if((col + charIndex) % 5 == 0)
                    charColor = QColor(50, 230, 130);
                else
                    charColor = QColor(0, 220, 255);

                int alpha = qBound(0, static_cast<int>(edgeFade * colAlphaScale * 140), 255);
                charColor.setAlpha(alpha);

                painter.setPen(charColor);
                painter.drawText(QPointF(colX - fmBin.horizontalAdvance(ch) / 2.0, streamY + fmBin.ascent() / 2.0), QString(ch));
            }
        }
        painter.restore();
    }
    else if(mVisibleText && !mInfinilyMode)
    {
        QString numStr = QString::number(value);

        QFont numFont("Consolas", qMax(12, static_cast<int>(coreR * 0.62)), QFont::Bold);
        numFont.setStyleHint(QFont::Monospace);
        painter.setFont(numFont);

        QFontMetrics fm(numFont);
        QRectF textRect(center.x() - coreR, center.y() - (fm.height() / 2.0), coreR * 2.0, fm.height());

        painter.setPen(QColor(240, 248, 255));
        painter.drawText(textRect, Qt::AlignCenter, numStr);
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