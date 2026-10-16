#include <QCommandLineParser>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
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

#include <atomic>

struct FetchResult
{
    QString remoteLink;
    QString md5hash;
    quint64 bytes = 0;
    QCryptographicHash::Algorithm hashAlgo = QCryptographicHash::Md5;
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

    void setSimulate(bool simulate)
    {
        m_simulate = simulate;
    }

    std::atomic<int> finishSuccess {0};

    static bool isSafeRelativePath(const QString &baseDir, const QString &relativePath, QString *resolvedPath = nullptr);

private:
    void setError(const QString &error, int status = -1);
    QStringList getFilesEx(const QString &path, int flags = GetFiles, QString _special = QString(""));
    QString getFileMD5Hash(const QString &filePath);
    QString getFileHash(const QString &filePath, QCryptographicHash::Algorithm algo);
    static QCryptographicHash::Algorithm parseHashAlgorithm(const QString &hashType, const QString &hashHex);
    bool moveFilesTo(const QString &sourcePath, const QString &destinationPath);

private:
    QNetworkAccessManager *m_manager;
    QString m_rootUrl;
    QString m_version;
    quint64 m_totalBytes;
    std::atomic<int> m_forclyExit {0};
    QMutex mutex;
    DownloadStatus m_statusDownload;
    int m_lastStatus;
    QString m_lastError;
    bool m_simulate = false;
};
