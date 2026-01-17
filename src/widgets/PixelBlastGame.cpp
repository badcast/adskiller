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
std::shared_ptr<QPixmap> gridCellBright {};
std::shared_ptr<QList<BlockResource>> BlockRes {};

QPixmap addjustBright(const QPixmap& pixmap, int brightness){
    QImage img = pixmap.toImage();
    QColor color;
    int x,y;
    for(x=0;x<img.width();++x)
    {
        for(y=0;y<img.height();++y)
        {
            color = std::move(img.pixelColor(x,y));
            color.setRed(qBound(0, color.red() + brightness, 255));
            color.setGreen(qBound(0, color.green() + brightness, 255));
            color.setBlue(qBound(0, color.blue() + brightness, 255));
            img.setPixelColor(x,y,color);
        }
    }
    return QPixmap::fromImage(img);
}

void init()
{
    // Load and initialize globalResources
    if(BlockRes != nullptr)
        return;

    constexpr auto _formatResourceName = ":/pixelblastgame/Blocks/%s";
    constexpr auto _formatBlocks = "%s %d %s";
    constexpr auto MaxBufLen = 128;
    int n;
    char buff[MaxBufLen];
    char buff0[64];
    char buff1[64];

    BlockResource tmp;
    QString content, bfName;
    std::snprintf(buff, MaxBufLen, _formatResourceName, "blocks.cfg");
    QFile file(buff);
    if(!file.open(QFile::ReadOnly | QFile::Text))
    {
        throw std::runtime_error("Resource is not access");
        return;
    }

    BlockRes = std::make_shared<QList<BlockResource>>();
    QTextStream stream(&file);
    while(stream.readLineInto(&content))
    {
        if(sscanf((content).toLocal8Bit().data(), _formatBlocks, buff, &n, buff0) != 3)
        {
            BlockRes.reset();
            return;
        }
        tmp.name = buff;
        for(int i = 0; i < n; ++i)
        {
            snprintf(buff, MaxBufLen, _formatResourceName, buff0);
            content = buff;
            content.replace(QChar('#'), QString::number(i + 1));
            QPixmap qp(content);
            tmp.resources.append(std::move(qp));
        }
        BlockRes->append(std::move(tmp));
    }
    gridCell = std::make_shared<QPixmap>(std::move(addjustBright(QPixmap(":/pixelblastgame/grid-cell"), 40)));
    gridCellBright = std::make_shared<QPixmap>(std::move(addjustBright(*gridCell, 90)));
}

PixelBlast::PixelBlast(QWidget *parent) : QWidget(parent), updateTimer(this), cellScale(1.0F, 1.0F),
                                          boardRegion(0, 0, 500, 500), scores(0), frames(0), frameIndex(0), destroyScaler(0)
{
    init();

    resize(boardRegion.size().scaled(boardRegion.width() + 100, boardRegion.height() + 100, Qt::AspectRatioMode::IgnoreAspectRatio).toSize());

    cellSquare = MaxCellWidth;
    grid.assign(MaxCellWidth * MaxCellWidth, 0);
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

QList<std::uint8_t> PixelBlast::createBlocks(int blocks)
{
    int x;
    QList<std::uint8_t> blockArr {};
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

void PixelBlast::assignBlocks(const QList<std::uint8_t> &blocks)
{
    int x, y, z;

    shape.rows = 0;
    shape.columns = 0;
    shape.shapeColor = 0;
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
            shape.blocks.emplaceBack(x,y);
        }
    }
    x = (BlockRes == nullptr) ? 0 : BlockRes->size();
    shape.shapeColor = QRandomGenerator::global()->bounded(0, x);
}

QPixmap *PixelBlast::getColoredPixmap(int color)
{
    BlockResource * br = &(*BlockRes)[color];
    return &(br->resources[frameIndex % br->resources.size()]);
}

void PixelBlast::resizeEvent(QResizeEvent *event)
{
    updateData();
}

void PixelBlast::updateData()
{
    cellSquare = qRound(sqrt(grid.size()));
    cellSize = {float(boardRegion.width()) / cellSquare, float(boardRegion.height()) / cellSquare};
    scaleFactor = {cellScale.width() * cellSize.width(), cellScale.height() * cellSize.height()};
    boardRegion.moveTopLeft({(width() - boardRegion.width()) / 2, (height() - boardRegion.height()) / 2});
}

