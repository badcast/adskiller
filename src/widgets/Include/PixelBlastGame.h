#pragma once

#include <QList>
#include <QPixmap>
#include <QTimer>
#include <QWidget>

class PixelBlast : public QWidget
{
    struct BlockOrigin
    {
        int totalH;
        int totalV;
        QSize blockSize;
        QList<QPoint> blocks;
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

    int cellCount;
    int mouseBtn;
    QPoint mousePoint;
    QSizeF cellSize;
    QRect _drawRegion;
    QTimer updateTimer;
    QList<int> matrix;
    BlockOrigin curBlock;
};
