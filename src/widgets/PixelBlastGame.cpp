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
    block3x3 = 0x70707,

    /*
        # # # # #
        . . . . .
        . . . . .
    */
    line6x1 = 0x3F,

    /*
        # # . . .
        . . # # .
        . . . . .
    */
    zeta4x4 = 0xC03
};

PixelBlast::PixelBlast(QWidget *parent) : QWidget(parent), updateTimer(this), cellScale(1.0F, 1.0F), boardRegion(0, 0, 500, 500)
{
    resize(boardRegion.size().scaled(boardRegion.width() + 100, boardRegion.height() + 100, Qt::AspectRatioMode::IgnoreAspectRatio).toSize());

    matrix.assign(MaxCellCount * MaxCellCount, 0);

    setMouseTracking(true);
    updateTimer.setSingleShot(false);
    updateTimer.setInterval(1000.F / 60); // 60  FPS per sec
    QObject::connect(&updateTimer, &QTimer::timeout, this, &PixelBlast::updateScene);
    updateTimer.start();

    test.load(":/pixelblastgame/test");
}

void PixelBlast::startGame()
{
}

void PixelBlast::stopGame()
{
}

void PixelBlast::mousePressEvent(QMouseEvent *event)
{
    mouseBtn = event->button() & 0x3;
}

void PixelBlast::mouseReleaseEvent(QMouseEvent *event)
{
    mouseBtn = 0;
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

QList<int> rotateClockwise(QList<int> blocks)
{
    // TODO: Make rotate matrix
    QList<int> newblocks(blocks.size());
    return newblocks;
}

void PixelBlast::assignBlocks(QList<int> blocks)
{
    int i, x, y;

    plants.rows = 0;
    plants.columns = 0;
    plants.blockPoints.clear();
    plants.shadowBlockPoints.clear();
    if(blocks.empty())
    {
        return;
    }

    for(i = 0; i < blocks.size(); ++i)
    {
        if(blocks[i])
        {
            // get point from matrix
            x = i % cellSquare;
            y = i / cellSquare;

            // calcluate counts
            plants.columns = qMax(1, qMax(plants.columns, x + 1));
            plants.rows = qMax(1, qMax(plants.rows, y + 1));
            // append block
            plants.blockPoints.emplaceBack(x, y);
        }
    }
    plants.shadowBlockPoints.resize(plants.blockPoints.size(), {});
}

void PixelBlast::resizeEvent(QResizeEvent *event)
{
    updateData();
}

void PixelBlast::updateData()
{
    cellSquare = qRound(sqrt(matrix.size()));
    cellSize = {float(boardRegion.width()) / cellSquare, float(boardRegion.height()) / cellSquare};
    scaleFactor = {cellScale.width() * cellSize.width(), cellScale.height() * cellSize.height()};
    boardRegion.moveTopLeft({(width() - boardRegion.width()) / 2, (height() - boardRegion.height()) / 2});
    assignBlocks(createBlocks(line6x1));
}

void PixelBlast::updateScene()
{
    int x, y, z, w, d;
    static int lastCellIdx = -1;

    mousePoint = mapFromGlobal(QCursor::pos());

    for(auto &elem : matrix)
    {
        if(elem == 2)
            elem = 0;
    }

    if(!plants.blockPoints.empty())
    {
        // Calculate template points
        for(x = 0; x < plants.shadowBlockPoints.size(); ++x)
        {
            plants.shadowBlockPoints[x].x = (mousePoint.x() - static_cast<float>(plants.columns * scaleFactor.width()) / 2 + plants.blockPoints[x].x() * scaleFactor.width());
            plants.shadowBlockPoints[x].y = (mousePoint.y() - static_cast<float>(plants.rows * scaleFactor.height()) / 2 + plants.blockPoints[x].y() * scaleFactor.height());
            plants.shadowBlockPoints[x].idx = -1;
        }

        x = plants.columns * scaleFactor.width();
        y = plants.rows * scaleFactor.height();
        if(boardRegion.width() > 0 && boardRegion.height() > 0 && boardRegion.contains(QRectF(plants.shadowBlockPoints[0].x, plants.shadowBlockPoints[0].y, x, y)))
        {
            d = 0;
            for(w = 0; w < plants.shadowBlockPoints.size(); ++w)
            {
                x = qBound<int>(0, cellSquare * (plants.shadowBlockPoints[w].x - boardRegion.x() + scaleFactor.width() / 2) / (boardRegion.width()), cellSquare - 1);
                y = qBound<int>(0, cellSquare * (plants.shadowBlockPoints[w].y - boardRegion.y() + scaleFactor.height() / 2) / (boardRegion.height()), cellSquare - 1);
                z = y * cellSquare + x;
                if(matrix[z] != 0)
                {
                    break;
                }
                plants.shadowBlockPoints[w].idx = z;
                d |= 1 << w;
            }
            // verification
            if(d == (1 << plants.shadowBlockPoints.size()) - 1)
            {
                for(x = 0; x < w; ++x)
                {
                    matrix[plants.shadowBlockPoints[x].idx] = mouseBtn == Qt::LeftButton ? 1 : 2;
                }

                if(mouseBtn == Qt::LeftButton)
                {
                    assignBlocks(createBlocks(zeta4x4));
                }
            }
        }
    }

    update();
}

void PixelBlast::paintEvent(QPaintEvent *event)
{
    int x, y, z, w;

    QRectF dest;
    QBrush background(QColor(0x454976));
    QSizeF _scaleFactor = scaleFactor;
    QPainter p(this);

    p.setBackground(background);
    p.fillRect(rect(), background);
    p.drawRect(boardRegion);
    dest.setSize(_scaleFactor);

    for(x = 0; x < cellSquare; ++x)
    {
        for(y = 0; y < cellSquare; ++y)
        {
            z = y * cellSquare + x;
            dest.moveLeft(boardRegion.x() + x * _scaleFactor.width());
            dest.moveTop(boardRegion.y() + y * _scaleFactor.height());
            if(matrix[z] == 1)
            {
                p.drawPixmap(dest, test, {});
            }
            else if(matrix[z] == 2)
            {
                p.fillRect(dest, QGradient::BigMango);
            }
            else
            {
                p.drawRect(dest);
            }
            p.drawText(dest.x() + 15, dest.y() + 30, QString("%1").arg(z));
        }
    }

    p.setOpacity(0.5f);
    _scaleFactor * 0.9F;
    dest.setSize(_scaleFactor);
    for(x = 0; x < plants.blockPoints.size(); ++x)
    {
        // TODO: padding add for blocks with new scaleFactor (preview content)
        dest.moveLeft(plants.shadowBlockPoints[x].x);
        dest.moveTop(plants.shadowBlockPoints[x].y);
        p.drawPixmap(dest, test, QRectF());
    }
}
