#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <functional>
#include <memory>

#include <QWidget>
#include <QMainWindow>
#include <QList>
#include <QMap>
#include <QComboBox>
#include <QSettings>
#include <QVersionNumber>
#include <QSpacerItem>
#include <QPropertyAnimation>
#include <QListView>
#include <QLabel>
#include <QProgressBar>

#include "ProgressCircle.h"

#include "begin.h"
#include "adbfront.h"
#include "network.h"
#include "SystemTray.h"
#include "extension.h"
#include "AntiMalware.h"

enum ThemeScheme
{
    System,
    Light,
    Dark
};

enum PageIndex
{
    AuthPage = 0,
    LoaderPage = 1,
    CabinetPage = 2,
    MalwarePage,
    DevicesPage,

    LengthPages
};

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

struct ServiceItem
{
    bool active;
    QString title;
    QWidget * buttonWidget;
    std::shared_ptr<Service> handler;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

private:
    friend class AdsKillerService;

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void showMessageFromStatus(int statusCode);
    void setTheme(ThemeScheme theme);
    ThemeScheme getTheme();
    void delayTimer(int ms);
    void delayPushLoop(int ms, std::function<bool ()> call);
    void delayPush(int ms, std::function<void ()> call);
    void startDeviceConnect(DeviceConnectType targetType, std::shared_ptr<ServiceItem> service);

    Network network;
    QTimer *timerAuthAnim;
    QApplication *app;
    VersionInfo selfVersion;
    VersionInfo actualVersion;
    AdsAppSystemTray * tray;

    std::shared_ptr<ServiceItem> currentService;
    QList<std::shared_ptr<ServiceItem>> services;

    bool accessUi_adskiller(QListView *& processLogStatusV, QLabel *& malareStatusText0V, QLabel *& deviceLabelNameV, QProgressBar *&processBarStatusV);

    static MainWindow * current;

private slots:
    void on_actionAboutUs_triggered();
    void on_action_WhatsApp_triggered();
    void on_action_Qt_triggered();
    void on_authButton_clicked();
    void replyAuthFinish(int status, bool ok);
    void replyAdsData(const QStringList& adsList, int status, bool ok);
    void replyFetchVersionFinish(int status, const QString& version, const QString& url, bool ok);

public slots:
    void setThemeAction();
    void closeEvent(QCloseEvent * event) override;

signals:
    void AdbDeviceConnected(const AdbDevice& device);

private:
    Ui::MainWindow *ui;
    QSettings* settings;
    ProgressCircle* malwareProgressCircle;
    ProgressCircle* loaderProgressCircle;
    QList<QWidget*> malwareStatusLayouts;
    QMap<PageIndex,QWidget*> pages;
    QWidget * vPageSpacer;
    QPropertyAnimation * vPageSpacerAnimator;
    PageIndex curPage;
    PageIndex startPage = PageIndex::AuthPage;

    template<typename Pred>
    void showPageLoader(PageIndex pageNum, int msWait, Pred&& pred);
    void showPageLoader(PageIndex pageNum, int msWait = 1000, QString text = "");
    void showPage(PageIndex pageNum);
    void pageShown(int page);
    void clearAuthInfoPage();
    void fillAuthInfoPage();
    void initModules();
    void checkVersion();
    void runUpdateManager();
};
#endif // MAINWINDOW_H
