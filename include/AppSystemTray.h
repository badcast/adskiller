#pragma once

#include <QMenu>
#include <QSystemTrayIcon>

class MainWindow;

class AdsAppSystemTray : public QSystemTrayIcon
{
    Q_OBJECT

public:
    AdsAppSystemTray(MainWindow *parent = nullptr);
};
