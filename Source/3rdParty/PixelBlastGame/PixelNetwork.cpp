#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkReply>
#include <QNetworkRequest>

#include "PixelNetwork.h"

#ifndef CALLBACK_URL
constexpr char CallbackUrl[] = "https://example.com/callback";
#else
constexpr char CallbackUrl[] = CALLBACK_URL;
#endif

inline std::tuple<bool, PixelStats> getPixelStatObject(QJsonObject jsonObject)
{
    PixelStats stat {};
    bool success;
    if((success = (jsonObject["id"].isDouble() && jsonObject["name"].isString() && jsonObject["maxPoints"].isDouble())))
    {
        stat.id = jsonObject["id"].toInt();
        stat.name = jsonObject["name"].toString();
        stat.maxPoints = jsonObject["maxPoints"].toInt();
    }
    return {success, stat};
}

PixelNetwork::PixelNetwork(QObject *parent) : QObject(parent)
{
    manager = new QNetworkAccessManager(this);
    manager->setTransferTimeout(3000);
}

PixelNetwork::~PixelNetwork()
{
}

void PixelNetwork::newClient(QString nickname)
{
    QJsonObject json;
    QNetworkReply *reply;

    json["name"] = nickname;
    json["maxPoints"] = 0;
    reply = manager->post(QNetworkRequest(QUrl(CallbackUrl)), QJsonDocument(json).toJson(QJsonDocument::Compact));
    QObject::connect(reply, &QNetworkReply::finished, this, &PixelNetwork::onReplyCurrent);
}

void PixelNetwork::updateStats(PixelStats stat)
{
    QJsonObject json;
    QNetworkReply *reply;

    json["id"] = stat.id;
    json["name"] = stat.name;
    json["maxPoints"] = stat.maxPoints;
    reply = manager->post(QNetworkRequest(QUrl(CallbackUrl)), QJsonDocument(json).toJson(QJsonDocument::Compact));
    QObject::connect(reply, &QNetworkReply::finished, this, &PixelNetwork::onReplyCurrent);
}

void PixelNetwork::readUsers(std::uint32_t max)
{
    QUrl url(CallbackUrl);
    if(max != std::uint32_t(-1))
        url.setQuery(QString("top=%1").arg(max));
    QNetworkReply *reply = manager->get(QNetworkRequest(url));
    QObject::connect(reply, &QNetworkReply::finished, this, &PixelNetwork::onReplyStats);
}

bool PixelNetwork::isConnected()
{
    return 0;
}

// CALLBACK RETURN SLOTS

void PixelNetwork::onReplyCurrent()
{
    PixelStats curStat {};
    NetworkResultFlags state = NetworkResultFlags::NoNetwork;
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if(reply)
    {
        if(reply->error() == QNetworkReply::NoError)
        {
            QJsonDocument jdoc = QJsonDocument::fromJson(reply->readAll());
            QJsonObject data = jdoc["data"]["client"].toObject();
            state = NetworkResultFlags::UserNoExists;
            if((jdoc["ok"].toBool() && !data.isEmpty()))
            {
                const auto &result = getPixelStatObject(data);
                if(std::get<0>(result))
                {
                    curStat = std::get<1>(result);
                    state = NetworkResultFlags::Ok;
                }
            }
        }
        emit callbackCurrent(curStat, state);
        reply->deleteLater();
    }
}

void PixelNetwork::onReplyStats()
{
    QList<PixelStats> stats {};
    NetworkResultFlags state = NetworkResultFlags::NoNetwork;
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if(reply)
    {
        if(reply->error() == QNetworkReply::NoError)
        {
            QJsonDocument jdoc = QJsonDocument::fromJson(reply->readAll());
            QJsonArray items = jdoc["data"]["items"].toArray();
            if(jdoc["ok"].toBool())
            {
                state = NetworkResultFlags::Ok;
                for(int x = 0; x < items.size(); ++x)
                {
                    const auto &result = getPixelStatObject(items[x].toObject());
                    if(!(std::get<0>(result)))
                    {
                        state = NetworkResultFlags::ServerError;
                        stats.clear();
                        break;
                    }
                    stats.push_back(std::get<1>(result));
                }
            }
        }
        emit callbackUsers(stats, state);
        reply->deleteLater();
    }
}
