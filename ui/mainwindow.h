#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <functional>

#include <QMainWindow>
#include <QList>
#include <QComboBox>
#include <QSettings>
#include <QVersionNumber>

#include "adbtrace.h"

#ifndef ADSVERSION
#define ADSVERSION "custom"
#endif

enum MalwareStatus
{
    Idle,
    Running,
    Error
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

    Adb adb;
    Network network;
    QTimer *timerAuthAnim;

private slots:
    void on_actionAboutUs_triggered();

    void on_comboBoxDevices_currentIndexChanged(int index);

    void on_pushButton_2_clicked();

    void on_action_WhatsApp_triggered();

    void on_action_Qt_triggered();

    void on_pnext_clicked();

    void on_pprev_clicked();

    void on_authButton_clicked();

    void on_deviceChanged(const AdbDevice& device, AdbConState state);

    void on_buttonDecayMalware_clicked();

    void replyAuthFinish(int status, bool ok);
    void replyAdsData(const QStringList& adsList, int status, bool ok);
    void replyFetchVersionFinish(int status, const QString& version, const QString& url, bool ok);


private:
    Ui::MainWindow *ui;
    QTimer * malwareUpdateTimer;
    QSettings* settings;
    int minPage;
    QList<QWidget*> pages;
    int curPage;
    int startPage = 0;

    void updatePageState();
    void showPage(int pageNum);
    void pageShown(int page);
    void delayPush(int ms, std::function<void ()> call, bool loop = false);
    void doMalware();
    void softUpdateDevices();
    void checkVersion();
};
#endif // MAINWINDOW_H
