#pragma once

#include <QString>
#include <QObject>
#include <QNetworkAccessManager>

#include "PixelBegin.h"

struct PixelStats
{
    int id;
    QString name;
    int maxPoints;
    int rankPos;
};

enum NetworkResultFlags
{
    Ok = 0,
    NoNetwork = 1,
    UserNoExists = 2,
    ServerError = 4
};

class PB_EXPORT PixelNetwork : public QObject
{
    Q_OBJECT

private:
    QNetworkAccessManager *manager;

public:
    PixelNetwork(QObject *parent = nullptr);
    ~PixelNetwork();

    void newClient(QString nickname);
    void updateStats(PixelStats stat);
    void readUsers(std::uint32_t max = std::uint32_t(-1));
    bool isConnected();

signals:
    void callbackCurrent(const PixelStats &stats, NetworkResultFlags ok);
    void callbackUsers(const QList<PixelStats> &stats, NetworkResultFlags ok);

private slots:
    void onReplyCurrent();
    void onReplyStats();
};
