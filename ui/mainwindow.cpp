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
#include <QCryptographicHash>
#include <QHeaderView>
#include <QTemporaryDir>
#include <QRandomGenerator>

#include "mainwindow.h"
#include "ui_mainwindow.h"

constexpr auto AdskillerCompiledVersion = ADSVERSION;

#ifdef WIN32
constexpr auto UpdateManagerExecute = "update.exe";
#endif

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

extern QString malwareReadHeader();
extern std::pair<QStringList,int> malwareReadLog();
extern MalwareStatus malwareStatus();
extern void malwareStart(MainWindow *handler);
extern void malwareKill();
extern bool malwareClean();

QByteArray randomKey()
{
    QByteArray key;
    key.resize(8);
    for ( int x = 0; x < key.length(); ++x)
    {
        key[x] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    }
    return key;
}

QByteArray encryptData(const QByteArray& bytes, const QByteArray& key)
{
    int x;
    QByteArray retval {bytes};
    for(x = 0; x < bytes.length(); ++x)
    {
        retval[x] = bytes[x] ^ key[x % key.length()];
    }
    return retval;
}

inline QByteArray decryptData(const QByteArray& bytes, const QByteArray& key)
{
    return encryptData(bytes, key);
}

QString packDC(const QByteArray& dataInit, const QByteArray& key)
{
    QByteArray retval {};
    QByteArray data = encryptData(dataInit, key);
    int keylen = key.toHex().length();
    int hashlen = QCryptographicHash::hashLength(QCryptographicHash::Sha256);
    retval.resize(hashlen + data.length() + keylen + 1);
    retval[hashlen] = static_cast<char>(keylen);
    retval.replace(hashlen + 1, keylen, key.toHex());
    retval.replace(hashlen + 1 + keylen, data.length(), data);
    retval.replace(0, hashlen, QCryptographicHash::hash(retval.mid(hashlen), QCryptographicHash::Sha256));
    return QLatin1String(retval.toBase64());
}

QByteArray unpackDC(const QString& packed)
{
    QByteArray key{}, data{};
    int keylen;
    int hashlen = QCryptographicHash::hashLength(QCryptographicHash::Sha256);
    data = QByteArray::fromBase64(packed.toLatin1());
    if (!data.isEmpty() && data.mid(0,hashlen) == QCryptographicHash::hash(data.mid(hashlen), QCryptographicHash::Sha256))
    {
        keylen = data[hashlen];
        key = QByteArray::fromHex(data.mid(hashlen + 1, keylen));
        data = decryptData(data.mid(hashlen + 1 + keylen),key);
    }
    else
    {
        data.clear();
    }
    return data;
}

