#include <algorithm>

#include <QTimer>
#include <QHash>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>
#include <QStringListModel>
#include <QTableView>
#include <QVector>
#include <QStandardItemModel>
#include <QHeaderView>

#include "mainwindow.h"
#include "ui_mainwindow.h"

constexpr auto COMPILED_VERSION = ADSVERSION;

constexpr auto acceptLinkWaMe = "aHR0cHM6Ly93YS5tZS8rNzcwMjEwOTQ4MTQ/dGV4dD3QryUyMNGF0L7RgtC10LslMjDQsdGLJTIw"
                                "0L/QvtC00LXQu9C40YLRjNGB0Y8lMjDRgdCy0L7QuNC8JTIw0L7RgtC30YvQstC+0LwlMjDQuCUy"
                                "MNC+0LHRgdGD0LTQuNGC0YwlMjDQv9GA0L7Qs9GA0LDQvNC80YMlMjDQv9C+JTIw0YPQtNCw0LvQ"
                                "tdC90LjRjiUyMNGA0LXQutC70LDQvNGLJTIw0LTQu9GPJTIwQW5kcm9pZC0lRDElODMlRDElODEl"
                                "RDElODIlRDElODAlRDAlQkUlRDAlQjklRDElODElRDElODIlRDAlQjIuCg==";
constexpr auto acceptLinkMail = "aHR0cHM6Ly93YS5tZS8rNzcwMjEwOTQ4MTQK";

constexpr auto infoMessage = "Программа для удаления реклам (назоиливых и не приятных) на телефонах/смартфонах Android.\n\n"
                             "Программа предоставлена из imister.kz.";

constexpr auto infoNoBalance = "Попытка войти в аккаунт была безуспешной, так как у вас закончился баланс. "
                               "Если вы хотите пополнить баланс, пожалуйста, свяжитесь с нашей службой поддержки. "
                               "Для этого перейдите в меню и выберите раздел 'Поддержка', затем нажмите на опцию "
                               "'Связаться через WhatsApp'.";
constexpr auto infoNoNetwork = "Не удалось связаться с сервером обновления.\n"
                               "Проверьте интернет соединение и повторите попытку еще раз.\n"
                               "Программа будет завершена.";

extern QPair<QStringList,int> malwareReadLog();
extern MalwareStatus malwareStatus();
extern void malwareStart(MainWindow *handler);
extern void malwareKill();
extern void malwareClean();

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
    QStringListModel *model = new QStringListModel(ui->processStatus);
    ui->processStatus->setModel(model);
    // Show First Page
    minPage = 0;
    showPage(startPage);

    // Signals
    connect(&network, &Network::loginFinish, this, &MainWindow::replyAuthFinish);
    connect(&network, &Network::fetchingVersion, this, &MainWindow::replyFetchVersionFinish);
    connect(&adb, &Adb::onDeviceChanged, this, &MainWindow::on_deviceChanged);

    // Run check version
    checkVersion();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_actionAboutUs_triggered()
{
    QString text;
    QMessageBox msg(this);
    text = QString("Версия программного обеспечения: %1\n\n").arg(ADSVERSION);
    text += infoMessage;
    msg.setWindowTitle("О программе");
    msg.setText(text);
    msg.setStandardButtons(QMessageBox::Ok);
    msg.exec();
}

void MainWindow::on_comboBoxDevices_currentIndexChanged(int index)
{
    if(index == -1)
        return;
    if(index == 0)
        adb.disconnect();
    else if(!adb.devices.isEmpty())
        adb.connect(adb.devices[index - 1].devId);
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
    QList<AdbDevice> devicesNew;
    uint hOld, hNew;
    devicesNew = adb.getDevices();
    hOld = hash_from_adbdevs(adb.devices);
    hNew = hash_from_adbdevs(devicesNew);
    if(hOld == hNew)
        return;
    adb.devices = std::move(devicesNew);
    softUpdateDevices();
}

void MainWindow::softUpdateDevices()
{
    QStringList qlist;
    int i = 0, index = 0;
    for(AdbDevice &dev : adb.devices)
    {
        if(index == 0 && dev.devId == adb.device.devId)
            index = i + 1;
        ++i;
    }
    std::transform(adb.devices.cbegin(), adb.devices.cend(), std::back_inserter(qlist), [](const AdbDevice &dev) { return dev.displayName + " (" + dev.devId + ")"; });
    ui->comboBoxDevices->blockSignals(true);
    for(; ui->comboBoxDevices->count() > 1;)
        ui->comboBoxDevices->removeItem(1);
    ui->comboBoxDevices->addItems(qlist);
    ui->comboBoxDevices->setCurrentIndex(index);
    ui->comboBoxDevices->blockSignals(false);
}

