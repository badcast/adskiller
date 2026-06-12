#include "mainwindow.h"
#include <QEventLoop>
#include <QTimer>
#include <QMessageBox>

MainWindow *MainWindow::current = nullptr;

QVariantList MainWindow::serviceList() const
{
    QVariantList list;
    for(const auto &s : services)
    {
        QVariantMap map;
        map["title"] = s->title;
        map["icon"] = s->widgetIconName();
        map["active"] = s->active;
        list << map;
    }
    return list;
}

QObject *MainWindow::activeService() const
{
    return ServiceProvider::currentService().get();
}

MainWindow::MainWindow(QObject *parent) : QObject(parent), timerAuthAnim(nullptr)
{
    current = this;
    AppSetting::load();

    malwareProgressCircle = new ProgressCircle();
    loaderProgressCircle = new ProgressCircle();
    processLogStatus = new QListView();
    malwareStatusText0 = new QLabel();
    deviceLabelName = new QLabel();
    processBarStatus = new QProgressBar();
    malwareReRun = new QPushButton();
    myDeviceActual = new QTableView();
    myDeviceFilterDateStart = new QDateEdit();
    myDeviceFilterDateEnd = new QDateEdit();
    myDeviceSend = new QPushButton();
    myDeviceQuaranteeFilter = new QCheckBox();
    comboBoxSelectVIPDays = new QComboBox();
    labelVipBalance = new QLabel();
    buttonBuyVip = new QPushButton();
    labelInfoVip = new QLabel();

    bool paramCheck;
    std::tuple<QString, QString> _ps = AppSetting::loginAndPass(&paramCheck);
    m_savedLogin = std::get<0>(_ps);
    m_savedPassword = std::get<1>(_ps);

    versionChecker = new QTimer(this);
    versionChecker->setSingleShot(true);
    versionChecker->setInterval(VersionCheckRate);

    QObject::connect(&network, &Network::sLoginFinish, this, &MainWindow::slotAuthFinish);
    QObject::connect(&network, &Network::sFetchingVersion, this, &MainWindow::slotFetchVersionFinish);
    QObject::connect(&network, &Network::sPullServiceList, this, &MainWindow::slotPullServiceList);

    QObject::connect(versionChecker, &QTimer::timeout, [this]() { checkVersion(false); });

    QTimer *networkPollTimer = new QTimer(this);
    QObject::connect(
        networkPollTimer,
        &QTimer::timeout,
        this,
        [this]()
        {
            static bool lastPending = false;
            bool currentPending = network.pending();
            if(currentPending != lastPending)
            {
                lastPending = currentPending;
                emit networkPendingChanged();
            }
        });
    networkPollTimer->start(100);

    tray = new AdsAppSystemTray(nullptr);
}

MainWindow::~MainWindow()
{
    ServiceProvider::closeService();
    Adb::killServer();
    AppSetting::save();
}

#include <QDesktopServices>
#include <QMessageBox>
#include "Strings.h"

void MainWindow::openSupport()
{
    QDesktopServices::openUrl(QUrl(QString(QByteArray::fromBase64(acceptLinkWaMe))));
}

void MainWindow::openAbout()
{
    QMessageBox::information(nullptr, "О программе", infoMessage);
}

// removed delayUI

void MainWindow::delayUICallLoop(int ms, std::function<bool()> callFalseEnd)
{
    QTimer *qtimer = new QTimer(this);
    qtimer->setInterval(ms);
    QObject::connect(
        qtimer,
        &QTimer::timeout,
        [qtimer, callFalseEnd]()
        {
            if(!callFalseEnd())
            {
                qtimer->stop();
                qtimer->deleteLater();
            }
        });
    qtimer->start();
}

void MainWindow::delayUICall(int ms, std::function<void()> call)
{
    delayUICallLoop(
        ms,
        [call]() -> bool
        {
            call();
            return false;
        });
}

void MainWindow::login(const QString &user, const QString &pass)
{
    network.forclyExit = false;
    if(network.pending() || network.isAuthed())
        return;
    network.pushLoginPass(user, pass);
    setStatusAuthText("Авторизация...");
}

void MainWindow::slotAuthFinish(int status, bool ok)
{
    static bool fetchingProfile = false;

    if(ok && status == NetworkStatus::OK)
    {
        if(network.authedId.idName.isEmpty() && !fetchingProfile)
        {
            fetchingProfile = true;
            network.pushAuthToken();
            return;
        }
        fetchingProfile = false;

        QString welcomeStr = network.authedId.idName.isEmpty() ? "Добро пожаловать!" : "Добро пожаловать " + network.authedId.idName + "!";
        setStatusAuthText(welcomeStr);
        QTimer::singleShot(
            1500,
            this,
            [this]()
            {
                emit pageChangeRequested(CabinetPage);
                network.pullFetchVersion(false);
                network.pullServiceList();
            });
    }
    else
    {
        fetchingProfile = false;
        setStatusAuthText("Ошибка");
    }
}

void MainWindow::slotFetchVersionFinish(int status, const QString &version, const QString &url, bool ok)
{
    // simplified
}

