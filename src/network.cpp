#include <utility>

#include <QJsonArray>
#include <QJsonObject>
#include <QVersionNumber>

#include "adbfront.h"
#include "begin.h"
#include "network.h"

#ifndef NREMOTEADDR
#define NREMOTEADDR "http://localhost:8000/api"
#endif

constexpr auto URL_Remote = NREMOTEADDR;
constexpr auto URL_CDN = "cdn";
constexpr auto URL_SupVer = "v2";
constexpr auto URL_Work = "callback";
constexpr auto URL_Version = "version";

enum
{
    FpullAdsData = 1,
    FpullFetchVersion = 2,
    FpullLabState = 4,
    FpullServiceList = 16,
    FpullUserPackages = 32,
    FpullServiceUUID = 64,
    Fauth = 128
};

inline const QUrl &url_fetch()
{
    static const QUrl url(QStringLiteral("%1/%2/%3").arg(URL_Remote, URL_SupVer, URL_Work));
    return url;
}

inline const QUrl &url_version()
{
    static const QUrl url(QStringLiteral("%1/%2/%3").arg(URL_Remote, URL_CDN, URL_Version));
    return url;
}

inline LabStatusInfo fromJsonLabs(const QJsonValue &jroot)
{
    LabStatusInfo retval {};
    if(jroot.isObject() && jroot["analyzeStatus"].isString() && jroot["mdKey"].isString())
    {
        retval.analyzeStatus = jroot["analyzeStatus"].toString();
        retval.mdKey = jroot["mdKey"].toString();
        retval.purchased = jroot["purchased"].toBool();
    }
    return retval;
}

inline QString so_strify(ServiceOperation so)
{
    switch(so)
    {
    case ServiceOperation::Get:
        return QStringLiteral("get");
    case ServiceOperation::Set:
        return QStringLiteral("set");
    case ServiceOperation::Open:
        return QStringLiteral("open");
    case ServiceOperation::Close:
        return QStringLiteral("close");
    case ServiceOperation::Other:
        return QStringLiteral("other");
    default:
        return QStringLiteral("invalid");
    }
}

inline ServiceOperation so_destrify(const QString &so)
{
    if(so == QLatin1String("get"))
        return ServiceOperation::Get;
    if(so == QLatin1String("set"))
        return ServiceOperation::Set;
    if(so == QLatin1String("open"))
        return ServiceOperation::Open;
    if(so == QLatin1String("close"))
        return ServiceOperation::Close;
    if(so == QLatin1String("other"))
        return ServiceOperation::Other;
    return ServiceOperation::Invalid;
}

Network::Network(const Network &other) : QObject(nullptr), manager(new QNetworkAccessManager(this)), _token(other._token), _lastBytes(0), _pending(0), forclyExit(false)
{
    manager->setTransferTimeout(NetworkTimeoutDefault);
}

Network::Network(QObject *parent) : QObject(parent), manager(new QNetworkAccessManager(this)), _lastBytes(0), _pending(0), forclyExit(false)
{
    manager->setTransferTimeout(NetworkTimeoutDefault);
}

QString Network::defaultUserAgent()
{
    static const QString userAgent = QStringLiteral("AdsKiller-Desktop/%1.%2.%3")
        .arg(AppVerMajor).arg(AppVerMinor).arg(AppVerPatch);
    return userAgent;
}

QNetworkRequest Network::generalCreateRequest(const QUrl &url, bool needAuth) const
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setHeader(QNetworkRequest::UserAgentHeader, defaultUserAgent());
    if(needAuth && !_token.isEmpty())
    {
        request.setRawHeader("Authorization", "Bearer " + _token.toUtf8());
    }
    return request;
}

QNetworkReply *Network::generalCreateRequest(const QUrl &url, const QJsonObject &json, int pendingFlag, bool needAuth)
{
    if(needAuth && !isAuthed())
    {
        return nullptr;
    }

    _pending |= pendingFlag;
    const QNetworkRequest request = generalCreateRequest(url, needAuth);
    const QByteArray data = QJsonDocument(json).toJson(QJsonDocument::Compact);
    return manager->post(request, data);
}

