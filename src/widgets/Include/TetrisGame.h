#pragma once

#include <QList>
#include <QPixmap>
#include <QTimer>
#include <QWidget>

class Tetris : public QWidget
{
public:
    Tetris(QWidget *parent = nullptr);

    void startGame();
    void stopGame();

private slots:
    void updateScene();

private:
    void paintEvent(QPaintEvent *event) override;

    QSizeF cellSize;
    QSize blockSize;
    QTimer updateTimer;
    QList<int> matrix;
};
