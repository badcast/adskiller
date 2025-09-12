#include <algorithm>
#include <cctype>

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
#include <QTemporaryDir>
#include <QFontDatabase>
#include <QFuture>
#include <QEventLoop>
#include <QCloseEvent>
#include <QGraphicsOpacityEffect>
#include <QScrollBar>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "SystemTray.h"
#include "extension.h"
#include "Services.h"
#include "network.h"

#include "icontextbutton.h"

#include "Strings.h"

MainWindow * MainWindow::current;

constexpr struct {
    PageIndex index;
    char widgetName[16];
} PageConstNames[LengthPages] = {
    {AuthPage, "page_auth"},
    {CabinetPage, "page_cabinet"},
    {MalwarePage, "page_adsmalware"},
    {LoaderPage, "page_loader"},
    {DevicesPage, "page_devices"}
};

constexpr struct {
    char title[64];
    char iconName[24];
    bool active;
    int price;
} AvailableServices[] = {
    {"Удалить рекламу", "remove-ads", true, 0},
    {"APK Менеджер", "icon-malware", false,0},
    {"Мои устройства и гарантия", "icon-malware", false,0},
    {"Очистка Мусора", "icon-malware", false, 0},
    {"Samsung FRP", "icon-malware", false, 0},
    {"Перенос WhatsApp", "icon-malware", false,0},
    {"Перенос на iOS", "icon-malware", false, 0},
    {"Сброс к заводским", "icon-malware", false,0}
};

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    int x;
    QStringListModel *model;
    ui->setupUi(this);

    // Load settings
    settings = new QSettings("imister.kz-app.ads", "AdsKiller", this);
    if (settings->contains("encrypted_token"))
    {
        QByteArray decData = CipherAlgoCrypto::UnpackDC(settings->value("encrypted_token").toString());
        network._token = QLatin1String(decData);
    }
    if(settings->contains("autologin"))
    {
        ui->checkAutoLogin->setChecked(settings->value("autologin").toBool());
    }
    else
    {
        ui->checkAutoLogin->setChecked(true);
    }

    if (!std::all_of(std::begin(network._token), std::end(network._token), [](auto &lhs)
                     { return std::isalnum(lhs.toLatin1()); }))
    {
        network._token.clear();
    }

    // Refresh TabPages to Content widget (Selective)
    QList<QWidget*> _w;
    for (x = 0; x < ui->tabWidget->count(); ++x)
        _w << ui->tabWidget->widget(x);

    for(const auto & item : std::as_const(PageConstNames))
    {
        auto iter = std::find_if(_w.begin(), _w.end(), [&item](const QWidget* it) { return it->objectName() == item.widgetName;});
        if(iter != std::end(_w))
            pages.insert(item.index, *iter);
    }

    vPageSpacer = ui->topcontent;
    vPageSpacer->setMaximumHeight(400);
    vPageSpacerAnimator = new QPropertyAnimation(vPageSpacer, "maximumHeight");
    vPageSpacerAnimator->setDuration(500);
    vPageSpacerAnimator->setStartValue(400);
    vPageSpacerAnimator->setEndValue(0);

    QGraphicsOpacityEffect * effect = new QGraphicsOpacityEffect(ui->contentLayout);
    ui->contentLayout->setGraphicsEffect(effect);

    contentOpacityAnimator = new QPropertyAnimation(effect, "opacity");
    contentOpacityAnimator->setDuration(1000);
    contentOpacityAnimator->setStartValue(0);
    contentOpacityAnimator->setEndValue(1.0);

    deviceLeftAnimator = new QPropertyAnimation(ui->device_left_group, "maximumWidth");
    deviceLeftAnimator->setDuration(1000);
    deviceLeftAnimator->setStartValue(1000);
    deviceLeftAnimator->setEndValue(0);

    // Top header back to main page.
    ui->contentLayout->layout()->addWidget(ui->toplevel_backpage);

    for (x = 0; x < _w.count(); ++x)
        ui->contentLayout->layout()->addWidget(_w[x]);

    ui->tabWidget->deleteLater();

    malwareProgressCircle = new ProgressCircle(this);
    malwareProgressCircle->setInfinilyMode(false);
    ui->progressCircleLayout->addWidget(malwareProgressCircle);

    loaderProgressCircle = new ProgressCircle(this);
    loaderProgressCircle->setInfinilyMode(true);
    loaderProgressCircle->setVisibleText(false);
    loaderProgressCircle->setInnerRadius(0);
    loaderProgressCircle->setColor(Qt::darkRed);
    loaderProgressCircle->setInnerRadius(.5);
    loaderProgressCircle->setMinimumHeight(225);
    ui->loaderLayout->addWidget(loaderProgressCircle);

    QList<QAction *> menusTheme{ui->mThemeSystem, ui->mThemeLight, ui->mThemeDark};
    for (QAction *q : menusTheme)
    {
        q->setChecked(false);
        QObject::connect(q, &QAction::triggered, this, &MainWindow::setThemeAction);
    }

    model = new QStringListModel(ui->processLogStatus);
    ui->processLogStatus->setModel(model);

    // Signals
    QObject::connect(&network, &Network::loginFinish, this, &MainWindow::replyAuthFinish);
    QObject::connect(&network, &Network::fetchingVersion, this, &MainWindow::replyFetchVersionFinish);
    QObject::connect(ui->authpageUpdate, &QPushButton::clicked, this, &MainWindow::updateAuthInfoFromNet);
    QObject::connect(ui->buttonBackTo, &QPushButton::clicked, this, &MainWindow::updateAuthInfoFromNet);
    QObject::connect(ui->logoutButton, &QPushButton::clicked, this, &MainWindow::logout);
    QObject::connect(ui->malwareReRun, &QPushButton::clicked, [this](){if(currentService && currentService->handler && !currentService->handler->isStarted()) currentService->handler->start();});

    checkerVer = new QTimer(this);
    checkerVer->setSingleShot(true);
    checkerVer->setInterval(VersionCheckRate);
    QObject::connect(checkerVer, &QTimer::timeout, [this](){checkVersion(false);});

    // Font init
    int fontId = QFontDatabase::addApplicationFont(":/resources/font-DigitalNumbers");
    QStringList fontFamils = QFontDatabase::applicationFontFamilies(fontId);
    if(!fontFamils.isEmpty())
    {
        QString fontFamily = fontFamils.first();
        malwareProgressCircle->setStyleSheet(QString("QWidget { Font-family: '%1'; }").arg(fontFamily));
    }

    // Set Default Theme is Light (1)
    setTheme(static_cast<ThemeScheme>(static_cast<ThemeScheme>(std::clamp<int>(settings->value("theme", 1).toInt(), 0, 2))));

    // Init tray
    tray = new AdsAppSystemTray(this);

    QString _version;
    _version += QString::number(AppVerMajor);
    _version += ".";
    _version += QString::number(AppVerMinor);
    _version += ".";
    _version += QString::number(AppVerPatch);

    selfVersion = { _version, {}, 0};

    // Run check version
