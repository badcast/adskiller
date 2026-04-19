#ifndef NETWORK_H
#define NETWORK_H

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QVersionNumber>

constexpr auto NetworkTimeoutDefault = 15000;

class MainWindow;

struct AdbDevice;

struct UserDataInfo
{
    QString idName;
    QString location;
    QString currencyType;
    QString login;
    QString pass;

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

    void cleanExceptLoginPass();
};

struct VersionInfo
{
    int mStatus = -1;
    QVersionNumber mVersion;
    QString mDownloadUrl;

    VersionInfo() = default;
    VersionInfo(const QString &version, const QString &url, int status);

    bool empty() const;
};

struct DeviceItemInfo
{
    QString mdkey;
    QDateTime logTime;
    QDateTime lastConnectTime;
    QDateTime expire;

    int deviceId;
    int packages;
    int connectionCount;
    int serverQuarantee;
    int purchasedType;
    int purchasedValue;

    QString vendor;
    QString model;

    bool operator<(const DeviceItemInfo &other) const;
};

struct ServiceItemInfo
{
    bool active;
    bool needVIP;
    bool hide;
    std::uint32_t price;
    QString uuid;
    QString name;
    QString description;
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

enum ServiceOperation
{
    Invalid = -1,
    Get,
    Set,
    Open,
    Close,
    Other
};

enum NetworkStatus
{
    OK,
    ServerError = 500,
    NoEnoughMoney = 601,
    AccountBlocked = 666,
    NetworkError = 1000
};

// TODO: Make Mutex lock by run Network opertion
class Network : public QObject
{
    Q_OBJECT
    friend class MainWindow;

private:
    QNetworkAccessManager *manager;
    QString _token;
    std::int64_t _lastBytes;
    int _pending;
    bool forclyExit;

public:
    UserDataInfo authedId;

    Network(QObject *parent = nullptr);
    bool isAuthed();
    bool pending();
    bool checkNet();

    void setTimeout(int value);

    void pullFetchVersion(bool populate);
    void pullServiceList();

    void pullServiceUUID(const QString &uuid, const QJsonObject &request, ServiceOperation so);

    [[deprecated]] void pushAuthOld(const QString &token);
    void pushLoginPass(const QString &login, const QString &pass);
    void pushAuthToken();

signals:
    void sOldTokenFinish(int status, bool ok);
    void sLoginFinish(int status, bool ok);
    void sFetchingVersion(int status, const QString &version, const QString &url, bool ok);
    void sPullServiceList(const QList<ServiceItemInfo> &services, bool ok);
    void sPullServiceUUID(const QJsonObject responce, const QString uuid, ServiceOperation so, bool ok);

private slots:
    void onAuthOldFinished();
    void onAuthJWTFinished();
    void onFetchingVersion();
    void onPullServiceList();
    void onPullServiceUUID();
};

#endif // NETWORK_H