uint hash_from_AdbDevice(const QList<AdbDevice> &devs)
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
    QStringListModel *model;
    ui->setupUi(this);

    // Load settings
    settings = new QSettings("imister.kz-app.ads", "AdsKiller", this);
    if(settings->contains("_token") && !settings->contains("encrypted_token"))
    {
        network._token = settings->value("_token", "").toString();
        settings->remove("_token");
        settings->setValue("encrypted_token", packDC(network._token.toLatin1(), randomKey()));
    }
    else if(settings->contains("encrypted_token"))
    {
        QByteArray decData = unpackDC(settings->value("encrypted_token").toString());
        network._token = QLatin1String(decData);
    }

    // Refresh TabPages to Content widget (Selective)
    for(x = 0; x < ui->tabWidget->count(); ++x)
        pages << ui->tabWidget->widget(x);
    for(x = 0; x < pages.count(); ++x)
        ui->contentLayout->layout()->addWidget(pages[x]);
    for(x = 0; x< ui->tabWidget_2->count(); ++x)
        malwareStatusLayouts << ui->tabWidget_2->widget(x);
    for(x = 0; x < malwareStatusLayouts.count(); ++x)
        ui->malwareContentLayout->addWidget(malwareStatusLayouts[x]);
    malwareStatusLayouts[0]->show();
    connect(ui->malwareLayoutSwitchButton, &QPushButton::clicked, [this](bool){
        for(auto & layout : malwareStatusLayouts)
        {
            if(layout->isHidden())
            {
                layout->show();
            }
            else
            {
                layout->hide();
            }
        }
    });

    ui->tabWidget->deleteLater();
    ui->tabWidget_2->deleteLater();

    malwareProgressCircle = new ProgressCircle(this);
    ui->progressCircleLayout->addWidget(malwareProgressCircle);
    malwareProgressCircle->setInfinilyMode(false);

    QList<QAction*> menusTheme {ui->mThemeSystem,ui->mThemeLight,ui->mThemeDark};
    for(QAction* q : menusTheme)
    {
        q->setChecked(false);
        connect(q, &QAction::triggered, this, &MainWindow::setThemeAction);
    }

    model = new QStringListModel(ui->processLogStatus);
    ui->processLogStatus->setModel(model);

    // Show First Page
    minPage = 0;
    showPage(startPage);

    // Signals
    connect(&network, &Network::loginFinish, this, &MainWindow::replyAuthFinish);
    connect(&network, &Network::fetchingVersion, this, &MainWindow::replyFetchVersionFinish);
    connect(&adb, &Adb::onDeviceChanged, this, &MainWindow::on_deviceChanged);

    // Set Default Theme is Light (1)
    setTheme(static_cast<ThemeScheme>(static_cast<ThemeScheme>(std::clamp<int>(settings->value("theme", 1).toInt(),0,2))));

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
    hardUpdateAdbDevices();
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

void MainWindow::hardUpdateAdbDevices()
{
    QList<AdbDevice> devicesNew;
    uint hOld, hNew;
    devicesNew = adb.getDevices();
    hOld = hash_from_AdbDevice(adb.devices);
    hNew = hash_from_AdbDevice(devicesNew);
    if(hOld == hNew)
        return;
    adb.devices = std::move(devicesNew);
    softUpdateAdbDevices();
}

void MainWindow::softUpdateAdbDevices()
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
    int x,y;
    curPage = -1;
    for(x = 0,y=pages.count(); x < y; ++x)
    {
        bool paged = (x == pageNum);
        if(paged)
            curPage = pageNum;
        pages[x]->setVisible(paged);
    }
    QVBoxLayout * layout = qobject_cast<QVBoxLayout*>(ui->pageIconBoxes->layout());
    for(x = 0,y=layout->count(); x < y; ++x)
    {
        bool paged = x == pageNum;
        layout->itemAt(x)->widget()->setEnabled(paged);
        if(paged)
        {
            QRect arrowPos = ui->arrowIconMark->geometry();
            arrowPos.setY(layout->itemAt(x)->widget()->geometry().y()-20);
            ui->arrowIconMark->setGeometry(arrowPos);
        }
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

        model->setItem(3,0, new QStandardItem("VIP дней"));
        model->setItem(3,1, new QStandardItem("-"));

        model->setItem(4, 0, new QStandardItem("Подключений"));
        model->setItem(4, 1, new QStandardItem("-"));

        model->setItem(5,0, new QStandardItem("Расположение"));
        model->setItem(5,1, new QStandardItem("-"));

        ui->authInfo->setModel(model);

        ui->authInfo->horizontalHeader()->setStretchLastSection(true);
        ui->authInfo->verticalHeader()->setVisible(false);

        ui->authInfo->resizeColumnToContents(0);
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
        hardUpdateAdbDevices();
        break;
    }
    // Malware
    case 3:
    {
        QStringListModel *model = static_cast<QStringListModel *>(ui->processLogStatus->model());
        ui->processBarStatus->setValue(0);
        ui->buttonDecayMalware->setEnabled(true);
        ui->malwareStatusText0->setText("Malware не запущен.");
        malwareProgressCircle->setValue(0);
        malwareProgressCircle->setMaximum(100);
        malwareProgressCircle->setInfinilyMode(false);
        cirlceMalwareStateReset();
        model->setStringList(QStringList() << "<< Все готово для запуска >>"
                                           << "<< Во время процесса не отсоединяйте устройство от компьютера >>");
        break;
    }
    default:
        break;
    }
}

