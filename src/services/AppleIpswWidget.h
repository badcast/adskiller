#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QTextEdit>
#include <QRadioButton>
#include <QButtonGroup>
#include <QSplitter>
#include <QIcon>
#include <QTimer>
#include <QProcess>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QComboBox>
#include <QTabWidget>
#include <QStackedWidget>
#include <QScrollArea>
#include <QStorageInfo>
#include <QCheckBox>
#include <QThread>
#include <QCryptographicHash>
#include <atomic>

#include "AppleFront.h"

struct IpswFirmwareInfo
{
    QString identifier;
    QString version;
    QString buildid;
    QString sha1sum;
    QString md5sum;
    qint64 size = 0;
    QString url;
    bool isSigned = false;
    QString releaseDate;
};

enum class ChecksumStatus
{
    NotChecked,
    Calculating,
    Matched,
    Mismatch,
    VerifiedWithoutExpected,
    Error
};

class IpswChecksumWorker : public QThread
{
    Q_OBJECT

public:
    explicit IpswChecksumWorker(QObject *parent = nullptr);
    ~IpswChecksumWorker() override;

    void startVerification(const QString &filePath, const QString &expectedSha1 = QString(), const QString &expectedMd5 = QString());
    void cancel();
    bool isCancelRequested() const
    {
        return m_cancelRequested.load();
    }

signals:
    void progress(int percent, double speedMBs, qint64 processedBytes, qint64 totalBytes);
    void finished(bool matched, const QString &calculatedSha1, const QString &expectedSha1, const QString &calculatedMd5, const QString &errorStr);

protected:
    void run() override;

private:
    QString m_filePath;
    QString m_expectedSha1;
    QString m_expectedMd5;
    std::atomic<bool> m_cancelRequested {false};
};

class IpswDownloadWorker : public QObject
{
    Q_OBJECT

public:
    explicit IpswDownloadWorker(QObject *parent = nullptr);
    ~IpswDownloadWorker() override;

    void startDownload(const QString &url, const QString &destPath);
    void cancelDownload();

signals:
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal, double speedMBs);
    void downloadFinished(bool success, const QString &filePath, const QString &errorStr);

private slots:
    void onReadyRead();
    void onFinished();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);

private:
    QNetworkAccessManager *m_nam = nullptr;
    QNetworkReply *m_reply = nullptr;
    QFile *m_outputFile = nullptr;
    QString m_destPath;
    qint64 m_lastBytes = 0;
    qint64 m_lastTime = 0;
};

class AppleRestoreWorker : public QObject
{
    Q_OBJECT

public:
    explicit AppleRestoreWorker(QObject *parent = nullptr);
    ~AppleRestoreWorker() override;

    void startRestore(const QString &ipswPath, bool retainUserData, const QString &udid = QString());
    void abortRestore();

signals:
    void progressChanged(int percent, const QString &stageText);
    void logReceived(const QString &line);
    void restoreFinished(bool success, const QString &errorMsg);

private slots:
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    void parseLogLine(const QString &line);

    QProcess *m_process = nullptr;
    bool m_aborted = false;
};

class AppleIpswWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AppleIpswWidget(QWidget *parent = nullptr);
    ~AppleIpswWidget() override;

    void refreshDevice();
    void setDevice(const AppleDevice &device);
    const AppleDevice &device() const
    {
        return m_device;
    }
    QString downloadDirectory() const;
    void resetSession();
    void showBetaDisclaimer();

public slots:
    void scanDevices();
    void onExitRecoveryClicked();
    void onEnterRecoveryClicked();
    void onRebootClicked();
    void onSelectLocalIpswClicked();
    void onStartFlashClicked();
    void onCancelFlashClicked();
    void onModelSelected(int index);
    void onOpenFolderClicked();
    void onChangeFolderClicked();
    void scanLocalFirmwares();
    void onRefreshCatalogClicked();
    void onFilterCatalogChanged();
    void onClearLogClicked();
    void onCopyLogClicked();
    void onStartChecksumVerification();
    void onCancelChecksumVerification();

    // Wizard navigation slots
    void onSwitchMode(int mode);
    void onSimpleStep1Next();
    void onSimpleStep2Back();
    void onSimpleStep2Start();
    void onSimpleRetry();
    void onAdvStep1Next();
    void onAdvStep2Back();
    void onAdvStep2Next();
    void onAdvRetry();