void PixelBlast::updateScene()
{
    int x, y, z, w, d, i;
    QPointF tmp;

    mousePoint = mapFromGlobal(QCursor::pos());

    if(destroyScaler == 0.0F)
    {
        destroyBlocks.clear();
    }

    for(x = 0; x < shape.blocks.size(); ++x)
    {
        y = shape.blocks[x].idx;
        if(y == -1)
            break;
        grid[y] = 0;
    }

    if(!currentBlocks.empty())
    {
        assignBlocks(currentBlocks);
        currentBlocks.clear();
    }

    if(!shape.blocks.empty())
    {
        // Calculate template points
        for(x = 0; x < shape.blocks.size(); ++x)
        {
            // shape.blocks[x].x = (mousePoint.x() - static_cast<float>(shape.columns * scaleFactor.width()) / 2 + shape.blockPoints[x].x() * scaleFactor.width());
            // shape.blocks[x].y = (mousePoint.y() - static_cast<float>(shape.rows * scaleFactor.height()) / 2 + shape.blockPoints[x].y() * scaleFactor.height());
            shape.blocks[x].idx = -1;
        }

        if(boardRegion.width() > 0 && boardRegion.height() > 0)
        {
            d = 0;
            for(w = 0; w < shape.blocks.size(); ++w)
            {
                tmp.setX(mousePoint.x() - static_cast<float>(shape.columns * scaleFactor.width()) / 2);
                tmp.setY(mousePoint.y() - static_cast<float>(shape.rows * scaleFactor.height()) / 2 );
                tmp = std::move(shape.blocks[w].adjustPoint(tmp,scaleFactor));
                x = qBound<int>(0, cellSquare * (tmp.x() - boardRegion.x() + scaleFactor.width() / 2) / (boardRegion.width()), cellSquare - 1);
                y = qBound<int>(0, cellSquare * (tmp.y() - boardRegion.y() + scaleFactor.height() / 2) / (boardRegion.height()), cellSquare - 1);
                z = y * cellSquare + x;
                if((grid[z] & 0x3) != 0 || ((d >> z) & 0x1) == 1)
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
                    grid[y] = d | shape.shapeColor << 2;
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
                            if((grid[z * cellSquare + i] & 0x3) == 1)
                                x++;

                            if((grid[i * cellSquare + w] & 0x3) == 1)
                                y++;
                        }
                        if(x == cellSquare)
                        {
                            scores += cellSquare;
                            for(x = 0; x < cellSquare; ++x)
                            {
                                i = z * cellSquare + x;
                                destroyBlocks.append(std::make_pair(std::move(BlockObject(x,z,i)), grid[i]>>2));
                                grid[i] = 0x0;
                            }
                            destroyScaler = 1.0F;
                        }
                        if(y == cellSquare)
                        {
                            scores += cellSquare;
                            for(y = 0; y < cellSquare; ++y)
                            {
                                i = y * cellSquare + w;
                                destroyBlocks.append(std::make_pair(std::move(BlockObject(w,y,i)), grid[i]>>2));
                                grid[i] = 0x0;
                            }
                            destroyScaler = 1.0F;
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
    destroyScaler = qBound(0.0F, destroyScaler - 0.06F ,1.0F);
}

void PixelBlast::paintEvent(QPaintEvent *event)
{
    int x, y, z, w;
    QRectF dest;
    QBrush background(QColor(0x6a2e76));
    QPainter p(this);
    QPixmap *pixmap;
    QPointF adjust;

    p.setBackground(background);
    p.fillRect(rect(), background);
    dest.setSize(scaleFactor);

    for(z = 0; z < grid.size(); ++z)
    {
        x = z % cellSquare;
        y = z / cellSquare;
        dest.moveLeft(boardRegion.x() + x * scaleFactor.width());
        dest.moveTop(boardRegion.y() + y * scaleFactor.height());

        w = grid[z] & 0x3;
        if(w == 2)
        {
            p.drawPixmap(dest, *gridCellBright, {});
        }
        else
        {
            p.drawPixmap(dest, *gridCell, {});
        }

        if(w == 1)
        {
            pixmap = getColoredPixmap(grid[z] >> 2);
            p.drawPixmap(dest, *pixmap, {});
        }
    }

    dest.setSize(scaleFactor * destroyScaler);
    adjust = {static_cast<float>(shape.columns * dest.width()) / 2,
              static_cast<float>(shape.rows * dest.height()) / 2 };
    for(x = 0; x < destroyBlocks.size(); ++x)
    {
        const auto & db = destroyBlocks[x];
        dest.moveTopLeft(db.first.adjustPoint(adjust, dest.size()));
        pixmap = getColoredPixmap(db.second);
        p.drawPixmap(dest, *pixmap, {});
    }

    p.setOpacity(0.7f);
    dest.setSize(scaleFactor* 0.7F);
    adjust = {mousePoint.x() - static_cast<float>(shape.columns * dest.width()) / 2,
              mousePoint.y() - static_cast<float>(shape.rows * dest.height()) / 2 };
    for(x = 0; x < shape.blocks.size(); ++x)
    {
        dest.moveTopLeft(shape.blocks[x].adjustPoint(adjust, dest.size()));
        pixmap = getColoredPixmap(shape.shapeColor);
        p.drawPixmap(dest, *pixmap, {});
    }
}

QPointF PixelBlast::BlockObject::adjustPoint(const QPointF &adjust, const QSizeF &scale) const
{
    QPointF out;
    out.setX((adjust.x() + x * scale.width()));
    out.setY( (adjust.y() + y * scale.height()));
    return out;
}
