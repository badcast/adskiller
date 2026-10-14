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
#include <QVariantAnimation>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QSpacerItem>
#include <QTableView>
#include <QVersionNumber>
#include <QWidget>
#include <QPushButton>
#include <QIcon>
#include <QTimer>
#include <QDialog>

#include "ProgressCircle.h"

#include "AppSystemTray.h"
#include "Services.h"
#include "ServiceTileButton.h"
#include "Snowflake.h"
#include "AdbFront.h"
#include "AppleFront.h"
#include "Begin.h"
#include "Extension.h"
#include "Network.h"

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

class DeviceVisualizer;
class Analytics;
class FileManagerWidget;
class ApkManagerWidget;
class FileManagerService;
class ApkManagerService;
class ContactFixerWidget;
class ContactFixerService;
class AITranslaterWidget;
class AITranslaterService;
class AppleIpswWidget;
class AppleIpswService;
class StorageCacheCleanWidget;
class StorageCacheCleanService;
class ImeiVerificationKzWidget;
class ImeiVerificationKzService;
class MiAccountWidget;
class SamsungFrpWidget;
class SamsungFrpService;
class PixelBlastSideButton;
class PixelBlastGameService;
class RadioPlayerWidget;
class CyberReactorLoader;
class QToolBar;
class QEnterEvent;
class QButtonGroup;

class ServiceInfoDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ServiceInfoDialog(const ServiceTileButton::Details &details, QWidget *parent = nullptr);

private slots:
    void onAskAiClicked();

private:
    ServiceTileButton::Details m_details;
};

class ServiceShockwaveOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit ServiceShockwaveOverlay(QWidget *parent = nullptr);
    void trigger(const QPoint &centerInWindow, const QColor &accentColor);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private:
    QPoint m_center;
    QColor m_accentColor {0, 229, 255};
    qreal m_progress = 0.0;
    QVariantAnimation *m_anim = nullptr;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

    friend class AdsKillerService;
    friend class BoostRamService;
    friend class ServiceProvider;
    friend class FileManagerWidget;
    friend class ApkManagerWidget;
    friend class FileManagerService;
    friend class ApkManagerService;
    friend class ContactFixerWidget;
    friend class ContactFixerService;
    friend class AITranslaterWidget;
    friend class AITranslaterService;
    friend class AppleIpswWidget;
    friend class AppleIpswService;
    friend class StorageCacheCleanWidget;
    friend class StorageCacheCleanService;
    friend class ImeiVerificationKzWidget;
    friend class ImeiVerificationKzService;
    friend class MiAccountWidget;
    friend class MiDeviceUnlockService;
    friend class SamsungFrpWidget;
    friend class SamsungFrpService;
    friend class PixelBlastGameService;
    friend class PBGInternal;
    friend class ServiceTileButton;
    friend class ServiceInfoDialog;
    friend class Analytics;

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void askAiQuestion(const QString &question);
    void getAnalyticsData(QString &data);
    void showMessageFromStatus(int statusCode);
    void setTheme(ThemeScheme theme);
    ThemeScheme getTheme();
    void delayUI(int ms);
    void delayUICallLoop(int ms, std::function<bool()> callFalseEnd);
    void delayUICall(int ms, std::function<void()> call);

    Network network;
    QTimer *timerAuthAnim;
    QApplication *app;
    VersionInfo runtimeVersion;
    VersionInfo actualVersion;
    AdsAppSystemTray *tray;
    bool isShownedVipExp = false;

#ifdef NDEBUG
    int verChansesAvailable = ChansesRunInvalid;
#endif

    QList<std::shared_ptr<Service>> services {};
    std::shared_ptr<QList<ServiceItemInfo>> serverServices {};

    bool accessUi_page_longinfo(QListView *&processLogStatusV, QLabel *&malareStatusText0V, QLabel *&deviceLabelNameV, QProgressBar *&processBarStatusV, QPushButton *&pushButtonReRun);
    bool accessUi_page_devices(QTableView *&tableActual, QDateEdit *&dateEditStart, QDateEdit *&dateEditEnd, QPushButton *&refreshButton, QCheckBox *&quaranteeFilter);
    bool accessUi_page_buyvip(QComboBox *&listVariants, QLabel *&balanceText, QLabel *&infoAfterPeriod, QPushButton *&buyButton);

    AdbDevice currentAdbDevice() const;
    AppleDevice currentAppleDevice() const;
    QWidget *pageWidget(PageIndex page) const;
    void updateDevicePageInstructions(DeviceConnectType type);

    static MainWindow *current;
    DeviceVisualizer *adbVisualizer = nullptr;
    QToolBar *radioToolBar = nullptr;
    RadioPlayerWidget *radioPlayer = nullptr;

protected:
private slots:
    void on_actionAboutUs_triggered();
    void on_actionUsLic_triggered();
    void on_action_WhatsApp_triggered();
    void on_action_Qt_triggered();
    void on_authButton_clicked();
    void slotAuthFinish(int status, bool ok);
    void slotPullServiceList(const QList<ServiceItemInfo> &services, bool ok);
    void slotFetchVersionFinish(int status, const QString &version, const QString &url, bool ok);
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

    void on_butShowPass_clicked();

public slots:
    void setThemeAction();
    void updateCabinet();
    void logoutSystem();

private:
    Ui::MainWindow *ui;
    ProgressCircle *malwareProgressCircle;
    ProgressCircle *loaderProgressCircle;
    CyberReactorLoader *cyberReactorLoader = nullptr;
    QList<QWidget *> malwareStatusLayouts;
    QMap<PageIndex, QWidget *> pages;
    QWidget *vPageSpacer;
    QPropertyAnimation *vPageSpacerAnimator;
    QPropertyAnimation *contentOpacityAnimator;
    QPropertyAnimation *deviceLeftAnimator;
    Analytics *mAnalytics = nullptr;
    PageIndex startPage = PageIndex::AuthPage;
    PageIndex curPage = startPage;
    PageIndex lastPage = startPage;
    QTimer *versionChecker;
    Snowflake *snows;
    bool deviceSelectSwitched;
    PixelBlastSideButton *m_pixelBlastSideButton = nullptr;
    std::shared_ptr<PixelBlastGameService> m_pixelBlastService;

    void showPageLoader(PageIndex pageNum, int msWait, std::function<bool()> predFalseEnd, QString text = QString {});

    inline void showPageLoader(PageIndex pageNum, int msWait = 1000, QString text = QString {})
    {
        if(text.isEmpty())
            text = "Ожидайте";

        std::function<bool()> everything_true = []() -> bool { return true; };
        showPageLoader(pageNum, msWait, everything_true, text);
    }

    void showPage(PageIndex pageNum);
    void pageShownPreStart(int page);
    void runService(std::shared_ptr<Service> service);
    void closeService(std::shared_ptr<Service> service);

    void clearAuthInfoPage();
    void fillAuthInfoPage();
    void updateCabinetUserCard();

    void setupWindowLayoutAndAnim();
    void setupAiPanel();
    void setupRadioPlayer();
    void setupPagesDesign();
    void initServiceModules();
    void applyServiceFilters();
    void checkVersion(bool firstRun);
    void willTerminate();

    ServiceShockwaveOverlay *m_shockwaveOverlay = nullptr;
    bool m_isServiceLaunching {false};
    void showWindowShockwave(const QPoint &globalPos, const QColor &color);

    struct
    {
        bool isAuthed;
        AdbDevice adbDevice;
        AppleDevice appleDevice;
        DeviceConnectType connectionType;
    } connectPhone;
};
#endif // MAINWINDOW_H
