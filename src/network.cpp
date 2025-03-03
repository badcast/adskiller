#include <QJsonObject>
#include <QJsonArray>

#include "adbtrace.h"
#include "network.h"

constexpr auto URL_Remote = "https://adskill.imister.kz";
//constexpr auto URL_Remote = "http://localhost:8080";
constexpr auto URL_SupVer = "v1";
constexpr auto URL_Auth = "fetch.php";

QString url_fetch()
{
    QString url = URL_Remote;
    url += "/";
    url += URL_SupVer;
    url += "/";
    url += URL_Auth;
    return url;
}

Network::Network(QObject * parent) : QObject(parent)
{
    manager = new QNetworkAccessManager{this};
}

void Network::getToken(const QString& token)
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

void Network::getAds()
{
    QStringList list;
    QNetworkReply *reply;
    QUrl url(url_fetch());
    QNetworkRequest request(url);
    if(!isAuthed())
        return;
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("token", authedId.token.toUtf8());
    reply = manager->post(request, "{\"request\":\"GETADS\"}");
    connect(reply, &QNetworkReply::finished, this, &Network::onAdsFinished);
}

void Network::sendUserPackages(const AdbDevice &device, const QStringList &packages)
{
    QNetworkReply *reply;
    QUrl url(url_fetch());
    QNetworkRequest request(url);
    if(!isAuthed() || packages.empty())
        return;
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("token", authedId.token.toUtf8());
    QJsonObject json;
    QJsonArray array;
    json["request"]= "UPLOADPKGS";
    json["deviceSerial"] = device.devId;
    json["deviceModel"] = device.model;
    json["deviceVendor"] = device.vendor;
    for(const QString & str : packages)
        array.append(str);
    json["packages"] = array;
    reply = manager->post(request, QJsonDocument(json).toJson());
    connect(reply, &QNetworkReply::finished, this, &Network::onUserPackagesUploadFinished);
}

bool Network::isAuthed()
{
    return !authedId.token.isEmpty();
}

void Network::onAuthFinished()
{
    QNetworkReply * reply = qobject_cast<QNetworkReply*>(sender());
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
                    emit loginFinish(-1, false);
                    break;
                }
                if(jsonResp["status"].toInt() != 0)
                {
                    emit loginFinish(jsonResp["status"].toInt(), false);
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
                emit loginFinish(0, true);
            }
            else
            {
                emit loginFinish(1000, false);
            }
            break;
        }

        reply->deleteLater();
    }
}

void Network::onAdsFinished()
{
    int status = 500;
    QStringList adsList;
    QNetworkReply * reply = qobject_cast<QNetworkReply*>(sender());
    if(reply)
    {
        for(;;)
        {
            if(reply->error() == QNetworkReply::NoError)
            {
                QByteArray responce = reply->readAll();
                QJsonDocument jsonResp = QJsonDocument::fromJson(responce);
                status = 0;
                if(jsonResp.isNull() || !jsonResp["status"].isDouble())
                    status = 500;
                else
                    status = jsonResp["status"].toInt();
                if(status == 0)
                {
                    QString split = jsonResp["ads"].toString();
                    adsList = split.split('\n', Qt::SkipEmptyParts);
                }
            }
            break;
        }
        emit adsFinished(adsList, status, status==0);
        reply->deleteLater();
    }
}

void Network::onUserPackagesUploadFinished()
{
    int status = 500;
    QNetworkReply * reply = qobject_cast<QNetworkReply*>(sender());
    if(reply)
    {
        for(;;)
        {
            if(reply->error() == QNetworkReply::NoError)
            {
                QByteArray responce = reply->readAll();
                QJsonDocument jsonResp = QJsonDocument::fromJson(responce);
                if(jsonResp.isNull() || !jsonResp["status"].isDouble())
                    status = 500;
                else
                    status = jsonResp["status"].toInt();
            }
            break;
        }
        emit uploadUserPackages(status, status == 0);
    }
}
