#pragma once

#include <memory>
#include <functional>

#include <QByteArray>
#include <QString>
#include <QVariant>

#include "begin.h"
#include "adbfront.h"

enum DeviceConnectType
{
    ADB
};

enum MalwareStatus
{
    Idle = 0,
    Running,
    Error
};

class Service;

class CipherAlgoCrypto
{
private:
    static QByteArray ConvBytes(const QByteArray &bytes, const QByteArray &key);

public:
    static QString PackDC(const QByteArray &dataInit, const QByteArray &key);
    static QByteArray UnpackDC(const QString &packed);
    static QByteArray RandomKey();
};

class Service : public QObject
{
    Q_OBJECT

protected:
    DeviceConnectType mDeviceType;
    AdbDevice mAdbDevice;

public:
    Service(DeviceConnectType deviceType, QObject * parent = nullptr) : QObject(parent), mDeviceType(deviceType){}

    virtual void setDevice(const AdbDevice& adbDevice);

    virtual bool canStart();
    virtual bool isStarted() = 0;
    virtual bool isFinish()= 0;
    virtual bool start()= 0;
    virtual void stop()= 0;
    virtual void reset() = 0;

    DeviceConnectType deviceType() const;
};

