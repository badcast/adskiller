#include <algorithm>
#include <cctype>
#include <list>

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
#include <QVBoxLayout>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "AppSystemTray.h"
#include "extension.h"
#include "Services.h"
#include "network.h"
#include "Strings.h"

constexpr struct {
    PageIndex index;
    const char * widgetName;
} PageConstNames[LengthPages] = {
    {AuthPage, "page_auth"},
    {CabinetPage, "page_cabinet"},
    {LongInfoPage, "page_adsmalware"},
    {LoaderPage, "page_loader"},
    {DevicesPage, "page_devices"},
    {MyDevicesPage, "page_mydevices"}
};

MainWindow * MainWindow::current;

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    int x;
    QStringListModel *model;
    ui->setupUi(this);

    // Load settings
    AppSetting::load();

    bool containsParam;
    QVariant value;
    value = AppSetting::encryptedToken(&containsParam);
    if (containsParam)
    {
        QByteArray decData = CipherAlgoCrypto::UnpackDC(value.toString());
        network._token = QLatin1String(decData);
    }

    value = AppSetting::autoLogin(&containsParam);
    if(containsParam)
    {
        ui->checkAutoLogin->setChecked(value.toBool());
    }
    else
    {
        ui->checkAutoLogin->setChecked(true);
    }

    value = AppSetting::networkTimeout(&containsParam);
    if(containsParam)
    {
        value = value.toInt() < 1000 ? 1000 : value.toInt() > 30000 ? 30000 : value;
    }
    else
    {
        value = NetworkTimeoutDefault;
    }
    AppSetting::networkTimeout(nullptr, value);
    network.setTimeout(value.toInt());

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
    QObject::connect(&network, &Network::sLoginFinish, this, &MainWindow::slotAuthFinish);
    QObject::connect(&network, &Network::sFetchingVersion, this, &MainWindow::slotFetchVersionFinish);
    QObject::connect(&network, &Network::sPullServiceList, this, &MainWindow::slotPullServiceList);

    QObject::connect(ui->authpageUpdate, &QPushButton::clicked, this, &MainWindow::updateCabinet);
    QObject::connect(ui->buttonBackTo, &QPushButton::clicked, this, &MainWindow::updateCabinet);
    QObject::connect(ui->logoutButton, &QPushButton::clicked, this, &MainWindow::logout);
    QObject::connect(ui->malwareReRun, &QPushButton::clicked, [this](){if(currentService && !currentService->isStarted()) currentService->start();});

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
    setTheme(static_cast<ThemeScheme>(static_cast<ThemeScheme>(std::clamp<int>(AppSetting::themeIndex(), 0, 2))));

    // Init tray
    tray = new AdsAppSystemTray(this);

    QString _version;
    _version += QString::number(AppVerMajor);
    _version += ".";
    _version += QString::number(AppVerMinor);
    _version += ".";
    _version += QString::number(AppVerPatch);

    runtimeVersion = { _version, {}, 0};

    // Run check version
#ifdef NDEBUG
    verChansesAvailable = -1;
#endif
    checkVersion(true);

    // ADD Snowflakes
    snows = new Snowflake(this, 50);

    // QVBoxLayout * vboxLayout = new QVBoxLayout(snows);
    // vboxLayout->setContentsMargins(0,0,0,0);
    // vboxLayout->setSpacing(0);
    // vboxLayout->addWidget(snows);
    ui->centralwidget_Layout->addWidget(snows, 0, 0, 0, 0);
    snows->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    snows->setSnowPixmap(QPixmap(":/resources/snowflake-image"));
}

