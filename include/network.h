#ifndef NETWORK_H
#define NETWORK_H

#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDateTime>
#include <QList>

class MainWindow;

struct AdbDevice;

struct AuthIDData
{
    QString token;
    QString idName;
    QString location;
    int vipDays;
    int expires;
    int scores;
    QDateTime lastLogin;
    QDateTime serverLastTime;
};

struct LabStatusInfo
{
    QString mdKey;
    QString analyzeStatus;

    bool ready() const;
    bool exists() const;
};

enum NetworkStatus
{
    OK,
    ServerError = 500,
    NoEnoughMoney = 601,
    NetworkError = 1000
};

class Network : public QObject
{
    Q_OBJECT
    friend class MainWindow;
private:
    QNetworkAccessManager* manager;
    QString _token;

public:
    AuthIDData authedId;

    Network(QObject * parent = nullptr);
    void authenticate(const QString &token);
    bool isAuthed();
    void fetchVersion();

    void getAdsData(const QString &mdKey);
    bool sendUserPackages(const AdbDevice& device, const QStringList& packages);
    void fetchLabState(const QString &mdKey);
    // TODO: Check Network
    bool checkNet();

signals:
    void loginFinish(int status, bool ok);
    void labAdsFinish(int status, const std::pair<LabStatusInfo,QStringList>& adsData, bool ok);
    void uploadUserPackages(int status, const LabStatusInfo& labs, bool ok);
    void fetchingVersion(int status, const QString& version, const QString& url, bool ok);
    void fetchingLabs(int status, const LabStatusInfo& labs, bool ok);

private slots:
    void onAuthFinished();
    void onAdsFinished();
    void onUserPackagesUploadFinished();
    void onFetchingVersion();
    void onFetchingLabs();
};

#endif // NETWORK_H