void MainWindow::checkVersion()
{
    ui->labelStat->setText("Проверка обновления");
    ui->pnext->setEnabled(false);
    network.fetchVersion();
}

void MainWindow::updatePageState()
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
        bool paged = (x == pageNum);
        if(paged)
            curPage = pageNum;
        pages[x]->setVisible(paged);
    }
    updatePageState();
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
    {
        ui->lineEditToken->setText(network._token);
        ui->statusAuthText->setText("Выполните аутентификацию");
        ui->pnext->setEnabled(false);
        ui->authButton->setEnabled(true);

        // Создаем модель данных
        QStandardItemModel *model = new QStandardItemModel(ui->authInfo);

        // Устанавливаем количество строк и столбцов
        model->setRowCount(4);
        model->setColumnCount(2);

        // Устанавливаем заголовки столбцов
        model->setHorizontalHeaderItem(0, new QStandardItem("Параметр"));
        model->setHorizontalHeaderItem(1, new QStandardItem("Значение"));

        // Заполняем модель данными
        model->setItem(0, 0, new QStandardItem("Логин"));
        model->setItem(0, 1, new QStandardItem("-"));

        model->setItem(1, 0, new QStandardItem("Последний вход"));
        model->setItem(1, 1, new QStandardItem("-"));

        model->setItem(2, 0, new QStandardItem("Баланс"));
        model->setItem(2, 1, new QStandardItem("-"));

        model->setItem(3, 0, new QStandardItem("Подключений"));
        model->setItem(3, 1, new QStandardItem("-"));

        ui->authInfo->setModel(model);

        ui->authInfo->horizontalHeader()->setStretchLastSection(true);
        ui->authInfo->verticalHeader()->setVisible(false);

        break;
    }
    // DEVICES
    case 2:
    {
        ui->pnext->setEnabled(false);
        adb.blockSignals(true);
        adb.devices.clear();
        adb.disconnect();
        adb.blockSignals(false);
        ui->comboBoxDevices->blockSignals(true);
        for(; ui->comboBoxDevices->count() > 1;)
            ui->comboBoxDevices->removeItem(1);
        ui->comboBoxDevices->blockSignals(false);
        ui->connectDeviceStateStat->setText("Выберите доступное устройство из списка.");
        updateAdbDevices();
        break;
    }
    // Malware
    case 3:
    {
        QStringListModel *model = static_cast<QStringListModel *>(ui->processStatus->model());
        QStringList place {};
        ui->malwareProgressBar->setValue(0);
        ui->buttonDecayMalware->setEnabled(true);
        place << "<< Все готово для запуска обезвредителя >>";
        place << "<< Во время процесса не отсоединяйте устройство от компьютера >>";
        model->setStringList(place);
        break;
    }
    default:
        break;
    }
}

void MainWindow::on_deviceChanged(const AdbDevice &device, AdbConState state)
{
    switch(curPage)
    {
    case 2:
    {
        softUpdateDevices();
        ui->pnext->setEnabled(state == Add);
        QString text = "Устройство ";
        text += device.displayName;
        text += " успешно ";
        if(state == Add)
            text += "подключено.";
        else
            text += "отключено.";
        ui->connectDeviceStateStat->setText(text);
        break;
    }
    case 3:
    {
        if(state == Removed)
        {
            malwareKill();
            delayPush(5000, [this](){ showPage(curPage - 1); });
        }
        break;
    }
    default:
        break;
    }
}

void MainWindow::delayPush(int ms, std::function<void()> call, bool loop)
{
    QTimer *qtimer = new QTimer(this);
    (void)qtimer;
    qtimer->setSingleShot(!loop);
    qtimer->setInterval(ms);
    connect(
        qtimer,
        &QTimer::timeout,
        [qtimer, call]()
        {
            call();
            qtimer->stop();
            qtimer->deleteLater();
        });
    qtimer->start();
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

    QStandardItemModel * model = qobject_cast<QStandardItemModel*>(ui->authInfo->model());
    for(int x = model->rowCount()-1; x > -1; --x)
    {
        model->item(x,1)->setText("-");
    }
}

