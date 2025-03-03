#include <QTimer>
#include <QHash>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>

#include "mainwindow.h"
#include "ui_mainwindow.h"

constexpr auto acceptLinkWaMe = "aHR0cHM6Ly93YS5tZS8rNzcwMjEwOTQ4MTQ/dGV4dD3QryUyMNGF0L7RgtC10LslMjDQsdGLJTIw"
                                "0L/QvtC00LXQu9C40YLRjNGB0Y8lMjDRgdCy0L7QuNC8JTIw0L7RgtC30YvQstC+0LwlMjDQuCUy"
                                "MNC+0LHRgdGD0LTQuNGC0YwlMjDQv9GA0L7Qs9GA0LDQvNC80YMlMjDQv9C+JTIw0YPQtNCw0LvQ"
                                "tdC90LjRjiUyMNGA0LXQutC70LDQvNGLJTIw0LTQu9GPJTIwQW5kcm9pZC0lRDElODMlRDElODEl"
                                "RDElODIlRDElODAlRDAlQkUlRDAlQjklRDElODElRDElODIlRDAlQjIuCg==";
constexpr auto acceptLinkMail = "aHR0cHM6Ly93YS5tZS8rNzcwMjEwOTQ4MTQK";

constexpr auto infoMessage = "Программа для удаления реклам (назоиливых и не приятных) на телефонах/смартфонах Android.\n\nПрограмма предоставлена из imister.kz.";

constexpr auto infoNoBalance = "Попытка войти в аккаунт была безуспешной, так как у вас закончился баланс. "
                               "Если вы хотите пополнить баланс, пожалуйста, свяжитесь с нашей службой поддержки. "
                               "Для этого перейдите в меню и выберите раздел 'Поддержка', затем нажмите на опцию "
                               "'Связаться через WhatsApp'.";

