#pragma once

#include <QWidget>
#include <QMenu>
#include <QSystemTrayIcon>

class AdsAppSystemTray : public QSystemTrayIcon
{
    Q_OBJECT

public:
    AdsAppSystemTray(QWidget* parent = nullptr);
};
