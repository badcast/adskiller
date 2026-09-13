#include <functional>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <list>
#include <memory>

#include <QCloseEvent>
#include <QDesktopServices>
#include <QEasingCurve>
#include <QEventLoop>
#include <QFontDatabase>
#include <QFuture>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStringListModel>
#include <QTableView>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QVector>
#include <QWheelEvent>
#include <QRandomGenerator>

#include "AppSystemTray.h"
#include "Services.h"
#include "Strings.h"
#include "extension.h"
#include "mainwindow.h"
#include "network.h"
#include "AIChatView.h"
#include "about_dialog.h"
#include "ui_mainwindow.h"
#include "FileManagerWidget.h"
#include "ApkManagerWidget.h"
#include "ContactFixerWidget.h"
#include "AdbDeviceVisualizer.h"

MainWindow *MainWindow::current;

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow), timerAuthAnim(nullptr)
{
    QStringListModel *model;
    ui->setupUi(this);

    this->setMinimumSize(1060, 600);
    this->resize(1180, 680);

    // Load settings
    AppSetting::load();

    bool paramCheck;
    QVariant value;

    // V1 Old Token ID
    AppSetting::removeEncToken();
    // value = AppSetting::encryptedToken(&paramCheck);

    // V2 - newer JWT
    std::tuple<QString, QString> _ps = AppSetting::loginAndPass(&paramCheck);
    if(paramCheck)
    {
        ui->lineLoginEdit->setText(std::get<0>(_ps));
        ui->linePassEdit->setText(std::get<1>(_ps));
    }

    value = AppSetting::autoLogin(&paramCheck);
    if(paramCheck)
    {
        ui->checkAutoLogin->setChecked(value.toBool());
    }
    else
    {
        ui->checkAutoLogin->setChecked(true);
    }

    value = AppSetting::networkTimeout(&paramCheck);
    if(paramCheck)
    {
        value = value.toInt() < 1000 ? 1000 : value.toInt() > 60000 ? 60000 : value;
    }
    else
    {
        value = NetworkTimeoutDefault;
    }

    AppSetting::networkTimeout(nullptr, value);
    network.setTimeout(value.toInt());

    // Setup window layout, animated containers, and progress circles
    setupWindowLayoutAndAnim();

    QList<QAction *> menusTheme {ui->mThemeSystem, ui->mThemeLight, ui->mThemeDark};
    for(QAction *q : menusTheme)
    {
        q->setChecked(false);
        QObject::connect(q, &QAction::triggered, this, &MainWindow::setThemeAction);
    }

    model = new QStringListModel(ui->processLogStatus);
    ui->processLogStatus->setModel(model);

    versionChecker = new QTimer(this);
    versionChecker->setSingleShot(true);
    versionChecker->setInterval(VersionCheckRate);

    // Signals
    QObject::connect(&network, &Network::sLoginFinish, this, &MainWindow::slotAuthFinish);
    QObject::connect(&network, &Network::sFetchingVersion, this, &MainWindow::slotFetchVersionFinish);
    QObject::connect(&network, &Network::sPullServiceList, this, &MainWindow::slotPullServiceList);

    QObject::connect(ui->authpageUpdate, &QPushButton::clicked, this, &MainWindow::updateCabinet);
    QObject::connect(ui->buttonBackTo, &QPushButton::clicked, this, &MainWindow::updateCabinet);
    QObject::connect(ui->logoutButton, &QPushButton::clicked, this, &MainWindow::logoutSystem);
    QObject::connect(
        ui->malwareReRun,
        &QPushButton::clicked,
        [this]()
        {
            if(ServiceProvider::currentService() && !ServiceProvider::currentService()->isStarted())
                ServiceProvider::currentService()->start();
        });
    QObject::connect(versionChecker, &QTimer::timeout, this, [this]() { checkVersion(false); });

    // Set Default Theme DARK ONLY
    ui->menu_4->deleteLater();

    setTheme(ThemeScheme::Dark);

    QString _version;
    _version += QString::number(AppVerMajor);
    _version += ".";
    _version += QString::number(AppVerMinor);
    _version += ".";
    _version += QString::number(AppVerPatch);

    runtimeVersion = {_version, {}, 0};

    // Run check version
#ifdef NDEBUG
    verChansesAvailable = -1;
#endif
    checkVersion(true);

    // Init tray
    tray = new AdsAppSystemTray(this);

    // Apply convenient modern design for all pages
    setupPagesDesign();

    // Modern AI Panel configuration
    setupAiPanel();

    // Top Radio Station Player
    setupRadioPlayer();
}

MainWindow::~MainWindow()
{
    ServiceProvider::closeService();
    Adb::killServer();
    AppSetting::save();
    delete ui;
}

