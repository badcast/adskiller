#include <cstdint>
#include <cstring>
#include <utility>
#include <memory>
#include <stdexcept>
#include <functional>

#include <QFile>
#include <QRandomGenerator>
#include <QPainter>
#include <QBrush>
#include <QPalette>
#include <QMouseEvent>
#include <QTextStream>
#include <QMessageBox>
#include <QCursor>

#include "PixelBlastGame.h"
#include "PixelBlastShapes.h"

constexpr int MaxCellWidth = 8;

struct BlockResource
{
    QString name;
    QList<QPixmap> resources;
};

std::shared_ptr<QPixmap> gameLogo {};
std::shared_ptr<QPixmap> gridCell {};
std::shared_ptr<QPixmap> gridCellBright {};
std::shared_ptr<QPixmap> backgroundPix {};
std::shared_ptr<QPixmap> cursorPix {};
std::shared_ptr<QPixmap> gridBackgroundBorder {};
std::shared_ptr<QPixmap> gridBackground {};
std::shared_ptr<QPixmap> gridBackgroundBg {};
std::shared_ptr<QPixmap> uiTopHeader {};
std::shared_ptr<QList<BlockResource>> BlockRes {};

QPixmap adjustBright(const QPixmap &pixmap, int brightness)
{
    QImage img = pixmap.toImage();
    QColor color;
    int x, y;
    for(x = 0; x < img.width(); ++x)
    {
        for(y = 0; y < img.height(); ++y)
        {
            color = std::move(img.pixelColor(x, y));
            color.setRed(qBound(0, color.red() + brightness, 255));
            color.setGreen(qBound(0, color.green() + brightness, 255));
            color.setBlue(qBound(0, color.blue() + brightness, 255));
            img.setPixelColor(x, y, color);
        }
    }
    return QPixmap::fromImage(img);
}

template <typename InT, typename OutT>
constexpr inline OutT map(const InT x, const InT in_min, const InT in_max, const OutT out_min, const OutT out_max)
{
    if(in_max == in_min)
    {
        return out_min;
    }
    OutT mapped_value = (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
    return qMin(qMax(mapped_value, out_min), out_max);
}

inline QPixmap *getColoredPixmap(int color, int frameIndex)
{
    BlockResource *br = &(*BlockRes)[color];
    return &(br->resources[frameIndex % br->resources.size()]);
}

inline void drawShapeAt(const ShapeBlock &shape, QPointF destPoint, QSizeF size, int frameIndex, QPainter &p, QPixmap *pixmap = nullptr)
{
    int x;
    QRectF dest;
    dest.setSize(size);
    if(pixmap == nullptr)
        pixmap = getColoredPixmap(shape.shapeColor, frameIndex);
    for(x = 0; x < shape.blocks.size(); ++x)
    {
        dest.moveTopLeft(shape.blocks[x].adjustPoint(destPoint, dest.size()));
        p.drawPixmap(dest, *pixmap, {});
    }
};

void prepareResources()
{
    // Load and initialize globalResources
    if(BlockRes != nullptr)
        return;

    constexpr auto _formatResourceName = ":/pixelblastgame/resourcepacks/blocks/%s";
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
    gameLogo = std::make_shared<QPixmap>(std::move(QPixmap(":/pixelblastgame/game-logo")));
    uiTopHeader = std::make_shared<QPixmap>(std::move(QPixmap(":/pixelblastgame/ui-top")));
    backgroundPix = std::make_shared<QPixmap>(std::move(QPixmap(":/pixelblastgame/background")));
    cursorPix = std::make_shared<QPixmap>(std::move(QPixmap(":/pixelblastgame/arrow")));
    gridBackgroundBorder = std::make_shared<QPixmap>(std::move(QPixmap(":/pixelblastgame/grid-border")));
    gridBackground = std::make_shared<QPixmap>(std::move(QPixmap(":/pixelblastgame/grid-background")));
    gridBackgroundBg = std::make_shared<QPixmap>(std::move(QPixmap(":/pixelblastgame/grid-background-bg")));
    gridCell = std::make_shared<QPixmap>(std::move(adjustBright(QPixmap(":/pixelblastgame/grid-cell"), 40)));
    gridCellBright = std::make_shared<QPixmap>(std::move(adjustBright(*gridCell, 90)));
}

PixelBlast::PixelBlast(QWidget *parent) : QWidget(parent), updateTimer(this), boardRegion(0, 0, 400, 400), round(0), cellScale(1.0F, 1.0F), shapeCandidateIdx(-1), scores(0), frames(0), frameIndex(0), destroyScaler(0)
{
    prepareResources();

    resize(boardRegion.size().scaled(boardRegion.width() + 50, boardRegion.height() + 50, Qt::AspectRatioMode::IgnoreAspectRatio).toSize());

    cellSquare = MaxCellWidth;
    grid.assign(MaxCellWidth * MaxCellWidth, 0);

    setMouseTracking(true);
    updateTimer.setSingleShot(false);
    updateTimer.setInterval(1000.F / 60); // 60  FPS per sec

    QBrush background(*backgroundPix);
    QPalette pal = palette();
    pal.setBrush(QPalette::Window, background);
    setPalette(pal);
    setAutoFillBackground(true);

    QCursor cur(*cursorPix, 0, 0);
    setCursor(cur);

    QObject::connect(&updateTimer, &QTimer::timeout, this, &PixelBlast::updateScene);
}

void PixelBlast::startGame()
{
    round = 0;
    shapeCandidateIdx = -1;
    currentShape.reset();
    std::fill(std::begin(shapeCandidates), std::end(shapeCandidates), nullptr);

    updateTimer.start();
}

void PixelBlast::stopGame()
{
    updateTimer.stop();
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
    int x, y;
    QList<std::uint8_t> blockArr {};
    blockArr.resize(sizeof(blocks) * 8);
    for(x = 0, y = 0; x < blockArr.size(); ++x)
    {
        if((blockArr[x] = (blocks >> x) & 0x1))
            y = x;
    }
    blockArr.resize(y + 1);
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

void PixelBlast::assignBlocks(const QList<std::uint8_t> &blocks, ShapeBlock &assign)
{
    int x, y, z;

    assign.rows = 0;
    assign.columns = 0;
    assign.shapeColor = 0;
    assign.rawBlocks.clear();
    assign.blocks.clear();
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
            assign.columns = qMax(1, qMax(assign.columns, x + 1));
            assign.rows = qMax(1, qMax(assign.rows, y + 1));
            assign.blocks.emplaceBack(x, y);
        }
    }
    x = (BlockRes == nullptr) ? 0 : BlockRes->size();
    assign.rawBlocks = blocks;
    assign.shapeColor = QRandomGenerator::global()->bounded(0, x);
}