void Network::pushLoginPass(const QString &login, const QString &pass)
{
    authedId = {}; // Clean last info
    QJsonObject json;
    json[QStringLiteral("request")] = QStringLiteral("TOKENVERIFY");
    json[QStringLiteral("login")] = login;
    json[QStringLiteral("pass")] = pass;

    generalCreateRequest(url_fetch(), json, Fauth, &Network::onAuthJWTFinished, false);
}

void Network::pushAuthToken()
{
    if(!isAuthed())
        return;

    QJsonObject json;
    json[QStringLiteral("request")] = QStringLiteral("TOKENVERIFY");

    generalCreateRequest(url_fetch(), json, Fauth, &Network::onAuthJWTFinished, true);
}

void Network::pullServiceList()
{
    if(!isAuthed())
        return;

    QJsonObject json;
    json[QStringLiteral("request")] = QStringLiteral("LISTSERVICES");

    generalCreateRequest(url_fetch(), json, FpullServiceList, &Network::onPullServiceList, true);
}

void Network::pullServiceUUID(const QString &uuid, const QJsonObject &request, ServiceOperation so)
{
    if(!isAuthed())
        return;

    QJsonObject json;
    json[QStringLiteral("request")] = QStringLiteral("SERVICEREQ");
    json[QStringLiteral("uuid")] = uuid;
    json[QStringLiteral("type")] = so_strify(so);
    json[QStringLiteral("service")] = request;

    generalCreateRequest(url_fetch(), json, FpullServiceUUID, &Network::onPullServiceUUID, true);
}

void Network::pullFetchVersion(bool populate)
{
    QJsonObject json;
    if(populate)
    {
        json[QStringLiteral("currentClient")] = QStringLiteral("%1.%2.%3").arg(AppVerMajor).arg(AppVerMinor).arg(AppVerPatch);
    }

    generalCreateRequest(url_version(), json, FpullFetchVersion, &Network::onFetchingVersion, false);
}

bool Network::checkNet()
{
    return _lastBytes <= 0;
}

void Network::setTimeout(int value)
{
    manager->setTransferTimeout(value);
}

bool Network::isAuthed()
{
    return !_token.isEmpty();
}

bool Network::pending()
{
    return _pending != 0;
}

void Network::onAuthJWTFinished()
{
    int status = NetworkStatus::NetworkError;
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    _lastBytes = 0;
    if(reply)
    {
        if(reply->error() == QNetworkReply::NoError)
        {
            const QByteArray responce = reply->readAll();
            _lastBytes = responce.size();
            const QJsonDocument jsonResp = QJsonDocument::fromJson(responce);
            status = NetworkStatus::ServerError;

            if(!jsonResp.isNull() && jsonResp[QStringLiteral("status")].isDouble())
            {
                status = jsonResp[QStringLiteral("status")].toInt();
                if(status == NetworkStatus::OK)
                {
                    if(!jsonResp[QStringLiteral("token")].isUndefined())
                    {
                        _token = jsonResp[QStringLiteral("token")].toString();
                    }
                    else
                    {
                        const QJsonObject obj = jsonResp.object();
                        authedId.idName = obj[QStringLiteral("username")].toString();
                        authedId.lastLogin = obj[QStringLiteral("lastLogin")].toVariant().toDateTime();
                        authedId.serverLastTime = obj[QStringLiteral("serverLastTime")].toVariant().toDateTime();
                        authedId.connectedDevices = obj[QStringLiteral("scores")].toInt();
                        authedId.credits = obj[QStringLiteral("credits")].toVariant().toUInt();
                        authedId.vipDays = obj[QStringLiteral("vip_period")].toVariant().toUInt();
                        authedId.location = obj[QStringLiteral("location")].toString();
                        authedId.blocked = obj[QStringLiteral("blocked")].toBool();
                        authedId.basePrice = obj[QStringLiteral("base_price")].toVariant().toUInt();
                        authedId.currencyType = obj[QStringLiteral("currency_type")].toString();
                    }
                }
            }
        }
        emit sLoginFinish(status, status == NetworkStatus::OK);
        reply->deleteLater();
    }
    _pending &= ~Fauth;
}

