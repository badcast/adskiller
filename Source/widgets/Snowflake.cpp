#include <QPainter>
#include <QRandomGenerator>
#include <cmath>

#include "Snowflake.h"

Snowflake::Snowflake(QWidget *parent, std::uint32_t createCount) : QWidget(parent), frames(0)
{
    setWindowFlags(Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    mMoveSnows.setSingleShot(false);
    connect(&mMoveSnows, &QTimer::timeout, this, &Snowflake::updateAnimation);
    mMoveSnows.setInterval(16);

    createSnowflake(createCount);
}

void Snowflake::createSnowflake(std::uint32_t num)
{
    for(std::uint32_t i = 0; i < num; ++i)
    {
        mSnowflakes.push_back(SnowflakeObject {});
    }
}

void Snowflake::setSnowPixmap(const QPixmap &pixmap)
{
    mSnowflakeImage = pixmap;
}

void Snowflake::start()
{
    for(SnowflakeObject &unit : mSnowflakes)
    {
        unit.point.setX(QRandomGenerator::global()->bounded(width()));
        unit.speed = QRandomGenerator::global()->bounded(3.0 - 1.0) + 1.0;
        unit.radius = QRandomGenerator::global()->bounded(20, 30);
        unit.point.setY(-unit.radius * 10);
    }
    mMoveSnows.start();
}

void Snowflake::updateAnimation()
{
    for(SnowflakeObject &unit : mSnowflakes)
    {
        // Wind effect
        unit.point.setX(unit.point.x() + qSin(unit.point.y() / 60.0) * 0.5);
        unit.point.setY(unit.point.y() + unit.speed);
        if(unit.point.y() > height())
        {
            unit.point.setX(QRandomGenerator::global()->bounded(0, width()));
            unit.point.setY(-unit.radius);
        }
    }
    ++frames;
    update();
}

void Snowflake::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    if(!mMoveSnows.isActive())
        return;
    for(const SnowflakeObject &unit : std::as_const(mSnowflakes))
    {
        float p = unit.point.y() / std::max(1, height());
        painter.setOpacity(1 - p);
        painter.drawPixmap(std::round(unit.point.x()), std::round(unit.point.y()), unit.radius, unit.radius, mSnowflakeImage);
    }
}