void PixelBlast::resizeEvent(QResizeEvent *event)
{
    updateData();
}

void PixelBlast::updateData()
{
    cellSquare = qRound(sqrt(grid.size()));
    cellSize = boardRegion.size() / static_cast<float>(cellSquare);
    scaleFactor = {cellScale.width() * cellSize.width(), cellScale.height() * cellSize.height()};
    boardRegion.moveTopLeft({(width() - boardRegion.width()) / 2, (height() - boardRegion.height()) / 2});
}

bool PixelBlast::canTrigger(const QList<std::uint8_t> &blocks, QList<std::uint8_t> &grids, bool placeTo)
{
    int x, y, z, w, d;
    bool fits;
    int width = qRound(std::sqrt(grids.size()));
    for(x = 0; x < width; ++x)
    {
        for(y = 0; y < width; ++y)
        {
            fits = true;
            for(z = 0; z < blocks.size(); ++z)
            {
                if(blocks[z] == 0)
                    continue;
                w = x + z % width;
                d = y + z / width;
                if(w < 0 || d < 0 || w >= width || d >= width)
                {
                    fits = false;
                    break;
                }
                w = grids[d * width + w] & 0x1;
                if(blocks[z] == 1 && w == 1)
                {
                    fits = false;
                    break;
                }
            }
            if(fits)
            {
                for(z = 0; placeTo && z < blocks.size(); ++z)
                {
                    w = (y + z / width) * width + (x + z % width);
                    grids[w] |= 1;
                }
                return true;
            }
        }
    }
    return false;
}

void PixelBlast::generateCandidates(bool randomOnly)
{
    int x, y, z, w;
    QList<std::uint8_t> _virtualGrid = grid;
    QList<std::uint8_t> _shapes(MaxShapes, 0);
    QList<int> _candidates(shapeCandidates.size(), 0);

    for(x = 0; x < MaxShapes; ++x)
        _shapes[x] = x;

    w = static_cast<int>(randomOnly);
    for(x = 0; x < shapeCandidates.size(); ++x)
    {
        switch(w)
        {
                // SELECTIVE
            case 0:
            {
                std::shuffle(std::begin(_shapes), std::end(_shapes), *QRandomGenerator::global());
                for(z = 0; z < MaxShapes; ++z)
                {
                    y = getShape(_shapes[z]);
                    if(canTrigger(createBlocks(y), _virtualGrid, true))
                    {
                        break;
                    }
                }
                break;
            }
                // RANDOM
            case 1:
            {
                do
                {
                    y = randomShapes();
                } while(std::any_of(std::begin(_candidates), std::end(_candidates), [y](const auto i) { return i == y; }));
            }
        }

        _candidates[x] = y;
        shapeCandidates[x] = std::make_shared<ShapeBlock>();
        assignBlocks(createBlocks(y), *shapeCandidates[x]);
    }
}

