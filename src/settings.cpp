#include "extension.h"

std::shared_ptr<QSettings> AppSetting::settings;

constexpr auto ParamEncryptedToken = "encrypted_token";
constexpr auto ParamAutoLogin = "autologin";
constexpr auto ParamThemeIndex = "theme";
constexpr auto ParamNetworkTimeout = "networkTimeout";
constexpr auto ParamLogin = "auth_login";
constexpr auto ParamPassword = "auth_pass_enc";

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

void AppSetting::load()
{
    if(settings != nullptr)
        return;
    settings = std::make_shared<QSettings>(QString("imister.kz-app.ads"), QString("AdsKiller"));
}

bool AppSetting::readValue(const QString &key, QVariant *value)
{
    bool retval = settings->contains(key);
    if(retval && value != nullptr)
        (*value) = settings->value(key);
    return retval;
}

void AppSetting::writeValue(const QString &key, const QVariant &value)
{
    settings->setValue(key, value);
}

void AppSetting::removeKey(const QString &key)
{
    settings->remove(key);
}

void AppSetting::removeEncToken()
{
    removeKey(ParamEncryptedToken);
}

QString AppSetting::encryptedToken(bool *contains, const QVariant &set)
{
    QVariant retval;
    retval = GenericValue(ParamEncryptedToken, contains, set);
    return retval.toString();
}

std::tuple<QString, QString> AppSetting::loginAndPass(bool *contains, const QVariant &login, const QVariant &pass)
{
    QString lg, ps;
    QVariant retval;
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
    QVariant retval;
    retval = GenericValue(ParamAutoLogin, contains, set);
    return retval.toBool();
}

int AppSetting::themeIndex(bool *contains, const QVariant &set)
{
    QVariant retval;
    retval = GenericValue(ParamThemeIndex, contains, set);
    return retval.toInt();
}

int AppSetting::networkTimeout(bool *contains, const QVariant &set)
{
    QVariant retval;
    retval = GenericValue(ParamNetworkTimeout, contains, set);
    return retval.toInt();
}
