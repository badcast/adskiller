#include <QApplication>
#include <QBuffer>
#include <QColor>
#include <QCursor>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPalette>
#include <QPixmap>
#include <QScreen>
#include <QSharedMemory>
#include <QStyleFactory>
#include <QWidget>

#include "begin.h"
#include "mainwindow.h"
#include "network.h"

constexpr auto ShowCommandPipe = "adskiller_window_show";
constexpr auto HideCommandPipe = "adskiller_window_hide";

bool checkout();

QWidget *createBanner();

int main(int argc, char **argv)
{
    int exitCode;
    QApplication app(argc, argv);

    // Force Fusion style across all platforms (especially Windows) to avoid native light theme bleed
    app.setStyle(QStyleFactory::create("Fusion"));

    // Global Dark Palette for any Qt controls / dialogs without explicit CSS
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(10, 14, 26));            // #0A0E1A
    darkPalette.setColor(QPalette::WindowText, QColor(248, 250, 252));    // #F8FAFC
    darkPalette.setColor(QPalette::Base, QColor(7, 10, 18));              // #070A12
    darkPalette.setColor(QPalette::AlternateBase, QColor(15, 23, 42));    // #0F172A
    darkPalette.setColor(QPalette::ToolTipBase, QColor(15, 23, 42));      // #0F172A
    darkPalette.setColor(QPalette::ToolTipText, QColor(248, 250, 252));   // #F8FAFC
    darkPalette.setColor(QPalette::Text, QColor(248, 250, 252));          // #F8FAFC
    darkPalette.setColor(QPalette::Button, QColor(15, 23, 42));           // #0F172A
    darkPalette.setColor(QPalette::ButtonText, QColor(248, 250, 252));    // #F8FAFC
    darkPalette.setColor(QPalette::BrightText, QColor(56, 189, 248));     // #38BDF8
    darkPalette.setColor(QPalette::Link, QColor(56, 189, 248));           // #38BDF8
    darkPalette.setColor(QPalette::Highlight, QColor(2, 132, 199));       // #0284C7
    darkPalette.setColor(QPalette::HighlightedText, QColor(255, 255, 255)); // #FFFFFF
    darkPalette.setColor(QPalette::PlaceholderText, QColor(100, 116, 139)); // #64748B

    // Disabled states
    darkPalette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(71, 85, 105)); // #475569
    darkPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(71, 85, 105));
    darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(71, 85, 105));
    darkPalette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(30, 41, 59));   // #1E293B
    darkPalette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(71, 85, 105));

    app.setPalette(darkPalette);
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
    sharedMem.lock();
    memset(sharedMem.data(), 0, sharedMem.size());
    sharedMem.unlock();

    MainWindow *w = nullptr;

    QWidget *banner = createBanner();
    banner->show();
    w = new MainWindow;
    w->current = w;
    w->app = &app;
    w->delayUICallLoop(
        100,
        [&sharedMem, &w]() -> bool
        {
            if(!sharedMem.lock())
                return true;

            const char *data = reinterpret_cast<const char *>(sharedMem.constData());
            int len = 0;
            while(len < sharedMem.size() && data[len] != '\0')
            {
                len++;
            }
            QString cmd = QString::fromLatin1(data, len);

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

    QTimer::singleShot(
        1500,
        [&]()
        {
            if(w)
            {
                QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
                if(!screen)
                    screen = QGuiApplication::primaryScreen();
                if(screen)
                {
                    QRect screenGeo = screen->availableGeometry();
                    int x = qMax(screenGeo.x(), screenGeo.x() + (screenGeo.width() - w->width()) / 2);
                    int y = qMax(screenGeo.y(), screenGeo.y() + (screenGeo.height() - w->height()) / 2);
                    w->move(x, y);
                }
                w->show();
            }
            if(banner)
            {
                banner->close();
                delete banner;
                banner = nullptr;
            }
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
    banner->setStyleSheet("background: transparent;");

    QPixmap pixmap(":/resources/banner");
    QLabel *lab = new QLabel(banner);
    lab->setAttribute(Qt::WA_TranslucentBackground);
    lab->setStyleSheet("background: transparent; border: none;");

    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if(!screen)
    {
        screen = QGuiApplication::primaryScreen();
    }

    QRect screenGeo = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);

    if(!pixmap.isNull())
    {
        // Scale down proportionally if image height exceeds 80% of available screen height
        int maxH = static_cast<int>(screenGeo.height() * 0.80);
        if(pixmap.height() > maxH && maxH > 200)
        {
            pixmap = pixmap.scaledToHeight(maxH, Qt::SmoothTransformation);
        }
        lab->setPixmap(pixmap);
        banner->setFixedSize(pixmap.size());
    }
    else
    {
        banner->setFixedSize(500, 478);
    }

    QHBoxLayout *layout = new QHBoxLayout(banner);
    layout->addWidget(lab);
    layout->setContentsMargins(0, 0, 0, 0);

    // Center precisely on the active screen
    int x = qMax(screenGeo.x(), screenGeo.x() + (screenGeo.width() - banner->width()) / 2);
    int y = qMax(screenGeo.y(), screenGeo.y() + (screenGeo.height() - banner->height()) / 2);
    banner->move(x, y);

    return banner;
}
