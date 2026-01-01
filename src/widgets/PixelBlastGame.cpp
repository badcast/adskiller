#include <cstdint>

#include <QPainter>
#include <QBrush>
#include <QMouseEvent>

#include "PixelBlastGame.h"

constexpr int MaxCellCount = 8;

enum StaticBlocks
{
    /*
        # # . . .
        # # . . .
        . . . . .
    */
    block4xHV = 0x303,

    /*
        # # # # #
        . . . . .
        . . . . .
    */
    line6xH = 0x007E,

    /*
        # # # # #
        . . . . .
        . . . . .
    */
    zeta4xHV = 0x300C
};

PixelBlast::PixelBlast(QWidget *parent) : QWidget(parent), updateTimer(this), cellSize(1.0F, 1.0F), _drawRegion(0, 0, 500, 500)
{
    resize(_drawRegion.size().scaled(_drawRegion.width() + 100, _drawRegion.height() + 100, Qt::AspectRatioMode::KeepAspectRatio));

    matrix.assign(MaxCellCount * MaxCellCount, 0);

    setMouseTracking(true);
    updateTimer.setSingleShot(false);
    updateTimer.setInterval(1000.F / 60); // 60  FPS per sec
    QObject::connect(&updateTimer, &QTimer::timeout, this, &PixelBlast::updateScene);
    updateTimer.start();
}

void PixelBlast::startGame()
{
}

void PixelBlast::stopGame()
{
}

void PixelBlast::updateScene()
{
    mousePoint = mapFromGlobal(QCursor::pos());

    QPoint mouselocal = mousePoint - _drawRegion.topLeft();
    if(mouseBtn == Qt::LeftButton && _drawRegion.width() > 0 && _drawRegion.height() > 0 && mouselocal.x() >= 0 && mouselocal.y() >= 0 && mouselocal.x() <= _drawRegion.width() && mouselocal.y() <= _drawRegion.height())
    {
        int x = qBound(0, cellCount * mouselocal.x() / _drawRegion.width(), cellCount - 1);
        int y = qBound(0, cellCount * mouselocal.y() / _drawRegion.height(), cellCount - 1);

        matrix[y * cellCount + x] = 1;
    }

    update();
}

void PixelBlast::mousePressEvent(QMouseEvent *event)
{
    mouseBtn = event->button() & 0x3;
}

void PixelBlast::mouseReleaseEvent(QMouseEvent *event)
{
    mouseBtn = 0;
}

void PixelBlast::updateData()
{
    _drawRegion = {(width() - _drawRegion.width()) / 2, (height() - _drawRegion.height()) / 2, _drawRegion.width(), _drawRegion.height()};
    cellCount = qRound(sqrt(matrix.size()));
    cellSize = {float(_drawRegion.width()) / cellCount, float(_drawRegion.height()) / cellCount};
    assignBlocks(createBlocks(line6xH));
}

QList<int> PixelBlast::createBlocks(int blocks)
{
    int i;
    QList<int> blockArr {};
    blockArr.resize(sizeof(blocks) * 8);
    for(i = blockArr.size() - 1; i >= 0; --i)
    {
        blockArr[i] = (blocks >> i) & 0x1;
    }
    return blockArr;
}

void PixelBlast::assignBlocks(QList<int> blocks)
{
    int i, x, y;

    curBlock.totalH = 0;
    curBlock.totalV = 0;
    curBlock.blocks.clear();
    curBlock.blockSize = {};
    if(blocks.empty())
    {
        return;
    }

    for(i = 0; i < blocks.size(); ++i)
    {
        if(blocks[i])
        {
            // get point from matrix
            x = i % MaxCellCount;
            y = i / MaxCellCount;
            // calcluate total size
            curBlock.totalH = qMax(1, qMax(curBlock.totalH, x+1));
            curBlock.totalV = qMax(1, qMax(curBlock.totalV, y+1));

            // append block
            curBlock.blocks.append(QPoint(x, y));
        }
    }

    x = curBlock.totalH * cellSize.width();
    y = curBlock.totalV * cellSize.height();

    curBlock.blockSize = QSize {x, y};
}

void PixelBlast::resizeEvent(QResizeEvent *event)
{
    updateData();
}

void PixelBlast::paintEvent(QPaintEvent *event)
{
    int x, y, z, w;

    QPainter p(this);
    QBrush background(QColor(0x454976));
    p.setBackground(background);
    p.fillRect(rect(), background);

    p.drawRect(_drawRegion);

    for(x = 0; x < cellCount; ++x)
    {
        for(y = 0; y < cellCount; ++y)
        {
            z = _drawRegion.x() + x * cellSize.width();
            w = _drawRegion.y() + y * cellSize.height();
            if(matrix[y * cellCount + x])
                p.fillRect(QRectF(z, w, cellSize.width(), cellSize.height()), QGradient::CrystalRiver);
            else
                p.drawRect(QRectF(z, w, cellSize.width(), cellSize.height()));
            p.drawText(z + 15, w + 30, QString("%1").arg(y * cellCount + x));
        }
    }

    for(z = 0; z < curBlock.blocks.size(); ++z)
    {
        x = mousePoint.x() - curBlock.blocks[z].x() * cellSize.width();
        y = mousePoint.y() - curBlock.blocks[z].y() * cellSize.height();
        p.fillRect(QRect(x, y, cellSize.width(), cellSize.height()), QGradient::CrystalRiver);
    }
}
