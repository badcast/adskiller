#include <QApplication>
#include <QBuffer>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QSharedMemory>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QSplashScreen>
#include <QTimer>

#include "begin.h"
#include "mainwindow.h"
#include "network.h"

constexpr auto ShowCommandPipe = "adskiller_window_show";
constexpr auto HideCommandPipe = "adskiller_window_hide";

bool checkout();

int main(int argc, char **argv)
{
    int exitCode;
    QApplication app(argc, argv);
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

    QQmlApplicationEngine engine;
    QQuickStyle::setStyle("Material");

    MainWindow *w = new MainWindow();
    w->app = &app;

    qmlRegisterSingletonInstance("Adskiller", 1, 0, "AppController", w);

    QPixmap pixmap(":/resources/banner");
    QSplashScreen *splash = new QSplashScreen(pixmap);
    splash->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::SplashScreen);
    splash->setAttribute(Qt::WA_TranslucentBackground);
    splash->show();

    const QUrl url("qrc:/Adskiller/qml/main.qml");
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &app,
        [url](QObject *obj, const QUrl &objUrl)
        {
            if(!obj && url == objUrl)
                QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);

    QTimer::singleShot(
        3000,
        [&engine, url, splash]()
        {
            splash->close();
            splash->deleteLater();
            engine.load(url);
        });

    w->delayUICallLoop(
        100,
        [&sharedMem, &w]() -> bool
        {
            if(!sharedMem.lock())
                return true;
            QString cmd = QLatin1String(reinterpret_cast<const char *>(sharedMem.constData()));
            if(cmd == ShowCommandPipe)
            {
                // To do: Show window
            }
            if(cmd == HideCommandPipe)
            {
                // To do: Hide window
            }
            memset(sharedMem.data(), 0, 1);
            sharedMem.unlock();
            return true;
        });

    exitCode = app.exec();
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
