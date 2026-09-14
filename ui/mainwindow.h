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
#include <QPushButton>
#include <QIcon>

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

class AdbDeviceVisualizer;
class FileManagerWidget;
class ApkManagerWidget;
class FileManagerService;
class ApkManagerService;
class ContactFixerWidget;
class ContactFixerService;
class AITranslaterWidget;
class AITranslaterService;
class RadioPlayerWidget;
class QToolBar;
class QEnterEvent;
class QButtonGroup;

class ServiceTileButton : public QPushButton
{
    Q_OBJECT

public:
    enum class Tier
    {
        Vip,
        Free,
        Credit,
        Dynamic,
        Disabled
    };

    explicit ServiceTileButton(const QIcon &icon,
                               const QString &title,
                               const QString &badgeText,
                               Tier tier,
                               bool showRibbon = true,
                               const QString &ribbonText = QString::fromUtf8("NEW"),
                               QWidget *parent = nullptr);

    void setTier(Tier tier);
    Tier tier() const { return m_tier; }

    void setTitle(const QString &title);
    QString title() const { return m_title; }

    void setBadgeText(const QString &text);
    QString badgeText() const { return m_badgeText; }

    void setShowRibbon(bool show);
    bool showRibbon() const { return m_showRibbon; }

    void setRibbonText(const QString &text);
    QString ribbonText() const { return m_ribbonText; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent *event) override;
#else
    void enterEvent(QEvent *event) override;
#endif
    void leaveEvent(QEvent *event) override;

private:
    QString m_title;
    QString m_badgeText;
    Tier m_tier;
    bool m_showRibbon;
    QString m_ribbonText;
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

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

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

#ifdef NDEBUG
    int verChansesAvailable = ChansesRunInvalid;
#endif

    QList<std::shared_ptr<Service>> services {};
    std::shared_ptr<QList<ServiceItemInfo>> serverServices {};

    bool accessUi_page_longinfo(QListView *&processLogStatusV, QLabel *&malareStatusText0V, QLabel *&deviceLabelNameV, QProgressBar *&processBarStatusV, QPushButton *&pushButtonReRun);
    bool accessUi_page_devices(QTableView *&tableActual, QDateEdit *&dateEditStart, QDateEdit *&dateEditEnd, QPushButton *&refreshButton, QCheckBox *&quaranteeFilter);
    bool accessUi_page_buyvip(QComboBox *&listVariants, QLabel *&balanceText, QLabel *&infoAfterPeriod, QPushButton *&buyButton);

    AdbDevice currentAdbDevice() const;
    QWidget *pageWidget(PageIndex page) const;

    static MainWindow *current;
    AdbDeviceVisualizer *adbVisualizer = nullptr;
    QToolBar *radioToolBar = nullptr;
    RadioPlayerWidget *radioPlayer = nullptr;

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

    void on_butShowPass_clicked();

public slots:
    void setThemeAction();
    void updateCabinet();
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

    void setupWindowLayoutAndAnim();
    void setupAiPanel();
    void setupRadioPlayer();
    void setupPagesDesign();
    void initServiceModules();
    void applyServiceFilters();
    void createAppleServiceButton();
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