private slots:
    void onFirmwareSelected(int row, int column);
    void onDownloadIpswClicked(int row);
    void onDownloadProgress(qint64 received, qint64 total, double speedMBs);
    void onDownloadFinished(bool success, const QString &path, const QString &errorStr);
    void onRestoreProgress(int percent, const QString &stageText);
    void onRestoreLog(const QString &line);
    void onRestoreFinished(bool success, const QString &errorStr);
    void onChecksumProgress(int percent, double speedMBs, qint64 processedBytes, qint64 totalBytes);
    void onChecksumFinished(bool matched, const QString &calcSha1, const QString &expSha1, const QString &calcMd5, const QString &errorStr);

private:
    void setupUi();
    void setupSimpleWizardPage();
    void setupAdvancedWizardPage();
    void updateDeviceCard();
    void loadDeviceList();
    void fetchFirmwareCatalog(const QString &modelId);
    void populateFirmwareTable();
    void populateLocalFirmwareTable();
    void updateDiskSpaceInfo();
    void updateSimpleWizardStep1();
    void updateSimpleWizardStep2();
    void updateSimpleStepPills(int step);
    void updateAdvStepPills(int step);
    void appendLog(const QString &line, const QString &color = "#E0E0E0");
    void startChecksumForFile(const QString &filePath, const QString &expectedSha1 = QString(), const QString &expectedMd5 = QString());
    void updateChecksumUi();

    AppleDevice m_device;
    QList<IpswFirmwareInfo> m_firmwares;
    QString m_selectedIpswPath;
    IpswFirmwareInfo m_selectedFirmware;
    bool m_isLocalIpsw = false;
    QString m_customDownloadDir;
    QString m_currentSelectedModelId;

    QTimer *m_detectTimer = nullptr;
    QNetworkAccessManager *m_netManager = nullptr;
    IpswDownloadWorker *m_downloader = nullptr;
    AppleRestoreWorker *m_restoreWorker = nullptr;
    IpswChecksumWorker *m_checksumWorker = nullptr;

    // Checksum state
    ChecksumStatus m_checksumStatus = ChecksumStatus::NotChecked;
    QString m_currentCalculatedSha1;
    QString m_currentExpectedSha1;
    QString m_currentCalculatedMd5;
    QString m_checksumErrorStr;

    // Top Header & Mode Switcher
    QPushButton *m_btnModeSimple = nullptr;
    QPushButton *m_btnModeAdvanced = nullptr;
    QLabel *m_lblHeaderDevicePill = nullptr;
    QStackedWidget *m_modeStack = nullptr;

    // ---------------------------------------------------------
    // 1. SIMPLE MODE (Быстрый пошаговый мастер)
    // ---------------------------------------------------------
    QStackedWidget *m_simpleStepStack = nullptr;
    QLabel *m_lblSimpleWizardStepHeader = nullptr;
    QLabel *m_lblSimplePill1 = nullptr;
    QLabel *m_lblSimplePill2 = nullptr;
    QLabel *m_lblSimplePill3 = nullptr;

    // Simple Step 1: USB Connection
    QLabel *m_lblSimpleUsbStatus = nullptr;
    QLabel *m_lblSimpleDetectedModel = nullptr;
    QWidget *m_simpleConnectedDeviceCard = nullptr;
    QLabel *m_lblSimpleCardDeviceTitle = nullptr;
    QLabel *m_lblSimpleCardDeviceMode = nullptr;
    QLabel *m_lblSimpleCardModel = nullptr;
    QLabel *m_lblSimpleCardIos = nullptr;
    QLabel *m_lblSimpleCardSerial = nullptr;
    QLabel *m_lblSimpleCardEcid = nullptr;
    QPushButton *m_btnSimpleStep1Next = nullptr;

    // Simple Step 2: Auto settings & Reset mode selection
    QLabel *m_lblSimpleDeviceSummary = nullptr;
    QLabel *m_lblSimpleAutoIpswInfo = nullptr;
    QRadioButton *m_rbSimpleRetain = nullptr;
    QRadioButton *m_rbSimpleErase = nullptr;
    QButtonGroup *m_simpleResetGroup = nullptr;
    QPushButton *m_btnSimpleStep2Back = nullptr;
    QPushButton *m_btnSimpleStep2Start = nullptr;

    // Simple Checksum Card
    QWidget *m_simpleChecksumCard = nullptr;
    QLabel *m_lblSimpleChecksumStatus = nullptr;
    QProgressBar *m_simpleChecksumProgress = nullptr;
    QPushButton *m_btnSimpleVerifyChecksum = nullptr;
    QPushButton *m_btnSimpleCancelChecksum = nullptr;

    // Simple Step 3: Flash progress & result
    QProgressBar *m_simpleProgressBar = nullptr;
    QLabel *m_lblSimpleProgressStatus = nullptr;
    QPushButton *m_btnSimpleCancel = nullptr;
    QPushButton *m_btnSimpleRetry = nullptr;

    // ---------------------------------------------------------
    // 2. ADVANCED MODE (Расширенный пошаговый экспертный режим)
    // ---------------------------------------------------------
    QStackedWidget *m_advancedStepStack = nullptr;
    QLabel *m_lblAdvStepHeader = nullptr;
    QLabel *m_lblAdvPill1 = nullptr;
    QLabel *m_lblAdvPill2 = nullptr;
    QLabel *m_lblAdvPill3 = nullptr;

    // Advanced Step 1: Catalog, Download manager & Storage
    QTabWidget *m_tabWidget = nullptr;
    QComboBox *m_comboDeviceModel = nullptr;
    QLineEdit *m_editCatalogFilter = nullptr;
    QCheckBox *m_chkOnlySigned = nullptr;
    QPushButton *m_btnRefreshCatalog = nullptr;
    QTableWidget *m_firmwareTable = nullptr;
    QPushButton *m_btnSelectLocal = nullptr;
    QLabel *m_lblSelectedIpsw = nullptr;
    QTableWidget *m_localFirmwareTable = nullptr;
    QLabel *m_lblStoragePath = nullptr;
    QLabel *m_lblDiskSpace = nullptr;
    QPushButton *m_btnOpenFolder = nullptr;
    QPushButton *m_btnChangeFolder = nullptr;
    QPushButton *m_btnRescanLocal = nullptr;
    QPushButton *m_btnAdvStep1Next = nullptr;

    // Advanced Checksum Card (Step 1)
    QWidget *m_advChecksumCard = nullptr;
    QLabel *m_lblAdvChecksumStatus = nullptr;
    QProgressBar *m_advChecksumProgress = nullptr;
    QPushButton *m_btnAdvVerifyChecksum = nullptr;
    QPushButton *m_btnAdvCancelChecksum = nullptr;

    // Advanced Step 2: Fine-tuning reset mode & parameters
    QRadioButton *m_rbRetainData = nullptr;
    QRadioButton *m_rbCleanRestore = nullptr;
    QButtonGroup *m_flashModeGroup = nullptr;
    QLabel *m_lblAdvStep2FirmwareSummary = nullptr;
    QPushButton *m_btnAdvStep2Back = nullptr;
    QPushButton *m_btnAdvStep2Next = nullptr;

    // Advanced Step 3: Device check, Flashing & Terminal logs
    QWidget *m_deviceCardWidget = nullptr;
    QLabel *m_lblDeviceIcon = nullptr;
    QLabel *m_lblDeviceName = nullptr;
    QLabel *m_lblDeviceMode = nullptr;
    QLabel *m_lblIosVersion = nullptr;
    QLabel *m_lblSerial = nullptr;
    QLabel *m_lblEcid = nullptr;
    QLabel *m_lblUdid = nullptr;
    QPushButton *m_btnExitRecovery = nullptr;
    QPushButton *m_btnEnterRecovery = nullptr;
    QPushButton *m_btnReboot = nullptr;
    QPushButton *m_btnRefreshDevice = nullptr;
    QPushButton *m_btnAdvStep3Back = nullptr;
    QPushButton *m_btnStartFlash = nullptr;
    QPushButton *m_btnCancelFlash = nullptr;
    QPushButton *m_btnAdvRetry = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QLabel *m_lblProgressStatus = nullptr;
    QLabel *m_lblAdvStep3ChecksumBadge = nullptr;
    QTextEdit *m_logTerminal = nullptr;
};
