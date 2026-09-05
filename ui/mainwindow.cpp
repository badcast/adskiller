#include <functional>
#include <algorithm>
#include <cctype>
#include <list>

#include <QCloseEvent>
#include <QDesktopServices>
#include <QEventLoop>
#include <QFontDatabase>
#include <QFuture>
#include <QGraphicsOpacityEffect>
#include <QHash>
#include <QHeaderView>
#include <QMessageBox>
#include <QScrollArea>
#include <QScrollBar>
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

constexpr struct
{
    PageIndex index;
    const char *widgetName;
} PageConstNames[LengthPages] = {{AuthPage, "page_auth"}, {CabinetPage, "page_cabinet"}, {LongInfoPage, "page_adsmalware"}, {LoaderPage, "page_loader"}, {DevicesPage, "page_devices"}, {MyDevicesPage, "page_mydevices"}, {BuyVIPPage, "page_buyvip"}};

namespace
{
    class HorizontalWheelFilter : public QObject
    {
    public:
        explicit HorizontalWheelFilter(QScrollArea *scrollArea) : QObject(scrollArea), m_scrollArea(scrollArea)
        {
        }

    protected:
        bool eventFilter(QObject *obj, QEvent *event) override
        {
            if(event->type() == QEvent::Wheel && m_scrollArea)
            {
                QWheelEvent *wheelEvent = static_cast<QWheelEvent *>(event);
                int delta = wheelEvent->angleDelta().y();
                if(delta == 0)
                    delta = wheelEvent->angleDelta().x();
                if(delta != 0)
                {
                    QScrollBar *hBar = m_scrollArea->horizontalScrollBar();
                    if(hBar)
                        hBar->setValue(hBar->value() - delta);
                    return true;
                }
            }
            return QObject::eventFilter(obj, event);
        }

    private:
        QScrollArea *m_scrollArea;
    };
} // namespace

