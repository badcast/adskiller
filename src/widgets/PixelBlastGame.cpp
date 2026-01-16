#include <cstdint>
#include <cstring>
#include <utility>
#include <memory>
#include <stdexcept>

#include <QFile>
#include <QRandomGenerator>
#include <QPainter>
#include <QBrush>
#include <QMouseEvent>
#include <QTextStream>

#include "PixelBlastGame.h"
#include "PixelBlastShapes.h"

constexpr int MaxCellWidth = 8;

struct BlockResource
{
    QString name;
    QList<QPixmap> resources;
};

std::shared_ptr<QPixmap> gridCell {};
std::shared_ptr<QList<BlockResource>> gr {};

void init()
{
    // Load and initialize globalResources
    if(gr != nullptr)
        return;

    constexpr auto _formatResourceName = ":/pixelblastgame/Blocks/%s";
    constexpr auto _formatBlocks = "%s %d %s";
    constexpr auto MaxBufLen = 128;
    int n;
    char buff[MaxBufLen];
    char buff0[64];
    char buff1[64];

    BlockResource bRes;
    QString content, bfName;
    std::snprintf(buff, MaxBufLen, _formatResourceName, "blocks.cfg");
    QFile file(buff);
    if(!file.open(QFile::ReadOnly | QFile::Text))
    {
        throw std::runtime_error("Resource is not access");
        return;
    }

    gr = std::make_shared<QList<BlockResource>>();
    QTextStream stream(&file);
    while(stream.readLineInto(&content))
    {
        if(sscanf((content).toLocal8Bit().data(), _formatBlocks, buff, &n, buff0) != 3)
        {
            gr.reset();
            return;
        }
        bRes.name = buff;
        for(int i = 0; i < n; ++i)
        {
            snprintf(buff, MaxBufLen, _formatResourceName, buff0);
            content = buff;
            content.replace(QChar('#'), QString::number(i + 1));
            QPixmap qp(content);
            bRes.resources.append(std::move(qp));
        }
        gr->append(std::move(bRes));
    }
    gridCell = std::make_shared<QPixmap>(":/pixelblastgame/grid-cell");
}

PixelBlast::PixelBlast(QWidget *parent) : QWidget(parent), updateTimer(this), cellScale(1.0F, 1.0F), boardRegion(0, 0, 500, 500), scores(0), frames(0), frameIndex(0)
{
    init();

    resize(boardRegion.size().scaled(boardRegion.width() + 100, boardRegion.height() + 100, Qt::AspectRatioMode::IgnoreAspectRatio).toSize());

    cellSquare = MaxCellWidth;
    matrix.assign(MaxCellWidth * MaxCellWidth, 0);
    assignBlocks(createBlocks(0x020302));

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
    int x;
    QList<int> blockArr {};
    blockArr.resize(sizeof(blocks) * 8);
    for(x = blockArr.size() - 1; x >= 0; --x)
    {
        blockArr[x] = (blocks >> x) & 0x1;
    }
    return blockArr;
}

// TEST ALGO FOR ROTATE BY CLOCKWISE
// QList<int> rotateClockwise(const QList<int> &blockSrc, int blockWidth = MaxCellWidth)
// {
//     int height, x, y, z, w, d;
//     QList<int> rotated;
//     rotated.resize(blockSrc.size(), 0);
//     height = blockSrc.size() / blockWidth;
//     for(y = 0; y < height; ++y)
//     {
//         for(x = 0; x < blockWidth; ++x)
//         {
//             z = y * blockWidth + x;
//             w = height - 1 - y;
//             d = x * height + w;
//             rotated[d] = blockSrc[z];
//         }
//     }
//     return rotated;
// }

void PixelBlast::assignBlocks(const QList<int> &blocks)
{
    int x, y, z;

    shape.rows = 0;
    shape.columns = 0;
    shape.shapeColor = 0;
    shape.blockPoints.clear();
    shape.blocks.clear();
    if(blocks.empty())
    {
        return;
    }
    for(z = 0; z < blocks.size(); ++z)
    {
        if(blocks[z])
        {
            // get point from matrix
            x = z % cellSquare;
            y = z / cellSquare;

            // calcluate counts
            shape.columns = qMax(1, qMax(shape.columns, x + 1));
            shape.rows = qMax(1, qMax(shape.rows, y + 1));
            // append block
            shape.blockPoints.emplaceBack(x, y);
        }
    }
    x = gr == nullptr ? 0 : gr->size();
    shape.shapeColor = QRandomGenerator::global()->bounded(0, x);
    shape.blocks.resize(shape.blockPoints.size(), {});
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
}

int PixelBlast::getBlockColor(int idx)
{
    return matrix[idx] >> 2;
}

void PixelBlast::setBlockColor(int idx, int val)
{
    matrix[idx] = (matrix[idx] & 0x3) | (val << 2);
}

