#include "update_window.h"
#include "ui_update_window.h"


update_window::update_window(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::update_window)
{
    ui->setupUi(this);
}

update_window::~update_window()
{
    delete ui;
}

void update_window::setText(const QString &value)
{
    ui->label->setText(value);
}

void update_window::setProgress(int v1, int v2)
{
    ui->progressBarCurrent->setValue(v1);
    ui->progressBarTotal->setValue(v2);
}

void update_window::delayPush(int ms, std::function<void()> call, bool loop)
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
            qtimer->stop();
            qtimer->deleteLater();
        });
    qtimer->start();
}