void PixelBlast::updateScene()
{
    int x, y, z, w, d, i;
    QPointF tmp;
    QRectF dest;

    mousePoint = mapFromGlobal(QCursor::pos());

    if(currentShape == nullptr && !std::any_of(std::cbegin(shapeCandidates), std::cend(shapeCandidates), [](const auto &t) { return t != nullptr; }))
    {
        ++round;
        generateCandidates(false);
    }

    if(destroyScaler == 0.0F)
    {
        destroyBlocks.clear();
    }

    if(currentShape)
    {
        // Reset old mask
        for(x = 0; x < currentShape->blocks.size(); ++x)
        {
            y = currentShape->blocks[x].idx;
            if(y != -1)
                grid[y] = grid[y] & 0x1;
            currentShape->blocks[x].idx = -1;
        }

        if(shapeCandidateIdx != -1 && mouseBtn == Qt::RightButton)
        {
            shapeCandidates[shapeCandidateIdx] = std::move(currentShape);
            shapeCandidateIdx = -1;
        }
    }

    if(currentShape)
    {
        d = 0;
        for(w = 0; w < currentShape->blocks.size(); ++w)
        {
            tmp.setX(mousePoint.x() - static_cast<float>(currentShape->columns * scaleFactor.width()) / 2);
            tmp.setY(mousePoint.y() - static_cast<float>(currentShape->rows * scaleFactor.height()) / 2);
            tmp = std::move(currentShape->blocks[w].adjustPoint(tmp, scaleFactor));
            x = cellSquare * (tmp.x() - boardRegion.x() + scaleFactor.width() / 2) / (boardRegion.width());
            y = cellSquare * (tmp.y() - boardRegion.y() + scaleFactor.height() / 2) / (boardRegion.height());
            z = y * cellSquare + x;
            if(x < 0 || y < 0 || x >= cellSquare || y >= cellSquare || (grid[z] & 0x3) != 0 || ((d >> z) & 0x1) == 1)
                break;
            // d |= 1 << w;
            d |= 1 << z;
            currentShape->blocks[w].idx = z;
        }
        // verification
        if(w == currentShape->blocks.size())
        {
            d = (mouseBtn == Qt::LeftButton) ? 1 : 2;
            for(x = 0; x < w; ++x)
            {
                y = currentShape->blocks[x].idx;
                grid[y] = d | currentShape->shapeColor << 0x2;
            }

            // Place complete.
            if(d == 1)
            {
                // Test destroy block-points, and optimization
                for(d = 0; d < currentShape->blocks.size(); ++d)
                {
                    x = 0;
                    y = 0;
                    w = currentShape->blocks[d].idx % cellSquare;
                    z = currentShape->blocks[d].idx / cellSquare;
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
                            destroyBlocks.append(std::make_pair(std::move(BlockObject(x, z, i)), grid[i] >> 2));
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
                            destroyBlocks.append(std::make_pair(std::move(BlockObject(w, y, i)), grid[i] >> 2));
                            grid[i] = 0x0;
                        }
                        destroyScaler = 1.0F;
                    }
                }
                currentShape = nullptr;

                for(x = 0, y = 0, z = 0; x < shapeCandidates.size(); ++x)
                {
                    shapeCandidates[x] && ++z && !canTrigger(shapeCandidates[x]->rawBlocks, grid, false) && ++y;
                }
                if(z == y && y > 0)
                {
                    // GAME OVER
                    QMessageBox::warning(this, "Game Lost", "Game over!");
                    stopGame();
                }
            }
        }
    }
    else if(!shapeCandidates.empty())
    {
        tmp.setX(0);
        tmp.setY(boardRegion.height() + heightOffsetCandidates);
        tmp = mousePoint - (boardRegion.topLeft() + tmp);
        dest.setSize((scaleFactor * (static_cast<float>(cellSquare) / shapeCandidates.size())));
        if(tmp.y() < 0 || tmp.y() > dest.height() || tmp.x() < 0 || tmp.x() > dest.width() * shapeCandidates.size())
            x = -1;
        else
            x = static_cast<int>(tmp.x() / qMax(1.0F, dest.width()));

        x = qBound<int>(-1, x, shapeCandidates.size());
        shapeCandidateIdx = -1;
        if(!(x < 0 || x == shapeCandidates.size()))
        {
            shapeCandidateIdx = x;
            if(mouseBtn == Qt::LeftButton && shapeCandidates[x])
            {
                currentShape = std::move(shapeCandidates[x]);
            }
        }
    }

    update();
    frames++;
    frameIndex += frames % 5 == 0;
    destroyScaler = qBound(0.0F, destroyScaler - 0.06F, 1.0F);
    mouseBtn = 0x0;
}

