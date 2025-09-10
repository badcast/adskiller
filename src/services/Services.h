#pragma once

#include <chrono>

#include <QTimer>
#include <QHash>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>
#include <QStringListModel>
#include <QCryptographicHash>
#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <QEventLoop>
#include <QLabel>
#include <QListView>
#include <QProgressBar>
#include <QPushButton>

#include "begin.h"
#include "extension.h"
#include "network.h"
#include "adbfront.h"
#include "mainwindow.h"

class Service;
class AdsKillerService;
class StorageCacheCleanService;

class Service : public QObject
{
    Q_OBJECT

protected:
    DeviceConnectType mDeviceType;
    AdbDevice mAdbDevice;

public:
    inline Service(DeviceConnectType deviceType, QObject * parent = nullptr) : QObject(parent), mDeviceType(deviceType){}

    virtual void setDevice(const AdbDevice& adbDevice);

    virtual bool canStart();
    virtual bool isStarted() = 0;
    virtual bool isFinish()= 0;
    virtual bool start()= 0;
    virtual void stop()= 0;
    virtual void reset() = 0;

    DeviceConnectType deviceType() const;
};

class AdsKillerService : public Service
{
private:
    QListView * processLogStatus;
    QLabel * malwareStatusText0;
    QLabel * deviceLabelName;
    QProgressBar * processBarStatus;
    QPushButton * pushButtonReRun;

    void cirlceMalwareState(bool success);
    void cirlceMalwareStateReset();

public:
    AdsKillerService(QObject *parent = nullptr);

    void setDevice(const AdbDevice& adbDevice) override;

    bool canStart() override;
    bool isStarted() override;
    bool isFinish() override;
    bool start() override;
    void stop() override;
    void reset() override;
};

class StorageCacheCleanService : public Service
{
public:
    StorageCacheCleanService(QObject *parent = nullptr);

    void setDevice(const AdbDevice& adbDevice) override;

    bool canStart() override;
    bool isStarted() override;
    bool isFinish() override;
    bool start() override;
    void stop() override;
    void reset() override;
};
