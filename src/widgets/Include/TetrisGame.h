#pragma once

#include <QList>
#include <QPixmap>
#include <QTimer>
#include <QWidget>

class Tetris : public QWidget
{
private:
    QList<int> matrix;

public:
    Tetris(QWidget *parent = nullptr);

    void startGame();
    void stopGame();
};