void MainWindow::initServiceModules()
{
    QString tmp0;
    int x, y;

    if(!services.isEmpty() || !serverServices)
        return;

    std::shared_ptr<Service> instance = nullptr;
    std::list<std::shared_ptr<Service>> buildServices = Service::EnumAppServices(this);

    for(x = 0, y = serverServices->size(); x < y; ++x)
    {
        const ServiceItemInfo *remoteService = &(serverServices->at(x));
        if(remoteService->hide)
            continue;

        // Find build uuid service.
        for(auto iter = std::begin(buildServices); iter != std::end(buildServices); ++iter)
        {
            if(remoteService->uuid == (*iter)->uuid())
            {
                instance = std::move(*iter);
                buildServices.erase(iter);
                break;
            }
        }

        if(!instance)
            instance = std::make_shared<UnavailableService>(this);

#if SHOW_SERVICE_BY_DEBUG
        instance->active = instance->isAvailable(); // EVERYTHING TRUE
#else
        instance->active = remoteService->active && instance->isAvailable();
#endif

        QString badgeText;
        QString styleSheet;
        uint32_t effectivePrice = (x == 0 && network.authedId.basePrice > 0 ? network.authedId.basePrice : remoteService->price);
        bool hasVIP = network.authedId.hasVipAccount();
        bool isDynamic = (remoteService->price == static_cast<std::uint32_t>(-1));
        bool isFree = (effectivePrice == 0);
        bool canBypassWithVip = (remoteService->needVIP && hasVIP);

        if(!instance->active)
        {
            if(!instance->isAvailable())
                badgeText = QString::fromUtf8("🔧 В разработке");
            else if(!remoteService->active)
                badgeText = QString::fromUtf8("⛔ Не доступен");
            else
                badgeText = QString::fromUtf8("⛔ Отключен");

            styleSheet = "QPushButton {"
                         "   text-align: left;"
                         "   padding: 10px 14px 10px 12px;"
                         "   font-size: 12px;"
                         "   font-weight: 500;"
                         "   background-color: #17181C;"
                         "   color: #64748B;"
                         "   border-radius: 12px;"
                         "   border: 1px solid #282A30;"
                         "   border-left: 5px solid #475569;"
                         "}";
        }
        else if(canBypassWithVip)
        {
            // Service supports VIP and user HAS VIP account -> Unlimited access
            badgeText = QString::fromUtf8("👑 VIP • БЕЗЛИМИТ");

            styleSheet = "QPushButton {"
                         "   text-align: left;"
                         "   padding: 10px 14px 10px 12px;"
                         "   font-size: 12px;"
                         "   font-weight: bold;"
                         "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #2C2213, stop:0.5 #231B0E, stop:1 #1A140A);"
                         "   color: #FEF3C7;"
                         "   border-radius: 12px;"
                         "   border: 1px solid rgba(245, 158, 11, 0.45);"
                         "   border-left: 5px solid #F59E0B;"
                         "}"
                         "QPushButton:hover {"
                         "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #3D2E17, stop:0.5 #312411, stop:1 #241A0B);"
                         "   border-color: #FBBF24;"
                         "   border-left: 5px solid #FCD34D;"
                         "   color: #FFFFFF;"
                         "}"
                         "QPushButton:pressed {"
                         "   background: #140E05;"
                         "   border-color: #D97706;"
                         "}";
        }
        else if(isFree)
        {
            // Completely free service for everyone
            badgeText = QString::fromUtf8("БЕСПЛАТНО");

            styleSheet = "QPushButton {"
                         "   text-align: left;"
                         "   padding: 10px 14px 10px 12px;"
                         "   font-size: 12px;"
                         "   font-weight: bold;"
                         "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #15271D, stop:0.5 #112017, stop:1 #0D1912);"
                         "   color: #D1FAE5;"
                         "   border-radius: 12px;"
                         "   border: 1px solid rgba(52, 211, 153, 0.4);"
                         "   border-left: 5px solid #10B981;"
                         "}"
                         "QPushButton:hover {"
                         "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #1E382A, stop:0.5 #182D21, stop:1 #122219);"
                         "   border-color: #34D399;"
                         "   border-left: 5px solid #6EE7B7;"
                         "   color: #FFFFFF;"
                         "}"
                         "QPushButton:pressed {"
                         "   background: #09120D;"
                         "   border-color: #059669;"
                         "}";
        }
        else if(isDynamic)
        {
            badgeText = remoteService->needVIP ? QString::fromUtf8("⚙ ТАРИФ НА ВЫБОР (или 👑 VIP)") : QString::fromUtf8("⚙ ТАРИФ НА ВЫБОР");

            styleSheet = "QPushButton {"
                         "   text-align: left;"
                         "   padding: 10px 14px 10px 12px;"
                         "   font-size: 12px;"
                         "   font-weight: bold;"
                         "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #1A2232, stop:0.5 #141B28, stop:1 #0F141F);"
                         "   color: #E0F2FE;"
                         "   border-radius: 12px;"
                         "   border: 1px solid rgba(56, 189, 248, 0.4);"
                         "   border-left: 5px solid #38BDF8;"
                         "}"
                         "QPushButton:hover {"
                         "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #233047, stop:0.5 #1C2638, stop:1 #151D2C);"
                         "   border-color: #38BDF8;"
                         "   border-left: 5px solid #7DD3FC;"
                         "   color: #FFFFFF;"
                         "}"
                         "QPushButton:pressed {"
                         "   background: #0B0E16;"
                         "   border-color: #0284C7;"
                         "}";
        }
        else if(remoteService->needVIP)
        {
            // Service supports VIP, but user has NO VIP -> Show price in credits with VIP alternative
            badgeText = QString::fromUtf8("💳 %1 %2  (или 👑 VIP)").arg(effectivePrice).arg(network.authedId.currencyType);

            styleSheet = "QPushButton {"
                         "   text-align: left;"
                         "   padding: 10px 14px 10px 12px;"
                         "   font-size: 12px;"
                         "   font-weight: bold;"
                         "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #1C2333, stop:0.5 #161C2A, stop:1 #111520);"
                         "   color: #F0F9FF;"
                         "   border-radius: 12px;"
                         "   border: 1px solid rgba(245, 158, 11, 0.4);"
                         "   border-left: 5px solid #F59E0B;"
                         "}"
                         "QPushButton:hover {"
                         "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #253147, stop:0.5 #1D2638, stop:1 #161D2B);"
                         "   border-color: #FBBF24;"
                         "   border-left: 5px solid #FCD34D;"
                         "   color: #FFFFFF;"
                         "}"
                         "QPushButton:pressed {"
                         "   background: #0D121B;"
                         "   border-color: #D97706;"
                         "}";
        }
        else
        {
            // Service does NOT require/support VIP -> Standard credit price for all users
            badgeText = QString::fromUtf8("💳 %1 %2").arg(effectivePrice).arg(network.authedId.currencyType);

            styleSheet = "QPushButton {"
                         "   text-align: left;"
                         "   padding: 10px 14px 10px 12px;"
                         "   font-size: 12px;"
                         "   font-weight: bold;"
                         "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #1A2232, stop:0.5 #141B28, stop:1 #0F141F);"
                         "   color: #E0F2FE;"
                         "   border-radius: 12px;"
                         "   border: 1px solid rgba(56, 189, 248, 0.4);"
                         "   border-left: 5px solid #0284C7;"
                         "}"
                         "QPushButton:hover {"
                         "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #233047, stop:0.5 #1C2638, stop:1 #151D2C);"
                         "   border-color: #38BDF8;"
                         "   border-left: 5px solid #38BDF8;"
                         "   color: #FFFFFF;"
                         "}"
                         "QPushButton:pressed {"
                         "   background: #0B0E16;"
                         "   border-color: #0284C7;"
                         "}";
        }

        tmp0 = remoteService->name + "\n" + badgeText;

        if(instance->uuid() != IDServiceAIAgentString)
        {
            QPushButton *button = new QPushButton(QIcon(":/service-icons/" + instance->widgetIconName()), tmp0, ui->serviceContents);
            button->setCursor(instance->active ? Qt::PointingHandCursor : Qt::ForbiddenCursor);
            button->setStyleSheet(styleSheet);
            button->setIconSize({52, 52});
            button->setFixedSize(260, 80);
            button->setEnabled(instance->active);

            QString tip = remoteService->name;
            if(!remoteService->description.isEmpty())
                tip += "\n" + remoteService->description;
            if(instance->active)
            {
                if(canBypassWithVip)
                {
                    tip += "\n\n• Включено в VIP-подписку (безлимитно, без списания кредитов)";
                }
                else if(isFree)
                {
                    tip += "\n\n• Бесплатная услуга (0 кредитов)";
                }
                else if(remoteService->needVIP)
                {
                    tip += QString("\n\n• Стоимость: %1 %2 (или бесплатно с активным VIP-аккаунтом)").arg(effectivePrice).arg(network.authedId.currencyType);
                }
                else if(!isDynamic)
                {
                    tip += QString("\n\n• Стоимость: %1 %2 (оплата кредитами)").arg(effectivePrice).arg(network.authedId.currencyType);
                }
            }
            button->setToolTip(tip);

            // Target service by slot with credit verification
            QObject::connect(
                button,
                &QPushButton::clicked,
                this,
                [this, instance, effectivePrice, isDynamic, needVIP = remoteService->needVIP, name = remoteService->name]()
                {
                    bool bypass = (needVIP && network.authedId.hasVipAccount());
                    if(!bypass && effectivePrice > 0 && !isDynamic && network.authedId.credits < effectivePrice)
                    {
                        QMessageBox msgBox(this);
                        msgBox.setWindowTitle(QString::fromUtf8("Недостаточно кредитов"));
                        msgBox.setIcon(QMessageBox::Warning);

                        const int shortage = effectivePrice - static_cast<int>(network.authedId.credits);
                        const QString &currency = network.authedId.currencyType;

                        QString infoHtml = QString::fromUtf8(
                                               "<div style='min-width: 330px; font-family: Segoe UI, sans-serif; font-size: 13px; color: #E2E8F0;'>"
                                               "<p style='font-size: 15px; font-weight: bold; color: #F87171; margin-bottom: 8px;'>Недостаточно кредитов для запуска</p>"
                                               "<p style='margin-bottom: 10px;'>Для запуска сервиса <b style='color: #38BDF8;'>«%1»</b> на вашем балансе не хватает средств.</p>"
                                               "<table style='width: 100%; border-collapse: collapse; margin-bottom: 12px; background: rgba(30, 41, 59, 0.7); border: 1px solid rgba(148, 163, 184, 0.2); border-radius: 6px;'>"
                                               "<tr><td style='padding: 6px 10px; color: #94A3B8;'>Стоимость услуги:</td><td style='padding: 6px 10px; text-align: right; font-weight: bold; color: #F8FAFC;'>%2 %3</td></tr>"
                                               "<tr><td style='padding: 6px 10px; color: #94A3B8;'>На вашем балансе:</td><td style='padding: 6px 10px; text-align: right; font-weight: bold; color: #FBBF24;'>%4 %3</td></tr>"
                                               "<tr style='border-top: 1px solid rgba(148, 163, 184, 0.2);'><td style='padding: 6px 10px; color: #EF4444; font-weight: bold;'>Не хватает:</td><td style='padding: 6px 10px; text-align: right; font-weight: bold; color: #EF4444;'>%5 %3</td></tr>"
                                               "</table>"
                                               "%6"
                                               "</div>")
                                               .arg(name)
                                               .arg(effectivePrice)
                                               .arg(currency)
                                               .arg(network.authedId.credits)
                                               .arg(shortage > 0 ? shortage : 0)
                                               .arg(
                                                   needVIP ? QString::fromUtf8("<p style='margin: 4px 0 0 0; color: #CBD5E1; line-height: 1.4;'>💡 <i>Вы можете активировать <b>VIP-статус</b> для безлимитного доступа без списания кредитов, либо пополнить баланс через службу поддержки.</i></p>")
                                                           : QString::fromUtf8("<p style='margin: 4px 0 0 0; color: #CBD5E1; line-height: 1.4;'>💳 <i>Данная услуга оплачивается только кредитами (VIP-статус не поддерживается). Пополните баланс через раздел Поддержка.</i></p>"));

                        msgBox.setText(infoHtml);

                        msgBox.setStyleSheet(
                            "QMessageBox {"
                            "   background-color: #0F172A;"
                            "   color: #F8FAFC;"
                            "   border: 1px solid #334155;"
                            "   border-radius: 12px;"
                            "}"
                            "QLabel {"
                            "   color: #F8FAFC;"
                            "   background: transparent;"
                            "}"
                            "QPushButton {"
                            "   background-color: #334155;"
                            "   color: #F8FAFC;"
                            "   border: 1px solid #475569;"
                            "   border-radius: 6px;"
                            "   padding: 6px 16px;"
                            "   font-size: 12px;"
                            "   font-weight: bold;"
                            "   min-width: 85px;"
                            "}"
                            "QPushButton:hover {"
                            "   background-color: #475569;"
                            "   border-color: #64748B;"
                            "}"
                            "QPushButton:pressed {"
                            "   background-color: #1E293B;"
                            "}");

                        QPushButton *btnVip = nullptr;
                        QPushButton *btnSupport = nullptr;

                        if(needVIP)
                        {
                            btnVip = msgBox.addButton(QString::fromUtf8("👑 Оформить VIP"), QMessageBox::ActionRole);
                            btnVip->setCursor(Qt::PointingHandCursor);
                            btnVip->setStyleSheet(
                                "QPushButton {"
                                "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #D97706, stop:1 #F59E0B);"
                                "   color: #FFFFFF;"
                                "   font-size: 12px;"
                                "   font-weight: bold;"
                                "   border: none;"
                                "   border-radius: 6px;"
                                "   padding: 6px 16px;"
                                "   min-width: 125px;"
                                "}"
                                "QPushButton:hover {"
                                "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #F59E0B, stop:1 #FBBF24);"
                                "}"
                                "QPushButton:pressed {"
                                "   background: #B45309;"
                                "}");
                        }
                        else
                        {
                            btnSupport = msgBox.addButton(QString::fromUtf8("💬 Поддержка"), QMessageBox::ActionRole);
                            btnSupport->setCursor(Qt::PointingHandCursor);
                            btnSupport->setStyleSheet(
                                "QPushButton {"
                                "   background-color: #0284C7;"
                                "   color: #FFFFFF;"
                                "   font-size: 12px;"
                                "   font-weight: bold;"
                                "   border: none;"
                                "   border-radius: 6px;"
                                "   padding: 6px 16px;"
                                "   min-width: 110px;"
                                "}"
                                "QPushButton:hover {"
                                "   background-color: #0EA5E9;"
                                "}"
                                "QPushButton:pressed {"
                                "   background-color: #0369A1;"
                                "}");
                        }

                        QPushButton *btnClose = msgBox.addButton(needVIP ? QString::fromUtf8("Закрыть") : QString::fromUtf8("Понятно"), QMessageBox::RejectRole);
                        btnClose->setCursor(Qt::PointingHandCursor);
                        msgBox.setDefaultButton(btnVip ? btnVip : (btnSupport ? btnSupport : btnClose));

                        msgBox.exec();

                        if(needVIP && msgBox.clickedButton() == btnVip)
                        {
                            for(const auto &s : std::as_const(services))
                            {
                                if(s && s->uuid() == IDServiceVIPBuyString)
                                {
                                    if(s->active)
                                    {
                                        this->runService(s);
                                    }
                                    else
                                    {
                                        QMessageBox::warning(this, QString::fromUtf8("Пополнение VIP"), QString::fromUtf8("Сервис пополнения VIP временно недоступен."));
                                    }
                                    break;
                                }
                            }
                        }
                        else if(btnSupport && msgBox.clickedButton() == btnSupport)
                        {
                            this->on_action_WhatsApp_triggered();
                        }
                        return;
                    }
                    this->runService(instance);
                });

            instance->title = remoteService->name;
            instance->ownerWidget = button;
        }
        else
        {
            if(auto *aiService = qobject_cast<AIAgentService *>(instance.get()))
            {
                connect(
                    aiService,
                    &AIAgentService::onRunService,
                    this,
                    [this](const QString &service_uuid)
                    {
                        for(const auto &s : std::as_const(services))
                        {
                            if(s && s->uuid() == service_uuid)
                            {
                                if(s->active)
                                {
                                    runService(s);
                                }
                                break;
                            }
                        }
                    });
            }

            if(!instance->active)
            {
                ui->aiChatEdit->setDisabled(true);
                ui->aiChatSend->setDisabled(true);
                auto *chatView = findChild<AIChatView *>("aiChatView");
                if(chatView)
                    chatView->addAIMessage("К сожалению Сервис ИИ не доступен. Попробуйте позднее.");
            }
            else
            {
                instance->start(); // Auto start for AI
            }
        }

        services << std::move(instance);
    }
    std::sort(std::begin(services), std::end(services), [](const std::shared_ptr<Service> &lhs, const std::shared_ptr<Service> &rhs) { return static_cast<int>(lhs->active) > static_cast<int>(rhs->active); });

    QGridLayout *layoutSpace = qobject_cast<QGridLayout *>(ui->serviceContents->layout());
    if(layoutSpace)
    {
        layoutSpace->setSpacing(14);
        layoutSpace->setContentsMargins(10, 10, 10, 10);
    }

    x = 0;
    for(const std::shared_ptr<Service> &item : std::as_const(services))
    {
        // Adds widget to a grid
        if(item->uuid() != IDServiceAIAgentString)
        {
            static_cast<QGridLayout *>(ui->serviceContents->layout())->addWidget(item->ownerWidget, x / 2, x % 2);
            ++x;
        }
    }
    serverServices.reset();
}

