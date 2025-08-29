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

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "SystemTray.h"
#include "extension.h"
#include "AntiMalware.h"
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
    (ui->scrollAreaWidgetContents->layout())->addWidget((vPageSpacer = new QWidget(this)));
    vPageSpacerAnimator = new QPropertyAnimation(vPageSpacer, "maximumHeight");
    vPageSpacerAnimator->setDuration(900);
    vPageSpacerAnimator->setStartValue(1000);
    vPageSpacerAnimator->setEndValue(0);

    // Top header back to main page.
    ui->contentLayout->layout()->addWidget(ui->toplevel_backpage);

    for (x = 0; x < _w.count(); ++x)
        ui->contentLayout->layout()->addWidget(_w[x]);
    for (x = 0; x < ui->tabWidget_2->count(); ++x)
        malwareStatusLayouts << ui->tabWidget_2->widget(x);
    for (x = 0; x < malwareStatusLayouts.count(); ++x)
        ui->malwareContentLayout->addWidget(malwareStatusLayouts[x]);
    malwareStatusLayouts[0]->show();
    QObject::connect(ui->malwareLayoutSwitchButton, &QPushButton::clicked, [this](bool checked)
                     {
                         (void)checked;
                         for(auto & layout : malwareStatusLayouts)
                         {
                             layout->isHidden() ? layout->show() : layout->hide();
                         } });

    ui->tabWidget->deleteLater();
    ui->tabWidget_2->deleteLater();

    malwareProgressCircle = new ProgressCircle(this);
    malwareProgressCircle->setInfinilyMode(false);
    ui->progressCircleLayout->addWidget(malwareProgressCircle);

    loaderProgressCircle = new ProgressCircle(this);
    loaderProgressCircle->setInfinilyMode(true);
    loaderProgressCircle->setVisibleText(false);
    loaderProgressCircle->setInnerRadius(0);
    loaderProgressCircle->setColor(Qt::darkCyan);
    loaderProgressCircle->setInnerRadius(.5);
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
    QObject::connect(ui->authpageUpdate, &QPushButton::clicked, [this](){ ui->authButton->click(); showPageLoader((network.checkNet() ? CabinetPage : AuthPage), 1000, QString("Обновление странницы")); });
    QObject::connect(ui->buttonBackTo, &QPushButton::clicked, [this](){showPageLoader(CabinetPage);});

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
    checkVersion();
}

MainWindow::~MainWindow()
{
    malwareKill();
    delete ui;
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

void MainWindow::checkVersion()
{
#ifdef NDEBUG
    ui->loaderPageText->setText("Проверка обновления");
    // Show First Page
    showPageLoader(startPage, 1000, [this]()->bool {
        if(actualVersion.empty())
            return false;
        if(actualVersion.mStatus != NetworkStatus::OK)
        {
            ui->loaderPageText->setText("Проблема с интернетом?");
        }
        else
        {
            ui->loaderPageText->setText("Ваша версия актуальная!");
        }
        delayTimer(2000);
        return true;
    });

    network.fetchVersion(true);
#else
    showPageLoader(AuthPage);
#endif
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
        break;
    case DevicesPage:
        if(nullptr == currentService || nullptr == currentService->handler || currentService->handler->deviceType() != ADB)
        {
            QMessageBox::warning(this, "Service is not connected", "Service module is no load.");
            showPage(AuthPage);
            return;
        }
        delayPushLoop(96, [this]()->bool{
            QList<AdbDevice> devices = Adb::getDevices();
            devices << AdbDevice{};
            devices.back().devId = "asdasdasd";
            for(const AdbDevice& device : std::as_const(devices))
            {
                AdbConStatus status = Adb::deviceStatus(device.devId);
                if(status == DEVICE || true)
                {
                    currentService->handler->setDevice(device);
                    break;
                }
            }
            if(currentService->handler->canStart())
            {
                showPageLoader(MalwarePage);
            }
            return curPage == DevicesPage;
        });

        break;
    case CabinetPage:
    {
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
        place << "<< Все готово для запуска >>";
        place << "<< Во время процесса не отсоединяйте устройство от компьютера >>";
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
    (void)targetType;
    // TODO: use next any type <Target Type>| now use ADB
    if(currentService && currentService->handler && currentService->handler->isStarted())
    {
        // TODO: Show error is busy
        QMessageBox::critical(this, "Fatal module", "Module is already started.");
        return;
    }
    currentService = service;
    showPageLoader(DevicesPage, 2000, QString("Запуск службы\n\"%1\"").arg(service->title));
}

bool MainWindow::accessUi_adskiller(QListView *& processLogStatusV, QLabel *& malareStatusText0V, QLabel *&deviceLabelNameV, QProgressBar *& processBarStatusV)
{
    if(!processLogStatusV || !malareStatusText0V || !deviceLabelNameV || !processBarStatusV)
        return false;
    processLogStatusV = ui->processLogStatus;
    malareStatusText0V = ui->malwareStatusText0;
    deviceLabelNameV = ui->deviceLabelName;
    processBarStatusV = ui->processBarStatus;
    return true;
}

void MainWindow::on_authButton_clicked()
{
    constexpr int Dots = 3;
    QString tryToken = ui->lineEditToken->text();
    network.authenticate(tryToken);
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

            if(status == NetworkStatus::OK)
            {

                showPageLoader(CabinetPage);

            }

            ui->lineEditToken->setEnabled(true);
            ui->authButton->setEnabled(true);
        });
}

void MainWindow::replyFetchVersionFinish(int status, const QString &version, const QString &url, bool ok)
{
    VersionInfo actualVersion = {{}, {}, status};
    if(status == NetworkStatus::NetworkError)
    {
        this->showMessageFromStatus(status);
        delayPush(5000, [this](){this->close();});
        QMessageBox::question(this, "Нет соединение с интернетом", "Программа будет аварийно завершена через 5 секунд.", QMessageBox::StandardButton::Ok);
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

