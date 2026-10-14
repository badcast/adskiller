#include "Extension.h"

std::shared_ptr<QSettings> AppSetting::settings;

constexpr auto ParamOldEncryptedToken = "encrypted_token";
constexpr auto ParamAutoLogin = "autologin";
constexpr auto ParamThemeIndex = "theme";
constexpr auto ParamNetworkTimeout = "network_timeout";
constexpr auto ParamLogin = "auth_login";
constexpr auto ParamPassword = "auth_pass_enc";
constexpr auto ParamAnalyticsEnabled = "analytics_enabled";
constexpr auto ParamPixelBlastHighScore = "pixelblast_highscore";

inline QVariant GenericValue(const QString &key, bool *contains, const QVariant &set)
{
    QVariant retval;
    bool ret;
    if(set.isValid())
    {
        AppSetting::writeValue(key, set);
        if(contains != nullptr)
            (*contains) = true;
    }
    else
    {
        ret = AppSetting::readValue(key, &retval);
        if(contains != nullptr)
            (*contains) = ret;
    }
    return retval;
}

void AppSetting::save()
{
    if(settings)
        settings->sync();
}

void AppSetting::sync()
{
    save();
}

void AppSetting::load()
{
    if(settings != nullptr)
        return;
    settings = std::make_shared<QSettings>(QString("imister.kz-app.ads"), QString("AdsKiller"));
}

std::shared_ptr<QSettings> AppSetting::raw()
{
    if(!settings)
        load();
    return settings;
}

bool AppSetting::contains(const QString &key)
{
    if(!settings)
        load();
    return settings->contains(key);
}

QVariant AppSetting::value(const QString &key, const QVariant &defaultValue)
{
    if(!settings)
        load();
    return settings->value(key, defaultValue);
}

void AppSetting::setValue(const QString &key, const QVariant &value)
{
    if(!settings)
        load();
    settings->setValue(key, value);
}

bool AppSetting::readValue(const QString &key, QVariant *value)
{
    if(!settings)
        load();
    bool retval = settings->contains(key);
    if(retval && value != nullptr)
        (*value) = settings->value(key);
    return retval;
}

void AppSetting::writeValue(const QString &key, const QVariant &value)
{
    if(!settings)
        load();
    settings->setValue(key, value);
}

void AppSetting::removeKey(const QString &key)
{
    if(!settings)
        load();
    settings->remove(key);
}

void AppSetting::removeEncToken()
{
    removeKey(ParamOldEncryptedToken);
}

std::tuple<QString, QString> AppSetting::loginAndPass(bool *contains, const QVariant &login, const QVariant &pass)
{
    QString lg, ps;
    QVariant retval;
    if(!settings)
        load();
    retval = GenericValue(ParamLogin, contains, login);
    lg = retval.toString();

    if(pass.isValid())
        retval = CipherAlgoCrypto::PackDC(pass.toByteArray(), CipherAlgoCrypto::RandomKey());
    else
        retval = {};
    retval = CipherAlgoCrypto::UnpackDC(GenericValue(ParamPassword, contains, retval).toByteArray());
    ps = retval.toString();
    return {lg, ps};
}

bool AppSetting::autoLogin(bool *contains, const QVariant &set)
{
    if(!settings)
        load();
    QVariant retval;
    retval = GenericValue(ParamAutoLogin, contains, set);
    return retval.toBool();
}

int AppSetting::themeIndex(bool *contains, const QVariant &set)
{
    if(!settings)
        load();
    QVariant retval;
    retval = GenericValue(ParamThemeIndex, contains, set);
    return retval.toInt();
}

int AppSetting::networkTimeout(bool *contains, const QVariant &set)
{
    if(!settings)
        load();
    QVariant retval;
    // REMOVE OLD Params
    settings->remove("networkTimeout");
    retval = GenericValue(ParamNetworkTimeout, contains, set);
    return retval.toInt();
}

bool AppSetting::analyticsEnabled(bool *contains, const QVariant &set)
{
    if(!settings)
        load();
    QVariant retval;
    retval = GenericValue(ParamAnalyticsEnabled, contains, set);
    return retval.toBool();
}

int AppSetting::pixelBlastHighScore(bool *contains, const QVariant &set)
{
    if(!settings)
        load();
    QVariant retval;
    retval = GenericValue(ParamPixelBlastHighScore, contains, set);
    return retval.toInt();
}