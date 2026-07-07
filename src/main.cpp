#include <QApplication>
#include <QBuffer>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QLibraryInfo>
#include <QMessageBox>
#include <QSharedMemory>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
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
    // Force single-threaded render loop and OpenGL backend for Wine compatibility.
    // The default threaded D3D11 pipeline can break input event delivery under Wine.
    if(!qEnvironmentVariableIsSet("QSG_RENDER_LOOP"))
        qputenv("QSG_RENDER_LOOP", "basic");
    if(!qEnvironmentVariableIsSet("QSG_RHI_BACKEND"))
        qputenv("QSG_RHI_BACKEND", "opengl");

    int exitCode;
    QApplication app(argc, argv);
    QSharedMemory sharedMemUpdate("imister.kz-app_adskiller_v1_update");
    if(sharedMemUpdate.attach())
    {
        // Update process is running, exit
        sharedMemUpdate.detach();
        return EXIT_FAILURE;
    }
    if(!checkout())
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

    // Explicitly add Qt's QML imports path so the engine can find
    // QtQuick.Controls and other standard modules at runtime
    QString qtQmlPath = QLibraryInfo::path(QLibraryInfo::QmlImportsPath);
    engine.addImportPath(qtQmlPath);
    engine.addImportPath(QCoreApplication::applicationDirPath() + "/qml");

    // Diagnostic: print where the QML engine is searching for modules
    qDebug() << "Qt prefix:" << QLibraryInfo::path(QLibraryInfo::PrefixPath);
    qDebug() << "Qt QML imports path:" << qtQmlPath;
    qDebug() << "QML engine import paths:" << engine.importPathList();

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

    // Collect QML engine warnings for error reporting
    QStringList qmlWarnings;
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, [&qmlWarnings](const QList<QQmlError> &warnings) {
        for(const auto &w : warnings)
            qmlWarnings << w.toString();
    });

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &app,
        [url, &qmlWarnings](QObject *obj, const QUrl &objUrl)
        {
            if(!obj && url == objUrl)
            {
                QString errorDetail = qmlWarnings.isEmpty()
                    ? "Неизвестная ошибка загрузки QML."
                    : qmlWarnings.join("\n");
                qCritical() << "QML load failed:" << errorDetail;
                QMessageBox::critical(
                    nullptr,
                    "Ошибка загрузки интерфейса",
                    QString("Не удалось загрузить QML интерфейс:\n\n%1").arg(errorDetail));
                QCoreApplication::exit(-1);
            }
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
    QString adbfile = AdbExecutableFilename();
    if(!QFile::exists(adbfile))
    {
        qDebug() << "Adb not found:" << adbfile;
        QMessageBox::critical(
            nullptr,
            "Ошибка запуска",
            QString(
                "Не найден ADB файл:\n%1\n\nУбедитесь, что файл ADB "
                "находится рядом с исполняемым файлом приложения.")
                .arg(adbfile));
        return false;
    }
    return true;
}