void Network::onFetchingVersion()
{
    int status = NetworkStatus::NetworkError;
    QString version, url;
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    _lastBytes = 0;
    if(reply)
    {
        if(reply->error() == QNetworkReply::NoError)
        {
            const QByteArray resp = reply->readAll();
            _lastBytes = resp.size();
            const QJsonDocument jsonResp = QJsonDocument::fromJson(resp);
            if(!jsonResp.isNull())
            {
                const QJsonObject obj = jsonResp.object();
                const auto versionVal = obj[QStringLiteral("version")];
                const auto urlVal = obj[QStringLiteral("url")];
                if(!versionVal.isNull() && !urlVal.isNull())
                {
                    version = versionVal.toString();
                    url = urlVal.toString();
                    status = NetworkStatus::OK;
                }
            }
        }
        emit sFetchingVersion(status, version, url, status == NetworkStatus::OK);
        reply->deleteLater();
    }
    _pending &= ~FpullFetchVersion;
}

void Network::onPullServiceList()
{
    int status = NetworkStatus::NetworkError;
    QList<ServiceItemInfo> services;
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    _lastBytes = 0;
    if(reply)
    {
        if(reply->error() == QNetworkReply::NoError)
        {
            const QByteArray resp = reply->readAll();
            _lastBytes = resp.size();
            const QJsonDocument jsonResp = QJsonDocument::fromJson(resp);
            if(!jsonResp.isNull())
            {
                const QJsonObject rootObj = jsonResp.object();
                status = rootObj[QStringLiteral("status")].toInt();
                const QJsonValue resultVal = rootObj[QStringLiteral("result")];
                if(status == 0 && resultVal.isArray())
                {
                    const QJsonArray result = resultVal.toArray();
                    services.reserve(result.size());
                    for(const auto &item : result)
                    {
                        const QJsonObject obj = item.toObject();
                        ServiceItemInfo sii;
                        sii.uuid = obj[QStringLiteral("uuid")].toString();
                        sii.active = obj[QStringLiteral("active")].toBool();
                        sii.name = obj[QStringLiteral("name")].toString();
                        sii.description = obj[QStringLiteral("description")].toString();
                        sii.price = obj[QStringLiteral("price")].toVariant().toUInt();
                        sii.needVIP = obj[QStringLiteral("useVIP")].toBool();
                        sii.hide = obj[QStringLiteral("hide")].toBool();
                        services.append(std::move(sii));
                    }
                    status = NetworkStatus::OK;
                }
            }
        }
        emit sPullServiceList(services, status == NetworkStatus::OK);
        reply->deleteLater();
    }
    _pending &= ~FpullServiceList;
}

void Network::onPullServiceUUID()
{
    int status = NetworkStatus::NetworkError;
    QJsonObject responce {};
    QString guid {};
    ServiceOperation so = ServiceOperation::Invalid;
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    _lastBytes = 0;
    if(reply)
    {
        if(reply->error() == QNetworkReply::NoError)
        {
            const QByteArray resp = reply->readAll();
            _lastBytes = resp.size();
            const QJsonDocument jsonResp = QJsonDocument::fromJson(resp);
            if(!jsonResp.isNull())
            {
                const QJsonObject rootObj = jsonResp.object();
                status = rootObj[QStringLiteral("status")].toInt();
                const auto resVal = rootObj[QStringLiteral("result")];
                const auto guidVal = rootObj[QStringLiteral("guid")];

                if(status == 0 && !resVal.isNull() && !guidVal.isNull())
                {
                    guid = guidVal.toString();
                    responce = resVal.toObject();
                    so = so_destrify(rootObj[QStringLiteral("type")].toString());
                    status = NetworkStatus::OK;
                }
            }
        }

        emit sPullServiceUUID(responce, guid, so, status == NetworkStatus::OK);
        reply->deleteLater();
    }
    _pending &= ~FpullServiceUUID;
}

VersionInfo::VersionInfo(const QString &version, const QString &url, int status) : mDownloadUrl(url), mStatus(status)
{
    mVersion = QVersionNumber::fromString(version);
}

bool VersionInfo::empty() const
{
    return mStatus == -1 && mDownloadUrl.isEmpty() && mVersion.isNull();
}

bool DeviceItemInfo::operator<(const DeviceItemInfo &other) const
{
    return logTime < other.logTime;
}