uint hash_from_adbdevs(const QList<AdbDevice> &devs)
{
    QString _compare;
    for(const AdbDevice &dev : devs)
    {
        _compare += dev.devId;
        _compare += dev.model;
        _compare += dev.vendor;
    }
    return qHash(_compare);
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    int x;
    ui->setupUi(this);
    // Load settings
    settings = new QSettings("imister.kz-app.ads", "AdsKiller", this);
    network._token = settings->value("_token", "").toString();
    // Refresh TabPages to Content widget (Selective)
    for(x = 0; x < ui->tabWidget->count(); ++x)
        pages.append(ui->tabWidget->widget(x));
    for(x = 0; x < pages.count(); ++x)
        ui->contentLayout->layout()->addWidget(pages[x]);
    ui->tabWidget->deleteLater();
    // Show First Page
    minPage = 0;
    showPage(0);
    // Signals
    connect(&network, &Network::loginFinish, this, &MainWindow::replyAuthFinish);
    connect(&network, &Network::adsFinished, this, &MainWindow::replyAdsData);
    connect(&adb, &Adb::onDeviceChanged, this, &MainWindow::on_deviceChanged);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushButton_clicked()
{
}

void MainWindow::on_actionAboutUs_triggered()
{
    QMessageBox msg(this);
    msg.setWindowTitle("О программе");
    msg.setText(infoMessage);
    msg.setStandardButtons(QMessageBox::Ok);
    msg.exec();
}

void MainWindow::on_comboBoxDevices_currentIndexChanged(int index)
{
    if(devices.isEmpty() || index == -1)
        return;
    if(index == 0 && adb.isConnected())
        adb.disconnect();
    else
        adb.connect(devices[index-1].devId);
}

void MainWindow::on_pushButton_2_clicked()
{
    updateAdbDevices();
}

void MainWindow::on_action_WhatsApp_triggered()
{
    QString dec = acceptLinkWaMe;
    dec = QByteArray::fromBase64(dec.toUtf8());
    QDesktopServices::openUrl(QUrl(dec));
}

void MainWindow::on_action_Qt_triggered()
{
    QMessageBox::aboutQt(this);
}

void MainWindow::on_pnext_clicked()
{
    if(curPage < pages.count() - 1 && curPage > (-1))
        showPage(curPage + 1);
}

void MainWindow::on_pprev_clicked()
{
    if(curPage < pages.count() && curPage > (0))
        showPage(curPage - 1);
}

void MainWindow::updateAdbDevices()
{
    QStringList qlist;
    QList<AdbDevice> devicesNew = adb.devices();
    uint hOld, hNew;
    int index = -1;
    int i = 0;
    for(AdbDevice &dev : devices)
    {
        // TODO: Make event for changed devices.
        if(index == -1 && dev.devId == adb.device.devId)
            index = i+1;
        ++i;
    }
    hOld = hash_from_adbdevs(devices);
    hNew = hash_from_adbdevs(devicesNew);
    if(hOld == hNew)
        return;
    devices = std::move(devicesNew);
    std::transform(devices.cbegin(), devices.cend(), std::back_inserter(qlist), [](const AdbDevice &dev) { return dev.displayName + " (" + dev.devId + ")"; });
    for(; ui->comboBoxDevices->count() > 1;)
        ui->comboBoxDevices->removeItem(1);
    ui->comboBoxDevices->addItems(qlist);
    if(index == -1 && !devices.isEmpty())
        index = 0;
    ui->comboBoxDevices->setCurrentIndex(index);
}

void MainWindow::refreshTabState()
{
    constexpr char labelPage[] = "Странница <u><b>%1</b></u> из <u><b>%2</b></u>";
    QString labelText = QString::fromUtf8(labelPage);
    ui->pprev->setEnabled(curPage > 0 && curPage != -1 && curPage > minPage);
    ui->pnext->setEnabled(curPage < pages.count() - 1 && curPage != -1);
    ui->labelStat->setText(labelText.arg(curPage + 1).arg(pages.count()));
}

void MainWindow::showPage(int pageNum)
{
    curPage = -1;
    for(int x = 0; x < pages.count(); ++x)
    {
        bool paged = x == pageNum;
        if(paged)
            curPage = pageNum;
        pages[x]->setVisible(paged);
    }
    refreshTabState();
    pageShown(curPage);
}

void MainWindow::pageShown(int page)
{
    switch(page)
    {
        // WELCOME
    case 0:

        break;
        // AUTH
    case 1:
        ui->lineEditToken->setText(network._token);
        ui->statusAuthText->setText("Выполните аутентификацию");
        ui->pnext->setEnabled(false);
        ui->authButton->setEnabled(true);
        break;
        // DEVICES
    case 2:
    {
        ui->pnext->setEnabled(false);
        for(; ui->comboBoxDevices->count() > 1;)
            ui->comboBoxDevices->removeItem(1);
        ui->connectDeviceStateStat->setText("Выберите доступное устройство из списка.");
        updateAdbDevices();
        // refreshDeviceWatch = [this](){
        //     if( curPage == 2 )
        //     {
        //         updateAdbDevices();
        //         delayPush(500, refreshDeviceWatch);
        //     }};
        // refreshDeviceWatch();
        break;
    }
    default:
        break;
    }
}

void MainWindow::delayPush(int ms, std::function<void()> call, bool loop)
{
    QTimer * qtimer = new QTimer(this);
    qtimer->setSingleShot(!loop);
    qtimer->setInterval(ms);
    connect(qtimer, &QTimer::timeout, [qtimer,call](){
        call();
        qtimer->deleteLater();
    });
    qtimer->start();
}

void MainWindow::doMalware()
{
    ui->buttonDecayMalware->setEnabled(false);

    QList<std::function<void()>> queueCmds = { [](){return;} };
    (void)queueCmds;
    delayPush(500, [this](){
        ui->buttonDecayMalware->setEnabled(true);
    });
}

void MainWindow::on_authButton_clicked()
{
    QString tryToken = ui->lineEditToken->text();
    network.getToken(tryToken);
    ui->statusAuthText->setText("Подключение");
    timerAuthAnim = new QTimer(this);
    timerAuthAnim->start(350);

    qobject_cast<QWidget *>(sender())->setEnabled(false);
    ui->pprev->setEnabled(false);
    ui->pnext->setEnabled(false);
    ui->lineEditToken->setEnabled(false);
    connect(
        timerAuthAnim,
        &QTimer::timeout,
        this,
        [this]()
        {
            QString temp = ui->statusAuthText->text();
            int dotCount = std::accumulate(temp.begin(), temp.end(), 0, [](int count, const QChar &c) { return count += (c == '.' ? 1 : 0); });
            if(dotCount == 3)
                temp.remove('.');
            else
                temp += '.';
            ui->statusAuthText->setText(temp);
        });
}

void MainWindow::on_deviceChanged(const AdbDevice &device, AdbConState state)
{
    updateAdbDevices();
    if(curPage == 2)
    {
        ui->pnext->setEnabled(state == Add);
        QString text = "Устройство ";
        text += device.displayName;
        text += " успешно ";
        if(state == Add)
            text += "подключено.";
        else
            text += "отключено.";
        ui->connectDeviceStateStat->setText(text);
    }

}

void MainWindow::replyAuthFinish(int status, bool ok)
{
    delayPush(1000, [ok,status,this](){
        bool state = ok;
        timerAuthAnim->stop();

        if(state)
        {
            if(network.authedId.expires == 0)
            {
                ui->statusAuthText->setText("Закончился баланс, пополните, чтобы продолжить.");
                QMessageBox::warning(this, "Сервер отклонил запрос", infoNoBalance, "Завершить");
                state = false;
            }
            else
            {
                ui->statusAuthText->setText("Аутентификация прошла успешно.");
            }

            settings->setValue("_token", network.authedId.token);
            minPage = curPage + 1;
            refreshTabState();
        }
        else
        {
            QString errText;
            switch(status)
            {
            case 0:
                errText = "Успех.";
                break;
            case 401:
                errText = "Не действительный токен.";
                break;
            default:
                errText = "Проверьте интернет соединение.";
                break;
            }

            ui->statusAuthText->setText(errText);
            ui->pprev->setEnabled(true);
        }
        ui->lineEditToken->setEnabled(true);
        ui->pnext->setEnabled(state);
        ui->authButton->setEnabled(true);
    });
}

void MainWindow::replyAdsData(const QStringList &adsList, int status, bool ok)
{
    if(status == 601)
    {
        showPage(1);
        QMessageBox::warning(this, "Сервер отклонил запрос", infoNoBalance, "Завершить");
        return;
    }

}

void MainWindow::on_buttonDecayMalware_clicked()
{
    doMalware();
}