void MainWindow::on_deviceChanged(const AdbDevice &device, AdbConState state)
{
    softUpdateAdbDevices();
    switch(curPage)
    {
    case 2:
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
        break;
    }
    case 3:
    {
        if(state == Removed)
        {
            malwareKill();
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
    network.authenticate(tryToken);
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

void MainWindow::setThemeAction()
{
    QList<QAction*> menus {ui->mThemeSystem,ui->mThemeLight,ui->mThemeDark};
    QAction * sel = qobject_cast<QAction*>(sender());
    int scheme;
    for(scheme = (0); scheme < menus.size() && sel != menus[scheme]; ++scheme)
        ;
    setTheme(static_cast<ThemeScheme>(scheme));
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

                value = QString::number(network.authedId.vipDays);
                model->item(3,1)->setText(value);

                value = QString::number(network.authedId.scores);
                model->item(4,1)->setText(value);

                value = network.authedId.location;
                model->item(5,1)->setText(value);
                value.clear();

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

                settings->setValue("encrypted_token", packDC(network.authedId.token.toLatin1(), randomKey()));
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

        QString text;
        text = "Обнаружена новая версия программного обеспечения. После нажатия кнопки \"ОК\" ";
#ifdef WIN32
        text += "будет запущена обновление ПО.";
#else
        text += "откроется ссылка в вашем браузере.\n"
                "Пожалуйста, скачайте обновление по прямой ссылке.\n";
#endif
        text += "\nС уважением ваша команда imister.kz.";
        text += "\n\nВаша версия: v";
        text +=  verApp.toString();
        text += "\nВерсия на сервере: v";
        text += verServer.toString();
        // TURNED OFF INFO ABOUT UPDATE
        // QMessageBox::information(this, "Обнаружена новая версия", text);
        this->close();

#ifdef WIN32
        QTemporaryDir tempdir;
        tempdir.setAutoRemove(false);
        QDir appDir(QCoreApplication::applicationDirPath());
        QStringList entries = appDir.entryList(QStringList() << "*.dll" << UpdateManagerExecute, QDir::Files);
        for(const QString & e : entries)
        {
            QFile::copy(appDir.filePath(e), tempdir.filePath(e));
        }
        appDir.mkdir(tempdir.filePath("platforms"));
        QFile::copy(appDir.filePath("platforms/qwindows.dll"), tempdir.filePath("platforms/qwindows.dll"));
        appDir.mkdir(tempdir.filePath("networkinformation"));
        QFile::copy(appDir.filePath("networkinformation/qnetworklistmanager.dll"), tempdir.filePath("networkinformation/qnetworklistmanager.dll"));
        appDir.mkdir(tempdir.filePath("tls"));
        QFile::copy(appDir.filePath("tls/qcertonlybackend.dll"), tempdir.filePath("tls/qcertonlybackend.dll"));
        QFile::copy(appDir.filePath("tls/qschannelbackend.dll"), tempdir.filePath("tls/qschannelbackend.dll"));
        //#error BUG "EXISTS PROCESS NOT BY REPLACE"
        if(QProcess::startDetached(tempdir.filePath(UpdateManagerExecute), QStringList() << QString("--dir") << appDir.path() << QString("--exec") << QCoreApplication::applicationFilePath()))
        {
            QApplication::quit();
            return;
        }
#endif
        QDesktopServices::openUrl(QUrl(url));
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

void MainWindow::setTheme(ThemeScheme theme)
{
    int scheme;
    const char * resourceName;
    QList<QAction*> menus {ui->mThemeSystem,ui->mThemeLight,ui->mThemeDark};
    QAction * sel = qobject_cast<QAction*>(sender());
    for(scheme = (0); scheme < menus.size(); ++scheme)
    {
        menus[scheme]->setChecked(theme == scheme);
    }

    switch(theme)
    {
    case System:
        resourceName = nullptr;
        break;
    case Dark:
        resourceName = ":/resources/app-style-dark";
        break;
    case Light:
    default:
        resourceName = ":/resources/app-style-light";
        break;
    }
    // Set application Design
    QFile styleRes(resourceName);
    styleRes.open(QFile::ReadOnly | QFile::Text);
    QString styleSheet = styleRes.readAll();
    styleRes.close();
    app->setStyleSheet(styleSheet);
    settings->setValue("theme", static_cast<int>(theme));
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
    QStringListModel *model = static_cast<QStringListModel *>(ui->processLogStatus->model());
    QStringList place {};

    place << "<< Запуск процесса удаления рекламы, пожалуйста подождите >>";
    place << "<< Не отсоединяйте устройство от компьютера >>";

    model->setStringList(place);
    ui->processBarStatus->setValue(0);
    cirlceMalwareStateReset();
    malwareProgressCircle->setInfinilyMode(true);
    malwareProgressCircle->setValue(0);

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
                    QStringListModel *model = static_cast<QStringListModel *>(ui->processLogStatus->model());
                    MalwareStatus status = malwareStatus();
                    std::pair<QStringList,int> reads;
                    QString header;
                    QStringList from;
                    header = malwareReadHeader();
                    reads = malwareReadLog();

                    ui->malwareStatusText0->setText(header);
                    ui->processBarStatus->setValue(reads.second);
                    malwareProgressCircle->setValue(reads.second);
                    if(!reads.first.isEmpty())
                    {
                        from = model->stringList();
                        from.append(reads.first);
                        model->setStringList(from);
                        ui->processLogStatus->scrollToBottom();
                    }
                    if(status != MalwareStatus::Running)
                    {
                        ui->pprev->setEnabled(true);
                        ui->buttonDecayMalware->setEnabled(true);
                        cirlceMalwareState(status != MalwareStatus::Error);
                        malwareProgressCircle->setInfinilyMode(false);
                        malwareUpdateTimer->stop();
                        malwareUpdateTimer->deleteLater();
                        malwareClean();
                    }
                });
        });
}

