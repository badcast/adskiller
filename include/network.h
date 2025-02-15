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
    int expires;
    QDateTime lastLogin;
    QDateTime serverLastTime;
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
    void getToken(const QString &token);
    bool isAuthed();

    void getAds();
    void sendUserPackages(const AdbDevice& device, const QStringList& packages);
    bool checkNet();

signals:
    void loginFinish(int status, bool ok);
    void adsFinished(const QStringList& adsData, int status, bool ok);
    void uploadUserPackages(int status, bool ok);

private slots:
    void onAuthFinished();
    void onAdsFinished();
    void onUserPackagesUploadFinished();
};

#endif // NETWORK_H
