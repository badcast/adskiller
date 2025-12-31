#pragma once

#include <functional>
#include <memory>

#include <QByteArray>
#include <QSettings>
#include <QString>
#include <QVariant>

#include "adbfront.h"
#include "begin.h"

enum DeviceConnectType
{
    None = 0,
    ADB = 1
};

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

    static bool readValue(const QString &key, QVariant *value);
    static void writeValue(const QString &key, const QVariant &value);

    static QString encryptedToken(bool *contains = nullptr, const QVariant &set = {});
    static bool autoLogin(bool *contains = nullptr, const QVariant &set = {});
    static int themeIndex(bool *contains = nullptr, const QVariant &set = {});
    static int networkTimeout(bool *contains = nullptr, const QVariant &set = {});
};