void PixelBlast::updateScene()
{
    int x, y, z, w, d, i;

    mousePoint = mapFromGlobal(QCursor::pos());

    for(x = 0; x < shape.blocks.size(); ++x)
    {
        y = shape.blocks[x].idx;
        if(y == -1)
            break;
        matrix[y] = 0;
    }

    if(!currentBlocks.empty())
    {
        assignBlocks(currentBlocks);
        currentBlocks.clear();
    }

    if(!shape.blockPoints.empty())
    {
        // Calculate template points
        for(x = 0; x < shape.blocks.size(); ++x)
        {
            shape.blocks[x].x = (mousePoint.x() - static_cast<float>(shape.columns * scaleFactor.width()) / 2 + shape.blockPoints[x].x() * scaleFactor.width());
            shape.blocks[x].y = (mousePoint.y() - static_cast<float>(shape.rows * scaleFactor.height()) / 2 + shape.blockPoints[x].y() * scaleFactor.height());
            shape.blocks[x].idx = -1;
        }

        if(boardRegion.width() > 0 && boardRegion.height() > 0 /* && boardRegion.contains(test)*/)
        {
            d = 0;
            for(w = 0; w < shape.blocks.size(); ++w)
            {
                x = qBound<int>(0, cellSquare * (shape.blocks[w].x - boardRegion.x() + scaleFactor.width() / 2) / (boardRegion.width()), cellSquare - 1);
                y = qBound<int>(0, cellSquare * (shape.blocks[w].y - boardRegion.y() + scaleFactor.height() / 2) / (boardRegion.height()), cellSquare - 1);
                z = y * cellSquare + x;
                if((matrix[z] & 0x3) != 0 || ((d >> z) & 0x1) == 1)
                    break;
                // d |= 1 << w;
                d |= 1 << z;
                shape.blocks[w].idx = z;
            }
            // verification
            if(w == shape.blocks.size())
            {
                d = mouseBtn == Qt::LeftButton ? 1 : 2;
                for(x = 0; x < w; ++x)
                {
                    y = shape.blocks[x].idx;
                    matrix[y] = d | shape.shapeColor << 2;
                }

                if(d == 1)
                {
                    // Test destroy block-points, and optimization
                    for(d = 0; d < shape.blocks.size(); ++d)
                    {
                        x = 0;
                        y = 0;
                        w = shape.blocks[d].idx % cellSquare;
                        z = shape.blocks[d].idx / cellSquare;
                        for(i = 0; i < cellSquare; ++i)
                        {
                            if((matrix[z * cellSquare + i] & 0x3) == 1)
                                x++;

                            if((matrix[i * cellSquare + w] & 0x3) == 1)
                                y++;
                        }
                        if(x == cellSquare)
                        {
                            scores += cellSquare;
                            for(x = 0; x < cellSquare; ++x)
                            {
                                matrix[z * cellSquare + x] = 0x0;
                            }
                        }
                        if(y == cellSquare)
                        {
                            scores += cellSquare;
                            for(y = 0; y < cellSquare; ++y)
                            {
                                matrix[y * cellSquare + w] = 0x0;
                            }
                        }
                    }
                    shape.blocks[0].idx = -1;

                    currentBlocks = createBlocks(randomShapes());
                }
            }
        }
    }

    update();
    frames++;
    frameIndex += frames % 8 == 0;
}

void PixelBlast::paintEvent(QPaintEvent *event)
{
    int x, y, z, w;
    QRectF dest;
    QBrush background(QColor(0x6a2e76));
    QSizeF _scaleFactor = scaleFactor;
    QPainter p(this);
    BlockResource *br;
    QPixmap *pixmap;

    p.setBackground(background);
    p.fillRect(rect(), background);
    dest.setSize(_scaleFactor);

    for(x = 0; x < cellSquare; ++x)
    {
        for(y = 0; y < cellSquare; ++y)
        {
            z = y * cellSquare + x;
            dest.moveLeft(boardRegion.x() + x * _scaleFactor.width());
            dest.moveTop(boardRegion.y() + y * _scaleFactor.height());
            p.drawPixmap(dest, *gridCell, {});

            w = matrix[z] & 0x3;
            if(w == 1)
            {
                br = &(*gr)[getBlockColor(z)];
                pixmap = &(br->resources[frameIndex % br->resources.size()]);
                p.drawPixmap(dest, *pixmap, {});
            }
            else if(w == 2)
            {
                p.fillRect(dest, QGradient::BigMango);
            }
        }
    }

    p.setOpacity(0.5f);
    _scaleFactor * 0.9F;
    dest.setSize(_scaleFactor);
    for(x = 0; x < shape.blockPoints.size(); ++x)
    {
        // TODO: padding add for blocks with new scaleFactor (preview content)
        dest.moveLeft(shape.blocks[x].x);
        dest.moveTop(shape.blocks[x].y);

        br = &(*gr)[shape.shapeColor];
        pixmap = &(br->resources[frameIndex % br->resources.size()]);
        p.drawPixmap(dest, *pixmap, {});
    }
}
