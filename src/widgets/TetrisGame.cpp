#include <QPainter>
#include <QBrush>

#include "TetrisGame.h"

Tetris::Tetris(QWidget *parent) : QWidget(parent), updateTimer(this), cellSize(1.0F, 1.0F), blockSize(500, 500)
{
    setFixedSize(blockSize.scaled(600,600,Qt::AspectRatioMode::KeepAspectRatio));


    matrix.assign(8 * 8, 0);

    updateTimer.setSingleShot(false);
    updateTimer.setInterval(1000.F / 60); // 60  FPS per sec
    QObject::connect(&updateTimer, &QTimer::timeout, this, &Tetris::updateScene);
    updateTimer.start();

}

void Tetris::startGame()
{
}

void Tetris::stopGame()
{
}

void Tetris::updateScene()
{
    update();
}

void Tetris::paintEvent(QPaintEvent *event)
{
    int cx = (width() - blockSize.width()) / 2;
    int cy = (height() - blockSize.height()) / 2;
    int sqrtCount = qRound(sqrt(matrix.size()));
    int x,y;
    float cellW = float(blockSize.width()) / sqrtCount * cellSize.width();
    float cellH = float(blockSize.height()) / sqrtCount * cellSize.height();

    QPainter p(this);
    QBrush background(QColor(0x454976));
    p.worldTransform()
    p.setBackground(background);

    p.drawRect(cx,cy,blockSize.width(),blockSize.height());

    for(x=0;x < sqrtCount;++x)
    {
        for(y=0;y<sqrtCount;++y)
        {
            p.drawRect(QRectF(cx + x*cellW,cy + y*cellH,cellW,cellH));
        }
    }

}
