#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QObject>
#include <QTimer>
#include <QList>
#include <QMap>
#include <QString>
#include <QApplication>
#include <QComboBox>
#include <QDateEdit>
#include <QLabel>
#include <QListView>
#include <QProgressBar>
#include <QTableView>
#include <QPushButton>
#include <QCheckBox>
#include <QIcon>

#include "Services.h"
#include "adbfront.h"
#include "network.h"
#include "AppSystemTray.h"
#include "ProgressCircle.h"

enum ThemeScheme
{
    System,
    Light,
    Dark
};
enum
{
    VersionCheckRate = 10000,
    ChansesRunInvalid = 3
};

class MainWindow : public QObject
{
    Q_OBJECT

    friend class AdsKillerService;
    friend class ServiceProvider;
    friend class BuyVIPService;
    friend class MyDeviceService;

    // QML Properties
    Q_PROPERTY(QString statusAuthText READ statusAuthText WRITE setStatusAuthText NOTIFY statusAuthTextChanged)
    Q_PROPERTY(QString loginName READ loginName NOTIFY authInfoChanged)
    Q_PROPERTY(int credits READ credits NOTIFY authInfoChanged)
    Q_PROPERTY(int vipDays READ vipDays NOTIFY authInfoChanged)
    Q_PROPERTY(QString location READ location NOTIFY authInfoChanged)
    Q_PROPERTY(QString currencyType READ currencyType NOTIFY authInfoChanged)
    Q_PROPERTY(bool blocked READ blocked NOTIFY authInfoChanged)
    Q_PROPERTY(int connectedDevices READ connectedDevices NOTIFY authInfoChanged)
    Q_PROPERTY(int basePrice READ basePrice NOTIFY authInfoChanged)
    Q_PROPERTY(QDateTime lastLogin READ lastLogin NOTIFY authInfoChanged)
    Q_PROPERTY(QDateTime serverLastTime READ serverLastTime NOTIFY authInfoChanged)

    Q_PROPERTY(QVariantList serviceList READ serviceList NOTIFY servicesChanged)
    Q_PROPERTY(bool isNetworkPending READ isNetworkPending NOTIFY networkPendingChanged)
    Q_PROPERTY(QObject *activeService READ activeService NOTIFY activeServiceChanged)
    Q_PROPERTY(QString savedLogin READ savedLogin CONSTANT)
    Q_PROPERTY(QString savedPassword READ savedPassword CONSTANT)
    Q_PROPERTY(QString textColor READ textColor CONSTANT)
    Q_PROPERTY(QString textSecondary READ textSecondary CONSTANT)
    Q_PROPERTY(QString cardColor READ cardColor CONSTANT)
    Q_PROPERTY(bool explicitLogout READ explicitLogout WRITE setExplicitLogout NOTIFY explicitLogoutChanged)

public:
    explicit MainWindow(QObject *parent = nullptr);
    ~MainWindow();

    Network network;
    QTimer *timerAuthAnim;
    QApplication *app;
    AdsAppSystemTray *tray;

    QList<std::shared_ptr<Service>> services {};
    std::shared_ptr<QList<ServiceItemInfo>> serverServices {};

    static MainWindow *current;

    void delayUI(int ms);
    void delayUICallLoop(int ms, std::function<bool()> callFalseEnd);
    void delayUICall(int ms, std::function<void()> call);

    void updateCabinet();
    Q_INVOKABLE void logoutSystem();
    Q_INVOKABLE void refreshServices();
    void showMessageFromStatus(int statusCode);

    // Dummy methods for AppSystemTray
    bool isHidden() const
    {
        return false;
    }
    void show()
    {
    }
    void hide()
    {
    }
    ThemeScheme getTheme() const
    {
        return System;
    }
    void setTheme(ThemeScheme t)
    {
    }
    QIcon windowIcon() const
    {
        return QIcon(":/resources/app-logo");
    }

    struct
    {
        bool isAuthed;
        AdbDevice adbDevice;
        DeviceConnectType connectionType;
    } connectPhone;

    // QML Getters/Setters
    QString statusAuthText() const
    {
        return m_statusAuthText;
    }
    void setStatusAuthText(const QString &t)
    {
        if(m_statusAuthText != t)
        {
            m_statusAuthText = t;
            emit statusAuthTextChanged();
        }
    }
    QString loginName() const
    {
        return network.authedId.idName;
    }
    int credits() const
    {
        return network.authedId.credits;
    }
    int vipDays() const
    {
        return network.authedId.vipDays;
    }
    QString location() const
    {
        return network.authedId.location;
    }
    QString currencyType() const
    {
        return network.authedId.currencyType;
    }
    bool blocked() const
    {
        return network.authedId.blocked;
    }
    int connectedDevices() const
    {
        return network.authedId.connectedDevices;
    }
    int basePrice() const
    {
        return network.authedId.basePrice;
    }
    QDateTime lastLogin() const
    {
        return network.authedId.lastLogin;
    }
    QDateTime serverLastTime() const
    {
        return network.authedId.serverLastTime;
    }

