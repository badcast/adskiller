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
#include <QVersionNumber>

class MainWindow;

struct AdbDevice;

struct UserData
{
    QString token;
    QString idName;
    QString location;
    QString currencyType;

    bool blocked;
    std::uint32_t vipDays;
    std::uint32_t connectedDevices;
    std::uint32_t credits;
    std::uint32_t basePrice;
    QDateTime lastLogin;
    QDateTime serverLastTime;

    bool isNotValidBalance() const;
    bool hasBalance() const;
    bool hasVipAccount() const;
};

struct VersionInfo
{
    int mStatus = -1;
    QVersionNumber mVersion;
    QString mDownloadUrl;

    VersionInfo() = default;
    VersionInfo(const QString& version, const QString& url, int status);

    bool empty() const;
};

struct LabStatusInfo
{
    QString mdKey;
    QString analyzeStatus;
    bool purchased;

    bool ready() const;
    bool exists() const;
};

struct AdsInfo
{
    LabStatusInfo labs;
    QStringList blacklist;
    QStringList disabling;
};

enum NetworkStatus
{
    OK,
    ServerError = 500,
    NoEnoughMoney = 601,
    AccountBlocked = 666,
    NetworkError = 1000
};

class Network : public QObject
{
    Q_OBJECT
    friend class MainWindow;
private:
    QNetworkAccessManager* manager;
    QString _token;
    std::int64_t _lastBytes;
    int _pending;

public:
    UserData authedId;

    Network(QObject * parent = nullptr);
    bool isAuthed();
    bool pending();
    bool checkNet();

    void pullAdsData(const QString &mdKey);
    void pullFetchVersion(bool populate);
    void pullLabState(const QString &mdKey);

    void pushAuth(const QString &token);
    bool pushUserPackages(const AdbDevice& device, const QStringList& packages);

signals:
    void loginFinish(int status, bool ok);
    void labAdsFinish(int status, const AdsInfo& adsData, bool ok);
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