void MainWindow::replyAuthFinish(int status, bool ok)
{
    delayPush(
        1000,
        [ok, status, this]()
        {
            bool state = ok;
            timerAuthAnim->stop();

            if(state)
            {
                QString value;
                QStandardItemModel * model = qobject_cast<QStandardItemModel*>(ui->authInfo->model());
                value = network.authedId.idName;
                model->item(0,1)->setText(value);
                value = QDateTime::currentDateTime().toString(Qt::TextDate);
                model->item(1,1)->setText(value);
                value = network.authedId.expires > -1 ? QString::number(network.authedId.expires) : "(безлимит)";
                model->item(2,1)->setText(value);
                value = QString::number(network.authedId.scores);
                model->item(3,1)->setText(value);
                value = QString();
                if(network.authedId.expires == 0)
                {
                    ui->statusAuthText->setText("Закончился баланс, пополните, чтобы продолжить.");
                    showMessageFromStatus(601);
                    state = false;
                }
                else
                {
                    ui->statusAuthText->setText("Аутентификация прошла успешно.");
                }

                settings->setValue("_token", network.authedId.token);
                minPage = curPage + 1;
                updatePageState();
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
                case NetworkStatus::NoEnoughMoney:
                    errText = infoNoBalance;
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

void MainWindow::replyFetchVersionFinish(int status, const QString &version, const QString &url, bool ok)
{
    delayPush(1000, [=](){
        if(status == NetworkStatus::NetworkError)
        {
            this->showMessageFromStatus(status);
            this->close();
            return;
        }
        auto _convertToInt = [](const QString& str) -> int
        {
            return str.toInt();
        };
        QVector<int> ints;
        QStringList list = std::move(QString(ADSVERSION).split('.', Qt::SkipEmptyParts));
        std::transform(std::begin(list), std::end(list), std::back_inserter(ints), _convertToInt);
        QVersionNumber verApp(ints);
        list = std::move(version.split('.', Qt::SkipEmptyParts));
        ints.clear();
        std::transform(std::begin(list), std::end(list), std::back_inserter(ints), _convertToInt);
        QVersionNumber verServer(ints);
        if(verApp >= verServer)
        {
            ui->labelStat->setText("Вы используете последнюю версию. Нажмите <b>Далее ></b>, чтобы продолжить.");
            ui->pnext->setEnabled(true);
            return;
        }
        // TODO: update to new version
        QString text;
        text = "Обнаружена новая версия программного обеспечения. После нажатия кнопки \"ОК\" откроется ссылка в вашем браузере.\n"
               "Пожалуйста, скачайте обновление по прямой ссылке.\n"
               "С уважением ваша команда imister.kz.";
        text += "\n\nВаша версия: v";
        text +=  verApp.toString();
        text += "\nВерсия на сервере: v";
        text += verServer.toString();
        QMessageBox::information(this, "Обнаружена новая версия", text);
        QDesktopServices::openUrl(QUrl(url));
        this->close();
    });
}

void MainWindow::replyAdsData(const QStringList &adsList, int status, bool ok)
{
    if(status == 601)
    {
        showPage(1);
    }
    showMessageFromStatus(status);
}

void MainWindow::showMessageFromStatus(int statusCode)
{
    if(statusCode == NetworkStatus::NetworkError)
        QMessageBox::warning(this, "Ошибка подключения", infoNoNetwork);

    if(statusCode == NetworkStatus::NoEnoughMoney)
        QMessageBox::warning(this, "Сервер отклонил запрос", infoNoBalance);
}

void MainWindow::on_buttonDecayMalware_clicked()
{
    doMalware();
}

void MainWindow::doMalware()
{
    QStringListModel *model = static_cast<QStringListModel *>(ui->processStatus->model());
    QStringList place {};

    place << "<< Запуск процесса удаления рекламы, пожалуйста подождите >>";
    place << "<< Не отсоединяйте устройство от компьютера >>";

    model->setStringList(place);
    ui->malwareProgressBar->setValue(0);

    ui->deviceLabelName->setText(adb.device.displayName  + " " + adb.device.devId);
    ui->pprev->setEnabled(false);
    ui->buttonDecayMalware->setEnabled(false);
    delayPush(
        500,
        [this]()
        {
            malwareUpdateTimer = new QTimer(this);
            malwareUpdateTimer->start(100);
            // START MALWARE
            malwareStart(this);
            connect(
                malwareUpdateTimer,
                &QTimer::timeout,
                this,
                [this]()
                {
                    QStringListModel *model = static_cast<QStringListModel *>(ui->processStatus->model());
                    MalwareStatus status = malwareStatus();
                    QPair<QStringList,int> reads;
                    QStringList from;
                    reads = malwareReadLog();
                    if(!reads.first.isEmpty())
                    {
                        from = model->stringList();
                        from.append(reads.first);
                        model->setStringList(from);
                        ui->malwareProgressBar->setValue(reads.second);
                        ui->processStatus->scrollToBottom();
                    }
                    if(status != MalwareStatus::Running)
                    {
                        ui->pprev->setEnabled(true);
                        ui->buttonDecayMalware->setEnabled(true);
                        malwareUpdateTimer->stop();
                        malwareUpdateTimer->deleteLater();
                        malwareClean();
                    }
                });
        });
}
