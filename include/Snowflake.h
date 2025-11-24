#pragma once

#include <QList>
#include <QWidget>
#include <QTimer>
#include <QPixmap>

struct SnowflakeObject
{
    QPointF point;
    float speed;
    int radius;
};

class Snowflake : public QWidget
{
public:
    explicit Snowflake(QWidget *parent = nullptr, std::uint32_t createCount = 10);

    void createSnowflake(std::uint32_t num);
    void setSnowPixmap(const QPixmap& pixmap);
    void start();

private slots:
    void updateAnimation();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QList<SnowflakeObject> mSnowflakes;
    QTimer mMoveSnows;
    QPixmap mSnowflakeImage;
};
