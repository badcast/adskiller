#pragma once

#include <QMenu>
#include <QSystemTrayIcon>
#include <QWidget>

class AdsAppSystemTray : public QSystemTrayIcon
{
    Q_OBJECT

public:
    AdsAppSystemTray(QWidget *parent = nullptr);
};