void MainWindow::on_actionAboutUs_triggered()
{
    AboutDialog dlg(this);
    dlg.setCurrentTab(AboutDialog::TabAbout);
    dlg.exec();
}

void MainWindow::on_actionUsLic_triggered()
{
    AboutDialog dlg(this);
    dlg.setCurrentTab(AboutDialog::TabGplV3);
    dlg.exec();
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
        showPageLoader(
            startPage,
            100,
            [this]() -> bool
            {
                if(actualVersion.empty())
                    return false;
                if(lastPage == AuthPage)
                {
                    if(actualVersion.mStatus != NetworkStatus::OK)
                    {
                        ui->loaderPageText->setText("Проблема с интернетом?");
                        ui->loaderPageText->update();
                        lastPage = PageIndex(-1);
                        QTimer::singleShot(2000, this, [this]() { willTerminate(); });
                        return false;
                    }
                    else
                    {
                        verChansesAvailable = ChansesRunInvalid;
                        ui->loaderPageText->setText("Ваша версия актуальная!");
                        ui->loaderPageText->update();
                        versionChecker->start();
                        return true;
                    }
                }
                return false;
            });
    }
    else if(verChansesAvailable > -1)
    {
        delayUICallLoop(
            70,
            [this]()
            {
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
                            versionChecker->stop();
                            return false;
                        }
                        else
                        {
                            QString warnMessage = "У вас осталось попыток (%1), срочно восстановите связь, иначе "
                                                  "приложение аварийно завершится.";
                            warnMessage = warnMessage.arg(verChansesAvailable);
                            verChansesAvailable = qMax<int>(verChansesAvailable - 1, 0);
                            QMessageBox::warning(this, "Отсутствие соединение с интернетом.", warnMessage);
                        }
                    }
                    else
                    {
                        verChansesAvailable = ChansesRunInvalid;
                    }
                }
                versionChecker->start();
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
    delayUICall(5000, std::bind(&MainWindow::close, this));
    QMessageBox::critical(this, "Нет соединение с интернетом", "Программа будет аварийно завершена через 5 секунд.", QMessageBox::Ok);
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
    if(ui->toplevel_backpage->isVisible())
    {
        switch(pageNum)
        {
            case DevicesPage:
                ui->label_8->setText("Подключение Android-устройства (ADB)");
                break;
            case LongInfoPage:
                ui->label_8->setText(ServiceProvider::currentService() ? ServiceProvider::currentService()->title : "Выполнение процедуры");
                break;
            case MyDevicesPage:
                ui->label_8->setText("История устройств и гарантия");
                break;
            case BuyVIPPage:
                ui->label_8->setText("Оформление VIP-подписки");
                break;
            case FileManagerPage:
                ui->label_8->setText("Файловый менеджер ADB");
                break;
            case ApkManagerPage:
                ui->label_8->setText("APK Менеджер");
                break;
            case ContactFixerPage:
                ui->label_8->setText("Исправление контактов");
                break;
            case AITranslaterPage:
                ui->label_8->setText("ИИ-Переводчик документов");
                break;
            default:
                ui->label_8->setText("Назад в личный кабинет");
                break;
        }
    }
    pageShownPreStart(curPage);
}

