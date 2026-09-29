#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QTextEdit>
#include <QProgressBar>
#include <QTimer>
#include <QPixmap>
#include <QIcon>
#include <QList>
#include <QMap>
#include <QThread>
#include <QMutex>

#include "CircularCleanVisualizer.h"
#include "AdbFront.h"

class RunningAppIconsStrip : public QWidget
{
    Q_OBJECT

public:
    explicit RunningAppIconsStrip(QWidget *parent = nullptr);
    ~RunningAppIconsStrip() override;

    void addAppIcon(const QString &pkgName, const QPixmap &icon);
    void setActivePackage(const QString &pkgName);
    void startRunning();
    void stopRunning();
    void clear();

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onTick();

private:
    struct ConveyorItem
    {
        QString pkgName;
        QPixmap icon;
        qreal x = 0.0;
    };

    QList<ConveyorItem> m_items;
    QMap<QString, QPixmap> m_iconMap;
    QString m_activePackage;
    QTimer *m_timer = nullptr;
    bool m_running = false;
    qreal m_scrollOffset = 0.0;
};

// ============================================================================
// StorageCacheCleanWidget
// Dedicated Metro UI Page Widget for Fast Cache & Thumbnails Cleaning
// ============================================================================
class StorageCacheCleanWidget : public QWidget
{
    Q_OBJECT

public:
    explicit StorageCacheCleanWidget(QWidget *parent = nullptr);
    ~StorageCacheCleanWidget() override;

    void setDevice(const AdbDevice &device);
    void startClean();
    void stopClean();
    void resetSession();
    bool isCleaning() const;

private slots:
    void onStartClicked();
    void onStopClicked();
    void onWorkerProgress(int percent, const QString &appName, const QString &pkgName, const QPixmap &icon, const QString &statusMsg);
    void onWorkerLog(const QString &message, int level);
    void onWorkerStatsUpdated(qint64 cacheBytes, int thumbnailsCount, qint64 thumbnailsBytes, int stoppedApps);
    void onWorkerFinished(bool success, const QString &summary);

private:
    void setupUi();
    void updateDeviceHeader();
    QString formatBytes(qint64 bytes) const;

    AdbDevice m_device;
    AdbFileIO m_fileIO;

    // Visualizers
    CircularCleanVisualizer *m_visualizer = nullptr;
    RunningAppIconsStrip *m_iconsStrip = nullptr;

    // Device capsule & header
    QLabel *m_lblDeviceTitle = nullptr;
    QLabel *m_lblDeviceBadge = nullptr;
    QLabel *m_lblStatusState = nullptr;

    // Options checkboxes
    QCheckBox *m_chkIconShown = nullptr;
    QCheckBox *m_chkAppCache = nullptr;
    QCheckBox *m_chkThumbnails = nullptr;
    QCheckBox *m_chkStopApps = nullptr;
    QCheckBox *m_chkTempFiles = nullptr;

    // Action buttons
    QPushButton *m_btnStart = nullptr;
    QPushButton *m_btnStop = nullptr;

    // Stats tiles
    QLabel *m_valAppCacheFreed = nullptr;
    QLabel *m_valThumbnails = nullptr;
    QLabel *m_valAppsStopped = nullptr;
    QLabel *m_valTotalFreed = nullptr;

    // Log viewer
    QTextEdit *m_logConsole = nullptr;

    // Threading & state
    bool m_isCleaning = false;
    QThread *m_workerThread = nullptr;
    class StorageCacheCleanWorker *m_worker = nullptr;

    qint64 m_totalFreedBytes = 0;
    qint64 m_cacheFreedBytes = 0;
    qint64 m_thumbnailsFreedBytes = 0;
    int m_thumbnailsCount = 0;
    int m_appsStoppedCount = 0;
};