MainWindow *MainWindow::current;

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow), timerAuthAnim(nullptr)
{
    int x;
    QStringListModel *model;
    ui->setupUi(this);

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

    // Refresh TabPages to Content widget (Selective)
    QList<QWidget *> _w;
    for(x = 0; x < ui->tabWidget->count(); ++x)
        _w << ui->tabWidget->widget(x);

    for(const auto &item : std::as_const(PageConstNames))
    {
        auto iter = std::find_if(_w.begin(), _w.end(), [&item](const QWidget *it) { return it->objectName() == item.widgetName; });
        if(iter != std::end(_w))
            pages.insert(item.index, *iter);
    }

    vPageSpacer = ui->topcontent;
    vPageSpacer->setMaximumHeight(400);
    vPageSpacerAnimator = new QPropertyAnimation(vPageSpacer, "maximumHeight", this);
    vPageSpacerAnimator->setDuration(500);
    vPageSpacerAnimator->setStartValue(400);
    vPageSpacerAnimator->setEndValue(0);

    QGraphicsOpacityEffect *effect = new QGraphicsOpacityEffect(ui->contentLayout);
    ui->contentLayout->setGraphicsEffect(effect);

    contentOpacityAnimator = new QPropertyAnimation(effect, "opacity", this);
    contentOpacityAnimator->setDuration(1000);
    contentOpacityAnimator->setStartValue(0);
    contentOpacityAnimator->setEndValue(1.0);

    deviceLeftAnimator = new QPropertyAnimation(ui->device_left_group, "maximumWidth", this);
    deviceLeftAnimator->setDuration(1000);
    deviceLeftAnimator->setStartValue(1000);
    deviceLeftAnimator->setEndValue(0);

    // Top header back to main page.
    ui->contentLayout->layout()->addWidget(ui->toplevel_backpage);

    for(x = 0; x < _w.count(); ++x)
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

    // Font init
    int fontId = QFontDatabase::addApplicationFont(":/resources/font-DigitalNumbers");
    QStringList fontFamils = QFontDatabase::applicationFontFamilies(fontId);
    if(!fontFamils.isEmpty())
    {
        QString fontFamily = fontFamils.first();
        malwareProgressCircle->setStyleSheet(QString("QWidget { Font-family: '%1'; }").arg(fontFamily));
    }

    // Set Default Theme DARK ONLY

    ui->menu_4->deleteLater();

    setTheme(ThemeScheme::Dark);
    // setTheme(static_cast<ThemeScheme>(static_cast<ThemeScheme>(std::clamp<int>(AppSetting::themeIndex(), 0, 2))));

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

    snows = nullptr;

    QDate d = QDate::currentDate();
    if(d >= QDate(d.year(), 12, 20) || d <= QDate(d.year(), 2, 1))
    {
        // ADD Snowflakes
        snows = new Snowflake(this, 50);
        ui->centralwidget_Layout->addWidget(snows, 0, 0, 0, 0);
        snows->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        snows->setSnowPixmap(QPixmap(":/resources/snowflake-image"));
        ui->mainapplogo->setStyleSheet("image: url(:/resources/app-logo-merry);");
        this->setWindowIcon(QIcon(":/resources/app-logo-merry"));
    }
    else
    {
        ui->mainapplogo->setStyleSheet("image: url(:/resources/app-logo);");
        this->setWindowIcon(QIcon(":/resources/app-logo"));
    }

    // Init tray
    tray = new AdsAppSystemTray(this);

    // Modernize Auth Page (Login Window)
    if(ui->frame_4)
    {
        ui->frame_4->setStyleSheet(
            "QFrame#frame_4 {"
            "   "
            "   border: 1px solid #363942;"
            "   border-radius: 16px;"
            "}");

        ui->label_4->setStyleSheet("color: #8E9297; font-size: 11px; font-weight: normal; background: transparent;");
        ui->label_4->setText("Войдите для доступа к функциям AdsKiller");

        ui->label_12->setStyleSheet("color: #A0A5AF; font-size: 10px; font-weight: bold; background: transparent; letter-spacing: 0.5px;");
        ui->label_14->setStyleSheet("color: #A0A5AF; font-size: 10px; font-weight: bold; background: transparent; letter-spacing: 0.5px;");

        ui->lineLoginEdit->setStyleSheet(
            "QLineEdit#lineLoginEdit {"
            "   background-color: #1E2026;"
            "   color: #FFFFFF;"
            "   border: 1px solid #383A42;"
            "   border-radius: 8px;"
            "   padding: 4px 10px;"
            "   font-size: 12px;"
            "   selection-background-color: #0078D4;"
            "}"
            "QLineEdit#lineLoginEdit:focus {"
            "   border: 1px solid #4CC2FF;"
            "   background-color: #18191E;"
            "}");
        ui->lineLoginEdit->setPlaceholderText("Введите логин или токен...");

        ui->linePassEdit->setStyleSheet(
            "QLineEdit#linePassEdit {"
            "   background-color: #1E2026;"
            "   color: #FFFFFF;"
            "   border: 1px solid #383A42;"
            "   border-radius: 8px;"
            "   padding: 4px 10px;"
            "   font-size: 12px;"
            "   selection-background-color: #0078D4;"
            "}"
            "QLineEdit#linePassEdit:focus {"
            "   border: 1px solid #4CC2FF;"
            "   background-color: #18191E;"
            "}");
        ui->linePassEdit->setPlaceholderText("Введите пароль...");

        ui->butShowPass->setText("👁");
        ui->butShowPass->setToolTip("Показать/Скрыть пароль");
        ui->butShowPass->setCursor(Qt::PointingHandCursor);
        ui->butShowPass->setStyleSheet(
            "QPushButton#butShowPass {"
            "   background: #282B33;"
            "   color: #8E9297;"
            "   border: 1px solid #383A42;"
            "   border-radius: 8px;"
            "   font-size: 13px;"
            "}"
            "QPushButton#butShowPass:hover {"
            "   background: #333640;"
            "   border-color: #4CC2FF;"
            "   color: #FFFFFF;"
            "}"
            "QPushButton#butShowPass:pressed {"
            "   background: #1C1E24;"
            "}");

        ui->checkAutoLogin->setStyleSheet(
            "QCheckBox#checkAutoLogin {"
            "   color: #C8CDD5;"
            "   font-size: 11px;"
            "   spacing: 6px;"
            "   background: transparent;"
            "}"
            "QCheckBox#checkAutoLogin::indicator {"
            "   width: 15px;"
            "   height: 15px;"
            "   border: 1px solid #444750;"
            "   border-radius: 4px;"
            "   background: #1E2026;"
            "}"
            "QCheckBox#checkAutoLogin::indicator:checked {"
            "   background: #0078D4;"
            "   border-color: #4CC2FF;"
            "}");

        ui->label_2->setStyleSheet("QLabel#label_2 a { color: #4CC2FF; text-decoration: none; font-size: 11px; } QLabel#label_2 a:hover { text-decoration: underline; }");

        ui->authButton->setStyleSheet(
            "QPushButton#authButton {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #0078D4, stop:1 #005A9E);"
            "   color: #FFFFFF;"
            "   border: none;"
            "   border-radius: 8px;"
            "   padding: 5px 18px;"
            "   font-weight: bold;"
            "   font-size: 12px;"
            "}"
            "QPushButton#authButton:hover {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #1084E3, stop:1 #0066BA);"
            "}"
            "QPushButton#authButton:pressed {"
            "   background: #004D80;"
            "}"
            "QPushButton#authButton:disabled {"
            "   background: #2D2F34;"
            "   color: #5C6067;"
            "}");
        ui->authButton->setCursor(Qt::PointingHandCursor);

        ui->statusAuthText->setStyleSheet("color: #FF6B6B; font-size: 11px; font-weight: bold; background: transparent;");

        ui->label_auth_ver->setText("версия:" + runtimeVersion.mVersion.toString());

    }

    // 1. Top Header Bar (Remove red banner, use sleek dark slate design)
    if(ui->toplevel_up)
    {
        ui->toplevel_up->setStyleSheet(
            "QFrame#toplevel_up {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #1C1E24, stop:1 #242730);"
            "   border-bottom: 1px solid #30333D;"
            "   min-height: 42px;"
            "   max-height: 42px;"
            "}");
        ui->label_6->setStyleSheet("color: #E3E5E8; font-size: 13px; font-weight: bold; font-style: normal; background: transparent;");
        ui->label_6->setText("AdsKiller  |  Личный кабинет");

        ui->logoutButton->setStyleSheet(
            "QPushButton#logoutButton {"
            "   background: rgba(255, 75, 75, 0.12);"
            "   color: #FF7B7B;"
            "   border: 1px solid rgba(255, 75, 75, 0.35);"
            "   border-radius: 6px;"
            "   padding: 4px 12px;"
            "   font-weight: bold;"
            "   font-size: 11px;"
            "}"
            "QPushButton#logoutButton:hover {"
            "   background: rgba(255, 75, 75, 0.28);"
            "   border-color: #FF5252;"
            "   color: #FFFFFF;"
            "}"
            "QPushButton#logoutButton:pressed {"
            "   background: #801818;"
            "}");
        ui->logoutButton->setCursor(Qt::PointingHandCursor);
    }

    if(ui->toplevel_backpage)
    {
        ui->toplevel_backpage->setStyleSheet(
            "QFrame#toplevel_backpage {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #1C1E24, stop:1 #242730);"
            "   border-bottom: 1px solid #30333D;"
            "   min-height: 42px;"
            "   max-height: 42px;"
            "}");
        ui->label_10->setStyleSheet("color: #E3E5E8; font-size: 13px; font-weight: bold; font-style: normal; background: transparent;");
        ui->buttonBackTo->setStyleSheet(
            "QPushButton#buttonBackTo {"
            "   background: rgba(0, 120, 212, 0.15);"
            "   color: #4CC2FF;"
            "   border: 1px solid rgba(76, 194, 255, 0.35);"
            "   border-radius: 6px;"
            "   padding: 4px 12px;"
            "   font-weight: bold;"
            "   font-size: 11px;"
            "}"
            "QPushButton#buttonBackTo:hover {"
            "   background: #0078D4;"
            "   color: #FFFFFF;"
            "}"
            "QPushButton#buttonBackTo:pressed {"
            "   background: #005A9E;"
            "}");
        ui->buttonBackTo->setCursor(Qt::PointingHandCursor);
    }

    // 2. Account Information Card (frame_7) with clean spacious layout & padding
    if(ui->frame_7)
    {
        ui->horizontalLayout->setContentsMargins(14, 14, 14, 10);
        ui->horizontalLayout->setSpacing(0);

        ui->frame_7->setMinimumWidth(560);
        ui->frame_7->setMaximumWidth(780);
        ui->frame_7->setFixedHeight(125);
        ui->frame_7->setStyleSheet(
            "QFrame#frame_7 {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #252830, stop:1 #1C1E24);"
            "   border: 1px solid #363942;"
            "   border-radius: 14px;"
            "}");

        // Install QHBoxLayout to dynamically place VIP, Avatar/Login, and Credits without overlaps
        QHBoxLayout *accountLayout = new QHBoxLayout(ui->frame_7);
        accountLayout->setContentsMargins(18, 12, 18, 12);
        accountLayout->setSpacing(14);

        // 1. VIP Pill (Left)
        ui->frame_6->setFixedSize(150, 85);
        ui->frame_6->setStyleSheet(
            "QFrame#frame_6 {"
            "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(255, 185, 0, 0.15), stop:1 rgba(255, 185, 0, 0.04));"
            "   border: 1px solid rgba(255, 185, 0, 0.35);"
            "   border-radius: 12px;"
            "   padding: 6px;"
            "}");
        ui->labelVipDays->setStyleSheet("color: #FFD700; font-weight: bold; font-size: 11px; text-decoration: none; background: transparent;");

        // 2. User Avatar + Name (Center)
        ui->authedMainWin->setFixedSize(170, 95);
        ui->authedMainWin->setStyleSheet("background: transparent; border: none;");
        ui->frame_3->setStyleSheet("image: url(:/resources/no-avatar); min-width: 50px; min-height: 50px; max-width: 50px; max-height: 50px; border-radius: 25px; background: rgba(255,255,255,0.06); border: 2px solid #0078D4;");
        ui->labelLoginAuthed->setStyleSheet("color: #FFFFFF; font-size: 14px; font-weight: bold; text-decoration: none; background: transparent; margin-top: 2px;");

        // 3. Credits Pill (Right)
        ui->frame_5->setFixedSize(150, 85);
        ui->frame_5->setStyleSheet(
            "QFrame#frame_5 {"
            "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(0, 120, 212, 0.16), stop:1 rgba(0, 120, 212, 0.04));"
            "   border: 1px solid rgba(76, 194, 255, 0.35);"
            "   border-radius: 12px;"
            "   padding: 6px;"
            "}");
        ui->labelCredits->setStyleSheet("color: #4CC2FF; font-weight: bold; font-size: 11px; text-decoration: none; background: transparent;");

        accountLayout->addWidget(ui->frame_6, 0, Qt::AlignCenter);
        accountLayout->addStretch(1);
        accountLayout->addWidget(ui->authedMainWin, 0, Qt::AlignCenter);
        accountLayout->addStretch(1);
        accountLayout->addWidget(ui->frame_5, 0, Qt::AlignCenter);
    }

    // 3. Section Divider Bar above services
    if(ui->toplevel_up_2)
    {
        ui->toplevel_up_2->setStyleSheet(
            "QFrame#toplevel_up_2 {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #1E2026, stop:1 #252830);"
            "   border-left: 4px solid #0078D4;"
            "   border-radius: 4px;"
            "   margin: 12px 14px 4px 14px;"
            "   min-height: 32px;"
            "   max-height: 32px;"
            "}");
        ui->label_7->setStyleSheet("color: #C8CDD5; font-weight: bold; font-size: 11.5px; font-style: normal; background: transparent; letter-spacing: 0.5px;");
        ui->label_7->setText("ДОСТУПНЫЕ УСЛУГИ И ИНСТРУМЕНТЫ");
    }

    if(ui->scrollArea_3)
    {
        ui->scrollArea_3->setStyleSheet(
            "QScrollArea#scrollArea_3 {"
            "   background: transparent;"
            "   border: none;"
            "}"
            "QScrollArea#scrollArea_3 QScrollBar:vertical {"
            "   width: 5px;"
            "   background: transparent;"
            "   margin: 0px;"
            "}"
            "QScrollArea#scrollArea_3 QScrollBar::handle:vertical {"
            "   background: #484B52;"
            "   border-radius: 2px;"
            "   min-height: 20px;"
            "}"
            "QScrollArea#scrollArea_3 QScrollBar::handle:vertical:hover {"
            "   background: #6D7179;"
            "}"
            "QScrollArea#scrollArea_3 QScrollBar::add-line:vertical, "
            "QScrollArea#scrollArea_3 QScrollBar::sub-line:vertical {"
            "   height: 0px;"
            "}");
    }

    // 4. Modernize page_adsmalware (Ad Removal & RAM Optimization Page)
    if(ui->deviceLabelName)
    {
        ui->deviceLabelName->setStyleSheet(
            "QLabel#deviceLabelName {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #252830, stop:1 #1C1E24);"
            "   border: 1px solid #363942;"
            "   border-radius: 12px;"
            "   padding: 10px 14px;"
            "   color: #FFFFFF;"
            "}");
        ui->deviceLabelName->setMinimumHeight(80);
    }
    if(ui->processLogStatus)
    {
        ui->processLogStatus->setStyleSheet(
            "QListView#processLogStatus {"
            "   background-color: #17181D;"
            "   border: 1px solid #2F323A;"
            "   border-radius: 10px;"
            "   color: #D6DAE0;"
            "   font-family: 'Consolas', 'Courier New', monospace;"
            "   font-size: 11px;"
            "   padding: 8px;"
            "   selection-background-color: #0078D4;"
            "}"
            "QListView#processLogStatus QScrollBar:vertical {"
            "   width: 5px;"
            "   background: transparent;"
            "   margin: 0px;"
            "}"
            "QListView#processLogStatus QScrollBar::handle:vertical {"
            "   background: #484B52;"
            "   border-radius: 2px;"
            "}");
    }
    if(ui->processBarStatus)
    {
        ui->processBarStatus->setStyleSheet(
            "QProgressBar#processBarStatus {"
            "   background-color: #1C1E24;"
            "   border: 1px solid #30333D;"
            "   border-radius: 6px;"
            "   text-align: center;"
            "   color: #FFFFFF;"
            "   font-weight: bold;"
            "   font-size: 10px;"
            "   min-height: 18px;"
            "   max-height: 18px;"
            "}"
            "QProgressBar#processBarStatus::chunk {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0078D4, stop:1 #00E5FF);"
            "   border-radius: 5px;"
            "}");
    }
    if(ui->malwareStatusText0)
    {
        ui->malwareStatusText0->setStyleSheet("color: #4CC2FF; font-size: 12px; font-weight: bold; background: transparent; padding: 4px;");
    }
    if(ui->malwareReRun)
    {
        ui->malwareReRun->setStyleSheet(
            "QPushButton#malwareReRun {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #0078D4, stop:1 #005A9E);"
            "   color: #FFFFFF;"
            "   border: none;"
            "   border-radius: 10px;"
            "   padding: 8px 16px;"
            "   font-weight: bold;"
            "   font-size: 12px;"
            "   min-height: 36px;"
            "}"
            "QPushButton#malwareReRun:hover {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #1084E3, stop:1 #0066BA);"
            "}"
            "QPushButton#malwareReRun:pressed {"
            "   background: #004D80;"
            "}"
            "QPushButton#malwareReRun:disabled {"
            "   background: #282A30;"
            "   color: #555860;"
            "}");
        ui->malwareReRun->setCursor(Qt::PointingHandCursor);
    }

    // 5. Modernize page_devices (Device Connecting & USB Guide Page)
    if(ui->device_left_group)
    {
        ui->device_left_group->setStyleSheet(
            "QFrame#device_left_group {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #252830, stop:1 #1C1E24);"
            "   border: 1px solid #363942;"
            "   border-radius: 14px;"
            "   padding: 8px;"
            "}");
    }
    if(ui->scrollArea_2)
    {
        ui->scrollArea_2->setStyleSheet("background: transparent; border: none;");
    }
    if(ui->label_5)
    {
        ui->label_5->setStyleSheet("color: #4CC2FF; font-size: 12px; font-weight: bold; background: transparent;");
    }

    // 6. Modernize page_mydevices (My Devices & Warranty Page)
    if(ui->page_mydevices)
    {
        ui->myDeviceFilterDateStart->setStyleSheet("background-color: #1E2026; color: #FFFFFF; border: 1px solid #383A42; border-radius: 6px; padding: 4px 8px; font-size: 11px;");
        ui->myDeviceFilterDateEnd->setStyleSheet("background-color: #1E2026; color: #FFFFFF; border: 1px solid #383A42; border-radius: 6px; padding: 4px 8px; font-size: 11px;");
        ui->myDeviceQuaranteeFilter->setStyleSheet("color: #C8CDD5; font-size: 11px;");
        ui->myDeviceSend->setStyleSheet(
            "QPushButton#myDeviceSend {"
            "   background: #0078D4;"
            "   color: #FFFFFF;"
            "   border: none;"
            "   border-radius: 6px;"
            "   padding: 5px 14px;"
            "   font-weight: bold;"
            "   font-size: 11px;"
            "}"
            "QPushButton#myDeviceSend:hover { background: #1084E3; }"
            "QPushButton#myDeviceSend:pressed { background: #005A9E; }");
        ui->myDeviceSend->setCursor(Qt::PointingHandCursor);

        ui->myDeviceActual->setStyleSheet(
            "QTableView#myDeviceActual {"
            "   background-color: #17181D;"
            "   border: 1px solid #2F323A;"
            "   border-radius: 10px;"
            "   color: #E3E5E8;"
            "   gridline-color: #26282E;"
            "   selection-background-color: #0078D4;"
            "   selection-color: #FFFFFF;"
            "   font-size: 11.5px;"
            "}"
            "QHeaderView::section {"
            "   background-color: #1F2128;"
            "   color: #A0A5AF;"
            "   padding: 6px;"
            "   border: 1px solid #2B2D35;"
            "   font-weight: bold;"
            "   font-size: 11px;"
            "}");
        ui->myDevicePageLabel->setStyleSheet("color: #8E9297; font-size: 11px; background: transparent;");
    }

    // 7. Modernize page_buyvip (Buy VIP Page)
    if(ui->groupBox)
    {
        ui->groupBox->setStyleSheet(
            "QGroupBox#groupBox {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #252830, stop:1 #1C1E24);"
            "   border: 1px solid #363942;"
            "   border-radius: 14px;"
            "   margin-top: 10px;"
            "   padding: 16px;"
            "   color: #FFFFFF;"
            "   font-weight: bold;"
            "   font-size: 12px;"
            "}");
        ui->labelVipBalance->setStyleSheet("color: #00E5FF; font-size: 13px; font-weight: bold; background: transparent;");
        ui->comboBoxSelectVIPDays->setStyleSheet("background-color: #1E2026; color: #FFFFFF; border: 1px solid #383A42; border-radius: 8px; padding: 6px 12px; font-size: 12px;");
        ui->labelInfoVip->setStyleSheet("color: #C8CDD5; font-size: 11px; background: transparent;");
        ui->buttonBuyVip->setStyleSheet(
            "QPushButton#buttonBuyVip {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #FFB900, stop:1 #D48800);"
            "   color: #000000;"
            "   border: none;"
            "   border-radius: 8px;"
            "   padding: 8px 18px;"
            "   font-weight: bold;"
            "   font-size: 12px;"
            "}"
            "QPushButton#buttonBuyVip:hover {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #FFC833, stop:1 #E69500);"
            "}"
            "QPushButton#buttonBuyVip:pressed { background: #B27000; }");
        ui->buttonBuyVip->setCursor(Qt::PointingHandCursor);
    }

    // AI toolbox toggle button behavior
    if(ui->aiToolBoxToggle)
    {
        ui->aiToolBoxToggle->setText(QString::fromUtf8("›"));
        ui->aiToolBoxToggle->setToolTip("Свернуть панель ИИ");
        ui->aiToolBoxToggle->setCursor(Qt::PointingHandCursor);
        ui->aiToolBoxToggle->setStyleSheet(
            "QPushButton#aiToolBoxToggle {"
            "   background-color: #26282E;"
            "   color: #8E9297;"
            "   border: 1px solid #363940;"
            "   border-radius: 4px;"
            "   font-size: 13px;"
            "   font-weight: bold;"
            "   padding: 0px;"
            "}"
            "QPushButton#aiToolBoxToggle:hover {"
            "   background-color: #32353D;"
            "   border-color: #4CC2FF;"
            "   color: #4CC2FF;"
            "}"
            "QPushButton#aiToolBoxToggle:pressed {"
            "   background-color: #1A1B1E;"
            "}");
        ui->aiToolBoxContainer->setMaximumWidth(324);

        QObject::connect(
            ui->aiToolBoxToggle,
            &QPushButton::clicked,
            this,
            [this]()
            {
                if(ui->aiToolBox->isVisible())
                {
                    ui->aiToolBox->setVisible(false);
                    ui->aiToolBoxToggle->setText(QString::fromUtf8("‹"));
                    ui->aiToolBoxToggle->setToolTip("Развернуть панель ИИ");
                    ui->aiToolBoxContainer->setMaximumWidth(24);
                }
                else
                {
                    ui->aiToolBox->setVisible(true);
                    ui->aiToolBoxToggle->setText(QString::fromUtf8("›"));
                    ui->aiToolBoxToggle->setToolTip("Свернуть панель ИИ");
                    ui->aiToolBoxContainer->setMaximumWidth(324);
                }
            });

        // Create custom widget-based AIChatView
        AIChatView *chatView = new AIChatView(ui->aiToolBoxPage1);
        chatView->setObjectName("aiChatView");
        ui->aiChatMessages->hide();

        // Add comfortable horizontally scrollable quick action buttons for AI in 2 rows
        QScrollArea *scrollArea = new QScrollArea(ui->aiChatEdit->parentWidget());
        scrollArea->setObjectName("aiQuickScrollArea");
        scrollArea->setWidgetResizable(true);
        scrollArea->setFixedHeight(58);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scrollArea->setStyleSheet(
            "QScrollArea#aiQuickScrollArea {"
            "   border: none;"
            "   background: transparent;"
            "}"
            "QScrollArea#aiQuickScrollArea QScrollBar:horizontal {"
            "   height: 3px;"
            "   background: rgba(0, 0, 0, 20);"
            "   margin: 0px;"
            "   border-radius: 1px;"
            "}"
            "QScrollArea#aiQuickScrollArea QScrollBar::handle:horizontal {"
            "   background: #555555;"
            "   min-width: 16px;"
            "   border-radius: 1px;"
            "}"
            "QScrollArea#aiQuickScrollArea QScrollBar::handle:horizontal:hover {"
            "   background: #888888;"
            "}"
            "QScrollArea#aiQuickScrollArea QScrollBar::add-line:horizontal, "
            "QScrollArea#aiQuickScrollArea QScrollBar::sub-line:horizontal {"
            "   width: 0px;"
            "   background: none;"
            "}"
            "QScrollArea#aiQuickScrollArea QScrollBar::add-page:horizontal, "
            "QScrollArea#aiQuickScrollArea QScrollBar::sub-page:horizontal {"
            "   background: none;"
            "}");

        scrollArea->viewport()->installEventFilter(new HorizontalWheelFilter(scrollArea));
        scrollArea->installEventFilter(new HorizontalWheelFilter(scrollArea));

        QWidget *quickButtonsWidget = new QWidget(scrollArea);
        quickButtonsWidget->setStyleSheet("background: transparent;");
        QGridLayout *quickButtonsLayout = new QGridLayout(quickButtonsWidget);
        quickButtonsLayout->setContentsMargins(4, 3, 4, 3);
        quickButtonsLayout->setSpacing(5);

        struct QuickQuestion
        {
            QString icon;
            QStringList variations;
        };

        QList<QuickQuestion> quickQuestions = {
            {"💳", {"Мои кредиты", "Сколько кредитов?", "Остаток баланса?", "Показать баланс"}},
            {"📧", {"Моя почта", "Мой email", "Какая у меня почта?", "Адрес эл. почты"}},
            {"📱", {"Запусти удаление рекламы", "Какие есть сервисы?", "Какой сервис не активен?", "Открой окно покупки VIP"}},
            {"📱", {"Мои устройства", "Список устройств", "Активные девайсы", "Привязанные устройства"}},
            {"👑", {"VIP статус", "Остаток VIP дней", "Сколько VIP дней?", "Когда истекает VIP?"}},
            {"💡", {"Что ты умеешь?", "Как удалить рекламу?", "Возможности AdsKiller", "Справка по функциям"}},
            {"🚀", {"Как ускорить телефон?", "Как очистить ОЗУ?", "Оптимизация системы", "Ускорить работу"}},
            {"🛡️", {"Безопасно ли это?", "Как включить отладку?", "Проверка безопасности", "Как подключить телефон?"}},
            {"⚡", {"Быстрая очистка", "Остановить приложения", "Очистить кэш", "Как закрыть вирусы?"}}};

        for(int i = 0; i < quickQuestions.size(); ++i)
        {
            const auto &qData = quickQuestions[i];
            QPushButton *btn = new QPushButton(QString("%1  %2").arg(qData.icon, qData.variations.first()), quickButtonsWidget);
            btn->setStyleSheet(
                "QPushButton { "
                "   background-color: #26272B; "
                "   color: #D1D5DB; "
                "   border-radius: 7px; "
                "   padding: 3px 8px; "
                "   font-size: 10.5px; "
                "   border: 1px solid #36383E; "
                "   white-space: nowrap; "
                "}"
                "QPushButton:hover { "
                "   background-color: #32353B; "
                "   border-color: #4CC2FF; "
                "   color: #4CC2FF; "
                "}"
                "QPushButton:pressed { "
                "   background-color: #1A1B1E; "
                "   color: #FFFFFF; "
                "}");
            btn->setCursor(Qt::PointingHandCursor);
            quickButtonsLayout->addWidget(btn, i % 2, i / 2);

            QObject::connect(
                btn,
                &QPushButton::clicked,
                this,
                [this, btn, icon = qData.icon, variations = qData.variations, lastIdx = 0]() mutable
                {
                    QString textToSend = variations[lastIdx];
                    ui->aiChatEdit->setText(textToSend);
                    ui->aiChatSend->click();

                    if(variations.size() > 1)
                    {
                        int r;
                        do
                        {
                            r = QRandomGenerator::global()->bounded(variations.size());
                        } while(r == lastIdx);
                        lastIdx = r;
                        btn->setText(QString("%1  %2").arg(icon, variations[r]));
                    }
                });
        }

        scrollArea->setWidget(quickButtonsWidget);

        ui->aiChatEdit->setStyleSheet(
            "QTextEdit#aiChatEdit {"
            "   background-color: #232428;"
            "   color: #FFFFFF;"
            "   border: 1px solid #383A40;"
            "   border-radius: 8px;"
            "   padding: 6px 9px;"
            "   font-size: 11px;"
            "   selection-background-color: #0078D4;"
            "}"
            "QTextEdit#aiChatEdit:focus {"
            "   border: 1px solid #4CC2FF;"
            "   background-color: #1E1F22;"
            "}");
        ui->aiChatEdit->setPlaceholderText("Задайте вопрос ИИ...");
        ui->aiChatEdit->setMaximumHeight(38);

        ui->aiChatSend->setStyleSheet(
            "QPushButton#aiChatSend {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #0078D4, stop:1 #005A9E);"
            "   color: #FFFFFF;"
            "   border: none;"
            "   border-radius: 8px;"
            "   padding: 6px 12px;"
            "   font-weight: bold;"
            "   font-size: 11px;"
            "}"
            "QPushButton#aiChatSend:hover {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #1084E3, stop:1 #0066BA);"
            "}"
            "QPushButton#aiChatSend:pressed {"
            "   background: #004D80;"
            "}"
            "QPushButton#aiChatSend:disabled {"
            "   background: #2D2F34;"
            "   color: #5C6067;"
            "}");
        ui->aiChatSend->setCursor(Qt::PointingHandCursor);
        ui->aiChatSend->setMinimumHeight(38);
        ui->aiChatSend->setMaximumHeight(38);

        QGridLayout *aiLayout = qobject_cast<QGridLayout *>(ui->aiChatMessages->parentWidget()->layout());
        if(aiLayout)
        {
            aiLayout->setContentsMargins(4, 4, 4, 4);
            aiLayout->setSpacing(6);
            aiLayout->removeWidget(ui->aiChatMessages);
            aiLayout->removeWidget(ui->aiChatEdit);
            aiLayout->removeWidget(ui->aiChatSend);
            aiLayout->addWidget(chatView, 0, 0, 1, 2);
            aiLayout->addWidget(scrollArea, 1, 0, 1, 2);
            aiLayout->addWidget(ui->aiChatEdit, 2, 0);
            aiLayout->addWidget(ui->aiChatSend, 2, 1);
        }
    }
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

        tmp0 = remoteService->name + '\n';
        if(instance->active)
        {
            if(network.authedId.hasVipAccount() && remoteService->needVIP)
                tmp0 += "(безлимит)";
            else if(remoteService->price == 0)
                tmp0 += "(бесплатно)";
            else if(remoteService->price == static_cast<std::uint32_t>(-1))
                tmp0 += "(на выбор)";
            else
                tmp0 += QString("%1 (%2)").arg(x == 0 ? network.authedId.basePrice : remoteService->price).arg(network.authedId.currencyType);
        }
        else
        {
            if(!instance->isAvailable())
                tmp0 += "(Не реализован)";
            else if(!remoteService->active)
                tmp0 += "(Не доступен)";
        }

        if(instance->uuid() != IDServiceAIAgentString)
        {
            QPushButton *button = new QPushButton(QIcon(":/service-icons/" + instance->widgetIconName()), tmp0, ui->serviceContents);
            button->setCursor(Qt::PointingHandCursor);
            button->setStyleSheet(
                "QPushButton {"
                "   text-align: left;"
                "   padding: 10px 14px 10px 12px;"
                "   font-size: 12.5px;"
                "   font-weight: bold;"
                "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #2A2D35, stop:1 #1F2127);"
                "   color: #F2F4F7;"
                "   border-radius: 13px;"
                "   border: 1px solid #383B44;"
                "}"
                "QPushButton:hover {"
                "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #333742, stop:1 #252830);"
                "   border-color: #4CC2FF;"
                "   color: #FFFFFF;"
                "}"
                "QPushButton:pressed {"
                "   background: #17181D;"
                "   border-color: #0078D4;"
                "}"
                "QPushButton:disabled {"
                "   background: #1C1D22;"
                "   color: #555860;"
                "   border: 1px solid #282A30;"
                "}");

            if(!instance->active)
                button->setEnabled(false);

            // Target service by slot
            QObject::connect(button, &QPushButton::clicked, this, std::bind(&MainWindow::runService, this, instance));

            instance->title = remoteService->name;
            instance->ownerWidget = button;

            button->setIconSize({56, 56});
            button->setFixedSize(260, 78);
            button->setEnabled(instance->active);
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
        layoutSpace->setSpacing(12);
        layoutSpace->setContentsMargins(8, 8, 8, 8);
    }

    x = 0;
    for(const std::shared_ptr<Service> &item : std::as_const(services))
    {
        // Adds widget to a grid
        if(item->uuid() != IDServiceAIAgentString)
        {
            static_cast<QGridLayout *>(ui->serviceContents->layout())->addWidget(item->ownerWidget, x / 3, x % 3);
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

            delayUI(1000);

            delayUICallLoop(
                300,
                [this]() -> bool
                {
                    if(!deviceSelectSwitched)
                    {
                        QList<AdbDevice> devices = Adb::getDevices();
                        for(const AdbDevice &device : std::as_const(devices))
                        {
                            AdbConStatus status = Adb::deviceStatus(device.devId);
                            if(status == DEVICE)
                            {
                                connectPhone.isAuthed = status == DEVICE;
                                connectPhone.adbDevice = device;
                                ServiceProvider::currentService()->setArgs(device);
                                break;
                            }
                        }
                    }
                    if(ServiceProvider::currentService()->canStart() && !deviceSelectSwitched)
                    {
                        deviceSelectSwitched = true;
                        deviceLeftAnimator->start();
                        delayUI(2000);
                        showPageLoader(ServiceProvider::currentService()->targetPage());
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
    ui->aiChatSend->setText("Отправить");

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
    ui->labelCredits->setText(QString("💳 %1 %2\nБаланс").arg(network.authedId.credits).arg(network.authedId.currencyType));
    ui->labelVipDays->setText(QString("👑 %1 ДНЕЙ\nVIP статус").arg(network.authedId.vipDays));

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
    ui->statusAuthText->setText("Авторизация");

    qobject_cast<QWidget *>(sender())->setEnabled(false);
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
            ui->statusAuthText->setText(resText);
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
            resourceName = ":/resources/app-style-dark";
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
