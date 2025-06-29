#include <QDebug>
#include <QApplication>

#include "SystemTray.h"
#include "mainwindow.h"

AdsAppSystemTray::AdsAppSystemTray(QWidget *parent) : QSystemTrayIcon(parent)
{
    QMenu * menu = new QMenu();
    MainWindow * mainWindow = qobject_cast<MainWindow*>(parent);
    if(mainWindow == nullptr)
    {
        qDebug() << "Require parent as MainWindow instance";
        delete menu;
        return;
    }
    QAction  * quitAction = new QAction("Exit", this);
    QObject::connect(quitAction, &QAction::triggered,  qApp, &QApplication::quit);

    QAction * setThemeToDark = new QAction("Dark", this);
    QObject::connect(setThemeToDark, &QAction::triggered,  [mainWindow](){ mainWindow->setTheme(ThemeScheme::Dark);});

    QAction * setThemeToLight = new QAction("Light", this);
    QObject::connect(setThemeToLight, &QAction::triggered,  [mainWindow](){ mainWindow->setTheme(ThemeScheme::Light);});

    menu->addAction(setThemeToDark);
    menu->addAction(setThemeToLight);
    menu->addSeparator();
    menu->addAction(quitAction);

    setIcon(mainWindow->windowIcon());

    setContextMenu(menu);
    show();
}
