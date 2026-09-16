#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QTabWidget>
#include <QTextEdit>
#include <QMap>
#include <QIcon>
#include <QSet>
#include "adbfront.h"

struct AppPackageInfo
{
    QString packageName;
    QString appName;
    QString apkPath;
    qint64 apkSize = 0;
    bool isSystem = false;
    bool isDisabled = false;
    QString versionName = "N/a";
    QIcon icon;
};

struct AppDetails
{
    QString packageName;
    QString appName;
    QString versionName = "N/a";
    QString versionCode = "N/a";
    QString minSdk = "N/a";
    QString targetSdk = "N/a";
    QString codePath = "N/a";
    qint64 apkSize = 0;
    QString dataDir = "N/a";
    QString installer = "N/a";
    QString firstInstallTime = "N/a";
    QString lastUpdateTime = "N/a";
    QString primaryCpuAbi = "N/a";
    QString mainActivity = "N/a";
    QString signatures = "N/a";
    bool isSystem = false;
    bool isDisabled = false;
    QStringList requestedPermissions;
    QSet<QString> grantedPermissions;
    QStringList activities;
    QStringList services;
    QStringList receivers;
    QStringList providers;
    QString rawDumpsys;
    QIcon icon;
};

class ApkManagerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ApkManagerWidget(QWidget *parent = nullptr);
    ~ApkManagerWidget() override = default;

    void setDevice(const AdbDevice &device);
    void loadPackages();
    void resetSession();

private slots:
    void populateTable();
    void onSearchOrFilterChanged();
    void onPackageSelected();
    void filterPermissions(const QString &text);
    void launchApp();
    void forceStopApp();
    void toggleFreezeApp();
    void clearAppData();
    void exportSelectedApk();
    void installApk();
    void uninstallApp();
    void copyPackageId();
    void copyRawDumpsys();

private:
    void setupUi();
    void updateButtonsEnabled(bool enabled);
    void clearInspector();
    void inspectPackage(AppPackageInfo &pkgInfo, int tableRow);
    void fillOverviewTab(const AppDetails &d);
    void fillPermissionsTab(const AppDetails &d);
    void fillComponentsTab(const AppDetails &d);

    AdbDevice m_device;
    AdbShell m_shell;
    QList<AppPackageInfo> m_allPackages;
    QMap<QString, QIcon> m_iconCache;
    AppDetails m_currentDetails;

    QLineEdit *m_searchEdit = nullptr;
    QComboBox *m_filterCombo = nullptr;
    QTableWidget *m_table = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_statCountLabel = nullptr;

    QLabel *m_appIconLabel = nullptr;
    QLabel *m_appNameLabel = nullptr;
    QLabel *m_packageIdLabel = nullptr;
    QLabel *m_typeBadge = nullptr;
    QLabel *m_statusBadge = nullptr;
    QLabel *m_installerBadge = nullptr;

    QPushButton *m_btnLaunch = nullptr;
    QPushButton *m_btnStop = nullptr;
    QPushButton *m_btnToggleFreeze = nullptr;
    QPushButton *m_btnClearData = nullptr;
    QPushButton *m_btnExportApk = nullptr;
    QPushButton *m_btnUninstall = nullptr;

    QTabWidget *m_tabs = nullptr;
    QTableWidget *m_overviewTable = nullptr;
    QLineEdit *m_permSearchEdit = nullptr;
    QLabel *m_permStatLabel = nullptr;
    QTableWidget *m_permTable = nullptr;
    QTableWidget *m_compTable = nullptr;
    QTextEdit *m_dumpsysEdit = nullptr;
};
