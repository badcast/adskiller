#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <functional>

#include <QMainWindow>
#include <QList>
#include <QMap>
#include <QComboBox>
#include <QSettings>
#include <QVersionNumber>

#include "ProgressCircle.h"

#include "begin.h"
#include "adbfront.h"
#include "network.h"
#include "SystemTray.h"

enum MalwareStatus
{
    Idle,
    Running,
    Error
};

enum ThemeScheme
{
    System,
    Light,
    Dark
};

enum PageIndex
{
    AuthPage,
    CabinetPage,
    MalwarePage,
    LoaderPage,
    LengthPages
};

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void updateAdbDevices();
    void showMessageFromStatus(int statusCode);
    void setTheme(ThemeScheme theme);
    ThemeScheme getTheme();
    void delayTimer(int ms);
    void delayPushLoop(int ms, std::function<bool ()> call);
    void delayPush(int ms, std::function<void ()> call);

    Adb adb;
    Network network;
    QTimer *timerAuthAnim;
    QApplication *app;
    VersionInfo selfVersion;
    VersionInfo actualVersion;
    AdsAppSystemTray * tray;

private slots:
    void on_actionAboutUs_triggered();

    void on_comboBoxDevices_currentIndexChanged(int index);

    void on_pushButton_2_clicked();

    void on_action_WhatsApp_triggered();

    void on_action_Qt_triggered();

    void on_authButton_clicked();

    void on_deviceChanged(const AdbDevice& device, AdbConState state);

    void on_buttonDecayMalware_clicked();

    void replyAuthFinish(int status, bool ok);

    void replyAdsData(const QStringList& adsList, int status, bool ok);

    void replyFetchVersionFinish(int status, const QString& version, const QString& url, bool ok);

public slots:
    void setThemeAction();

    void closeEvent(QCloseEvent * event) override;

private:
    Ui::MainWindow *ui;
    QTimer * malwareUpdateTimer;
    QSettings* settings;
    ProgressCircle* malwareProgressCircle;
    ProgressCircle* loaderProgressCircle;
    QList<QWidget*> malwareStatusLayouts;
    QMap<PageIndex,QWidget*> pages;
    PageIndex curPage;
    PageIndex startPage = PageIndex::AuthPage;

    template<typename Pred>
    void showPageLoader(PageIndex pageNum, int msWait, Pred&& pred);
    void showPageLoader(PageIndex pageNum, int msWait = 1000, QString text = "");
    void showPage(PageIndex pageNum);
    void pageShown(int page);
    void softUpdateDevices();
    void checkVersion();
    void runUpdateManager();
    void doMalware();
    void cirlceMalwareState(bool success);
    void cirlceMalwareStateReset();
};
#endif // MAINWINDOW_H
