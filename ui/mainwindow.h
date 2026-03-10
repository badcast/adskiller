#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <functional>
#include <memory>

#include <QComboBox>
#include <QDateEdit>
#include <QLabel>
#include <QList>
#include <QListView>
#include <QMainWindow>
#include <QMap>
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QSettings>
#include <QSpacerItem>
#include <QTableView>
#include <QVersionNumber>
#include <QWidget>

#include "ProgressCircle.h"

#include "AppSystemTray.h"
#include "Services.h"
#include "Snowflake.h"
#include "adbfront.h"
#include "begin.h"
#include "extension.h"
#include "network.h"

enum
{
    VersionCheckRate = 10000,
    ChansesRunInvalid = 3
};

enum ThemeScheme
{
    System,
    Light,
    Dark
};

QT_BEGIN_NAMESPACE
namespace Ui
{
    class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

    friend class AdsKillerService;
    friend class ServiceProvider;

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void showMessageFromStatus(int statusCode);
    void setTheme(ThemeScheme theme);
    ThemeScheme getTheme();
    void delayUI(int ms);
    void delayUICallLoop(int ms, std::function<bool()> call);
    void delayUICall(int ms, std::function<void()> call);

    Network network;
    QTimer *timerAuthAnim;
    QApplication *app;
    VersionInfo runtimeVersion;
    VersionInfo actualVersion;
    AdsAppSystemTray *tray;

#ifdef NDEBUG
    int verChansesAvailable = ChansesRunInvalid;
#endif

    QList<std::shared_ptr<Service>> services {};
    std::shared_ptr<QList<ServiceItemInfo>> serverServices {};

    bool accessUi_page_longinfo(QListView *&processLogStatusV, QLabel *&malareStatusText0V, QLabel *&deviceLabelNameV, QProgressBar *&processBarStatusV, QPushButton *&pushButtonReRun);
    bool accessUi_page_devices(QTableView *&tableActual, QDateEdit *&dateEditStart, QDateEdit *&dateEditEnd, QPushButton *&refreshButton, QCheckBox *&quaranteeFilter);
    bool accessUi_page_buyvip(QComboBox *&listVariants, QLabel *& balanceText, QLabel *&infoAfterPeriod, QPushButton *& buyButton);

    static MainWindow *current;

private slots:
    void on_actionAboutUs_triggered();
    void on_action_WhatsApp_triggered();
    void on_action_Qt_triggered();
    void on_authButton_clicked();
    void slotAuthFinish(int status, bool ok);
    void slotPullServiceList(const QList<ServiceItemInfo> &services, bool ok);
    void slotFetchVersionFinish(int status, const QString &version, const QString &url, bool ok);
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

public slots:
    void setThemeAction();
    void updateCabinet(bool newAuthenticate = true);
    void logoutSystem();

private:
    Ui::MainWindow *ui;
    ProgressCircle *malwareProgressCircle;
    ProgressCircle *loaderProgressCircle;
    QList<QWidget *> malwareStatusLayouts;
    QMap<PageIndex, QWidget *> pages;
    QWidget *vPageSpacer;
    QPropertyAnimation *vPageSpacerAnimator;
    QPropertyAnimation *contentOpacityAnimator;
    QPropertyAnimation *deviceLeftAnimator;
    PageIndex startPage = PageIndex::AuthPage;
    PageIndex curPage = startPage;
    PageIndex lastPage = startPage;
    QTimer *versionChecker;
    Snowflake *snows;
    bool deviceSelectSwitched;

    void showPageLoader(PageIndex pageNum, int msWait, std::function<bool()> pred, QString text = QString{});

    inline void showPageLoader(PageIndex pageNum, int msWait = 1000,  QString text = QString {})
    {
        if(text.isEmpty())
            text = "Ожидайте";

        std::function<bool()> func = []() -> bool { return true; };
        showPageLoader(pageNum, msWait, func, text);
    }

    void showPage(PageIndex pageNum);
    void pageShownPreStart(int page);
    void runService(std::shared_ptr<Service> service);

    void clearAuthInfoPage();
    void fillAuthInfoPage();

    void initServiceModules();
    void checkVersion(bool firstRun);
    void willTerminate();

    struct
    {
        bool isAuthed;
        AdbDevice adbDevice;
        DeviceConnectType connectionType;
    } connectPhone;
};
#endif // MAINWINDOW_H
