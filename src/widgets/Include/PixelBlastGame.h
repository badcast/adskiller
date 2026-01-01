#pragma once

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
        int rows;
        int columns;
        QList<QPoint> blockPoints;
        QList<BlockObject> shadowBlockPoints;
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
    void assignBlocks(QList<int> blocks);
    QList<int> createBlocks(int blocks);

    int cellSquare;
    int mouseBtn;
    QPoint mousePoint;
    QSizeF cellScale;
    QSizeF cellSize;
    QSizeF scaleFactor;
    QRectF boardRegion;
    QTimer updateTimer;
    QList<std::uint8_t> matrix;
    QPixmap test;

    BlockOrigin plants;
};
