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
    };
    struct BlockOrigin
    {
        int shapeColor;
        int rows;
        int columns;
        QList<QPoint> blockPoints;
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
    int getBlockColor(int idx);
    void setBlockColor(int idx, int val);
    void assignBlocks(const QList<int> &blocks);
    QList<int> createBlocks(int blocks);

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
    QList<int> currentBlocks;

    BlockOrigin shape;
};
