#pragma once

#include <QList>
#include <QWidget>
#include <QTimer>
#include <QPixmap>

class Tetris : public QWidget
{
private:
    QList<int> matrix;

public:
    Tetris(QWidget *parent = nullptr);

    void startGame();
    void stopGame();
};
