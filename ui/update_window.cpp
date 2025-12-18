#include "ui_update_window.h"
#include "update_window.h"

UpdateWindow::UpdateWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::update_window)
{
    ui->setupUi(this);
}

UpdateWindow::~UpdateWindow()
{
    delete ui;
}

void UpdateWindow::setText(const QString &value)
{
    ui->label->setText(value);
}

void UpdateWindow::setProgress(int v1, int v2)
{
    ui->progressBarCurrent->setValue(v1);
    ui->progressBarTotal->setValue(v2);
}

void UpdateWindow::delayPush(int ms, std::function<void()> call, bool loop)
{
    QTimer *qtimer = new QTimer(this);
    qtimer->setSingleShot(!loop);
    qtimer->setInterval(ms);
    connect(
        qtimer,
        &QTimer::timeout,
        [qtimer, call]()
        {
            call();
            if(qtimer->isSingleShot())
            {
                qtimer->stop();
                qtimer->deleteLater();
            }
        });
    qtimer->start();
}