void MainWindow::pageShownPreStart(int page)
{
    switch(page)
    {
            // WELCOME
        case AuthPage:
            ui->statusAuthText->setText("Выполните аутентификацию");
            ui->authButton->setEnabled(true);
            clearAuthInfoPage();
            if(lastPage == AuthPage && AppSetting::autoLogin() && !ui->linePassEdit->text().isEmpty() && ui->checkAutoLogin->isChecked())
                ui->authButton->click();
            break;
        case DevicesPage:
            if(nullptr == ServiceProvider::currentService())
            {
                QMessageBox::warning(this, "Service is not connected", "Service module is no load.");
                logoutSystem();
                return;
            }

            ServiceProvider::currentService()->stop();

            // Unset
            deviceSelectSwitched = false;
            deviceLeftAnimator->setDirection(QPropertyAnimation::Forward);

            if(s_adbVisualizer)
            {
                s_adbVisualizer->setStatus(UNKNOWN);
                s_adbVisualizer->startAnimation();
            }

            delayUI(500);

            delayUICallLoop(
                300,
                [this]() -> bool
                {
                    auto *vis = s_adbVisualizer;
                    if(!deviceSelectSwitched)
                    {
                        QList<AdbDevice> devices = Adb::getDevices();
                        if(devices.isEmpty())
                        {
                            if(vis && vis->status() != UNKNOWN)
                            {
                                vis->setStatus(UNKNOWN);
                                ui->label_5->setStyleSheet(
                                    "background-color: #0B1120;"
                                    "border: 1px solid #1E3A5F;"
                                    "border-radius: 10px;"
                                    "color: #38BDF8;"
                                    "font-size: 12.5px;"
                                    "font-weight: 600;"
                                    "padding: 10px;");
                                ui->label_5->setText(QString::fromUtf8("📡 Поиск подключенного Android-устройства..."));
                            }
                        }
                        else
                        {
                            bool hasAuth = false;
                            bool hasUnauth = false;
                            AdbDevice authDev;
                            AdbDevice unauthDev;

                            for(const AdbDevice &device : std::as_const(devices))
                            {
                                AdbConStatus status = Adb::deviceStatus(device.devId);
                                if(status == DEVICE)
                                {
                                    hasAuth = true;
                                    authDev = device;
                                    break;
                                }
                                else if(status == UNAUTH)
                                {
                                    hasUnauth = true;
                                    unauthDev = device;
                                }
                            }

                            if(hasAuth)
                            {
                                connectPhone.isAuthed = true;
                                connectPhone.adbDevice = authDev;
                                ServiceProvider::currentService()->setArgs(authDev);

                                QString devName = !authDev.marketingName.isEmpty() ? authDev.marketingName : (!authDev.displayName.isEmpty() ? authDev.displayName : (!authDev.model.isEmpty() ? authDev.model : authDev.devId));
                                QString devSub = !authDev.vendor.isEmpty() ? (authDev.vendor + " (" + authDev.model + ")") : authDev.devId;

                                if(vis && vis->status() != DEVICE)
                                {
                                    vis->setStatus(DEVICE, devName, devSub);
                                }
                                ui->label_5->setStyleSheet(
                                    "background-color: #064E3B;"
                                    "border: 1px solid #059669;"
                                    "border-radius: 10px;"
                                    "color: #34D399;"
                                    "font-size: 12.5px;"
                                    "font-weight: 600;"
                                    "padding: 10px;");
                                ui->label_5->setText(QString::fromUtf8("✅ Устройство подключено: %1! Запуск...").arg(devName));
                            }
                            else if(hasUnauth)
                            {
                                QString devName = !unauthDev.marketingName.isEmpty() ? unauthDev.marketingName : (!unauthDev.displayName.isEmpty() ? unauthDev.displayName : (!unauthDev.model.isEmpty() ? unauthDev.model : unauthDev.devId));

                                if(vis && vis->status() != UNAUTH)
                                {
                                    vis->setStatus(UNAUTH, devName);
                                }
                                ui->label_5->setStyleSheet(
                                    "background-color: #451A03;"
                                    "border: 1px solid #D97706;"
                                    "border-radius: 10px;"
                                    "color: #FBBF24;"
                                    "font-size: 12.5px;"
                                    "font-weight: 600;"
                                    "padding: 10px;");
                                ui->label_5->setText(QString::fromUtf8("⚠️ Нажмите «Разрешить отладку по USB» на экране телефона"));
                            }
                        }
                    }
                    if(ServiceProvider::currentService()->canStart() && !deviceSelectSwitched)
                    {
                        deviceSelectSwitched = true;
                        deviceLeftAnimator->start();
                        delayUI(1800);
                        showPageLoader(ServiceProvider::currentService()->targetPage());
                    }
                    if(curPage != DevicesPage)
                    {
                        deviceLeftAnimator->stop();
                        ui->device_left_group->setMaximumWidth(QWIDGETSIZE_MAX);
                        if(vis)
                            vis->stopAnimation();
                    }
                    return curPage == DevicesPage;
                });

            break;
        case CabinetPage:
        {
            ui->scrollArea_3->verticalScrollBar()->setValue(0);
            fillAuthInfoPage();
            break;
        }
        case LongInfoPage:
        {
            QStringList place {};
            QStringListModel *model = static_cast<QStringListModel *>(ui->processLogStatus->model());
            ui->processBarStatus->setValue(0);
            ui->malwareStatusText0->setText("Ожидание запуска сервиса.");
            malwareProgressCircle->setValue(0);
            malwareProgressCircle->setMaximum(100);
            malwareProgressCircle->setInfinilyMode(false);

            place << "<< Во время процесса не отсоединяйте устройство от компьютера >>";

            // TODO: set auto start mode flag.
            // IF THERE AUTO_START = YES?

            if(!ServiceProvider::currentService()->canStart())
            {
                place << "Внутреняя ошибка, сервис не может быть запущен. Нажмите назад "
                         "и повторите попытку.";
            }
            else
            {
                place << QString("<< Ожидаем >>").arg(ServiceProvider::currentService()->title);

                delayUICall(500, [this]() { ServiceProvider::currentService()->start(); });
            }

            model->setStringList(place);
            break;
        }
        case FileManagerPage:
        {
            if(pages.contains(FileManagerPage))
            {
                auto *widget = static_cast<FileManagerWidget *>(pages.value(FileManagerPage));
                if(widget)
                {
                    if(!connectPhone.adbDevice.isEmpty())
                        widget->setDevice(connectPhone.adbDevice);
                    widget->refreshList();
                }
            }
            break;
        }
        case ApkManagerPage:
        {
            if(pages.contains(ApkManagerPage))
            {
                auto *widget = static_cast<ApkManagerWidget *>(pages.value(ApkManagerPage));
                if(widget)
                {
                    if(!connectPhone.adbDevice.isEmpty())
                        widget->setDevice(connectPhone.adbDevice);
                    widget->loadPackages();
                }
            }
            break;
        }
        case ContactFixerPage:
        {
            if(pages.contains(ContactFixerPage))
            {
                auto *widget = static_cast<ContactFixerWidget *>(pages.value(ContactFixerPage));
                if(widget)
                {
                    if(!connectPhone.adbDevice.isEmpty())
                        widget->setDevice(connectPhone.adbDevice);
                }
            }
            break;
        }
        default:
            break;
    }
}