#ifdef NDEBUG
    verChansesAvailable = -1;
#endif
    checkVersion(true);
    Adb::startServer();
}

MainWindow::~MainWindow()
{
    if(currentService && currentService->handler)
    {
        currentService->handler->stop();
    }
    delete ui;
    Adb::killServer();
}

void MainWindow::on_actionAboutUs_triggered()
{
    QString text;
    QMessageBox msg(this);
    text = QString("Версия программного обеспечения: %1.%2.%3\n\n").arg(AppVerMajor).arg(AppVerMinor).arg(AppVerPatch);
    text += infoMessage;
    msg.setWindowTitle("О программе");
    msg.setText(text);
    msg.setStandardButtons(QMessageBox::Ok);
    msg.exec();
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

void MainWindow::checkVersion(bool firstRun)
{
#ifdef NDEBUG

    network.pullFetchVersion(firstRun);

    // Show First Page
    if(firstRun)
    {
        ui->loaderPageText->setText("Проверка обновления");
        showPageLoader(startPage, 100, [this]()->bool {
            if(actualVersion.empty())
                return false;
            if(lastPage == AuthPage)
            {
                if(actualVersion.mStatus != NetworkStatus::OK)
                {
                    ui->loaderPageText->setText("Проблема с интернетом?");
                    ui->loaderPageText->update();
                    delayTimer(2000);
                    lastPage = PageIndex(-1);
                    willTerminate();
                }
                else
                {
                    verChansesAvailable = ChansesRunInvalid;
                    ui->loaderPageText->setText("Ваша версия актуальная!");
                    ui->loaderPageText->update();
                    delayTimer(2000);
                    checkerVer->start();
                    return true;
                }
            }
            return false;
        });
    }
    else if(verChansesAvailable > -1)
    {
        delayPushLoop(70, [this](){
            if(actualVersion.empty())
                return true;
            if(verChansesAvailable > -1 && !isHidden())
            {
                if(actualVersion.mStatus != NetworkStatus::OK)
                {
                    if(verChansesAvailable == 0)
                    {
                        // Will terminate
                        verChansesAvailable = -1;
                        willTerminate();
                        checkerVer->stop();
                        return false;
                    }
                    else
                    {
                        QString warnMessage = "У вас осталось попыток (%1), срочно восстановите связь, иначе приложение аварийно завершится.";
                        warnMessage = warnMessage.arg(verChansesAvailable);
                        verChansesAvailable = qMax<int>(verChansesAvailable-1,0);
                        QMessageBox::warning(this, "Отсутствие соединение с интернетом.", warnMessage);
                    }
                }
                else
                {
                    verChansesAvailable = ChansesRunInvalid;
                }
            }
            checkerVer->start();
            return false;
        });
    }

#else
    if(firstRun)
        showPageLoader(AuthPage);
#endif
}

void MainWindow::willTerminate()
{
    setEnabled(false);
    showMessageFromStatus(NetworkError);
    delayPush(5000, [this](){this->close();});
    QMessageBox::question(this, "Нет соединение с интернетом", "Программа будет аварийно завершена через 5 секунд.", QMessageBox::StandardButton::Ok);
}

template<typename Pred>
void MainWindow::showPageLoader(PageIndex pageNum, int msWait, Pred &&pred)
{
    if(pageNum == LoaderPage)
        return;

    showPage(LoaderPage);
    delayPushLoop(msWait, [this,pageNum,pred]()
                  {
                      if(pred())
                      {
                          showPage(pageNum);
                          return false;
                      }
                      return true;
                  });
}

void MainWindow::showPageLoader(PageIndex pageNum, int msWait, QString text)
{
    if(text.isEmpty())
        text = "Ожидайте";

    ui->loaderPageText->setText(text);
    showPageLoader(pageNum, msWait, [](){return true;});
}

void MainWindow::showPage(PageIndex pageNum)
{
    PageIndex oldPage = curPage;
    if(oldPage != LoaderPage)
        lastPage = oldPage;
    if(pages.contains(oldPage))
    {
        pages[oldPage]->setEnabled(false);
        pages[oldPage]->setVisible(false);
    }
    curPage = pageNum;
    if(pages.contains(curPage))
    {
        pages[curPage]->setVisible(true);
        pages[curPage]->setEnabled(true);
    }

    vPageSpacerAnimator->start();
    contentOpacityAnimator->start();

    ui->toplevel_backpage->setVisible(pageNum > CabinetPage);
    pageShown(curPage);
}

void MainWindow::pageShown(int page)
{
    switch (page)
    {
        // WELCOME
    case AuthPage:
        ui->lineEditToken->setText(network._token);
        ui->statusAuthText->setText("Выполните аутентификацию");
        ui->authButton->setEnabled(true);
        clearAuthInfoPage();
        if(lastPage == AuthPage && settings->value("autologin", false).toBool() && ui->checkAutoLogin->isChecked())
            ui->authButton->click();
        break;
    case DevicesPage:
        if(nullptr == currentService || nullptr == currentService->handler || currentService->handler->deviceType() != ADB)
        {
            QMessageBox::warning(this, "Service is not connected", "Service module is no load.");
            showPage(AuthPage);
            return;
        }
        static struct
        {
            bool switched;
        } tempStruct;
        // Unset
        tempStruct = {};
        deviceLeftAnimator->setDirection(QPropertyAnimation::Forward);

        delayTimer(1000);

        delayPushLoop(300, [this,&tempStruct]()->bool{
            if(!tempStruct.switched)
            {
                QList<AdbDevice> devices = Adb::getDevices();
                for(const AdbDevice& device : std::as_const(devices))
                {
                    AdbConStatus status = Adb::deviceStatus(device.devId);
                    if(status == DEVICE)
                    {
                        connectPhone.isAuthed = status == DEVICE;
                        connectPhone.adbDevice = device;
                        currentService->handler->setDevice(device);
                        break;
                    }
                }
            }
            if(currentService->handler->canStart() && !tempStruct.switched)
            {
                tempStruct.switched = true;
                deviceLeftAnimator->start();
                delayTimer(2000);
                showPageLoader(MalwarePage);
            }
            if(curPage != DevicesPage)
            {
                deviceLeftAnimator->stop();
                ui->device_left_group->setMaximumWidth(QWIDGETSIZE_MAX);
            }
            return curPage == DevicesPage;
        });

        break;
    case CabinetPage:
    {
        ui->scrollArea->verticalScrollBar()->setValue(0);
        ui->scrollArea_3->verticalScrollBar()->setValue(0);
        fillAuthInfoPage();
        if(currentService)
        {
            currentService->handler->reset();
            currentService->handler->stop();
            currentService.reset();
        }
        break;
    }
    case MalwarePage:
    {
        QStringListModel *model = static_cast<QStringListModel *>(ui->processLogStatus->model());
        ui->processBarStatus->setValue(0);
        ui->malwareStatusText0->setText("Удаление рекламы не запущена.");
        malwareProgressCircle->setValue(0);
        malwareProgressCircle->setMaximum(100);
        malwareProgressCircle->setInfinilyMode(false);
        currentService->handler->reset();
        QStringList place{};
        place << "<< Во время процесса не отсоединяйте устройство от компьютера >>";

        // TODO: set auto start mode flag.
        // IF THERE AUTO_START = YES?

        if(!currentService->handler->canStart())
        {
            place << "Внутреняя ошибка, сервис не может быть запущен. Нажмите назад, чтобы повторить попытку.";
        }
        else
        {
            place << QString("<< Ожидаем >>").arg(currentService->title);

            delayPush(500, [this](){currentService->handler->start();});
        }

        model->setStringList(place);
        break;
    }
    default:
        break;
    }
}

void MainWindow::clearAuthInfoPage()
{
    int x,y;
    QStandardItemModel *model = new QStandardItemModel(ui->authInfo);
    model->setRowCount(7);
    model->setColumnCount(2);

    model->setHorizontalHeaderItem(0, new QStandardItem("Параметр"));
    model->setHorizontalHeaderItem(1, new QStandardItem("Значение"));

    model->setItem(0, 0, new QStandardItem("Логин"));
    model->setItem(0, 1, new QStandardItem("-"));

    model->setItem(1, 0, new QStandardItem("Последний вход"));
    model->setItem(1, 1, new QStandardItem("-"));

    model->setItem(2, 0, new QStandardItem("Баланс"));
    model->setItem(2, 1, new QStandardItem("-"));

    model->setItem(3, 0, new QStandardItem("VIP дней"));
    model->setItem(3, 1, new QStandardItem("-"));

    model->setItem(4, 0, new QStandardItem("Подключений"));
    model->setItem(4, 1, new QStandardItem("-"));

    model->setItem(5, 0, new QStandardItem("Расположение"));
    model->setItem(5, 1, new QStandardItem("-"));

    model->setItem(6, 0, new QStandardItem("Заблокирован"));
    model->setItem(6, 1, new QStandardItem("-"));

    ui->authInfo->setModel(model);
    ui->authInfo->horizontalHeader()->setStretchLastSection(true);
    ui->authInfo->verticalHeader()->setVisible(false);
    ui->authInfo->resizeColumnToContents(0);

    for(x = 0, y = ui->serviceContents->layout()->count(); x < y; ++x)
        ui->serviceContents->layout()->takeAt(0)->widget()->deleteLater();

    for(x = 0; x < services.count(); ++x)
    {
        services[x]->buttonWidget->deleteLater();
    }
    services.clear();
}

void MainWindow::fillAuthInfoPage()
{
    QString value;
    int x,y;
    QStandardItemModel *model = qobject_cast<QStandardItemModel *>(ui->authInfo->model());
    value = network.authedId.idName;
    model->item(0, 1)->setText(value);

    value = QDateTime::currentDateTime().toString(Qt::TextDate);
    model->item(1, 1)->setText(value);

    value = QString::number(network.authedId.credits) + " кредитов";
    model->item(2, 1)->setText(value);

    value = QString::number(network.authedId.vipDays);
    model->item(3, 1)->setText(value);

    value = QString::number(network.authedId.connectedDevices);
    model->item(4, 1)->setText(value);

    value = network.authedId.location;
    model->item(5, 1)->setText(value);

    value = network.authedId.blocked ? "Да" : "Нет";
    model->item(6, 1)->setText(value);

    ui->labelLoginAuthed->setText(network.authedId.idName);
    ui->labelCredits->setText(QString("%1\n(%2)").arg(network.authedId.credits).arg(network.authedId.currencyType));
    ui->labelVipDays->setText(QString("%1\n(VIP дней)").arg(network.authedId.vipDays));

    initModules();
}

void MainWindow::initModules()
{
    int x,y;

    if(!services.isEmpty())
        return;

    for(x = 0, y = static_cast<int>(sizeof(AvailableServices) / sizeof(AvailableServices[0])); x < y; ++x)
    {
        QString info;
        std::shared_ptr<ServiceItem> serviceItem = std::make_shared<ServiceItem>();

        info += AvailableServices[x].title;
        info += '\n';
        if(AvailableServices[x].active)
            if(network.authedId.hasVipAccount())
                info += "(безлимит)";
            else
                info += QString("%1 (%2)").arg(x == 0 ? network.authedId.basePrice : AvailableServices[x].price).arg(network.authedId.currencyType);
        else
            info += "(В разработке)";
        QPushButton * push = new QPushButton(QIcon(QString(":/resources/") + AvailableServices[x].iconName),
                                            info,
                                            ui->serviceContents);
        push->setStyleSheet("QPushButton {"
                            "   text-align: left;"
                            "   padding: 10px;"
                            "   font-size: 14px;"
                            "   border: 2px dashed black;"
                            "   background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #3d3d3d, stop: 1 #B71C1C);"
                            "   color: white;"
                            "}"
                            "QPushButton:hover {"
                            "   background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #B71C1C, stop: 1 #3d3d3d);"
                            "   color: white;"
                            "   border: 2px solid black;"
                            "}"
                            "QPushButton:pressed {"
                            "   background: #B71C1C;"
                            "   color: white;"
                            "   border: 2px solid black;"
                            "}");

        if(!AvailableServices[x].active)
            push->setStyleSheet( push->styleSheet() + "QPushButton { background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #3d3d3d, stop: 1 #232323); }" );

        serviceItem->title = AvailableServices[x].title;
        serviceItem->active = AvailableServices[x].active;
        serviceItem->buttonWidget = push;

        if(x == 0)
        {
            serviceItem->handler = static_cast<std::shared_ptr<Service>>(std::make_shared<AdsKillerService>(this));
            QObject::connect(push, &QPushButton::clicked, [this,serviceItem](){
                if(!serviceItem || !serviceItem->active)
                    return;
                startDeviceConnect(serviceItem->handler->deviceType(), serviceItem);
            });
        }
        push->setIconSize({50,50});
        push->setFixedSize(259,64);
        push->setEnabled(AvailableServices[x].active);
        services << serviceItem;

        // Adds a widget in the form of a grid
        static_cast<QGridLayout*>(ui->serviceContents->layout())->addWidget(push, x / 3, x % 3);
    }
}

void MainWindow::delayTimer(int ms)
{
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    timer.start(ms);
    loop.exec();
}

void MainWindow::delayPushLoop(int ms, std::function<bool()> call)
{
    QTimer *qtimer = new QTimer(this);
    (void)qtimer;
    qtimer->setSingleShot(false);
    qtimer->setInterval(ms);
    QObject::connect(
        qtimer,
        &QTimer::timeout,
        [qtimer, call]()
        {
            if(!call())
            {
                qtimer->stop();
                qtimer->deleteLater();
            }
        });
    qtimer->start();
}

void MainWindow::delayPush(int ms, std::function<void()> call)
{
    delayPushLoop(ms, [call]() -> bool { call(); return false;});
}

void MainWindow::startDeviceConnect(DeviceConnectType targetType, std::shared_ptr<ServiceItem> service)
{
    // TODO: use next any type <Target Type>| now use ADB
    if(currentService && currentService->handler && currentService->handler->isStarted())
    {
        // TODO: Show error is busy
        QMessageBox::critical(this, "Fatal module", "Module is already started.");
        return;
    }
    currentService = service;
    connectPhone = {};
    connectPhone.connectionType = targetType;
    showPageLoader(DevicesPage, 2000, QString("Запуск службы\n\"%1\"").arg(currentService->title));
}

bool MainWindow::accessUi_adskiller(QListView *& processLogStatusV, QLabel *& malareStatusText0V, QLabel *&deviceLabelNameV, QProgressBar *& processBarStatusV, QPushButton *& pushButtonReRun)
{
    processLogStatusV = ui->processLogStatus;
    malareStatusText0V = ui->malwareStatusText0;
    deviceLabelNameV = ui->deviceLabelName;
    processBarStatusV = ui->processBarStatus;
    pushButtonReRun = ui->malwareReRun;
    return true;
}

void MainWindow::on_authButton_clicked()
{
    if(network.pending() || network.isAuthed())
        return;

    constexpr int Dots = 3;
    QString tryToken = ui->lineEditToken->text();
    network.pushAuth(tryToken);
    ui->statusAuthText->setText("Подключение");
    timerAuthAnim = new QTimer(this);
    timerAuthAnim->start(350);

    qobject_cast<QWidget *>(sender())->setEnabled(false);
    ui->lineEditToken->setEnabled(false);
    QObject::connect(
        timerAuthAnim,
        &QTimer::timeout,
        [this]()
        {
            QString temp = ui->statusAuthText->text();
            int dotCount = std::accumulate(temp.begin(), temp.end(), 0, [](int count, const QChar &c)
                                           { return count += (c == '.' ? 1 : 0); });
            if (dotCount >= Dots)
                temp.remove(temp.length()-Dots,Dots);
            else
                temp += '.';
            ui->statusAuthText->setText(temp);
        });

    settings->setValue("autologin", ui->checkAutoLogin->isChecked());

    delayPushLoop(100, [this](){
        if(!network.pending() && network.isAuthed())
        {
            delayPush(2000, [this](){
                showPageLoader(CabinetPage);});
        }
        return network.pending();
    });
}

void MainWindow::setThemeAction()
{
    QList<QAction *> virtualSelectItems{ui->mThemeSystem, ui->mThemeLight, ui->mThemeDark};
    QAction *selfSender = qobject_cast<QAction *>(sender());
    int scheme;
    for (scheme = (0); scheme < virtualSelectItems.size() && selfSender != virtualSelectItems[scheme]; ++scheme)
        ;
    setTheme(static_cast<ThemeScheme>(scheme));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    tray->deleteLater();
    event->accept();
}

void MainWindow::replyAuthFinish(int status, bool ok)
{
    delayPush(
        1000,
        [ok, status, this]() -> void
        {
            timerAuthAnim->stop();

            QString resText;
            switch (status)
            {
            case 0:
                resText = "Токен успешно прошел проверку. Добро пожаловать, ";
                resText += network.authedId.idName;
                resText += "!";
                if (network.authedId.isNotValidBalance())
                {
                    ui->statusAuthText->setText("Закончился баланс, пополните, чтобы продолжить.");
                    showMessageFromStatus(NetworkStatus::NoEnoughMoney);
                }
                else
                {
                    ui->statusAuthText->setText("Аутентификация прошла успешно.");
                }

                if(network.authedId.blocked)
                {
                    ui->statusAuthText->setText("Аккаунт заблокирован");
                    showMessageFromStatus(NetworkStatus::AccountBlocked);
                }

                if(!network.authedId.token.isEmpty())
                    settings->setValue("encrypted_token", CipherAlgoCrypto::PackDC(network.authedId.token.toLatin1(), CipherAlgoCrypto::RandomKey()));

                break;
            case 401:
                resText = "Сервер вернул код 401 - не действительный токен или запрос обработан с ошибкой.";
                break;
            case NetworkStatus::NoEnoughMoney:
                resText = infoNoBalance;
                break;
            default:
                resText = "Проверьте интернет соединение.";
                break;
            }

            ui->statusAuthText->setText(resText);
            ui->lineEditToken->setEnabled(true);
            ui->authButton->setEnabled(true);

        });
}

void MainWindow::replyFetchVersionFinish(int status, const QString &version, const QString &url, bool ok)
{
    VersionInfo actualVersion = {{}, {}, status};
    if(status == NetworkStatus::NetworkError)
    {
        this->actualVersion = actualVersion;
        return;
    }

    actualVersion = {version, url, status};
    this->actualVersion = actualVersion;
    if(selfVersion.mVersion >= actualVersion.mVersion)
    {
        // TODO: Text update is latest
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
    text +=  selfVersion.mVersion.toString();
    text += "\nВерсия на сервере: v";
    text += actualVersion.mVersion.toString();
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
    if(QProcess::startDetached(tempdir.filePath(UpdateManagerExecute), QStringList() << QString("--dir") << appDir.path() << QString("--exec") << QCoreApplication::applicationFilePath()))
    {
        QApplication::quit();
        return;
    }
#endif
    QDesktopServices::openUrl(QUrl(url));

}

void MainWindow::replyAdsData(const QStringList &adsList, int status, bool ok)
{
    showMessageFromStatus(status);
}

void MainWindow::setTheme(ThemeScheme theme)
{
    int scheme;
    const char *resourceName;
    QList<QAction *> menus{ui->mThemeSystem, ui->mThemeLight, ui->mThemeDark};
    for (scheme = (0); scheme < menus.size(); ++scheme)
    {
        menus[scheme]->setChecked(theme == scheme);
    }

    switch (theme)
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

    QFile styleRes {};
    QString styleSheet {};
    if (resourceName)
    {
        styleRes.setFileName(resourceName);
        styleRes.open(QFile::ReadOnly | QFile::Text);
        styleSheet = styleRes.readAll();
        styleRes.close();
    }

    // Set application Design
    app->setStyleSheet(styleSheet);
    settings->setValue("theme", static_cast<int>(theme));
}

ThemeScheme MainWindow::getTheme()
{
    ThemeScheme scheme;
    scheme = static_cast<ThemeScheme>(settings->value("theme", static_cast<int>(ThemeScheme::Light)).toInt());
    return scheme;
}

void MainWindow::showMessageFromStatus(int statusCode)
{
    if (statusCode == NetworkStatus::NetworkError)
        QMessageBox::warning(this, "Ошибка подключения", infoNoNetwork);

    if (statusCode == NetworkStatus::NoEnoughMoney)
        QMessageBox::warning(this, "Сервер отклонил запрос", infoNoBalance);

    if(statusCode == NetworkStatus::AccountBlocked)
        QMessageBox::warning(this, "Сервер отклонил запрос", infoAccountBlocked);
}

void MainWindow::updateAuthInfoFromNet()
{
    if(!network.isAuthed())
    {
        logout();
        return;
    }

    QString tryToken = network.authedId.token;
    network.pushAuth(tryToken);
    showPageLoader(CabinetPage, 1000, [this]()->bool{
        ui->loaderPageText->setText("Обновление странницы");
        if(!network.pending() && !network.isAuthed())
        {
            delayPush(200, [this](){
                showPage(AuthPage);
            });
        }
        return !network.pending();
    });
}

void MainWindow::logout()
{
    if(network.isAuthed())
    {
        network.authedId = {};
    }
    showPageLoader(AuthPage, 500, QString("Выход из системы"));
}