void MainWindow::slotPullServiceList(const QList<ServiceItemInfo> &servicesList, bool ok)
{
    if(ok)
    {
        serverServices = std::make_shared<QList<ServiceItemInfo>>(servicesList);
        initServiceModules();
    }
}

void MainWindow::updateCabinet()
{
    emit authInfoChanged();
}

void MainWindow::logoutSystem()
{
    network.forclyExit = true;
    if(network.isAuthed())
    {
        network._token = {};
        network.authedId = {};
        clearAuthInfoPage();
        emit pageChangeRequested(AuthPage);
    }
    else
    {
        emit pageChangeRequested(AuthPage);
    }
}

void MainWindow::showMessageFromStatus(int statusCode)
{
    //
}

void MainWindow::checkVersion(bool firstRun)
{
    //
}

void MainWindow::willTerminate()
{
    //
}

void MainWindow::showPageLoader(PageIndex pageNum, int msWait, QString text)
{
    showPage(pageNum);
}

void MainWindow::showPageLoader(PageIndex pageNum, int msWait, std::function<bool()> predFalseEnd, QString text)
{
    if(predFalseEnd())
    {
        showPage(pageNum);
        emit activeServiceChanged();
    }
    else
    {
        emit pageChangeRequested(CabinetPage);
    }
}

void MainWindow::showPage(PageIndex pageNum)
{
    curPage = pageNum;
    emit pageChangeRequested(pageNum);

    QString qmlPage;
    if(pageNum == LongInfoPage)
        qmlPage = "LongInfoPage";
    else if(pageNum == BuyVIPPage)
        qmlPage = "BuyVIPPage";
    else if(pageNum == MyDevicesPage)
        qmlPage = "MyDevicesPage";
    else if(pageNum == DevicesPage)
        qmlPage = "DevicesPage";
    else if(pageNum == ContactFixerPage)
        qmlPage = "ContactFixerPage";
    else if(pageNum == FileExplorerPage)
        qmlPage = "FileExplorerPage";

    if(!qmlPage.isEmpty())
    {
        emit openServicePage(qmlPage);
    }
}

void MainWindow::clearAuthInfoPage()
{
    services.clear();
}

void MainWindow::fillAuthInfoPage()
{
    emit authInfoChanged();
}

void MainWindow::initServiceModules()
{
    if(!services.isEmpty() || !serverServices)
        return;
    auto buildServices = Service::EnumAppServices(this);
    for(auto remoteService : *serverServices)
    {
        if(remoteService.hide)
            continue;

        // Find matching uuid
        std::shared_ptr<Service> instance = nullptr;
        for(auto it = buildServices.begin(); it != buildServices.end(); ++it)
        {
            if(remoteService.uuid == (*it)->uuid())
            {
                instance = *it;
                buildServices.erase(it);
                break;
            }
        }
        if(!instance)
            instance = std::make_shared<UnavailableService>(this);

        if(instance->uuid() == IDServiceContactFixerString || instance->uuid() == "f0000000-0000-0000-0000-000000000001")
        {
            instance->active = true;
        }
        else
        {
            instance->active = remoteService.active && instance->isAvailable();
        }
        instance->title = remoteService.name;
        if(instance->uuid() == "f0000000-0000-0000-0000-000000000001")
        {
            instance->title = "Файловый менеджер";
        }
        services.append(instance);
    }

    // Force append FileExplorer if it wasn't matched (since it's a mock UUID, it won't be in remoteServices)
    bool hasFileExplorer = false;
    for(auto s : services)
    {
        if(s->uuid() == "f0000000-0000-0000-0000-000000000001")
            hasFileExplorer = true;
    }
    if(!hasFileExplorer)
    {
        for(auto it = buildServices.begin(); it != buildServices.end(); ++it)
        {
            if((*it)->uuid() == "f0000000-0000-0000-0000-000000000001")
            {
                (*it)->active = true;
                (*it)->title = "Файловый менеджер";
                services.append(*it);
                break;
            }
        }
    }

    emit servicesChanged();
    emit authInfoChanged();
}

void MainWindow::runServiceQml(int index)
{
    if(index >= 0 && index < services.size())
    {
        ServiceProvider::runService(services[index]);
    }
}

QVariantList MainWindow::getAdbDevices() const
{
    QVariantList list;
    const auto adbList = Adb::getDevices();
    for(const auto &dev : adbList)
    {
        QVariantMap map;
        map["devId"] = dev.devId;
        map["displayName"] = dev.displayName;
        map["model"] = dev.model;
        map["vendor"] = dev.vendor;
        list.append(map);
    }
    return list;
}

void MainWindow::selectAdbDevice(const QString &devId)
{
    if(ServiceProvider::currentService())
    {
        AdbDevice device = Adb::getDevice(devId);
        ServiceProvider::currentService()->setArgs(device);
        showPageLoader(ServiceProvider::currentService()->targetPage(), 1000, []() { return ServiceProvider::currentService()->start(); }, QString("Запуск службы\n\"%1\"").arg(ServiceProvider::currentService()->title));
    }
}

void MainWindow::closeService()
{
    ServiceProvider::closeService();
    emit activeServiceChanged();
}