void PixelBlast::paintEvent(QPaintEvent *event)
{
    int x, y, z, w;
    QRectF dest;
    QPointF destPoint;
    QPainter p(this);
    QPixmap *pixmap; //, tmpPixmap;

    QWidget::paintEvent(event);

    // Draw game logo
    dest.setSize(gameLogo->size().toSizeF());
    dest.moveTopLeft(boardRegion.topLeft() - QPointF(0, dest.height() - 100));


    dest.setWidth(map<float, float>(dest.width(), 0, gameLogo->size().width(), 0, boardRegion.width()));
    //dest.setHeight(map<float, float>(dest.height(), 0, gameLogo->size().height(), 0, 100));
    p.drawPixmap(dest, *gameLogo, {});

    // Draw grid & cells (central)
    dest = boardRegion;
    p.drawPixmap(dest + QMarginsF(100, 100, 100, 100), *gridBackgroundBorder, {});
    p.drawPixmap(dest, *gridBackgroundBg, {});
    p.drawPixmap(dest, *gridBackground, {});

    dest += QMarginsF(5, 5, 5, 5);
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
            p.setOpacity(1.0D);
            pixmap = &(*gridCellBright);
        }
        else
        {
            p.setOpacity(0.4D);
            pixmap = &(*gridCell);
        }
        p.drawPixmap(dest, *pixmap, {});

        if(w == 1)
        {
            p.setOpacity(1.0D);
            pixmap = getColoredPixmap(grid[z] >> 2, 0);
            p.drawPixmap(dest, *pixmap, {});
        }
    }

    p.setOpacity(1.0D);

    // Draw blocks
    if(currentShape)
    {
        dest.setSize(scaleFactor * destroyScaler);
        destPoint = {static_cast<float>(currentShape->columns * dest.width()) / 2, static_cast<float>(currentShape->rows * dest.height()) / 2};
        for(x = 0; x < destroyBlocks.size(); ++x)
        {
            const auto &db = destroyBlocks[x];
            dest.moveTopLeft(db.first.adjustPoint(destPoint, dest.size()));
            pixmap = getColoredPixmap(db.second, frameIndex);
            p.drawPixmap(dest, *pixmap, {});
        }
    }

    // Draw bottom INVENTORY
    dest.moveTopLeft(boardRegion.topLeft() + QPointF(0, boardRegion.height() + heightOffsetCandidates));
    dest.setSize(scaleFactor * (static_cast<float>(cellSquare) / shapeCandidates.size()));
    if(shapeCandidateIdx != -1 && shapeCandidates[shapeCandidateIdx])
    {
        // tmpPixmap = std::move(adjustBright(*getColoredPixmap((*shapeCandidates[shapeCandidateIdx]).shapeColor, frameIndex), 60));
        pixmap = getColoredPixmap((*shapeCandidates[shapeCandidateIdx]).shapeColor, frameIndex);
    }

    for(z = 0; z < shapeCandidates.size(); ++z)
    {
        p.drawPixmap(dest, *gridCell, {});
        if(shapeCandidates[z])
        {
            destPoint.setX(scaleFactor.width() * 0.6F * shapeCandidates[z]->columns);
            destPoint.setY(scaleFactor.height() * 0.6F * shapeCandidates[z]->rows);
            destPoint = dest.topLeft() + QPointF((dest.width() - destPoint.x()) / 2, (dest.height() - destPoint.y()) / 2);
            drawShapeAt(*shapeCandidates[z], destPoint, scaleFactor * 0.6F, 0, p, ((z == shapeCandidateIdx) ? pixmap : nullptr));
        }
        dest.moveLeft(dest.x() + dest.width());
    }

    // Draw blocks by select pointer
    if(currentShape)
    {
        p.setOpacity(0.8D);
        dest.setSize(scaleFactor * 0.9F);
        destPoint = {mousePoint.x() - static_cast<float>(currentShape->columns * dest.width()) / 2, mousePoint.y() - static_cast<float>(currentShape->rows * dest.height()) / 2};
        drawShapeAt(*currentShape, destPoint, dest.size(), frameIndex, p);
    }

    p.drawText(QPoint{10,200}, QString("Score: ") +  QString::number(scores));
}

QPointF BlockObject::adjustPoint(const QPointF &adjust, const QSizeF &scale) const
{
    QPointF out;
    out.setX((adjust.x() + x * scale.width()));
    out.setY((adjust.y() + y * scale.height()));
    return out;
}
