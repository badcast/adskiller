#pragma once
#include <utility>

#include <QList>
#include <QPixmap>
#include <QTimer>
#include <QWidget>

class PixelBlast : public QWidget
{
    struct BlockObject
    {
        int x;
        int y;
        int idx;

        BlockObject(int x, int y, int idx = -1) : x(x), y(y), idx(idx) {}

        inline QPointF adjustPoint(const QPointF &adjust, const QSizeF&scale) const;
    };
    struct BlockOrigin
    {
        int shapeColor;
        int rows;
        int columns;
        QList<BlockObject> blocks;
    };

public:
    PixelBlast(QWidget *parent = nullptr);

    void startGame();
    void stopGame();

private slots:
    void updateScene();

private:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

    void updateData();
    void assignBlocks(const QList<std::uint8_t> &blocks);
    QPixmap* getColoredPixmap(int color);
    QList<std::uint8_t> createBlocks(int blocks);

    int cellSquare;
    int mouseBtn;
    int scores;
    int frames;
    int frameIndex;

    QPoint mousePoint;
    QSizeF cellScale;
    QSizeF cellSize;
    QSizeF scaleFactor;
    QRectF boardRegion;
    QTimer updateTimer;
    QList<std::uint8_t> grid;
    QList<std::uint8_t> currentBlocks;

    float destroyScaler;
    QList<std::pair<BlockObject,int>> destroyBlocks;

    BlockOrigin shape;
};
