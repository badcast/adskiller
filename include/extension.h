#pragma once

#include <memory>
#include <functional>

#include <QByteArray>
#include <QString>
#include <QVariant>
#include <QSettings>

#include "begin.h"
#include "adbfront.h"

enum DeviceConnectType
{
    None = 0,
    ADB = 1
};

enum MalwareStatus
{
    Idle = 0,
    Running,
    Error
};

class Service;
class AppSetting;

class CipherAlgoCrypto
{
private:
    static QByteArray ConvBytes(const QByteArray &bytes, const QByteArray &key);

public:
    static QString PackDC(const QByteArray &dataInit, const QByteArray &key);
    static QByteArray UnpackDC(const QString &packed);
    static QByteArray RandomKey();
};

class AppSetting
{
private:
    static std::shared_ptr<QSettings> settings;

public:
    static void save();
    static void load();

    static bool readValue(const QString &key, QVariant * value);
    static void writeValue(const QString &key, const QVariant& value);

    static QString encryptedToken(bool * contains = nullptr, const QVariant &set = {});
    static bool autoLogin(bool * contains = nullptr, const QVariant &set = {});
    static int themeIndex(bool * contains = nullptr, const QVariant &set = {});
};
