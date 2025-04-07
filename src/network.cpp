#include <QJsonObject>
#include <QJsonArray>

#include "adbfront.h"
#include "network.h"

constexpr auto URL_Remote = "https://adskill.imister.kz";
//constexpr auto URL_Remote = "http://localhost:8080";
constexpr auto URL_SupVer = "v1";
constexpr auto URL_Fetch = "fetch1.php";
constexpr auto URL_Version = "version.php";

QString url_fetch()
{
    QString url = URL_Remote;
    url += "/";
    url += URL_SupVer;
    url += "/";
    url += URL_Fetch;
    return url;
}

QString url_version()
{
    QString url = URL_Remote;
    url += "/";
    url += URL_SupVer;
    url += "/";
    url += URL_Version;
    return url;
}

Network::Network(QObject *parent) : QObject(parent)
{
    manager = new QNetworkAccessManager {this};
    manager->setTransferTimeout(5000);
}

void Network::authenticate(const QString &token)
{
    QNetworkReply *reply;
    QUrl url(url_fetch());
    QNetworkRequest request(url);
    authedId = {}; // Clean last info
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Token", token.toUtf8());
    reply = manager->post(request, "{\"request\":\"TOKENVERIFY\"}");
    connect(reply, &QNetworkReply::finished, this, &Network::onAuthFinished);
}

void Network::getAdsData(const QString& mdKey)
{
    QNetworkReply *reply;
    QUrl url(url_fetch());
    QNetworkRequest request(url);
    if(!isAuthed())
        return;
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Token", authedId.token.toUtf8());
    QString requestBody = "{\"request\":\"GETADS\",\"mdKey\":\"";
    requestBody += mdKey + "\"}";
    reply = manager->post(request, requestBody.toUtf8());
    connect(reply, &QNetworkReply::finished, this, &Network::onAdsFinished);
}

bool Network::sendUserPackages(const AdbDevice &device, const QStringList &packages)
{
    QJsonObject json;
    QJsonArray array;
    QNetworkReply *reply;
    QUrl url(url_fetch());
    QNetworkRequest request(url);
    if(device.devId.isEmpty() || packages.empty())
        return false;
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Token", authedId.token.toUtf8());
    json["request"] = "UPLOADPKGS";
    json["deviceSerial"] = device.devId;
    json["deviceModel"] = device.model;
    json["deviceVendor"] = device.vendor;
    for(const QString &str : packages)
        array.append(str);
    json["packages"] = array;
    reply = manager->post(request, QJsonDocument(json).toJson());
    connect(reply, &QNetworkReply::finished, this, &Network::onUserPackagesUploadFinished);
    return true;
}

bool Network::isAuthed()
{
    return !authedId.token.isEmpty();
}

void Network::fetchVersion()
{
    QNetworkReply *reply;
    QUrl url(url_version());
    QNetworkRequest request(url);
    reply = manager->get(request);
    connect(reply, &QNetworkReply::finished, this, &Network::onFetchingVersion);
}

void Network::onAuthFinished()
{
    int status = NetworkStatus::NetworkError;
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if(reply)
    {
        for(;;)
        {
            if(reply->error() == QNetworkReply::NoError)
            {
                QByteArray responce = reply->readAll();
                QJsonDocument jsonResp = QJsonDocument::fromJson(responce);
                if(jsonResp.isNull())
                {
                    status = NetworkStatus::ServerError;
                    break;
                }
                if((status=jsonResp["status"].toInt()) != NetworkStatus::OK)
                {
                    break;
                }
                QString tmp = jsonResp["token"].toString();
                // Update current token with server replaced.
                _token = tmp;
                authedId.token = _token;
                authedId.idName = jsonResp["username"].toString();
                authedId.lastLogin = jsonResp["lastLogin"].toVariant().toDateTime();
                authedId.serverLastTime = jsonResp["serverLastTime"].toVariant().toDateTime();
                authedId.expires = jsonResp["expires"].toInt();
                authedId.scores = jsonResp["scores"].toInt();
                status = 0;
            }
            break;
        }
        emit loginFinish(status, status == NetworkStatus::OK);
        reply->deleteLater();
    }
}

void Network::onAdsFinished()
{
    int status = NetworkStatus::NetworkError;
    QStringList adsList;
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if(reply)
    {
        for(;;)
        {
            if(reply->error() == QNetworkReply::NoError)
            {
                QByteArray responce = reply->readAll();
                QJsonDocument jsonResp = QJsonDocument::fromJson(responce);
                if(jsonResp.isNull() || !jsonResp["status"].isDouble())
                    status = NetworkStatus::ServerError;
                else
                    status = jsonResp["status"].toInt();
                if(status == NetworkStatus::OK)
                {
                    if(jsonResp["expires"].isDouble())
                        authedId.expires = jsonResp["expires"].toInt();
                    QJsonArray adsData = jsonResp["ads"].toArray();
                    for(const QJsonValue & val : adsData)
                    {
                        adsList << val.toString();
                    }
                }
            }
            break;
        }
        emit adsFinished(adsList, status, status == NetworkStatus::OK);
        reply->deleteLater();
    }
}

void Network::onUserPackagesUploadFinished()
{
    int status = NetworkStatus::NetworkError;
    QString mdKey;
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if(reply)
    {
        for(;;)
        {
            if(reply->error() == QNetworkReply::NoError)
            {
                QByteArray responce = reply->readAll();
                QJsonDocument jsonResp = QJsonDocument::fromJson(responce);
                if(jsonResp.isNull() || !jsonResp["status"].isDouble())
                    status = NetworkStatus::ServerError;
                else
                {
                    status = jsonResp["status"].toInt();
                    if(status == NetworkStatus::OK)
                    {
                        mdKey = jsonResp["mdKey"].toString();
                    }
                }
            }
            break;
        }
        emit uploadUserPackages(status, mdKey, status == NetworkStatus::OK);
        reply->deleteLater();
    }
}

void Network::onFetchingVersion()
{
    int status = NetworkStatus::NetworkError;
    QString version, url;
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if(reply)
    {
        for(;;)
        {
            if(reply->error() == QNetworkReply::NoError)
            {
                QByteArray resp = std::move(reply->readAll());
                QJsonDocument jsonResp = QJsonDocument::fromJson(resp);
                if(!jsonResp.isNull() && !jsonResp["version"].isNull() && !jsonResp["url"].isNull())
                {
                    version = jsonResp["version"].toString();
                    url = jsonResp["url"].toString();
                    status = NetworkStatus::OK;
                }
            }
            break;
        }
        emit fetchingVersion(status, version, url, status == NetworkStatus::OK);
        reply->deleteLater();
    }
}