void MainWindow::runService(std::shared_ptr<Service> service)
{
    if(!ServiceProvider::runService(service))
    {
        QMessageBox::warning(this, "Service is shutdown", "Service module is no load or disabled by server.");
        logoutSystem();
    }
}

void MainWindow::closeService(std::shared_ptr<Service> service)
{
    if(service != nullptr)
        ServiceProvider::closeService();
    updateCabinet();
}

void MainWindow::clearAuthInfoPage()
{
    int x, y;
    delete ui->authInfo->model(); // fix: delete old model before replacing to avoid accumulation
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
        if(services[x]->ownerWidget != nullptr)
            services[x]->ownerWidget->deleteLater();

    serverServices.reset();
    services.clear();

    ui->aiChatEdit->clear();
    ui->aiChatSend->setText(QString::fromUtf8("➤"));

    ui->labelLoginAuthed->setText("-");
    ui->labelCredits->setText(QString::fromUtf8("💳 0\nБаланс"));
    ui->labelVipDays->setText(QString::fromUtf8("👑 0 ДНЕЙ\nVIP статус"));

    // Reset Side Reference Panel labels
    QLabel *lbl = nullptr;
    if((lbl = findChild<QLabel *>("cabinetVal_login")))
        lbl->setText("-");
    if((lbl = findChild<QLabel *>("cabinetVal_loginTime")))
        lbl->setText("-");
    if((lbl = findChild<QLabel *>("cabinetVal_credits")))
        lbl->setText("-");
    if((lbl = findChild<QLabel *>("cabinetVal_vip")))
    {
        lbl->setText("-");
        lbl->setStyleSheet("color: #F1F5F9; font-size: 12px; font-weight: bold; background: transparent; border: none;");
    }
    if((lbl = findChild<QLabel *>("cabinetVal_devices")))
        lbl->setText("-");
    if((lbl = findChild<QLabel *>("cabinetVal_location")))
        lbl->setText("-");
    if((lbl = findChild<QLabel *>("cabinetVal_status")))
    {
        lbl->setText("-");
        lbl->setStyleSheet("color: #F1F5F9; font-size: 12px; font-weight: bold; background: transparent; border: none;");
    }

    AIAgentService::resetHistory();
    ui->aiChatEdit->clear();
    ui->aiChatSend->setText(QString::fromUtf8("➤"));

    ui->aiChatEdit->setDisabled(true);
    ui->aiChatSend->setDisabled(true);

    auto *chatView = findChild<AIChatView *>("aiChatView");
    if(chatView)
        chatView->showLocked();
}

