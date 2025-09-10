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
