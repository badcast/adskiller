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
    int N;
    QList<QPixmap> resources;
};

std::shared_ptr<QList<BlockResource>> globalResources {};

PixelBlast::PixelBlast(QWidget *parent) : QWidget(parent), updateTimer(this), cellScale(1.0F, 1.0F), boardRegion(0, 0, 500, 500), scores(0), frames(0)
{
    resize(boardRegion.size().scaled(boardRegion.width() + 100, boardRegion.height() + 100, Qt::AspectRatioMode::IgnoreAspectRatio).toSize());

    cellSquare = MaxCellWidth;
    matrix.assign(MaxCellWidth * MaxCellWidth, 0);
    assignBlocks(createBlocks(0x020302));

    setMouseTracking(true);
    updateTimer.setSingleShot(false);
    updateTimer.setInterval(1000.F / 60); // 60  FPS per sec
    QObject::connect(&updateTimer, &QTimer::timeout, this, &PixelBlast::updateScene);
    updateTimer.start();

    // Load and initialize globalResources
    if(globalResources != nullptr)
        return;

    constexpr auto _formatResourceName = ":/pixelblastgame/Blocks/%s";
    constexpr auto _formatBlocks = "%s %d %s";
    constexpr auto MaxBufLen = 128;
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

    globalResources = std::make_shared<QList<BlockResource>>();
    QTextStream stream(&file);
    while(stream.readLineInto(&content))
    {
        if(sscanf((content).toLocal8Bit().data(), _formatBlocks, buff, &bRes.N, buff0) != 3)
        {
            globalResources.reset();
            return;
        }
        bRes.name = buff;
        for(int i = 0; i < bRes.N; ++i)
        {
            snprintf(buff, MaxBufLen, _formatResourceName, buff0);
            content = buff;
            content.replace(QChar('#'), QString::number(i + 1));
            QPixmap qp(content);
            bRes.resources.append(std::move(qp));
        }
        globalResources->append(std::move(bRes));
    }
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

    plants.rows = 0;
    plants.columns = 0;
    plants.blockPoints.clear();
    plants.shadowBlockPoints.clear();
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
}

void PixelBlast::updateScene()
{
    int x, y, z, w, d, i;

    mousePoint = mapFromGlobal(QCursor::pos());

    if(!currentBlocks.empty())
    {
        assignBlocks(currentBlocks);
        currentBlocks.clear();
    }

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

        if(boardRegion.width() > 0 && boardRegion.height() > 0 /* && boardRegion.contains(test)*/)
        {
            d = 0;
            for(w = 0; w < plants.shadowBlockPoints.size(); ++w)
            {
                x = qBound<int>(0, cellSquare * (plants.shadowBlockPoints[w].x - boardRegion.x() + scaleFactor.width() / 2) / (boardRegion.width()), cellSquare - 1);
                y = qBound<int>(0, cellSquare * (plants.shadowBlockPoints[w].y - boardRegion.y() + scaleFactor.height() / 2) / (boardRegion.height()), cellSquare - 1);
                z = y * cellSquare + x;
                if(matrix[z] != 0 || ((d >> z) & 0x1) == 1)
                    break;
                // d |= 1 << w;
                d |= 1 << z;
                plants.shadowBlockPoints[w].idx = z;
            }
            // verification
            // if(d == (1 << plants.shadowBlockPoints.size()) - 1)
            if(w == plants.shadowBlockPoints.size())
            {
                d = mouseBtn == Qt::LeftButton ? 1 : 2;
                for(x = 0; x < w; ++x)
                {
                    matrix[plants.shadowBlockPoints[x].idx] = d;
                }

                if(d == 1)
                {
                    // Test destroy block-points, and optimization
                    for(d = 0; d < plants.shadowBlockPoints.size(); ++d)
                    {
                        x = 0;
                        y = 0;
                        w = plants.shadowBlockPoints[d].idx % cellSquare;
                        z = plants.shadowBlockPoints[d].idx / cellSquare;
                        for(i = 0; i < cellSquare; ++i)
                        {
                            if(matrix[z * cellSquare + i] == 1)
                                x++;

                            if(matrix[i * cellSquare + w] == 1)
                                y++;
                        }
                        if(x == cellSquare)
                        {
                            scores += cellSquare;
                            for(x = 0; x < cellSquare; ++x)
                            {
                                matrix[z * cellSquare + x] = 0;
                            }
                        }
                        if(y == cellSquare)
                        {
                            scores += cellSquare;
                            for(y = 0; y < cellSquare; ++y)
                            {
                                matrix[y * cellSquare + w] = 0;
                            }
                        }
                    }

                    currentBlocks = createBlocks(randomShapes());
                }
            }
        }
    }

    update();

    frames++;
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
                p.drawPixmap(dest, (*globalResources)[matrix[z]].resources[frames % (*globalResources)[plants.shadowBlockPoints[x].idx].N], {});
            }
            else if(matrix[z] == 2)
            {
                p.fillRect(dest, QGradient::BigMango);
            }
            else
            {
                p.drawRect(dest);
            }
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
        z = plants.shadowBlockPoints[x].idx;
        if(z > -1)
        {
            BlockResource *t = &(globalResources->operator[](matrix[z]));
            p.drawPixmap(dest, t->resources[frames % t->N], QRectF());
        }
    }
}
