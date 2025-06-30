#include <QDebug>
#include <QApplication>
#include <QMessageBox>

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
    QAction * restoreWindow = new QAction("Restore", this);
    QObject::connect(restoreWindow, &QAction::triggered, [=](){ if(mainWindow->isHidden()) mainWindow->show(); else mainWindow->hide(); });

    QAction  * quitAction = new QAction("Exit", this);
    QObject::connect(quitAction, &QAction::triggered,  qApp, &QApplication::quit);

    QAction * setThemeToDark = new QAction("Dark", this);
    setThemeToDark->setCheckable(true);
    QObject::connect(setThemeToDark, &QAction::triggered,  [=](){ mainWindow->setTheme(ThemeScheme::Dark);});

    QAction * setThemeToLight = new QAction("Light", this);
    setThemeToLight->setCheckable(true);
    QObject::connect(setThemeToLight, &QAction::triggered,  [=](){ mainWindow->setTheme(ThemeScheme::Light);});

    QObject::connect(menu, &QMenu::aboutToShow, [=](){
        ThemeScheme scheme = mainWindow->getTheme();
        setThemeToDark->setChecked(scheme == ThemeScheme::Dark);
        setThemeToLight->setChecked(scheme == ThemeScheme::Light);

        restoreWindow->setText((mainWindow->isHidden() ? "Restore" : "Hide"));
    });

    QObject::connect(this, &AdsAppSystemTray::activated, restoreWindow, &QAction::triggered);

    menu->addAction(restoreWindow);
    menu->addSeparator();
    menu->addAction(setThemeToDark);
    menu->addAction(setThemeToLight);
    menu->addSeparator();
    menu->addAction(quitAction);

    setIcon(mainWindow->windowIcon());

    setContextMenu(menu);
    show();
}
