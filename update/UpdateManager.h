#include <QCommandLineParser>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QLockFile>
#include <QMutex>
#include <QMutexLocker>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <QSharedMemory>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QUrl>

struct FetchResult
{
    QString remoteLink;
    QString md5hash;
    quint64 bytes;
};

struct DownloadStatus
{
    quint64 currentDownloadBytes;
    quint64 currentMaxDownloadBytes;
    quint64 totalDownloadedBytes;
    quint64 totalDownloadBytes;
    int downloadStep;
    int maxDownloads;
    QString currentStatus;
};

class UpdateManager : public QObject
{
    Q_OBJECT

    enum
    {
        GetDirs = QDir::Dirs,
        GetFiles = QDir::Files,
        AllOF = GetDirs | GetFiles,
        WriteFullpath = 4
    };

public:
    UpdateManager(QObject *parent = nullptr);

    std::pair<QList<FetchResult>, int> fetch();

    std::pair<QList<FetchResult>, int> filter_by(const QString &existsDir, const QList<FetchResult> &updates);

    int downloadAll(const QString &existsDir, const QList<FetchResult> &contents);

    void stop();

    DownloadStatus downloadStatus();

    QString getLastError(int *lastStatus = nullptr);

    int finishSuccess;

private:
    QStringList getFilesEx(const QString &path, int flags = GetFiles, QString _special = QString(""));
    QString getFileMD5Hash(const QString &filePath);
    bool moveFilesTo(const QString &sourcePath, const QString &destinationPath);

private:
    QNetworkAccessManager *m_manager;
    QString m_rootUrl;
    QString m_version;
    quint64 m_totalBytes;
    int m_forclyExit;
    QMutex mutex;
    DownloadStatus m_statusDownload;
    int m_lastStatus;
    QString m_lastError;
};
