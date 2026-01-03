#include <cstdint>

#include <QPainter>
#include <QBrush>
#include <QMouseEvent>

#include "TetrisGame.h"

enum StaticBlocks : std::uint16_t
{
    b4x = 0x1818,
    line6xH = 0x7E,
    zInverse = 0x300C
};

QList<int> createBlocks(std::uint16_t blocks)
{
    QList<int> blockArr;
    int t,b,i;
    t = blocks & 0xFF;
    b = blocks >> 8 & 0xFF;
    blockArr.resize(16);
    for(i=0;i < 8;)
    return blockArr;
}

Tetris::Tetris(QWidget *parent) : QWidget(parent), updateTimer(this), cellSize(1.0F, 1.0F), blockSize(500, 500)
{
    resize(blockSize.scaled(blockSize.width() + 100, blockSize.height() + 100, Qt::AspectRatioMode::KeepAspectRatio));

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
    int count = qRound(sqrt(matrix.size()));
    int x, y, mx, my;
    float cellW = float(blockSize.width()) / count * cellSize.width();
    float cellH = float(blockSize.height()) / count * cellSize.height();
    QPoint mousePos = mapFromGlobal(QCursor::pos());

    QPainter p(this);
    QBrush background(QColor(0x454976));
    p.setBackground(background);

    p.drawRect(cx, cy, blockSize.width(), blockSize.height());

    mx = mousePos.x() - cx;
    my = mousePos.y() - cy;
    for(x = 0; x < count; ++x)
    {
        for(y = 0; y < count; ++y)
        {
            if(mx != -1 && mx > x * cellW && mx <= (1 + x) * cellW && my > y * cellH && my <= (1 + y) * cellH)
            {
                matrix[y * x] = 1;
                mx = -1;
            }
            if(matrix[y * x])
                p.fillRect(QRectF(cx + x * cellW, cy + y * cellH, cellW, cellH), QGradient::CrystalRiver);
            else
                p.drawRect(QRectF(cx + x * cellW, cy + y * cellH, cellW, cellH));
        }
    }
}
