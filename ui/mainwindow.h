#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <functional>

#include <QMainWindow>
#include <QList>
#include <QComboBox>
#include <QSettings>

#include "adbtrace.h"

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

    void updateAdb();

    Adb adb;
    QList<AdbDevice> devices;
    Network network;
    QTimer *timerAuthAnim;

private slots:
    void on_pushButton_clicked();

    void on_actionAboutUs_triggered();

    void on_comboBoxDevices_currentIndexChanged(int index);

    void on_pushButton_2_clicked();

    void on_action_WhatsApp_triggered();

    void on_action_Qt_triggered();

    void on_pnext_clicked();

    void on_pprev_clicked();

    void on_authButton_clicked();

    void replyAuthFinish(int status, bool ok);
    void replyAdsData(const QStringList& adsList, int status, bool ok);
private:
    Ui::MainWindow *ui;
    QSettings* settings;
    int minPage;
    QList<QWidget*> pages;
    int curPage;

    void refreshTabState();
    void showPage(int pageNum);
    void pageShown(int page);
    void delayPush(int ms, std::function<void ()> call, bool loop = false);
};
#endif // MAINWINDOW_H