void MainWindow::fillAuthInfoPage()
{
    QString value;
    int x, y;
    QStandardItemModel *model = qobject_cast<QStandardItemModel *>(ui->authInfo->model());
    if(model)
    {
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
    }

    ui->labelLoginAuthed->setText(network.authedId.idName);
    ui->labelCredits->setText(QString("💳 %1 %2\nБаланс").arg(network.authedId.credits).arg(network.authedId.currencyType));
    if(network.authedId.hasVipAccount())
        ui->labelVipDays->setText(QString("👑 %1 ДНЕЙ\nVIP активен").arg(network.authedId.vipDays));
    else
        ui->labelVipDays->setText(QString("👑 Нет VIP\nVIP статус"));

    // Populate Side Reference Panel (Справочник аккаунта)
    QLabel *sLbl = nullptr;
    if((sLbl = findChild<QLabel *>("cabinetVal_login")))
        sLbl->setText(network.authedId.idName.isEmpty() ? "-" : network.authedId.idName);

    if((sLbl = findChild<QLabel *>("cabinetVal_loginTime")))
        sLbl->setText(QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm"));

    if((sLbl = findChild<QLabel *>("cabinetVal_credits")))
        sLbl->setText(QString("%1 %2").arg(network.authedId.credits).arg(network.authedId.currencyType));

    if((sLbl = findChild<QLabel *>("cabinetVal_vip")))
    {
        if(network.authedId.hasVipAccount())
        {
            sLbl->setText(QString("👑 %1 дн.").arg(network.authedId.vipDays));
            sLbl->setStyleSheet("color: #FBBF24; font-size: 12px; font-weight: bold; background: transparent; border: none;");
        }
        else
        {
            sLbl->setText(QString::fromUtf8("Не активен"));
            sLbl->setStyleSheet("color: #94A3B8; font-size: 12px; font-weight: 500; background: transparent; border: none;");
        }
    }

    if((sLbl = findChild<QLabel *>("cabinetVal_devices")))
        sLbl->setText(QString("%1 шт.").arg(network.authedId.connectedDevices));

    if((sLbl = findChild<QLabel *>("cabinetVal_location")))
        sLbl->setText(network.authedId.location.isEmpty() ? QString::fromUtf8("Не определено") : network.authedId.location);

    if((sLbl = findChild<QLabel *>("cabinetVal_status")))
    {
        if(network.authedId.blocked)
        {
            sLbl->setText(QString::fromUtf8("⛔ Заблокирован"));
            sLbl->setStyleSheet("color: #F87171; font-size: 12px; font-weight: bold; background: transparent; border: none;");
        }
        else
        {
            sLbl->setText(QString::fromUtf8("✅ Активен"));
            sLbl->setStyleSheet("color: #34D399; font-size: 12px; font-weight: bold; background: transparent; border: none;");
        }
    }

    initServiceModules();
}

void MainWindow::delayUI(int ms)
{
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    timer.start(ms);
    loop.exec();
}

void MainWindow::delayUICallLoop(int ms, std::function<bool()> callFalseEnd)
{
    QTimer *qtimer = new QTimer(this);
    qtimer->setSingleShot(false);
    qtimer->setInterval(ms);
    auto isRunning = std::make_shared<bool>(false);
    QObject::connect(
        qtimer,
        &QTimer::timeout,
        [qtimer, callFalseEnd, isRunning]()
        {
            if(*isRunning)
                return;
            *isRunning = true;
            if(!callFalseEnd())
            {
                qtimer->stop();
                qtimer->deleteLater();
            }
            else
            {
                *isRunning = false;
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

bool MainWindow::accessUi_page_longinfo(QListView *&processLogStatusV, QLabel *&malareStatusText0V, QLabel *&deviceLabelNameV, QProgressBar *&processBarStatusV, QPushButton *&pushButtonReRun)
{
    processLogStatusV = ui->processLogStatus;
    malareStatusText0V = ui->malwareStatusText0;
    deviceLabelNameV = ui->deviceLabelName;
    processBarStatusV = ui->processBarStatus;
    pushButtonReRun = ui->malwareReRun;
    return true;
}

bool MainWindow::accessUi_page_devices(QTableView *&tableActual, QDateEdit *&dateEditStart, QDateEdit *&dateEditEnd, QPushButton *&refreshButton, QCheckBox *&quaranteeFilter)
{
    tableActual = ui->myDeviceActual;
    dateEditStart = ui->myDeviceFilterDateStart;
    dateEditEnd = ui->myDeviceFilterDateEnd;
    refreshButton = ui->myDeviceSend;
    quaranteeFilter = ui->myDeviceQuaranteeFilter;
    return true;
}

bool MainWindow::accessUi_page_buyvip(QComboBox *&listVariants, QLabel *&balanceText, QLabel *&infoAfterPeriod, QPushButton *&buyButton)
{
    ui->comboBoxSelectVIPDays->disconnect();
    ui->labelVipBalance->disconnect();
    ui->buttonBuyVip->disconnect();
    ui->labelInfoVip->disconnect();

    listVariants = ui->comboBoxSelectVIPDays;
    balanceText = ui->labelVipBalance;
    buyButton = ui->buttonBuyVip;
    infoAfterPeriod = ui->labelInfoVip;

    infoAfterPeriod->clear();
    listVariants->clear();
    balanceText->clear();

    return true;
}

AdbDevice MainWindow::currentAdbDevice() const
{
    return connectPhone.adbDevice;
}

QWidget *MainWindow::pageWidget(PageIndex page) const
{
    return pages.value(page, nullptr);
}

void MainWindow::on_authButton_clicked()
{
    network.forclyExit = false;
    if(network.pending() || network.isAuthed())
        return;

    if(ui->lineLoginEdit->text().isEmpty() && ui->linePassEdit->text().isEmpty())
    {
        QMessageBox::warning(this, "Предупреждение", "Поле авторизаций не заполнено.");
        return;
    }

    network.pushLoginPass(ui->lineLoginEdit->text(), ui->linePassEdit->text());
    ui->statusAuthText->setStyleSheet("color: #38BDF8; font-size: 12px; font-weight: 500; background: transparent; padding: 2px 8px;");
    ui->statusAuthText->setText("Авторизация");

    if(ui->authButton)
        ui->authButton->setEnabled(false);
    ui->lineLoginEdit->setEnabled(false);
    ui->linePassEdit->setEnabled(false);

    if(timerAuthAnim != nullptr)
    {
        delete timerAuthAnim;
        timerAuthAnim = nullptr;
    }

    constexpr int Dots = 3;
    timerAuthAnim = new QTimer(this);
    timerAuthAnim->start(350);
    QObject::connect(
        timerAuthAnim,
        &QTimer::timeout,
        this,
        [this]()
        {
            QString temp = ui->statusAuthText->text();
            int dotCount = std::accumulate(temp.begin(), temp.end(), 0, [](int count, const QChar &c) { return count += (c == '.' ? 1 : 0); });
            if(dotCount >= Dots)
                temp.remove(temp.length() - Dots, Dots);
            else
                temp += '.';
            ui->statusAuthText->setText(temp);
        });

    AppSetting::autoLogin(nullptr, ui->checkAutoLogin->isChecked());

    delayUICallLoop(
        100,
        [this]()
        {
            if(!network.pending() && network.isAuthed())
            {
                delayUICall(
                    2000,
                    [this]()
                    {
                        showPage(CabinetPage);
                        updateCabinet();
                    });
            }
            return network.pending();
        });
}

void MainWindow::setThemeAction()
{
    // QList<QAction *> virtualSelectItems {ui->mThemeSystem, ui->mThemeLight, ui->mThemeDark};
    // QAction *selfSender = qobject_cast<QAction *>(sender());
    // int scheme;
    // for(scheme = (0); scheme < virtualSelectItems.size() && selfSender != virtualSelectItems[scheme]; ++scheme)
    //     ;
    // setTheme(static_cast<ThemeScheme>(scheme));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    tray->deleteLater();
    event->accept();
}

void MainWindow::slotAuthFinish(int status, bool ok)
{
    delayUICall(
        1000,
        [ok, status, this]() -> void
        {
            QString resText;
            int _status = status;

            if(!network.isAuthed())
            {
                _status = NetworkStatus::NetworkError;
            }

            if(timerAuthAnim)
                timerAuthAnim->stop();
            switch(_status)
            {
                case 0:
                    resText = "Токен успешно прошел проверку. Добро пожаловать!";

                    if(!ui->lineLoginEdit->text().isEmpty() && !ui->linePassEdit->text().isEmpty())
                    {
                        AppSetting::loginAndPass(nullptr, ui->lineLoginEdit->text(), ui->linePassEdit->text());

                        // Only get token.
                        break;
                    }

                    if(network.authedId.isNotValidBalance())
                    {
                        ui->statusAuthText->setStyleSheet("color: #F87171; font-size: 12px; font-weight: 500; background: transparent; padding: 2px 8px;");
                        ui->statusAuthText->setText("Закончился баланс, пополните, чтобы продолжить.");
                        showMessageFromStatus(NetworkStatus::NoEnoughMoney);
                    }
                    else if(network.authedId.blocked)
                    {
                        ui->statusAuthText->setStyleSheet("color: #F87171; font-size: 12px; font-weight: 500; background: transparent; padding: 2px 8px;");
                        ui->statusAuthText->setText("Аккаунт заблокирован");
                        showMessageFromStatus(NetworkStatus::AccountBlocked);
                    }
                    else
                    {
                        ui->statusAuthText->setStyleSheet("color: #34D399; font-size: 12px; font-weight: 500; background: transparent; padding: 2px 8px;");
                        ui->statusAuthText->setText("Аутентификация прошла успешно.");
                    }

                    break;
                case 401:
                    resText = infoServer401;
                    break;
                case NetworkStatus::NoEnoughMoney:
                    resText = infoNoBalance;
                    break;
                default:
                    resText = infoNoInternet;
                    break;
            }

            ui->lineLoginEdit->setEnabled(true);
            ui->linePassEdit->setEnabled(true);
            ui->authButton->setEnabled(true);
            if(_status != 0)
            {
                ui->statusAuthText->setStyleSheet("color: #F87171; font-size: 12px; font-weight: 500; background: transparent; padding: 2px 8px;");
                ui->statusAuthText->setText(resText);
            }
        });
}

void MainWindow::slotPullServiceList(const QList<ServiceItemInfo> &services, bool ok)
{
    serverServices.reset();

    if(ok)
    {
        serverServices = std::move(std::make_shared<QList<ServiceItemInfo>>(services));
    }
    else
    {
        logoutSystem();
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
    text = "Обнаружена новая версия программного обеспечения. После нажатия "
           "кнопки \"ОК\" ";
#ifdef WIN32
    text += "будет запущена обновление ПО.";
#else
    text += "откроется ссылка в вашем браузере.\n"
            "Пожалуйста, скачайте обновление по прямой ссылке.\n";
#endif
    text += "\nС уважением ваша команда Adskiller Team.";
    text += "\n\nВаша версия: v";
    text += runtimeVersion.mVersion.toString();
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
    for(const QString &e : entries)
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
    if(snows)
        delayUICall(50, [this]() { snows->start(); });
    event->accept();
}

void MainWindow::setTheme(ThemeScheme theme)
{
    int scheme;
    const char *resourceName;
    QList<QAction *> menus {ui->mThemeSystem, ui->mThemeLight, ui->mThemeDark};
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
            resourceName = ":/resources/ApplicationStyle.qss";
            break;
        case Light:
        default:
            resourceName = ":/resources/app-style-light";
            break;
    }

    QFile styleRes {};
    QString styleSheet {};
    if(resourceName)
    {
        styleRes.setFileName(resourceName);
        if(!styleRes.open(QFile::ReadOnly | QFile::Text))
        {
            if(theme == Dark)
            {
                styleRes.setFileName(QCoreApplication::applicationDirPath() + "/ApplicationStyle.qss");
                if(!styleRes.open(QFile::ReadOnly | QFile::Text))
                {
                    styleRes.setFileName("res/style/ApplicationStyle.qss");
                    if(!styleRes.open(QFile::ReadOnly | QFile::Text))
                        styleRes.setFileName("ApplicationStyle.qss");
                }
            }
        }
        if(styleRes.isOpen() || styleRes.open(QFile::ReadOnly | QFile::Text))
        {
            styleSheet = styleRes.readAll();
            styleRes.close();
        }
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
    if(statusCode == NetworkStatus::NetworkError)
        QMessageBox::warning(this, "Ошибка подключения", infoNoNetworkUpdate);

    if(statusCode == NetworkStatus::NoEnoughMoney)
        QMessageBox::warning(this, "Сервер отклонил запрос", infoNoBalance);

    if(statusCode == NetworkStatus::AccountBlocked)
        QMessageBox::warning(this, "Сервер отклонил запрос", infoAccountBlocked);
}

void MainWindow::updateCabinet()
{
    if(!network.isAuthed())
    {
        logoutSystem();
        return;
    }

    network.pushAuthToken();

    for(int i = 0; i < services.count(); ++i)
        if(services[i]->ownerWidget != nullptr)
            services[i]->ownerWidget->deleteLater();

    if(ui->serviceContents->layout())
    {
        while(ui->serviceContents->layout()->count() > 0)
        {
            QLayoutItem *item = ui->serviceContents->layout()->takeAt(0);
            if(item->widget())
                item->widget()->deleteLater();
            delete item;
        }
    }

    services.clear();
    serverServices.reset();

    showPageLoader(
        CabinetPage,
        1000,
        [this]() -> bool
        {
            const char *str = "Обновление странницы";
            bool status = network.isAuthed() && !network.pending();

            if(network.forclyExit)
            {
                logoutSystem();
                return true;
            }

            if(network.isAuthed() && !serverServices)
            {
                str = "Еще чуть-чуть";
            }

            if(status && !serverServices)
            {
                network.pullServiceList();
                status = !network.pending();
            }

            ui->loaderPageText->setText(str);

            if(status && !serverServices)
            {
                delayUICall(1, [this]() { logoutSystem(); });
            }
            else if(status && network.authedId.vipDays < 5 && network.authedId.vipDays > 0)
            {
                delayUICall(300, [this]() { QMessageBox::warning(this, "Уведомление", infoVipExpire); });
            }
            return status;
        });
}

void MainWindow::logoutSystem()
{
    network.forclyExit = true;
    if(network.isAuthed())
    {
        network._token = {};
        network.authedId = {};
        clearAuthInfoPage();
        showPageLoader(AuthPage, 500, QString("Выход из системы"));
    }
    else
    {
        showPageLoader(AuthPage, 0, QString("Выход из системы"));
    }
}

void MainWindow::showPageLoader(PageIndex pageNum, int msWait, std::function<bool()> predFalseEnd, QString text)
{
    if(pageNum == LoaderPage)
        return;

    if(text.isEmpty())
        text = "Ожидайте";

    ui->loaderPageText->setText(text);

    showPage(LoaderPage);
    delayUICallLoop(
        msWait,
        [this, pageNum, predFalseEnd]()
        {
            if(predFalseEnd())
            {
                QTimer::singleShot(1500, this, [this, pageNum]() { showPage(pageNum); });
                return false;
            }
            return true;
        });
}

void MainWindow::on_butShowPass_clicked()
{
    if(ui->linePassEdit->echoMode() == QLineEdit::EchoMode::Password)
    {
        ui->linePassEdit->setEchoMode(QLineEdit::EchoMode::Normal);
        ui->butShowPass->setText("🔒");
        ui->butShowPass->setToolTip("Скрыть пароль");
    }
    else
    {
        ui->linePassEdit->setEchoMode(QLineEdit::EchoMode::Password);
        ui->butShowPass->setText("👁");
        ui->butShowPass->setToolTip("Показать пароль");
    }
}