void MainWindow::cirlceMalwareState(bool success)
{
    malwareProgressCircle->setInfinilyMode(false);

    QPropertyAnimation * animation;
    // animation = new QPropertyAnimation(malwareProgressCircle, "outerRadius", malwareProgressCircle);
    // animation->setDuration(1500);
    // animation->setEasingCurve(QEasingCurve::OutQuad);
    // animation->setEndValue(0.5);
    // animation->start(QAbstractAnimation::DeleteWhenStopped);

    animation = new QPropertyAnimation(malwareProgressCircle, "innerRadius", malwareProgressCircle);
    animation->setDuration(750);
    animation->setEasingCurve(QEasingCurve::OutQuad);
    animation->setEndValue(0.0);
    animation->start(QAbstractAnimation::DeleteWhenStopped);

    QColor color = success ? QColor(155,219,58) : QColor(255,100,100);

    animation = new QPropertyAnimation(malwareProgressCircle, "color", malwareProgressCircle);
    animation->setDuration(750);
    animation->setEasingCurve(QEasingCurve::OutQuad);
    animation->setEndValue(color);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::cirlceMalwareStateReset()
{
    QPropertyAnimation * animation;
    // animation = new QPropertyAnimation(malwareProgressCircle, "outerRadius", malwareProgressCircle);
    // animation->setDuration(1500);
    // animation->setEasingCurve(QEasingCurve::OutQuad);
    // animation->setEndValue(1.0);
    // animation->start(QAbstractAnimation::DeleteWhenStopped);

    animation = new QPropertyAnimation(malwareProgressCircle, "innerRadius", malwareProgressCircle);
    animation->setDuration(750);
    animation->setEasingCurve(QEasingCurve::OutQuad);
    animation->setEndValue(0.6);
    animation->start(QAbstractAnimation::DeleteWhenStopped);

    QColor color {110,190,235};

    animation = new QPropertyAnimation(malwareProgressCircle, "color", malwareProgressCircle);
    animation->setDuration(750);
    animation->setEasingCurve(QEasingCurve::OutQuad);
    animation->setEndValue(color);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}
