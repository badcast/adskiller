#pragma once

#include <functional>
#include <memory>

#include <QByteArray>
#include <QSettings>
#include <QString>
#include <QVariant>

#include "Begin.h"

enum DeviceConnectType
{
    None = 0,
    ADB = 1,
    Apple = 2
};

namespace Generic
{
    QString beautifyNumber(std::uint64_t number, const QString &symbol = " ");

    std::pair<int, QString> formatDesignSize(std::uint64_t bytes);

    QString formatSizes(std::uint64_t bytes);

    QPixmap generateFallbackIcon(const QString &appName, const QString &pkgName, bool isSystem);

} // namespace Generic

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
    static void sync();
    static void load();

    static std::shared_ptr<QSettings> raw();

    static bool contains(const QString &key);
    static QVariant value(const QString &key, const QVariant &defaultValue = QVariant());
    static void setValue(const QString &key, const QVariant &value);

    static bool readValue(const QString &key, QVariant *value);
    static void writeValue(const QString &key, const QVariant &value);

    static void removeKey(const QString &key);
    static void removeEncToken();

    static std::tuple<QString, QString> loginAndPass(bool *contains = nullptr, const QVariant &login = {}, const QVariant &pass = {});
    static bool autoLogin(bool *contains = nullptr, const QVariant &set = {});
    static int themeIndex(bool *contains = nullptr, const QVariant &set = {});
    static int networkTimeout(bool *contains = nullptr, const QVariant &set = {});
    static bool analyticsEnabled(bool *contains = nullptr, const QVariant &set = {});
    static int pixelBlastHighScore(bool *contains = nullptr, const QVariant &set = {});
};
