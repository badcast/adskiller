#include "update_window.h"
#include <QVBoxLayout>

UpdateWindow::UpdateWindow(QWidget *parent) : QMainWindow(parent)
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);
    label = new QLabel(this);
    progressBarCurrent = new QProgressBar(this);
    progressBarTotal = new QProgressBar(this);
    layout->addWidget(label);
    layout->addWidget(progressBarCurrent);
    layout->addWidget(progressBarTotal);
}

UpdateWindow::~UpdateWindow()
{
}

void UpdateWindow::setText(const QString &value)
{
    label->setText(value);
}

void UpdateWindow::setProgress(int v1, int v2)
{
    progressBarCurrent->setValue(v1);
    progressBarTotal->setValue(v2);
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
