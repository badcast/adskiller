#include <QApplication>
#include <QBuffer>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QSharedMemory>
#include <QWidget>

#include "begin.h"
#include "mainwindow.h"
#include "network.h"
#include "PixelBlastGame.h"

constexpr auto ShowCommandPipe = "adskiller_window_show";
constexpr auto HideCommandPipe = "adskiller_window_hide";

bool checkout();

QWidget *createBanner();

int main(int argc, char **argv)
{

    int exitCode;
    QApplication app(argc, argv);
    PixelBlast t;
    t.show();
    return app.exec();
    QSharedMemory sharedMemUpdate("imister.kz-app_adskiller_v1_update");
    if(sharedMemUpdate.attach() || !checkout())
    {
        return EXIT_FAILURE;
    }
    QSharedMemory sharedMem("imister.kz-app_adskiller_v1");
    if(sharedMem.attach())
    {
        sharedMem.lock();
        int len = qMin<int>(strlen(ShowCommandPipe), sharedMem.size());
        memcpy(sharedMem.data(), ShowCommandPipe, len);
        sharedMem.unlock();
        sharedMem.detach();
        return EXIT_SUCCESS;
    }
    if(!sharedMem.create(128))
    {
        return EXIT_FAILURE;
    }

    MainWindow *w = nullptr;

    QWidget *banner = createBanner();
    banner->show();
    QTimer::singleShot(
        3000,
        [&]()
        {
            banner->close();
            delete banner;
            banner = nullptr;
            w = new MainWindow;
            w->current = w;
            w->app = &app;
            w->delayPushLoop(
                100,
                [&sharedMem, &w]() -> bool
                {
                    if(!sharedMem.lock())
                        return true;
                    QString cmd = QLatin1String(reinterpret_cast<const char *>(sharedMem.constData()));
                    if(cmd == ShowCommandPipe)
                    {
                        w->showNormal();
                    }
                    if(cmd == HideCommandPipe)
                    {
                        w->hide();
                    }
                    memset(sharedMem.data(), 0, 1);
                    sharedMem.unlock();
                    return true;
                });
            w->show();
        });
    exitCode = app.exec();
    if(w != nullptr)
        delete w;
    sharedMem.detach();
    return exitCode;
}

bool checkout()
{
    QDir qdir;
    QString adbfile = AdbExecutableFilename();
    if(!qdir.exists(adbfile))
    {
        qDebug() << "Adb not found";
        return false;
    }
    return true;
}

QWidget *createBanner()
{
    QWidget *banner = new QWidget;
    banner->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    banner->setAttribute(Qt::WA_TranslucentBackground);
    banner->setCursor(Qt::WaitCursor);
    banner->resize(500, 478);
    banner->setStyleSheet("background: transparent;");

    QPixmap pixmap(":/resources/banner");
    QLabel *lab = new QLabel(banner);
    lab->setAttribute(Qt::WA_TranslucentBackground);
    lab->setStyleSheet("background: transparent; border: none;");
    lab->setPixmap(pixmap);

    QHBoxLayout *layout = new QHBoxLayout(banner);
    layout->addWidget(lab);
    layout->setContentsMargins(0, 0, 0, 0);
    return banner;
}