    bool isNetworkPending() const
    {
        return network.pending();
    }
    QVariantList serviceList() const;
    QObject *activeService() const;

    Q_INVOKABLE QVariantList getAdbDevices() const;
    Q_INVOKABLE void selectAdbDevice(const QString &devId);
    Q_INVOKABLE void closeService();

    QString savedLogin() const
    {
        return m_savedLogin;
    }
    QString savedPassword() const
    {
        return m_savedPassword;
    }
    QString textColor() const
    {
        return QStringLiteral("#FFFFFF");
    }
    QString textSecondary() const
    {
        return QStringLiteral("#B0BEC5");
    }
    QString cardColor() const
    {
        return QStringLiteral("#40000000");
    }
    bool explicitLogout() const
    {
        return m_explicitLogout;
    }
    void setExplicitLogout(bool value)
    {
        if(m_explicitLogout != value)
        {
            m_explicitLogout = value;
            emit explicitLogoutChanged();
        }
    }

    // Dummy widgets to satisfy services during migration
    ProgressCircle *malwareProgressCircle;
    ProgressCircle *loaderProgressCircle;
    QListView *processLogStatus;
    QLabel *malwareStatusText0;
    QLabel *deviceLabelName;
    QProgressBar *processBarStatus;
    QPushButton *malwareReRun;
    QTableView *myDeviceActual;
    QDateEdit *myDeviceFilterDateStart;
    QDateEdit *myDeviceFilterDateEnd;
    QPushButton *myDeviceSend;
    QCheckBox *myDeviceQuaranteeFilter;
    QComboBox *comboBoxSelectVIPDays;
    QLabel *labelVipBalance;
    QPushButton *buttonBuyVip;
    QLabel *labelInfoVip;

    bool accessUi_page_longinfo(QListView *&processLogStatusV, QLabel *&malareStatusText0V, QLabel *&deviceLabelNameV, QProgressBar *&processBarStatusV, QPushButton *&pushButtonReRun)
    {
        processLogStatusV = processLogStatus;
        malareStatusText0V = malwareStatusText0;
        deviceLabelNameV = deviceLabelName;
        processBarStatusV = processBarStatus;
        pushButtonReRun = malwareReRun;
        return true;
    }
    bool accessUi_page_devices(QTableView *&tableActual, QDateEdit *&dateEditStart, QDateEdit *&dateEditEnd, QPushButton *&refreshButton, QCheckBox *&quaranteeFilter)
    {
        tableActual = myDeviceActual;
        dateEditStart = myDeviceFilterDateStart;
        dateEditEnd = myDeviceFilterDateEnd;
        refreshButton = myDeviceSend;
        quaranteeFilter = myDeviceQuaranteeFilter;
        return true;
    }
    bool accessUi_page_buyvip(QComboBox *&listVariants, QLabel *&balanceText, QLabel *&infoAfterPeriod, QPushButton *&buyButton)
    {
        listVariants = comboBoxSelectVIPDays;
        balanceText = labelVipBalance;
        infoAfterPeriod = labelInfoVip;
        buyButton = buttonBuyVip;
        return true;
    }

public slots:
    void login(const QString &user, const QString &pass);
    void runServiceQml(int index);

signals:
    void statusAuthTextChanged();
    void authInfoChanged();
    void pageChangeRequested(int pageIndex);
    void servicesChanged();
    void openServicePage(const QString &pageName);
    void activeServiceChanged();
    void networkPendingChanged();
    void explicitLogoutChanged();
public slots:
    Q_INVOKABLE void openSupport();
    Q_INVOKABLE void openAbout();

private slots:
    void slotAuthFinish(int status, bool ok);
    void slotFetchVersionFinish(int status, const QString &version, const QString &url, bool ok);
    void slotPullServiceList(const QList<ServiceItemInfo> &services, bool ok);

private:
    QString m_statusAuthText;
    QString m_savedLogin;
    QString m_savedPassword;
    bool m_explicitLogout = false;
    int verChansesAvailable = 3;
    QTimer *versionChecker;
    PageIndex curPage = AuthPage;
    PageIndex lastPage = AuthPage;

    void checkVersion(bool firstRun);
    void willTerminate();
    void showPageLoader(PageIndex pageNum, int msWait = 1000, QString text = QString {});
    void showPageLoader(PageIndex pageNum, int msWait, std::function<bool()> predFalseEnd, QString text = QString {});
    void showPage(PageIndex pageNum);
    void clearAuthInfoPage();
    void fillAuthInfoPage();
    void initServiceModules();
};

#endif // MAINWINDOW_H