MainWindow::~MainWindow()
{
    if(currentService)
    {
        currentService->stop();
    }
    delete ui;
    Adb::killServer();
    AppSetting::save();
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
                    DelayUISync(2000);
                    lastPage = PageIndex(-1);
                    willTerminate();
                }
                else
                {
                    verChansesAvailable = ChansesRunInvalid;
                    ui->loaderPageText->setText("Ваша версия актуальная!");
                    ui->loaderPageText->update();
                    DelayUISync(2000);
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
    if(curPage != LoaderPage)
        lastPage = curPage;
    if(pages.contains(curPage))
    {
        pages[curPage]->setEnabled(false);
        pages[curPage]->setVisible(false);
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
        if(lastPage == AuthPage && AppSetting::autoLogin() && !ui->lineEditToken->text().isEmpty() && ui->checkAutoLogin->isChecked())
            ui->authButton->click();
        break;
    case DevicesPage:
        if(nullptr == currentService || currentService->deviceConnectType() != ADB)
        {
            QMessageBox::warning(this, "Service is not connected", "Service module is no load.");
            logout();
            return;
        }

        // Unset
        deviceSelectSwitched = false;
        deviceLeftAnimator->setDirection(QPropertyAnimation::Forward);

        DelayUISync(1000);

        delayPushLoop(300, [this]()->bool{
            if(!deviceSelectSwitched)
            {
                QList<AdbDevice> devices = Adb::getDevices();
                for(const AdbDevice& device : std::as_const(devices))
                {
                    AdbConStatus status = Adb::deviceStatus(device.devId);
                    if(status == DEVICE)
                    {
                        connectPhone.isAuthed = status == DEVICE;
                        connectPhone.adbDevice = device;
                        currentService->setDevice(device);
                        break;
                    }
                }
            }
            if(currentService->canStart() && !deviceSelectSwitched)
            {
                deviceSelectSwitched = true;
                deviceLeftAnimator->start();
                DelayUISync(2000);
                showPageLoader(currentService->targetPage());
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
        ui->scrollArea_3->verticalScrollBar()->setValue(0);
        fillAuthInfoPage();
        if(currentService)
        {
            currentService->reset();
            currentService->stop();
            currentService.reset();
        }
        break;
    }
    case LongInfoPage:
    {
        QStringList place{};
        QStringListModel *model = static_cast<QStringListModel *>(ui->processLogStatus->model());
        ui->processBarStatus->setValue(0);
        ui->malwareStatusText0->setText("Ожидание запуска сервиса.");
        malwareProgressCircle->setValue(0);
        malwareProgressCircle->setMaximum(100);
        malwareProgressCircle->setInfinilyMode(false);
        currentService->reset();

        place << "<< Во время процесса не отсоединяйте устройство от компьютера >>";

        // TODO: set auto start mode flag.
        // IF THERE AUTO_START = YES?

        if(!currentService->canStart())
        {
            place << "Внутреняя ошибка, сервис не может быть запущен. Нажмите назад и повторите попытку.";
        }
        else
        {
            place << QString("<< Ожидаем >>").arg(currentService->title);

            delayPush(500, [this](){currentService->start();});
        }

        model->setStringList(place);
        break;
    }
    case MyDevicesPage:
        currentService->reset();
        currentService->start();
        break;
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
        services[x]->ownerWidget->deleteLater();

    serverServices.reset();
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

    initServiceModules();
}

void MainWindow::initServiceModules()
{
    QString tmp0;
    int x,y;

    if(!services.isEmpty() || !serverServices)
        return;

    std::shared_ptr<Service> instance = nullptr;
    std::list<std::shared_ptr<Service>> buildServices = Service::EnumAppServices(this);

    for(x = 0, y = serverServices->size(); x < y; ++x)
    {
        const ServiceItemInfo * serverServ = &(serverServices->at(x));
        if(serverServ->hide)
            continue;

        // Find build uuid service.
        for(auto iter = std::begin(buildServices); iter != std::end(buildServices); ++iter)
        {
            if(serverServ->uuid == (*iter)->uuid())
            {
                instance = std::move(*iter);
                buildServices.erase(iter);
                break;
            }
        }

        if(!instance)
            instance = std::make_shared<UnavailableService>(this);

        instance->active = serverServ->active && instance->isAvailable();

        tmp0 = serverServ->name + '\n';
        if(instance->active)
        {
            if(network.authedId.hasVipAccount() && serverServ->needVIP)
                tmp0 += "(безлимит)";
            else if(serverServ->price == 0)
                tmp0 += "(бесплатно)";
            else
                tmp0 += QString("%1 (%2)").arg(x == 0 ? network.authedId.basePrice : serverServ->price).arg(network.authedId.currencyType);
        }
        else
        {
            if(!instance->isAvailable())
                tmp0 += "(Не реализован)";
            else if(!serverServ->active)
                tmp0 += "(Не доступен)";
        }

        QPushButton * button = new QPushButton(QIcon(":/service-icons/"+instance->widgetIconName()),
                                              tmp0,
                                              ui->serviceContents);
        button->setStyleSheet("QPushButton {"
                              "   text-align: left;"
                              "   padding: 10px;"
                              "   font-size: 14px;"
                              "   background:  #B71C1C;"
                              "   color: white;"
                              "   border-radius: 16px;"
                              "   border: none;"
                              "}"
                              "QPushButton:hover {"
                              "   background: gray;"
                              "   color: white;"
                              "   border-color: #363636; "
                              "}"
                              "QPushButton:pressed {"
                              "   background: gray;"
                              "   padding-top: 10px;"
                              "   padding-bottom: 7px;"
                              "   color: white;"
                              "}");

        if(!instance->active)
            button->setStyleSheet(button->styleSheet() + "QPushButton { background: #4D4D4D;}");

        if(serverServ->uuid == IDServiceAdsString)
        {
            QObject::connect(button, &QPushButton::clicked, [this,instance](){
                if(!instance || !instance->active)
                    return;
                startDeviceConnect(instance->deviceConnectType(), instance);
            });
        }
        else if(serverServ->uuid == IDServiceMyDeviceString)
        {
            QObject::connect(button, &QPushButton::clicked, [this,instance](){
                if(!instance || !instance->active)
                    return;
                currentService = instance;
                showPageLoader(MyDevicesPage);
            });
        }
        else if(serverServ->uuid == IDServiceBoostRamString)
        {
            QObject::connect(button, &QPushButton::clicked, [this,instance](){
                if(!instance || !instance->active)
                    return;
                startDeviceConnect(instance->deviceConnectType(), instance);
            });
        }

        instance->title =  serverServ->name;

        instance->ownerWidget = button;

        button->setIconSize({70,70});
        button->setFixedSize(270,80);
        button->setEnabled(instance->active);
        services << std::move(instance);
    }
    std::sort(std::begin(services), std::end(services), [](const std::shared_ptr<Service>& lhs, const std::shared_ptr<Service>& rhs){return static_cast<int>(lhs->active) > static_cast<int>(rhs->active);});
    x = 0;
    for(const std::shared_ptr<Service> & item : std::as_const(services))
    {
        // Adds widget to a grid
        static_cast<QGridLayout*>(ui->serviceContents->layout())->addWidget(item->ownerWidget, x / 3, x % 3);
        ++x;
    }
    serverServices.reset();
}

void MainWindow::DelayUISync(int ms)
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

void MainWindow::startDeviceConnect(DeviceConnectType targetType, std::shared_ptr<Service> service)
{
    // TODO: use next any type <Target Type>| now use ADB
    if(targetType == DeviceConnectType::None)
    {
        QMessageBox::critical(this, "Fatal module", "Module is invalid type.");
        return;
    }

    if(currentService && currentService->isStarted())
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

bool MainWindow::accessUi_myDevices(QTableView *&tableActual, QDateEdit *& dateEditStart, QDateEdit *& dateEditEnd, QPushButton *& refreshButton, QCheckBox *&quaranteeFilter)
{
    tableActual = ui->myDeviceActual;
    dateEditStart = ui->myDeviceFilterDateStart;
    dateEditEnd = ui->myDeviceFilterDateEnd;
    refreshButton = ui->myDeviceSend;
    quaranteeFilter = ui->myDeviceQuaranteeFilter;
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

    AppSetting::autoLogin(nullptr, ui->checkAutoLogin->isChecked());

    delayPushLoop(100, [this](){
        if(!network.pending() && network.isAuthed())
        {
            delayPush(2000, [this](){
                showPage(CabinetPage);
                updateCabinet(false);
            });
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

void MainWindow::slotAuthFinish(int status, bool ok)
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
                resText = "Токен успешно прошел проверку. Добро пожаловать, %1!";
                resText = resText.arg(network.authedId.idName);
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
                    AppSetting::encryptedToken(nullptr, CipherAlgoCrypto::PackDC(network.authedId.token.toLatin1(), CipherAlgoCrypto::RandomKey()));

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

void MainWindow::slotPullServiceList(const QList<ServiceItemInfo>& services, bool ok)
{
    if(ok)
    {
        serverServices = std::move(std::make_shared<QList<ServiceItemInfo>>(services));
    }
}

void MainWindow::slotFetchVersionFinish(int status, const QString &version, const QString &url, bool ok)
{
    VersionInfo actualVersion = {{}, {}, status};
    if(status == NetworkStatus::NetworkError)
    {
        this->actualVersion = actualVersion;
        return;
    }

    actualVersion = {version, url, status};
    this->actualVersion = actualVersion;
    if(runtimeVersion.mVersion >= actualVersion.mVersion)
    {
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
    text +=  runtimeVersion.mVersion.toString();
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

void MainWindow::showEvent(QShowEvent *event)
{
    delayPush(50, [this](){snows->start();});
    event->accept();
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
        if(!styleRes.open(QFile::ReadOnly | QFile::Text))
        {
            QMessageBox::warning(this, "FAIL", "Set theme failed. Default to SYSTEM theme");
        }
        else
        {
            styleSheet = styleRes.readAll();
        }
        styleRes.close();
    }

    // Set application Design
    app->setStyleSheet(styleSheet);
    AppSetting::themeIndex(nullptr, static_cast<int>(theme));
}

ThemeScheme MainWindow::getTheme()
{
    ThemeScheme scheme;
    scheme = static_cast<ThemeScheme>(AppSetting::themeIndex());
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

void MainWindow::updateCabinet(bool newAuthenticate)
{
    QString tryToken;
    if(!network.isAuthed())
    {
        logout();
        return;
    }

    tryToken = network.authedId.token;
    if(newAuthenticate)
        network.pushAuth(tryToken);

    serverServices.reset();
    network.pullServiceList();

    showPageLoader(CabinetPage, 1000, [this]()->bool{
        bool status = !network.pending();
        const char * str = "Обновление странницы";
        if(!serverServices)
        {
            str = "Еще чуть-чуть";
        }
        ui->loaderPageText->setText(str);
        if(status && (!serverServices || !network.isAuthed()))
        {
            delayPush(1, [this](){
                logout();
            });
        }
        return status;
    });
}

void MainWindow::logout()
{
    if(network.isAuthed())
        network.authedId = {};
    clearAuthInfoPage();
    showPageLoader(AuthPage, 500, QString("Выход из системы"));
}

void MainWindow::runService(std::shared_ptr<Service> service)
{

}

