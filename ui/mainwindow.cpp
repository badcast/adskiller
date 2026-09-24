#include <functional>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <list>
#include <memory>

#include <QButtonGroup>
#include <QCloseEvent>
#include <QResizeEvent>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QEnterEvent>
#endif
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
#include <QPalette>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStringListModel>
#include <QStyleFactory>
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
#include "AppleIpswWidget.h"
#include "AdbDeviceVisualizer.h"
#include "CyberReactorLoader.h"

MainWindow *MainWindow::current;

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow), timerAuthAnim(nullptr), app(qApp)
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

    m_shockwaveOverlay = new ServiceShockwaveOverlay(this);
    m_shockwaveOverlay->setGeometry(this->rect());

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

    // Setup SVG icons on action buttons
    ui->butShowPass->setText(QString());
    ui->butShowPass->setIcon(QIcon(":/svg/eye"));
    ui->butShowPass->setIconSize(QSize(16, 16));
    ui->butShowPass->setToolTip(tr("Показать пароль"));
    ui->butShowPass->setCursor(Qt::PointingHandCursor);

    ui->aiChatSend->setText(QString());
    ui->aiChatSend->setIcon(QIcon(":/svg/send"));
    ui->aiChatSend->setIconSize(QSize(15, 15));
    ui->aiChatSend->setCursor(Qt::PointingHandCursor);
}

MainWindow::~MainWindow()
{
    ServiceProvider::closeService();
    Adb::killServer();
    AppSetting::save();
    delete ui;
}

// ============================================================================
// ServiceInfoDialog Implementation
// ============================================================================

ServiceInfoDialog::ServiceInfoDialog(const ServiceTileButton::Details &details, QWidget *parent) : QDialog(parent), m_details(details)
{
    setWindowTitle(QString::fromUtf8("О сервисе — %1").arg(m_details.title));
    setModal(true);
    setMinimumWidth(520);
    setMaximumWidth(580);
    setStyleSheet(QStringLiteral(
        "QDialog {"
        "    background-color: #0B1120;"
        "    border: 1.5px solid #1E293B;"
        "    color: #F8FAFC;"
        "    font-family: 'Segoe UI', -apple-system, sans-serif;"
        "}"
        "QLabel {"
        "    background: transparent;"
        "    color: #CBD5E1;"
        "}"
        "QScrollArea {"
        "    background: transparent;"
        "    border: none;"
        "}"
        "QScrollBar:vertical {"
        "    border: none;"
        "    background: #0B0F19;"
        "    width: 6px;"
        "    margin: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "    background: #334155;"
        "    min-height: 20px;"
        "    border-radius: 3px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "    background: #38BDF8;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "    height: 0px;"
        "}"));

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 22, 24, 20);
    mainLayout->setSpacing(16);

    // 1. Header Section: Icon Plate + Title + Badges
    QHBoxLayout *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(16);

    QFrame *iconFrame = new QFrame(this);
    iconFrame->setFixedSize(54, 54);
    iconFrame->setStyleSheet(QStringLiteral(
        "QFrame {"
        "    background-color: #070A12;"
        "    border: 1.5px solid #1E293B;"
        "    border-radius: 6px;"
        "}"));
    QVBoxLayout *iconLayout = new QVBoxLayout(iconFrame);
    iconLayout->setContentsMargins(0, 0, 0, 0);
    iconLayout->setAlignment(Qt::AlignCenter);

    QLabel *iconLbl = new QLabel(iconFrame);
    iconLbl->setFixedSize(40, 40);
    iconLbl->setAlignment(Qt::AlignCenter);
    QIcon icon = m_details.icon;
    if(icon.isNull())
        icon = QIcon(":/svg/bot");
    iconLbl->setPixmap(icon.pixmap(38, 38));
    iconLayout->addWidget(iconLbl);
    headerLayout->addWidget(iconFrame);

    QVBoxLayout *titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(6);
    titleLayout->setAlignment(Qt::AlignVCenter);

    QLabel *titleLbl = new QLabel(m_details.title, this);
    titleLbl->setStyleSheet(QStringLiteral("color: #F8FAFC; font-size: 15px; font-weight: bold;"));
    titleLbl->setWordWrap(true);
    titleLayout->addWidget(titleLbl);

    QHBoxLayout *badgesLayout = new QHBoxLayout();
    badgesLayout->setSpacing(8);

    QLabel *tierBadge = new QLabel(this);
    tierBadge->setFixedHeight(22);
    if(!m_details.isActive || m_details.isUnderDev || m_details.tier == ServiceTileButton::Tier::Disabled)
    {
        tierBadge->setText(QString::fromUtf8("  В РАЗРАБОТКЕ / ОГРАНИЧЕН  "));
        tierBadge->setStyleSheet(QStringLiteral("background: #181E29; border: 1px solid #334155; color: #94A3B8; font-size: 10px; font-weight: bold; border-radius: 2px;"));
    }
    else if(m_details.tier == ServiceTileButton::Tier::Vip)
    {
        tierBadge->setText(QString::fromUtf8("  VIP СЕРВИС  "));
        tierBadge->setStyleSheet(QStringLiteral("background: #241806; border: 1px solid #92400E; color: #FBBF24; font-size: 10px; font-weight: bold; border-radius: 2px;"));
    }
    else if(m_details.tier == ServiceTileButton::Tier::Free)
    {
        tierBadge->setText(QString::fromUtf8("  БЕСПЛАТНЫЙ СЕРВИС  "));
        tierBadge->setStyleSheet(QStringLiteral("background: #062516; border: 1px solid #059669; color: #34D399; font-size: 10px; font-weight: bold; border-radius: 2px;"));
    }
    else if(m_details.tier == ServiceTileButton::Tier::Dynamic)
    {
        tierBadge->setText(QString::fromUtf8("  ТАРИФ НА ВЫБОР  "));
        tierBadge->setStyleSheet(QStringLiteral("background: #0E1B38; border: 1px solid #2563EB; color: #60A5FA; font-size: 10px; font-weight: bold; border-radius: 2px;"));
    }
    else
    {
        tierBadge->setText(QString::fromUtf8("  ПЛАТНЫЙ СЕРВИС  "));
        tierBadge->setStyleSheet(QStringLiteral("background: #0C2038; border: 1px solid #0284C7; color: #38BDF8; font-size: 10px; font-weight: bold; border-radius: 2px;"));
    }
    badgesLayout->addWidget(tierBadge);

    QLabel *connBadge = new QLabel(QString("  %1  ").arg(m_details.connectTypeName), this);
    connBadge->setFixedHeight(22);
    connBadge->setStyleSheet(QStringLiteral("background: #1E293B; border: 1px solid #334155; color: #CBD5E1; font-size: 10px; font-weight: bold; border-radius: 2px;"));
    badgesLayout->addWidget(connBadge);
    badgesLayout->addStretch(1);

    titleLayout->addLayout(badgesLayout);
    headerLayout->addLayout(titleLayout, 1);
    mainLayout->addLayout(headerLayout);

    // Separator line
    QFrame *sep1 = new QFrame(this);
    sep1->setFixedHeight(1);
    sep1->setStyleSheet(QStringLiteral("background: #1E293B; border: none;"));
    mainLayout->addWidget(sep1);

    // 2. Conditions of Use & Pricing Panel
    QFrame *condCard = new QFrame(this);
    condCard->setStyleSheet(QStringLiteral(
        "QFrame {"
        "    background-color: #0F172A;"
        "    border: 1px solid #1E293B;"
        "    border-radius: 4px;"
        "    padding: 12px;"
        "}"));
    QVBoxLayout *condLayout = new QVBoxLayout(condCard);
    condLayout->setContentsMargins(12, 10, 12, 10);
    condLayout->setSpacing(8);

    QLabel *condHeader = new QLabel(QString::fromUtf8("УСЛОВИЯ И СТОИМОСТЬ"), condCard);
    condHeader->setStyleSheet(QStringLiteral("color: #38BDF8; font-size: 11px; font-weight: 800; letter-spacing: 0.5px;"));
    condLayout->addWidget(condHeader);

    QString priceTextHtml;
    bool hasVip = (MainWindow::current && MainWindow::current->network.isAuthed() && MainWindow::current->network.authedId.hasVipAccount());

    if(!m_details.isActive || m_details.isUnderDev || m_details.tier == ServiceTileButton::Tier::Disabled)
    {
        priceTextHtml = QString::fromUtf8(
            "• <b>Статус:</b> <span style='color: #F87171;'>В разработке или временно отключен на сервере</span>.<br/>"
            "• Запуск модуля в текущей версии ограничен или ожидает тестирования.");
    }
    else if(m_details.tier == ServiceTileButton::Tier::Free || m_details.price == 0)
    {
        priceTextHtml = QString::fromUtf8(
            "• <b>Стоимость:</b> <span style='color: #34D399; font-weight: bold;'>0 кредитов (Бесплатно)</span>.<br/>"
            "• Сервис доступен всем пользователям без списания баланса.");
    }
    else if(m_details.tier == ServiceTileButton::Tier::Vip && hasVip)
    {
        priceTextHtml = QString::fromUtf8(
            "• <b>VIP • Безлимитно:</b> <span style='color: #FBBF24; font-weight: bold;'>Включено в вашу активную VIP-подписку</span>.<br/>"
            "• Безлимитный доступ без ограничений и без списания кредитов.");
    }
    else if(m_details.needVip)
    {
        priceTextHtml = QString::fromUtf8(
                            "• <b>Стоимость:</b> <span style='color: #38BDF8; font-weight: bold;'>%1 %2</span> "
                            "или <span style='color: #FBBF24; font-weight: bold;'>БЕСПЛАТНО с подпиской VIP</span>.<br/>"
                            "• Обладатели активного VIP-аккаунта пользуются сервисом безлимитно.")
                            .arg(m_details.price)
                            .arg(m_details.currency);
    }
    else if(m_details.tier == ServiceTileButton::Tier::Dynamic)
    {
        priceTextHtml = QString::fromUtf8(
            "• <b>Тариф на выбор:</b> Стоимость рассчитывается в зависимости от выбранного режима в окне сервиса.<br/>"
            "• При наличии VIP-подписки могут предоставляться специальные условия.");
    }
    else
    {
        priceTextHtml = QString::fromUtf8(
                            "• <b>Стоимость:</b> <span style='color: #38BDF8; font-weight: bold;'>%1 %2</span>.<br/>"
                            "• Списание кредитов производится только после успешного выполнения операции.")
                            .arg(m_details.price)
                            .arg(m_details.currency);
    }

    QLabel *priceDescLbl = new QLabel(condCard);
    priceDescLbl->setTextFormat(Qt::RichText);
    priceDescLbl->setText(priceTextHtml);
    priceDescLbl->setWordWrap(true);
    priceDescLbl->setStyleSheet(QStringLiteral("color: #CBD5E1; font-size: 11.5px; line-height: 1.4;"));
    condLayout->addWidget(priceDescLbl);

    mainLayout->addWidget(condCard);

    // 3. Service Description Panel
    QLabel *descHeader = new QLabel(QString::fromUtf8("ОПИСАНИЕ СЕРВИСА"), this);
    descHeader->setStyleSheet(QStringLiteral("color: #94A3B8; font-size: 11px; font-weight: 800; letter-spacing: 0.5px;"));
    mainLayout->addWidget(descHeader);

    QFrame *descFrame = new QFrame(this);
    descFrame->setStyleSheet(QStringLiteral(
        "QFrame {"
        "    background-color: #070A12;"
        "    border: 1px solid #1E293B;"
        "    border-radius: 4px;"
        "}"));
    QVBoxLayout *descFrameLayout = new QVBoxLayout(descFrame);
    descFrameLayout->setContentsMargins(12, 10, 12, 10);

    QScrollArea *scrollDesc = new QScrollArea(descFrame);
    scrollDesc->setWidgetResizable(true);
    scrollDesc->setMaximumHeight(130);

    QString descContent = m_details.description.trimmed();
    if(descContent.isEmpty())
    {
        descContent = QString::fromUtf8(
            "Интеллектуальный сервисный модуль платформы AdsKiller. "
            "Для получения детальной консультации и инструкций нажмите кнопку «Спросить ИИ» ниже.");
    }

    QLabel *descTextLbl = new QLabel(descContent, scrollDesc);
    descTextLbl->setWordWrap(true);
    descTextLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    descTextLbl->setStyleSheet(QStringLiteral("color: #E2E8F0; font-size: 12px; line-height: 1.45;"));
    scrollDesc->setWidget(descTextLbl);
    descFrameLayout->addWidget(scrollDesc);

    mainLayout->addWidget(descFrame);

    // 4. Action Buttons Footer: [ 🤖 Спросить ИИ ] on left, [ Закрыть ] on right
    QHBoxLayout *footerLayout = new QHBoxLayout();
    footerLayout->setContentsMargins(0, 6, 0, 0);
    footerLayout->setSpacing(12);

    QPushButton *btnAskAi = new QPushButton(QString::fromUtf8("🤖 Спросить ИИ"), this);
    btnAskAi->setCursor(Qt::PointingHandCursor);
    btnAskAi->setToolTip(QString::fromUtf8("Задать вопрос ИИ-ассистенту о возможностях и порядке работы данного сервиса"));
    btnAskAi->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0284C7, stop:1 #0369A1);"
        "    color: #FFFFFF;"
        "    border: 1px solid #38BDF8;"
        "    border-radius: 4px;"
        "    font-size: 12px;"
        "    font-weight: bold;"
        "    padding: 7px 18px;"
        "    min-height: 28px;"
        "}"
        "QPushButton:hover {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0EA5E9, stop:1 #0284C7);"
        "    border-color: #7DD3FC;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #0369A1;"
        "}"));
    connect(btnAskAi, &QPushButton::clicked, this, &ServiceInfoDialog::onAskAiClicked);
    footerLayout->addWidget(btnAskAi);

    footerLayout->addStretch(1);

    QPushButton *btnClose = new QPushButton(QString::fromUtf8("Закрыть"), this);
    btnClose->setCursor(Qt::PointingHandCursor);
    btnClose->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "    background-color: #1E293B;"
        "    color: #CBD5E1;"
        "    border: 1px solid #334155;"
        "    border-radius: 4px;"
        "    font-size: 12px;"
        "    font-weight: bold;"
        "    padding: 7px 20px;"
        "    min-height: 28px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #334155;"
        "    color: #FFFFFF;"
        "    border-color: #64748B;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #0F172A;"
        "}"));
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
    footerLayout->addWidget(btnClose);

    mainLayout->addLayout(footerLayout);
}

void ServiceInfoDialog::onAskAiClicked()
{
    if(!MainWindow::current)
    {
        accept();
        return;
    }

    if(!MainWindow::current->network.isAuthed())
    {
        QMessageBox msg(this);
        msg.setWindowTitle(QString::fromUtf8("Требуется авторизация"));
        msg.setIcon(QMessageBox::Information);
        msg.setStyleSheet(QStringLiteral(
            "QMessageBox { background-color: #0F172A; color: #F8FAFC; }"
            "QLabel { color: #E2E8F0; font-size: 12px; }"
            "QPushButton { background-color: #1E293B; color: #FFFFFF; border: 1px solid #334155; border-radius: 3px; padding: 5px 16px; font-weight: bold; }"
            "QPushButton:hover { background-color: #0284C7; border-color: #38BDF8; }"));
        msg.setText(QString::fromUtf8("Для общения с ИИ-ассистентом необходимо войти в свой аккаунт AdsKiller."));
        msg.exec();
        return;
    }

    // Authorized! Close info dialog
    accept();

    const QString query = QString::fromUtf8("Объясни подробнее, сервис \"%1\" что она делает и зачем?").arg(m_details.title);
    MainWindow::current->askAiQuestion(query);
}

// ============================================================================
// ServiceTileButton Implementation (Metro UI Tile Card with "NEW" Red Ribbon)
// ============================================================================

bool ServiceTileButton::s_isAnyLaunching = false;

ServiceTileButton::ServiceTileButton(const QIcon &icon, const QString &title, const QString &badgeText, Tier tier, bool showRibbon, const QString &ribbonText, QWidget *parent)
    : QPushButton(parent), m_title(title), m_badgeText(badgeText), m_tier(tier), m_showRibbon(showRibbon), m_ribbonText(ribbonText)
{
    m_details.title = title;
    m_details.badgeText = badgeText;
    m_details.tier = tier;
    m_details.icon = icon;

    setIcon(icon);
    setAttribute(Qt::WA_Hover, true);
    setMouseTracking(true);
    setFixedHeight(70);
    setMinimumWidth(260);
    setMaximumWidth(360);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setCursor(tier == Tier::Disabled ? Qt::ForbiddenCursor : Qt::PointingHandCursor);
    setStyleSheet("ServiceTileButton { background: transparent; border: none; outline: none; padding: 0px; margin: 0px; }");

    m_btnHelp = new QPushButton(QString::fromUtf8("(?) Что это?"), this);
    m_btnHelp->setObjectName(QStringLiteral("serviceHelpBtn"));
    m_btnHelp->setCursor(Qt::PointingHandCursor);
    m_btnHelp->setFocusPolicy(Qt::NoFocus);
    m_btnHelp->setStyleSheet(QStringLiteral(
        "QPushButton#serviceHelpBtn {"
        "    background-color: #1E293B;"
        "    color: #38BDF8;"
        "    border: 1px solid #0284C7;"
        "    border-radius: 3px;"
        "    font-family: 'Segoe UI', -apple-system, sans-serif;"
        "    font-size: 11px;"
        "    font-weight: bold;"
        "    padding: 1px 6px;"
        "    min-width: 76px;"
        "    min-height: 20px;"
        "    max-height: 22px;"
        "}"
        "QPushButton#serviceHelpBtn:hover {"
        "    background-color: #0284C7;"
        "    color: #FFFFFF;"
        "    border: 1px solid #38BDF8;"
        "}"
        "QPushButton#serviceHelpBtn:pressed {"
        "    background-color: #0369A1;"
        "    color: #E0F2FE;"
        "    border: 1px solid #0284C7;"
        "}"));
    m_btnHelp->setToolTip(QString::fromUtf8("Подробная информация о сервисе и условиях"));
    connect(m_btnHelp, &QPushButton::clicked, this, &ServiceTileButton::showInfoDialog);

    const int initW = 280;
    const int btnW = 86;
    const int btnH = 22;
    m_btnHelp->setGeometry(initW - btnW - 8, 70 - btnH - 6, btnW, btnH);
    m_btnHelp->setVisible(true);
    m_btnHelp->show();
    m_btnHelp->raise();

    m_rippleAnim = new QVariantAnimation(this);
    m_rippleAnim->setDuration(380);
    m_rippleAnim->setStartValue(0.0);
    m_rippleAnim->setEndValue(1.0);
    m_rippleAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(
        m_rippleAnim,
        &QVariantAnimation::valueChanged,
        this,
        [this](const QVariant &val)
        {
            m_rippleProgress = val.toReal();
            const qreal maxR = qMax(width(), height()) * 1.35;
            m_rippleRadius = m_rippleProgress * maxR;
            m_rippleOpacity = 0.40 * (1.0 - m_rippleProgress);
            update();
        });
    connect(
        m_rippleAnim,
        &QVariantAnimation::finished,
        this,
        [this]()
        {
            m_rippleRadius = 0.0;
            m_rippleOpacity = 0.0;
            update();
        });

    m_clickAnim = new QVariantAnimation(this);
    m_clickAnim->setDuration(1000);
    m_clickAnim->setStartValue(0.0);
    m_clickAnim->setEndValue(1.0);
    m_clickAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(
        m_clickAnim,
        &QVariantAnimation::valueChanged,
        this,
        [this](const QVariant &val)
        {
            m_clickProgress = val.toReal();
            update();
        });
    connect(
        m_clickAnim,
        &QVariantAnimation::finished,
        this,
        [this]()
        {
            m_clickProgress = 0.0;
            update();
        });
}

void ServiceTileButton::setTier(Tier tier)
{
    m_tier = tier;
    m_details.tier = tier;
    setCursor((!m_serviceActive || tier == Tier::Disabled) ? Qt::ForbiddenCursor : Qt::PointingHandCursor);
    update();
}

void ServiceTileButton::setTitle(const QString &title)
{
    m_title = title;
    m_details.title = title;
    update();
}

void ServiceTileButton::setBadgeText(const QString &text)
{
    m_badgeText = text;
    m_details.badgeText = text;
    update();
}

void ServiceTileButton::setServiceActive(bool active)
{
    m_serviceActive = active;
    m_details.isActive = active;
    setCursor((!m_serviceActive || m_tier == Tier::Disabled) ? Qt::ForbiddenCursor : Qt::PointingHandCursor);
    update();
}

void ServiceTileButton::setServiceDetails(const Details &details)
{
    m_details = details;
    m_serviceActive = details.isActive;
    setCursor((!m_serviceActive || m_tier == Tier::Disabled) ? Qt::ForbiddenCursor : Qt::PointingHandCursor);
    update();
}

void ServiceTileButton::showInfoDialog()
{
    ServiceInfoDialog dlg(m_details, this);
    dlg.exec();
}

void ServiceTileButton::showEvent(QShowEvent *event)
{
    QPushButton::showEvent(event);
    if(m_btnHelp)
    {
        const int btnW = 86;
        const int btnH = 22;
        const int btnX = width() - btnW - 8;
        const int btnY = height() - btnH - 6;
        m_btnHelp->setGeometry(btnX, btnY, btnW, btnH);
        m_btnHelp->setVisible(true);
        m_btnHelp->show();
        m_btnHelp->raise();
    }
}

void ServiceTileButton::resizeEvent(QResizeEvent *event)
{
    QPushButton::resizeEvent(event);
    if(m_btnHelp)
    {
        const int btnW = 86;
        const int btnH = 22;
        const int btnX = width() - btnW - 8;
        const int btnY = height() - btnH - 6;
        m_btnHelp->setGeometry(btnX, btnY, btnW, btnH);
        m_btnHelp->setVisible(true);
        m_btnHelp->show();
        m_btnHelp->raise();
    }
}

void ServiceTileButton::setShowRibbon(bool show)
{
    m_showRibbon = show;
    update();
}

void ServiceTileButton::setRibbonText(const QString &text)
{
    m_ribbonText = text;
    update();
}

void ServiceTileButton::triggerClickAnimation()
{
    if(m_clickAnim)
    {
        m_clickAnim->stop();
        m_clickProgress = 0.0;
        m_clickAnim->start();
    }
}

QSize ServiceTileButton::sizeHint() const
{
    return QSize(280, 70);
}

QSize ServiceTileButton::minimumSizeHint() const
{
    return QSize(260, 70);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void ServiceTileButton::enterEvent(QEnterEvent *event)
{
    QPushButton::enterEvent(event);
    m_isHovered = true;
    m_mousePos = event->position().toPoint();
    update();
}
#else
void ServiceTileButton::enterEvent(QEvent *event)
{
    QPushButton::enterEvent(event);
    m_isHovered = true;
    m_mousePos = mapFromGlobal(QCursor::pos());
    update();
}
#endif

void ServiceTileButton::leaveEvent(QEvent *event)
{
    QPushButton::leaveEvent(event);
    m_isHovered = false;
    update();
}

void ServiceTileButton::mouseMoveEvent(QMouseEvent *event)
{
    QPushButton::mouseMoveEvent(event);
    m_mousePos = event->pos();
    m_isHovered = true;
    update();
}

void ServiceTileButton::mousePressEvent(QMouseEvent *event)
{
    if(m_btnHelp && m_btnHelp->geometry().contains(event->pos()))
    {
        showInfoDialog();
        event->accept();
        return;
    }
    if(m_isLaunching || s_isAnyLaunching || !m_serviceActive || m_tier == Tier::Disabled)
        return;
    m_pressPos = event->pos();
    if(isEnabled())
    {
        m_rippleAnim->stop();
        m_rippleProgress = 0.0;
        m_rippleRadius = 15.0;
        m_rippleOpacity = 0.45;
        m_rippleAnim->start();
    }
    QPushButton::mousePressEvent(event);
}

void ServiceTileButton::mouseReleaseEvent(QMouseEvent *event)
{
    if(m_btnHelp && m_btnHelp->geometry().contains(event->pos()))
    {
        event->accept();
        return;
    }
    if(m_isLaunching || s_isAnyLaunching || !m_serviceActive || m_tier == Tier::Disabled)
        return;
    if(rect().contains(event->pos()) && isEnabled())
    {
        triggerClickAnimation();
    }
    QPushButton::mouseReleaseEvent(event);
}

void ServiceTileButton::keyPressEvent(QKeyEvent *event)
{
    if(m_isLaunching || s_isAnyLaunching || !m_serviceActive || m_tier == Tier::Disabled)
    {
        event->accept();
        return;
    }
    QPushButton::keyPressEvent(event);
}

void ServiceTileButton::keyReleaseEvent(QKeyEvent *event)
{
    if(m_isLaunching || s_isAnyLaunching || !m_serviceActive || m_tier == Tier::Disabled)
    {
        event->accept();
        return;
    }
    QPushButton::keyReleaseEvent(event);
}

void ServiceTileButton::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const int w = width();
    const int h = height();
    const bool enabled = isEnabled() && m_serviceActive && (m_tier != Tier::Disabled);
    const bool hovered = enabled && (underMouse() || m_isHovered);
    const bool pressed = enabled && isDown();

    // 1. Metro Card Shell: Base Dark Obsidian Background
    QColor bgColor;
    QColor borderColor;
    if(!enabled)
    {
        bgColor = QColor(11, 15, 25);     // #0B0F19
        borderColor = QColor(30, 41, 59); // #1E293B
    }
    else if(pressed)
    {
        bgColor = QColor(8, 14, 26);       // #080E1A
        borderColor = QColor(2, 132, 199); // #0284C7
    }
    else if(hovered)
    {
        bgColor = QColor(19, 31, 55);       // #131F37
        borderColor = QColor(56, 189, 248); // #38BDF8
    }
    else
    {
        bgColor = QColor(15, 23, 42);     // #0F172A
        borderColor = QColor(30, 41, 59); // #1E293B
    }

    painter.fillRect(QRect(0, 0, w, h), bgColor);

    // 1b. Windows 10 Reveal Highlight: cursor flashlight gradient
    if(enabled && hovered)
    {
        QRadialGradient revealLight(m_mousePos, 160);
        revealLight.setColorAt(0.0, QColor(0, 229, 255, 38)); // Electric cyan soft spotlight
        revealLight.setColorAt(0.4, QColor(2, 132, 199, 18)); // Deep azure falloff
        revealLight.setColorAt(1.0, QColor(2, 132, 199, 0));  // Transparent
        painter.fillRect(QRect(0, 0, w, h), revealLight);
    }

    // 1c. Windows 10 Bubble Press Ripple Effect
    if(m_rippleOpacity > 0.01)
    {
        painter.save();
        painter.setClipRect(QRect(0, 0, w, h));
        QRadialGradient bubble(m_pressPos, qMax<qreal>(10.0, m_rippleRadius));
        int alpha = qBound(0, static_cast<int>(m_rippleOpacity * 255), 255);
        bubble.setColorAt(0.0, QColor(0, 229, 255, alpha));
        bubble.setColorAt(0.5, QColor(2, 132, 199, alpha * 2 / 3));
        bubble.setColorAt(0.85, QColor(14, 165, 233, alpha / 3));
        bubble.setColorAt(1.0, QColor(2, 132, 199, 0));
        painter.setBrush(bubble);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(m_pressPos, static_cast<int>(m_rippleRadius), static_cast<int>(m_rippleRadius));
        painter.restore();
    }

    // 1d. Border: Reveal Border glow near mouse cursor
    if(enabled && hovered)
    {
        QRadialGradient borderGlow(m_mousePos, 140);
        borderGlow.setColorAt(0.0, QColor(0, 229, 255, 230));  // Hot glowing edge near mouse
        borderGlow.setColorAt(0.5, QColor(56, 189, 248, 120)); // Soft cyan
        borderGlow.setColorAt(1.0, borderColor);               // Base border color
        painter.setPen(QPen(QBrush(borderGlow), 1));
        painter.drawRect(QRect(0, 0, w - 1, h - 1));
    }
    else
    {
        painter.setPen(QPen(borderColor, 1));
        painter.drawRect(QRect(0, 0, w - 1, h - 1));
    }

    // 2. Left Status Accent Stripe (4px width)
    QColor accentColor;
    if(!enabled || m_tier == Tier::Disabled)
    {
        accentColor = QColor(71, 85, 105); // #475569 Slate
    }
    else
    {
        switch(m_tier)
        {
            case Tier::Vip:
                accentColor = hovered ? QColor(251, 191, 36) : QColor(245, 158, 11); // Gold #F59E0B / #FBBF24
                break;
            case Tier::Free:
                accentColor = hovered ? QColor(52, 211, 153) : QColor(16, 185, 129); // Emerald #10B981 / #34D399
                break;
            case Tier::Dynamic:
                accentColor = hovered ? QColor(129, 140, 248) : QColor(99, 102, 241); // Indigo #818CF8
                break;
            case Tier::Credit:
            default:
                accentColor = hovered ? QColor(56, 189, 248) : QColor(14, 165, 233); // Sky Cyan #38BDF8
                break;
        }
    }
    painter.fillRect(QRect(0, 0, 4, h), accentColor);

    // 3. Recessed Icon Plate Sub-Panel
    const int plateX = 12;
    const int plateY = (h - 54) / 2;
    const int plateSize = 54;
    const QRect plateRect(plateX, plateY, plateSize, plateSize);

    const QColor plateBg = hovered ? QColor(11, 17, 30) : QColor(7, 10, 18);
    const QColor plateBorder = hovered ? QColor(51, 65, 85) : QColor(26, 36, 54);
    painter.fillRect(plateRect, plateBg);
    painter.setPen(QPen(plateBorder, 1));
    painter.drawRect(plateRect);

    // Center icon inside plate (42x42)
    const QRect iconRect(plateX + 6, plateY + 6, plateSize - 12, plateSize - 12);
    if(!enabled)
    {
        painter.setOpacity(0.35);
        icon().paint(&painter, iconRect);
        painter.setOpacity(1.0);
    }
    else
    {
        icon().paint(&painter, iconRect);
    }

    // 4. Service Title Typography
    const int textX = plateRect.right() + 12;
    const int textTop = plateY + 1;
    // Reserved width for top-right ribbon
    const int ribbonReservation = (m_showRibbon && enabled) ? 68 : 12;
    const int maxTitleW = qMax(40, w - textX - ribbonReservation);

    QFont titleFont("Segoe UI", 10, QFont::Bold);
    painter.setFont(titleFont);
    QFontMetrics tfm(titleFont);
    const QString elidedTitle = tfm.elidedText(m_title, Qt::ElideRight, maxTitleW);

    QColor titleColor;
    if(!enabled)
        titleColor = QColor(100, 116, 139); // #64748B
    else if(hovered)
        titleColor = QColor(56, 189, 248); // #38BDF8
    else
        titleColor = QColor(248, 250, 252); // #F8FAFC

    painter.setPen(titleColor);
    painter.drawText(QRect(textX, textTop, maxTitleW, 22), Qt::AlignLeft | Qt::AlignVCenter, elidedTitle);

    // 5. Pricing / Status Badge Pill
    if(!m_badgeText.isEmpty())
    {
        QFont badgeFont("Segoe UI", 8, QFont::Bold);
        painter.setFont(badgeFont);
        QFontMetrics bfm(badgeFont);

        QColor pillBg;
        QColor pillBorder;
        QColor pillText;

        if(!enabled || m_tier == Tier::Disabled)
        {
            pillBg = QColor(24, 30, 41);
            pillBorder = QColor(51, 65, 85);
            pillText = QColor(148, 163, 184);
        }
        else
        {
            switch(m_tier)
            {
                case Tier::Vip:
                    pillBg = QColor(36, 24, 6);
                    pillBorder = QColor(146, 64, 14);
                    pillText = QColor(251, 191, 36);
                    break;
                case Tier::Free:
                    pillBg = QColor(6, 37, 22);
                    pillBorder = QColor(5, 150, 105);
                    pillText = QColor(52, 211, 153);
                    break;
                case Tier::Dynamic:
                    pillBg = QColor(14, 27, 56);
                    pillBorder = QColor(37, 99, 235);
                    pillText = QColor(96, 165, 250);
                    break;
                case Tier::Credit:
                default:
                    pillBg = QColor(12, 32, 56);
                    pillBorder = QColor(2, 132, 199);
                    pillText = QColor(56, 189, 248);
                    break;
            }
        }

        const int badgeH = 22;
        const int badgeY = plateRect.bottom() - badgeH;
        const int maxPillW = qMax(40, w - textX - 98);
        const int textW = bfm.horizontalAdvance(m_badgeText);
        const int pillW = qBound(50, textW + 16, maxPillW);
        const QRect pillRect(textX, badgeY, pillW, badgeH);

        painter.fillRect(pillRect, pillBg);
        painter.setPen(QPen(pillBorder, 1));
        painter.drawRect(pillRect);

        painter.setPen(pillText);
        const QString elidedBadge = bfm.elidedText(m_badgeText, Qt::ElideRight, pillRect.width() - 8);
        painter.drawText(pillRect, Qt::AlignCenter, elidedBadge);
    }

    // 6. Top-Right Red Ribbon Banner with "NEW" or "BETA" Inscription
    if(m_showRibbon)
    {
        const int ribbonW = 58;
        const int ribbonH = 21;
        const int notch = 8;
        const int rx = w - ribbonW;
        const int ry = 0;

        // Physical drop shadow behind ribbon
        QPolygon shadowPoly;
        shadowPoly << QPoint(w, ry + 2) << QPoint(rx + 1, ry + 2) << QPoint(rx + notch + 1, ry + ribbonH / 2 + 1) << QPoint(rx + 1, ry + ribbonH + 2) << QPoint(w, ry + ribbonH + 2);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 110));
        painter.drawPolygon(shadowPoly);

        // Ribbon polygon with decorative swallowtail notch cut on the left
        QPolygon ribbonPoly;
        ribbonPoly << QPoint(w, ry) << QPoint(rx, ry) << QPoint(rx + notch, ry + ribbonH / 2) << QPoint(rx, ry + ribbonH) << QPoint(w, ry + ribbonH);

        // Rich crimson gradient
        QLinearGradient ribbonGrad(rx, ry, w, ry + ribbonH);
        if(hovered)
        {
            ribbonGrad.setColorAt(0.0, QColor(254, 120, 120));
            ribbonGrad.setColorAt(0.3, QColor(248, 113, 113));
            ribbonGrad.setColorAt(0.8, QColor(220, 38, 38));
            ribbonGrad.setColorAt(1.0, QColor(185, 28, 28));
        }
        else
        {
            ribbonGrad.setColorAt(0.0, QColor(248, 113, 113)); // #F87171 Ruby highlight
            ribbonGrad.setColorAt(0.35, QColor(239, 68, 68));  // #EF4444 Vivid red
            ribbonGrad.setColorAt(0.8, QColor(220, 38, 38));   // #DC2626 Deep red
            ribbonGrad.setColorAt(1.0, QColor(153, 27, 27));   // #991B1B Crimson shadow
        }

        painter.setPen(QPen(QColor(185, 28, 28), 1));
        painter.setBrush(ribbonGrad);
        painter.drawPolygon(ribbonPoly);

        // Fine glossy highlight line along top edge
        painter.setPen(QPen(QColor(255, 255, 255, 90), 1));
        painter.drawLine(rx + 1, ry + 1, w - 1, ry + 1);

        // Ribbon Inscription "NEW"
        QFont ribbonFont("Segoe UI", 8, QFont::Black);
        ribbonFont.setBold(true);
        ribbonFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
        painter.setFont(ribbonFont);

        const QRect textRect(rx + notch / 2, ry, ribbonW - notch / 2, ribbonH);

        // Embossed 3D shadow for text
        painter.setPen(QColor(127, 29, 29)); // #7F1D1D
        painter.drawText(textRect.translated(0, 1), Qt::AlignCenter, m_ribbonText);

        // Pure white foreground
        painter.setPen(Qt::white);
        painter.drawText(textRect, Qt::AlignCenter, m_ribbonText);
    }

    // 7. Click / Activation Pulse Animation
    if(m_clickProgress > 0.0)
    {
        painter.save();
        painter.setClipRect(QRect(0, 0, w, h));

        const QColor accent = accentColor;

        // Luminous light sweep beam across the card
        const int beamW = 100;
        const int sweepX = -beamW + static_cast<int>(m_clickProgress * (w + beamW * 2));
        QLinearGradient sweepGrad(sweepX, 0, sweepX + beamW, 0);
        const int sweepAlpha = static_cast<int>((1.0 - m_clickProgress) * 110);
        sweepGrad.setColorAt(0.0, QColor(accent.red(), accent.green(), accent.blue(), 0));
        sweepGrad.setColorAt(0.5, QColor(accent.red(), accent.green(), accent.blue(), sweepAlpha));
        sweepGrad.setColorAt(1.0, QColor(accent.red(), accent.green(), accent.blue(), 0));
        painter.fillRect(QRect(0, 0, w, h), sweepGrad);

        // Expanding energy shockwave ring from the icon plate center
        const QPoint pCenter = plateRect.center();
        const qreal ringR = 26.0 + m_clickProgress * 55.0;
        const int ringAlpha = static_cast<int>((1.0 - m_clickProgress) * 220);
        painter.setPen(QPen(QColor(accent.red(), accent.green(), accent.blue(), ringAlpha), 2.5));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(pCenter, static_cast<int>(ringR), static_cast<int>(ringR));

        // Neon border flash
        const int borderPulseAlpha = static_cast<int>((1.0 - m_clickProgress) * 240);
        painter.setPen(QPen(QColor(accent.red(), accent.green(), accent.blue(), borderPulseAlpha), 1.5));
        painter.drawRect(QRect(0, 0, w - 1, h - 1));

        painter.restore();
    }
}

QColor ServiceTileButton::accentColor() const
{
    if(!isEnabled() || !m_serviceActive || m_tier == Tier::Disabled)
        return QColor(71, 85, 105);

    switch(m_tier)
    {
        case Tier::Vip:
            return QColor(245, 158, 11); // Gold #F59E0B
        case Tier::Free:
            return QColor(16, 185, 129); // Emerald #10B981
        case Tier::Dynamic:
            return QColor(129, 140, 248); // Indigo #818CF8
        case Tier::Credit:
        default:
            return QColor(0, 229, 255); // Cyan #00E5FF
    }
}

// ============================================================================
// ServiceShockwaveOverlay Implementation (Window-Wide Luminous Shockwave)
// ============================================================================

ServiceShockwaveOverlay::ServiceShockwaveOverlay(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_NoSystemBackground, true);
    setFocusPolicy(Qt::StrongFocus);
    setStyleSheet("background: transparent; border: none;");
    hide();

    m_anim = new QVariantAnimation(this);
    m_anim->setDuration(1000); // 1.0 second exact delay
    m_anim->setStartValue(0.0);
    m_anim->setEndValue(1.0);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);

    connect(
        m_anim,
        &QVariantAnimation::valueChanged,
        this,
        [this](const QVariant &val)
        {
            m_progress = val.toReal();
            update();
        });

    connect(
        m_anim,
        &QVariantAnimation::finished,
        this,
        [this]()
        {
            m_progress = 0.0;
            unsetCursor();
            hide();
        });
}

void ServiceShockwaveOverlay::trigger(const QPoint &centerInWindow, const QColor &accentColor)
{
    m_center = centerInWindow;
    m_accentColor = accentColor;
    if(parentWidget())
        setGeometry(parentWidget()->rect());
    setCursor(Qt::BusyCursor);
    raise();
    show();
    setFocus();
    m_anim->stop();
    m_progress = 0.0;
    m_anim->start();
}

void ServiceShockwaveOverlay::mousePressEvent(QMouseEvent *event)
{
    event->accept();
}

void ServiceShockwaveOverlay::mouseReleaseEvent(QMouseEvent *event)
{
    event->accept();
}

void ServiceShockwaveOverlay::mouseDoubleClickEvent(QMouseEvent *event)
{
    event->accept();
}

void ServiceShockwaveOverlay::keyPressEvent(QKeyEvent *event)
{
    event->accept();
}

void ServiceShockwaveOverlay::keyReleaseEvent(QKeyEvent *event)
{
    event->accept();
}

void ServiceShockwaveOverlay::paintEvent(QPaintEvent *)
{
    if(m_progress <= 0.0 || m_progress >= 1.0)
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const int w = width();
    const int h = height();
    const qreal maxRadius = std::hypot(qMax(m_center.x(), w - m_center.x()), qMax(m_center.y(), h - m_center.y())) * 1.08;
    const qreal currentRadius = m_progress * maxRadius;
    const qreal fade = qMax<qreal>(0.0, 1.0 - m_progress);
    const qreal easedFade = std::pow(fade, 0.65);

    // 1. Initial Epic Energy Core Blast (0.0 to 0.4 progress)
    if(m_progress < 0.4)
    {
        const qreal burstProgress = m_progress / 0.4;
        const qreal burstFade = 1.0 - burstProgress;
        const qreal burstRadius = 30.0 + burstProgress * 160.0;

        QRadialGradient burstGrad(m_center, burstRadius);
        QColor coreWhite = Qt::white;
        coreWhite.setAlphaF(burstFade * 0.85);

        QColor coreGlow = m_accentColor.lighter(140);
        coreGlow.setAlphaF(burstFade * 0.60);

        QColor outerGlow = m_accentColor;
        outerGlow.setAlphaF(0.0);

        burstGrad.setColorAt(0.0, coreWhite);
        burstGrad.setColorAt(0.35, coreGlow);
        burstGrad.setColorAt(1.0, outerGlow);

        painter.setBrush(burstGrad);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(m_center, static_cast<int>(burstRadius), static_cast<int>(burstRadius));
    }

    // 2. Window-Wide Expanding Radial Energy Distortion / Field ("Круг изменения по всему окну")
    if(currentRadius > 8.0)
    {
        const qreal ringBandWidth = qMin<qreal>(currentRadius * 0.45, 220.0);
        const qreal innerRadius = qMax<qreal>(0.0, currentRadius - ringBandWidth);

        QRadialGradient waveField(m_center, currentRadius);
        QColor fieldEdge = m_accentColor;
        fieldEdge.setAlphaF(easedFade * 0.24);

        QColor fieldPeak = m_accentColor.lighter(130);
        fieldPeak.setAlphaF(easedFade * 0.16);

        QColor fieldInner = m_accentColor;
        fieldInner.setAlphaF(0.0);

        waveField.setColorAt(0.0, fieldInner);
        if(currentRadius > 0.0)
        {
            qreal midStop = qBound<qreal>(0.0, innerRadius / currentRadius, 0.95);
            waveField.setColorAt(midStop, fieldInner);
            waveField.setColorAt((midStop + 1.0) * 0.5, fieldPeak);
        }
        waveField.setColorAt(1.0, fieldEdge);

        painter.setBrush(waveField);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(m_center, static_cast<int>(currentRadius), static_cast<int>(currentRadius));
    }

    // 3. Shockwave Front: High-Energy Outer Neon Rings
    // Ring A: Primary Leading Shockwave (sharp, bright beam)
    {
        const int shockAlpha = qBound(0, static_cast<int>(easedFade * 255), 255);
        QPen shockPen(QColor(m_accentColor.red(), m_accentColor.green(), m_accentColor.blue(), shockAlpha), 4.0);
        shockPen.setCapStyle(Qt::RoundCap);
        painter.setPen(shockPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(m_center, static_cast<int>(currentRadius), static_cast<int>(currentRadius));
    }

    // Ring B: Neon Corona Glow around Primary Ring (thick, soft halo)
    {
        const int haloAlpha = qBound(0, static_cast<int>(easedFade * 110), 255);
        QPen haloPen(QColor(m_accentColor.lighter(130).red(), m_accentColor.lighter(130).green(), m_accentColor.lighter(130).blue(), haloAlpha), 9.0);
        painter.setPen(haloPen);
        painter.drawEllipse(m_center, static_cast<int>(currentRadius), static_cast<int>(currentRadius));
    }

    // Ring C: Secondary Trailing Wavefront (at 82% radius)
    const qreal secondaryRadius = currentRadius * 0.82;
    if(secondaryRadius > 10.0)
    {
        const int secAlpha = qBound(0, static_cast<int>(easedFade * 150), 255);
        QPen secPen(QColor(m_accentColor.lighter(150).red(), m_accentColor.lighter(150).green(), m_accentColor.lighter(150).blue(), secAlpha), 2.2);
        painter.setPen(secPen);
        painter.drawEllipse(m_center, static_cast<int>(secondaryRadius), static_cast<int>(secondaryRadius));
    }

    // Ring D: Tertiary Harmonic Wavefront (at 64% radius)
    const qreal tertiaryRadius = currentRadius * 0.64;
    if(tertiaryRadius > 15.0)
    {
        const int terAlpha = qBound(0, static_cast<int>(easedFade * 90), 255);
        QPen terPen(QColor(m_accentColor.red(), m_accentColor.green(), m_accentColor.blue(), terAlpha), 1.4);
        painter.setPen(terPen);
        painter.drawEllipse(m_center, static_cast<int>(tertiaryRadius), static_cast<int>(tertiaryRadius));
    }

    // 4. Window Border Energy Impact Flash (progress 0.15 to 0.85)
    if(m_progress > 0.15 && m_progress < 0.85)
    {
        const qreal borderProgress = (m_progress - 0.15) / 0.70;
        const qreal borderIntensity = std::sin(borderProgress * 3.14159265);
        const int borderAlpha = qBound(0, static_cast<int>(borderIntensity * 140), 255);

        QPen borderPen(QColor(m_accentColor.red(), m_accentColor.green(), m_accentColor.blue(), borderAlpha), 2);
        painter.setPen(borderPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(rect().adjusted(1, 1, -1, -1));
    }
}

void MainWindow::showWindowShockwave(const QPoint &globalPos, const QColor &color)
{
    if(!m_shockwaveOverlay)
    {
        m_shockwaveOverlay = new ServiceShockwaveOverlay(this);
        m_shockwaveOverlay->setGeometry(this->rect());
    }
    const QPoint localPos = this->mapFromGlobal(globalPos);
    m_shockwaveOverlay->trigger(localPos, color);
}

void MainWindow::initServiceModules()
{
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
        uint32_t effectivePrice = (x == 0 && network.authedId.basePrice > 0 ? network.authedId.basePrice : remoteService->price);
        bool hasVIP = network.authedId.hasVipAccount();
        bool isDynamic = (remoteService->price == static_cast<std::uint32_t>(-1));
        bool isFree = (effectivePrice == 0);
        bool canBypassWithVip = (remoteService->needVIP && hasVIP);

        ServiceTileButton::Tier tier = ServiceTileButton::Tier::Credit;

        if(!instance->active)
        {
            tier = ServiceTileButton::Tier::Disabled;
            if(!instance->isAvailable())
                badgeText = QString::fromUtf8("В РАЗРАБОТКЕ");
            else if(!remoteService->active)
                badgeText = QString::fromUtf8("НЕ ДОСТУПЕН");
            else
                badgeText = QString::fromUtf8("ОТКЛЮЧЕН");
        }
        else if(canBypassWithVip)
        {
            // Service supports VIP and user HAS VIP account -> Unlimited access
            tier = ServiceTileButton::Tier::Vip;
            badgeText = QString::fromUtf8("VIP • БЕЗЛИМИТ");
        }
        else if(isFree)
        {
            // Completely free service for everyone
            tier = ServiceTileButton::Tier::Free;
            badgeText = QString::fromUtf8("БЕСПЛАТНО");
        }
        else if(isDynamic)
        {
            tier = ServiceTileButton::Tier::Dynamic;
            badgeText = remoteService->needVIP ? QString::fromUtf8("ТАРИФ (или VIP)") : QString::fromUtf8("ТАРИФ НА ВЫБОР");
        }
        else if(remoteService->needVIP)
        {
            // Service supports VIP, but user has NO VIP -> Show price in credits with VIP alternative
            tier = ServiceTileButton::Tier::Vip;
            badgeText = QString("%1 %2 (или VIP)").arg(effectivePrice).arg(network.authedId.currencyType);
        }
        else
        {
            // Service does NOT require/support VIP -> Standard credit price for all users
            tier = ServiceTileButton::Tier::Credit;
            badgeText = QString("%1 %2").arg(effectivePrice).arg(network.authedId.currencyType);
        }

        if(instance->uuid() != IDServiceAIAgentString)
        {
            QIcon svcIcon(":/svg/services/" + instance->widgetIconName());
            if(svcIcon.isNull())
                svcIcon = QIcon(":/service-icons/" + instance->widgetIconName());

            bool isApple = (remoteService->uuid == IDServiceAppleIpswString || instance->uuid() == IDServiceAppleIpswString);
            QString ribbonText = isApple ? QString::fromUtf8("BETA") : QString::fromUtf8("NEW");
            bool showRibbon = isApple || instance->active;

            ServiceTileButton *button = new ServiceTileButton(svcIcon, remoteService->name, badgeText, tier, showRibbon, ribbonText, ui->serviceContents);
            button->setServiceActive(instance->active);

            ServiceTileButton::Details details;
            details.title = remoteService->name;
            details.description = remoteService->description;
            details.badgeText = badgeText;
            details.tier = tier;
            details.isActive = instance->active;
            details.isUnderDev = !instance->isAvailable();
            details.needVip = remoteService->needVIP;
            details.price = effectivePrice;
            details.currency = network.authedId.currencyType;
            details.icon = svcIcon;

            if(instance->deviceConnectType() == DeviceConnectType::ADB)
                details.connectTypeName = QString::fromUtf8("Android (ADB)");
            else if(instance->deviceConnectType() == DeviceConnectType::Apple)
                details.connectTypeName = QString::fromUtf8("Apple iOS");
            else
                details.connectTypeName = QString::fromUtf8("Автономный");

            button->setServiceDetails(details);

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
            button->setProperty("serviceUuid", remoteService->uuid);
            button->setProperty("isAdsKiller", remoteService->uuid == IDServiceAdsString);
            button->setProperty("isFree", isFree);
            button->setProperty("isPaid", !isFree);
            button->setProperty("isActive", instance->active);
            button->setProperty("isUnderDev", false);
            button->setProperty("serviceName", remoteService->name);
            button->setProperty("serviceDesc", remoteService->description);

            // Target service by slot with credit verification
            QObject::connect(
                button,
                &QPushButton::clicked,
                this,
                [this, instance, effectivePrice, isDynamic, needVIP = remoteService->needVIP, name = remoteService->name, button]()
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
                                               "<table style='width: 100%; border-collapse: collapse; margin-bottom: 12px; background: rgba(30, 41, 59, 0.7); border: 1px solid rgba(148, 163, 184, 0.2); border-radius: 0px;'>"
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
                                                   needVIP ? QString::fromUtf8("<p style='margin: 4px 0 0 0; color: #CBD5E1; line-height: 1.4;'><i>Вы можете активировать <b>VIP-статус</b> для безлимитного доступа без списания кредитов, либо пополнить баланс через службу поддержки.</i></p>")
                                                           : QString::fromUtf8("<p style='margin: 4px 0 0 0; color: #CBD5E1; line-height: 1.4;'><i>Данная услуга оплачивается только кредитами (VIP-статус не поддерживается). Пополните баланс через раздел Поддержка.</i></p>"));

                        msgBox.setText(infoHtml);

                        msgBox.setStyleSheet(
                            "QMessageBox {"
                            "   background-color: #0F172A;"
                            "   color: #F8FAFC;"
                            "   border: 1px solid #334155;"
                            "   border-radius: 0px;"
                            "}"
                            "QLabel {"
                            "   color: #F8FAFC;"
                            "   background: transparent;"
                            "}"
                            "QPushButton {"
                            "   background-color: #1E293B;"
                            "   color: #F8FAFC;"
                            "   border: 1px solid #334155;"
                            "   border-radius: 0px;"
                            "   padding: 6px 16px;"
                            "   font-size: 12px;"
                            "   font-weight: bold;"
                            "   min-width: 85px;"
                            "}"
                            "QPushButton:hover {"
                            "   background-color: #334155;"
                            "   border-color: #38BDF8;"
                            "}"
                            "QPushButton:pressed {"
                            "   background-color: #0F172A;"
                            "}");

                        QPushButton *btnVip = nullptr;
                        QPushButton *btnSupport = nullptr;

                        if(needVIP)
                        {
                            btnVip = msgBox.addButton(QString::fromUtf8("Оформить VIP"), QMessageBox::ActionRole);
                            btnVip->setIcon(QIcon(":/svg/crown"));
                            btnVip->setIconSize(QSize(16, 16));
                            btnVip->setCursor(Qt::PointingHandCursor);
                            btnVip->setStyleSheet(
                                "QPushButton {"
                                "   background-color: #D97706;"
                                "   color: #FFFFFF;"
                                "   font-size: 12px;"
                                "   font-weight: bold;"
                                "   border: 1px solid #B45309;"
                                "   border-radius: 0px;"
                                "   padding: 6px 16px;"
                                "   min-width: 125px;"
                                "}"
                                "QPushButton:hover {"
                                "   background-color: #F59E0B;"
                                "   border-color: #FBBF24;"
                                "}"
                                "QPushButton:pressed {"
                                "   background-color: #B45309;"
                                "}");
                        }
                        else
                        {
                            btnSupport = msgBox.addButton(QString::fromUtf8("Поддержка"), QMessageBox::ActionRole);
                            btnSupport->setIcon(QIcon(":/svg/message-circle"));
                            btnSupport->setIconSize(QSize(16, 16));
                            btnSupport->setCursor(Qt::PointingHandCursor);
                            btnSupport->setStyleSheet(
                                "QPushButton {"
                                "   background-color: #0284C7;"
                                "   color: #FFFFFF;"
                                "   font-size: 12px;"
                                "   font-weight: bold;"
                                "   border: 1px solid #0284C7;"
                                "   border-radius: 0px;"
                                "   padding: 6px 16px;"
                                "   min-width: 110px;"
                                "}"
                                "QPushButton:hover {"
                                "   background-color: #0369A1;"
                                "   border-color: #38BDF8;"
                                "}"
                                "QPushButton:pressed {"
                                "   background-color: #075985;"
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

                    if(m_isServiceLaunching || button->isLaunching() || ServiceTileButton::isAnyLaunching())
                        return;

                    m_isServiceLaunching = true;
                    ServiceTileButton::setAnyLaunching(true);
                    button->setLaunching(true);
                    button->triggerClickAnimation();
                    showWindowShockwave(button->mapToGlobal(button->rect().center()), button->accentColor());

                    QTimer::singleShot(
                        1000,
                        this,
                        [this, button, instance]()
                        {
                            m_isServiceLaunching = false;
                            ServiceTileButton::setAnyLaunching(false);
                            if(button)
                                button->setLaunching(false);
                            this->runService(instance);
                        });
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

    for(auto &remaining : buildServices)
    {
        remaining->active = true;

        if(remaining->uuid() != IDServiceAIAgentString && !remaining->ownerWidget)
        {
            QString svcName;
            QString svcDesc;

            if(remaining->uuid() == IDServiceAppleIpswString)
            {
                svcName = QString::fromUtf8("Прошивки APPLE");
                svcDesc = QString::fromUtf8("Официальные прошивки IPSW, восстановление и чистая установка для iPhone/iPad");
            }
            else if(!remaining->title.isEmpty())
            {
                svcName = remaining->title;
            }
            else
            {
                svcName = remaining->widgetIconName();
            }

            remaining->title = svcName;

            QIcon svcIcon(":/svg/services/" + remaining->widgetIconName());
            if(svcIcon.isNull())
                svcIcon = QIcon(":/service-icons/" + remaining->widgetIconName());
            if(svcIcon.isNull())
                svcIcon = QIcon("res/svg/services/" + remaining->widgetIconName() + ".svg");
            if(svcIcon.isNull())
                svcIcon = QIcon("svg/services/" + remaining->widgetIconName() + ".svg");
            if(svcIcon.isNull())
                svcIcon = QIcon(":/svg/apple");

            QString ribbonText = (remaining->uuid() == IDServiceAppleIpswString) ? QString::fromUtf8("BETA") : QString::fromUtf8("NEW");
            ServiceTileButton *button = new ServiceTileButton(svcIcon, svcName, QString::fromUtf8("БЕСПЛАТНО"), ServiceTileButton::Tier::Free, true, ribbonText, ui->serviceContents);
            button->setServiceActive(true);
            button->setCursor(Qt::PointingHandCursor);

            ServiceTileButton::Details details;
            details.title = svcName;
            details.description = svcDesc;
            details.badgeText = QString::fromUtf8("БЕСПЛАТНО");
            details.tier = ServiceTileButton::Tier::Free;
            details.isActive = true;
            details.isUnderDev = false;
            details.needVip = false;
            details.price = 0;
            details.currency = network.authedId.currencyType;
            details.icon = svcIcon;

            if(remaining->deviceConnectType() == DeviceConnectType::ADB)
                details.connectTypeName = QString::fromUtf8("Android (ADB)");
            else if(remaining->deviceConnectType() == DeviceConnectType::Apple)
                details.connectTypeName = QString::fromUtf8("Apple iOS");
            else
                details.connectTypeName = QString::fromUtf8("Автономный");

            button->setServiceDetails(details);

            QString tip = svcName;
            if(!svcDesc.isEmpty())
                tip += "\n" + svcDesc;
            tip += QString::fromUtf8("\n\n• Бесплатная услуга (0 кредитов)");
            button->setToolTip(tip);

            button->setProperty("serviceUuid", remaining->uuid());
            button->setProperty("isAdsKiller", remaining->uuid() == IDServiceAdsString);
            button->setProperty("isAppleFirmware", remaining->uuid() == IDServiceAppleIpswString);
            button->setProperty("isFree", true);
            button->setProperty("isPaid", false);
            button->setProperty("isActive", true);
            button->setProperty("isUnderDev", false);
            button->setProperty("serviceName", svcName);
            button->setProperty("serviceDesc", svcDesc);

            std::shared_ptr<Service> serviceRef = remaining;
            QObject::connect(
                button,
                &QPushButton::clicked,
                this,
                [this, serviceRef, button]()
                {
                    if(m_isServiceLaunching || button->isLaunching() || ServiceTileButton::isAnyLaunching())
                        return;

                    m_isServiceLaunching = true;
                    ServiceTileButton::setAnyLaunching(true);
                    button->setLaunching(true);
                    button->triggerClickAnimation();
                    showWindowShockwave(button->mapToGlobal(button->rect().center()), button->accentColor());

                    QTimer::singleShot(
                        1000,
                        this,
                        [this, button, serviceRef]()
                        {
                            m_isServiceLaunching = false;
                            ServiceTileButton::setAnyLaunching(false);
                            if(button)
                                button->setLaunching(false);
                            this->runService(serviceRef);
                        });
                });

            remaining->ownerWidget = button;
        }

        services << std::move(remaining);
    }

    std::sort(
        std::begin(services),
        std::end(services),
        [](const std::shared_ptr<Service> &lhs, const std::shared_ptr<Service> &rhs)
        {
            bool lhsIsAds = (lhs && lhs->uuid() == IDServiceAdsString);
            bool rhsIsAds = (rhs && rhs->uuid() == IDServiceAdsString);
            if(lhsIsAds != rhsIsAds)
                return lhsIsAds;
            return static_cast<int>(lhs->active) > static_cast<int>(rhs->active);
        });

    QGridLayout *layoutSpace = qobject_cast<QGridLayout *>(ui->serviceContents->layout());
    if(layoutSpace)
    {
        layoutSpace->setSpacing(14);
        layoutSpace->setContentsMargins(10, 10, 10, 10);
        while(layoutSpace->count() > 0)
        {
            QLayoutItem *child = layoutSpace->takeAt(0);
            if(child->widget())
                child->widget()->deleteLater();
            delete child;
        }
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

    applyServiceFilters();
}

static QWidget *createServiceGroupDivider(const QString &title, const QString &iconPath, int count, const QString &accentHex, QWidget *parent)
{
    QWidget *divider = new QWidget(parent);
    divider->setObjectName(QStringLiteral("serviceGroupDivider"));
    divider->setFixedHeight(32);
    divider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    QHBoxLayout *layout = new QHBoxLayout(divider);
    layout->setContentsMargins(4, 10, 4, 4);
    layout->setSpacing(8);

    if(!iconPath.isEmpty())
    {
        QLabel *iconLbl = new QLabel(divider);
        iconLbl->setFixedSize(16, 16);
        iconLbl->setAlignment(Qt::AlignCenter);
        QPixmap pm(iconPath);
        if(!pm.isNull())
            iconLbl->setPixmap(pm.scaled(14, 14, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        layout->addWidget(iconLbl);
    }

    QLabel *titleLbl = new QLabel(title, divider);
    titleLbl->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: 800; letter-spacing: 1px; text-transform: uppercase; background: transparent;").arg(accentHex));
    layout->addWidget(titleLbl);

    QLabel *countBadge = new QLabel(QString("(%1)").arg(count), divider);
    countBadge->setStyleSheet(
        "color: #94A3B8; font-size: 10.5px; font-weight: bold; background: #1E293B; "
        "border: 1px solid #334155; padding: 1px 7px; border-radius: 2px;");
    layout->addWidget(countBadge);

    QFrame *line = new QFrame(divider);
    line->setFixedHeight(1);
    line->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    line->setStyleSheet(QString("background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 %1, stop:0.35 #334155, stop:1 transparent); border: none;").arg(accentHex));
    layout->addWidget(line, 1);

    return divider;
}

void MainWindow::applyServiceFilters()
{
    if(!ui || !ui->serviceContents)
        return;

    // Search query from top-right search box
    QLineEdit *searchEdit = ui->toplevel_up_2 ? ui->toplevel_up_2->findChild<QLineEdit *>("serviceSearchEdit") : nullptr;
    QString query = searchEdit ? searchEdit->text().trimmed() : QString();

    // Active filter from vertical filter panel
    QString currentFilter = "all";
    if(QButtonGroup *bg = this->findChild<QButtonGroup *>("serviceFilterGroup"))
    {
        QAbstractButton *checked = bg->checkedButton();
        if(checked)
            currentFilter = checked->property("filterMode").toString();
    }

    // Direct ServiceTileButton children of serviceContents
    QList<ServiceTileButton *> allButtons = ui->serviceContents->findChildren<ServiceTileButton *>(QString(), Qt::FindDirectChildrenOnly);

    auto getServicePriority = [](ServiceTileButton *btn) -> int
    {
        bool isUnderDev = btn->property("isUnderDev").toBool();
        bool isActive = btn->property("isActive").toBool();
        bool isUnavail = !isActive || isUnderDev || (btn->tier() == ServiceTileButton::Tier::Disabled);
        if(isUnavail)
            return 99; // Unavailable services always last

        bool isPaid = btn->property("isPaid").toBool() || (btn->tier() == ServiceTileButton::Tier::Credit || btn->tier() == ServiceTileButton::Tier::Vip || btn->tier() == ServiceTileButton::Tier::Dynamic);

        // Paid services ALWAYS have priority over free services!
        if(isPaid)
        {
            if(btn->tier() == ServiceTileButton::Tier::Vip)
                return 1;
            if(btn->tier() == ServiceTileButton::Tier::Credit)
                return 2;
            return 3;
        }

        // Free services
        if(btn->property("isAdsKiller").toBool() || btn->property("serviceUuid").toString() == IDServiceAdsString || btn->title().contains(QString::fromUtf8("реклам"), Qt::CaseInsensitive))
        {
            return 10;
        }
        if(btn->property("isAppleFirmware").toBool() || btn->property("serviceUuid").toString() == IDServiceAppleIpswString || btn->title().contains(QString::fromUtf8("APPLE"), Qt::CaseInsensitive))
        {
            return 11;
        }
        return 20;
    };

    // Sort order: Paid first (VIP, Credit), then Free (AdsKiller, Apple, rest), then Unavailable
    std::sort(
        allButtons.begin(),
        allButtons.end(),
        [getServicePriority](ServiceTileButton *a, ServiceTileButton *b)
        {
            int prioA = getServicePriority(a);
            int prioB = getServicePriority(b);
            if(prioA != prioB)
                return prioA < prioB;

            bool aDev = a->property("isUnderDev").toBool();
            bool bDev = b->property("isUnderDev").toBool();
            if(aDev != bDev)
                return !aDev;

            bool aAct = a->property("isActive").toBool();
            bool bAct = b->property("isActive").toBool();
            if(aAct != bAct)
                return aAct > bAct;

            return a->title() < b->title();
        });

    int visibleIndex = 0;
    int totalCount = allButtons.size();
    int availCount = 0;
    int unavailCount = 0;
    int freeCount = 0;
    int paidCount = 0;

    for(ServiceTileButton *btn : allButtons)
    {
        bool isUnderDev = btn->property("isUnderDev").toBool();
        bool isActive = btn->property("isActive").toBool();
        bool isFree = btn->property("isFree").toBool() || (btn->tier() == ServiceTileButton::Tier::Free);
        bool isPaid = btn->property("isPaid").toBool() || (btn->tier() == ServiceTileButton::Tier::Credit || btn->tier() == ServiceTileButton::Tier::Vip || btn->tier() == ServiceTileButton::Tier::Dynamic);
        bool isAvail = isActive && !isUnderDev;
        bool isUnavail = !isActive || isUnderDev || (btn->tier() == ServiceTileButton::Tier::Disabled);

        if(isAvail)
            ++availCount;
        if(isUnavail)
            ++unavailCount;
        if(isFree)
            ++freeCount;
        if(isPaid)
            ++paidCount;

        bool matchesFilter = true;
        if(currentFilter == "available")
            matchesFilter = isAvail;
        else if(currentFilter == "unavailable")
            matchesFilter = isUnavail;
        else if(currentFilter == "free")
            matchesFilter = isFree;
        else if(currentFilter == "paid")
            matchesFilter = isPaid;

        bool matchesSearch = true;
        if(!query.isEmpty())
        {
            matchesSearch = btn->title().contains(query, Qt::CaseInsensitive) || btn->toolTip().contains(query, Qt::CaseInsensitive) || btn->property("serviceDesc").toString().contains(query, Qt::CaseInsensitive) || btn->badgeText().contains(query, Qt::CaseInsensitive);
        }

        bool visible = matchesFilter && matchesSearch;
        btn->setVisible(visible);
        if(visible)
            ++visibleIndex;
    }

    // --- Responsive column calculation based on scroll area viewport ---
    int availW = 0;
    if(ui->scrollArea_3 && ui->scrollArea_3->viewport())
        availW = ui->scrollArea_3->viewport()->width();
    if(availW <= 100 && ui->page_cabinet)
        availW = ui->page_cabinet->width();
    if(availW <= 100)
        availW = this->width() - 80;

    // Side space is only layout margins (28px) + scrollbar (~24px); filters are now horizontal above the grid!
    const int sideSpace = 28 + 24;
    const int tileAreaW = qMax(280, availW - sideSpace);

    // Tiles have target width 280, spacing 12px, strictly 70px height
    const int tileTargetW = 280;
    const int tileSpacing = 12;
    int cols = qMax(1, (tileAreaW + tileSpacing) / (tileTargetW + tileSpacing));
    cols = qBound(1, cols, 6);

    QGridLayout *grid = qobject_cast<QGridLayout *>(ui->serviceContents->layout());
    if(!grid)
    {
        grid = new QGridLayout(ui->serviceContents);
    }
    grid->setSpacing(tileSpacing);
    grid->setContentsMargins(4, 4, 4, 4);

    // Clear existing layout items from the grid (deletes previous divider widgets, preserves buttons)
    while(grid->count() > 0)
    {
        QLayoutItem *item = grid->takeAt(0);
        if(item->widget())
        {
            if(item->widget()->objectName() == "serviceGroupDivider")
                item->widget()->deleteLater();
        }
        delete item;
    }

    // Partition visible buttons into logical groups
    QList<ServiceTileButton *> freeGroup;
    QList<ServiceTileButton *> paidGroup;
    QList<ServiceTileButton *> unavailGroup;

    for(ServiceTileButton *btn : allButtons)
    {
        if(!btn->isVisible())
            continue;

        bool isActive = btn->property("isActive").toBool();
        bool isUnderDev = btn->property("isUnderDev").toBool();
        bool isUnavail = !isActive || isUnderDev || (btn->tier() == ServiceTileButton::Tier::Disabled);
        bool isFree = btn->property("isFree").toBool() || (btn->tier() == ServiceTileButton::Tier::Free);

        if(isUnavail)
            unavailGroup.append(btn);
        else if(isFree)
            freeGroup.append(btn);
        else
            paidGroup.append(btn);
    }

    int row = 0;
    int visibleIndexFinal = 0;

    auto placeGroup = [&](const QList<ServiceTileButton *> &groupList, const QString &groupTitle, const QString &groupIcon, const QString &accentHex)
    {
        if(groupList.isEmpty())
            return;

        // Group section divider spanning all columns
        QWidget *divider = createServiceGroupDivider(groupTitle, groupIcon, groupList.size(), accentHex, ui->serviceContents);
        grid->addWidget(divider, row, 0, 1, cols);
        row++;

        int col = 0;
        for(ServiceTileButton *btn : groupList)
        {
            grid->addWidget(btn, row, col, Qt::AlignTop);
            ++visibleIndexFinal;
            col++;
            if(col >= cols)
            {
                col = 0;
                row++;
            }
        }
        if(col != 0)
            row++;
    };

    placeGroup(paidGroup, QString::fromUtf8("Платные и VIP сервисы"), ":/svg/crown", "#FBBF24");
    placeGroup(freeGroup, QString::fromUtf8("Бесплатные сервисы"), ":/svg/tag", "#34D399");
    placeGroup(unavailGroup, QString::fromUtf8("В разработке и ограниченные"), ":/svg/services/unavailable", "#94A3B8");

    // Configure column stretches so columns expand evenly
    for(int c = 0; c < cols; ++c)
        grid->setColumnStretch(c, 1);
    for(int c = cols; c < cols + 6; ++c)
        grid->setColumnStretch(c, 0);

    // Top-align all tile rows; bottom stretch row keeps tiles firmly at 70px height
    for(int r = 0; r <= row; ++r)
        grid->setRowStretch(r, 0);
    grid->setRowStretch(row + 1, 1);

    // Update counts on filter buttons
    if(QButtonGroup *bg = this->findChild<QButtonGroup *>("serviceFilterGroup"))
    {
        for(QAbstractButton *ab : bg->buttons())
        {
            QString mode = ab->property("filterMode").toString();
            if(mode == "all")
                ab->setText(QString::fromUtf8("Все сервисы (%1)").arg(totalCount));
            else if(mode == "available")
                ab->setText(QString::fromUtf8("Доступные (%1)").arg(availCount));
            else if(mode == "unavailable")
                ab->setText(QString::fromUtf8("Не доступные (%1)").arg(unavailCount));
            else if(mode == "free")
                ab->setText(QString::fromUtf8("Бесплатные (%1)").arg(freeCount));
            else if(mode == "paid")
                ab->setText(QString::fromUtf8("Платные (%1)").arg(paidCount));
        }
    }

    // Empty state message
    QLabel *emptyLbl = ui->serviceContents->findChild<QLabel *>("servicesEmptyLabel");
    if(visibleIndexFinal == 0)
    {
        if(!emptyLbl)
        {
            emptyLbl = new QLabel(ui->serviceContents);
            emptyLbl->setObjectName("servicesEmptyLabel");
            emptyLbl->setAlignment(Qt::AlignCenter);
            emptyLbl->setStyleSheet("color: #64748B; font-size: 13px; font-weight: 600; padding: 40px; background: transparent;");
        }
        emptyLbl->setText(query.isEmpty() ? QString::fromUtf8("В данной категории нет сервисов") : QString::fromUtf8("По запросу «%1» ничего не найдено").arg(query));
        grid->addWidget(emptyLbl, 0, 0, 1, cols, Qt::AlignCenter);
        emptyLbl->show();
    }
    else if(emptyLbl)
    {
        emptyLbl->hide();
    }
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

void MainWindow::updateDevicePageInstructions(DeviceConnectType type)
{
    if(!ui || !ui->label)
        return;

    if(type == DeviceConnectType::Apple)
    {
        if(s_adbVisualizer)
        {
            s_adbVisualizer->setIsApple(true);
        }

        ui->label->setText(
            "<html><head/><body>"
            "<div style=\"font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; color: #FFFFFF; padding: 2px;\">"
            "  <div style=\"margin-bottom: 12px;\">"
            "    <span style=\"font-size: 15px; font-weight: 700; color: #FFFFFF; letter-spacing: 0.3px;\">Подключение устройства Apple (iOS / iPadOS)</span>"
            "  </div>"
            "  <p style=\"color: #A1A1AA; font-size: 11.5px; margin: 0 0 12px 0; line-height: 1.45;\">"
            "    Для выполнения процедур прошивки и обслуживания подключите ваш iPhone или iPad к ПК:"
            "  </p>"
            "  <div style=\"background-color: #18181B; border: 1px solid #27272A; border-radius: 0px; padding: 10px 12px; margin-bottom: 8px;\">"
            "    <span style=\"background-color: #0284C7; color: #FFFFFF; border-radius: 0px; padding: 2px 7px; font-weight: bold; font-size: 11px;\">1</span>"
            "    <strong style=\"color: #FFFFFF; font-size: 12.5px; margin-left: 6px;\">Кабель Lightning или Type-C</strong>"
            "    <p style=\"margin: 5px 0 0 24px; color: #A1A1AA; font-size: 11.5px; line-height: 1.45;\">"
            "      Подключите iPhone / iPad к компьютеру оригинальным или сертифицированным кабелем. Рекомендуется подключать напрямую в разъём системного блока без переходников и хабов."
            "    </p>"
            "  </div>"
            "  <div style=\"background-color: #18181B; border: 1px solid #27272A; border-radius: 0px; padding: 10px 12px; margin-bottom: 8px;\">"
            "    <span style=\"background-color: #0284C7; color: #FFFFFF; border-radius: 0px; padding: 2px 7px; font-weight: bold; font-size: 11px;\">2</span>"
            "    <strong style=\"color: #FFFFFF; font-size: 12.5px; margin-left: 6px;\">Доверие этому компьютеру</strong>"
            "    <p style=\"margin: 5px 0 0 24px; color: #A1A1AA; font-size: 11.5px; line-height: 1.45;\">"
            "      Разблокируйте экран устройства. В появившемся диалоговом окне нажмите <b>«Доверять»</b> и введите код-пароль разблокировки на экране iPhone/iPad."
            "    </p>"
            "  </div>"
            "  <div style=\"background-color: #18181B; border: 1px solid #27272A; border-radius: 0px; padding: 10px 12px; margin-bottom: 8px;\">"
            "    <span style=\"background-color: #0284C7; color: #FFFFFF; border-radius: 0px; padding: 2px 7px; font-weight: bold; font-size: 11px;\">3</span>"
            "    <strong style=\"color: #FFFFFF; font-size: 12.5px; margin-left: 6px;\">Режимы Recovery и DFU (при необходимости)</strong>"
            "    <p style=\"margin: 5px 0 0 24px; color: #A1A1AA; font-size: 11.5px; line-height: 1.45;\">"
            "      Если устройство заблокировано или зависло на яблоке, переведите его в режим <b>Recovery Mode</b> или <b>DFU</b>. Приложение автоматически определит его и откроет прошивку."
            "    </p>"
            "  </div>"
            "  <div style=\"background-color: #18181B; border: 1px dashed #0284C7; border-radius: 0px; padding: 9px 12px; margin-top: 6px;\">"
            "    <span style=\"color: #00E5FF; font-size: 11.5px; font-weight: 600;\"><img src=\":/svg/lightbulb\" width=\"13\" height=\"13\" style=\"vertical-align:middle;\"/> Устройство не определяется?</span>"
            "    <p style=\"margin: 4px 0 0 0; color: #71717A; font-size: 11px; line-height: 1.4;\">"
            "      Проверьте исправность кабеля и разъёма USB или удерживайте комбинацию кнопок для входа в Recovery Mode."
            "    </p>"
            "  </div>"
            "</div>"
            "</body></html>");

        if(ui->label_3)
        {
            ui->label_3->setText(
                "<a style=\"color: #00E5FF; text-decoration: none; font-size: 12px; font-weight: 500;\" href=\"https://support.apple.com/ru-ru/109043\"><img src=\":/svg/clipboard\" width=\"13\" height=\"13\" style=\"vertical-align:middle;\"/> Справка Apple: Как ввести iPhone/iPad в режим "
                "восстановления (Recovery) &rarr;</a>");
        }
        if(ui->label_5)
        {
            ui->label_5->setStyleSheet(
                "background-color: #18181B;"
                "border: 1px solid #0284C7;"
                "border-radius: 0px;"
                "color: #00E5FF;"
                "font-size: 12.5px;"
                "font-weight: 600;"
                "padding: 10px;");
            ui->label_5->setText(QString::fromUtf8("Поиск подключенного устройства Apple (Lightning / Type-C)..."));
        }
    }
    else
    {
        if(s_adbVisualizer)
        {
            s_adbVisualizer->setIsApple(false);
        }

        ui->label->setText(
            "<html><head/><body>"
            "<div style=\"font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; color: #FFFFFF; padding: 2px;\">"
            "  <div style=\"margin-bottom: 12px;\">"
            "    <span style=\"font-size: 15px; font-weight: 700; color: #FFFFFF; letter-spacing: 0.3px;\">Подключение устройства Android (ADB)</span>"
            "  </div>"
            "  <p style=\"color: #A1A1AA; font-size: 11.5px; margin: 0 0 12px 0; line-height: 1.45;\">"
            "    Для выполнения процедур активируйте <b>Отладку по USB</b> на вашем Android-смартфоне:"
            "  </p>"
            "  <div style=\"background-color: #18181B; border: 1px solid #27272A; border-radius: 0px; padding: 10px 12px; margin-bottom: 8px;\">"
            "    <span style=\"background-color: #0284C7; color: #FFFFFF; border-radius: 0px; padding: 2px 7px; font-weight: bold; font-size: 11px;\">1</span>"
            "    <strong style=\"color: #FFFFFF; font-size: 12.5px; margin-left: 6px;\">Режим разработчика</strong>"
            "    <p style=\"margin: 5px 0 0 24px; color: #A1A1AA; font-size: 11.5px; line-height: 1.45;\">"
            "      Откройте <b>Настройки</b> &rarr; <b>О телефоне</b>. Найдите <b>Номер сборки</b> (или версию MIUI/HyperOS) и нажмите на него <b>7 раз</b> подряд."
            "    </p>"
            "  </div>"
            "  <div style=\"background-color: #18181B; border: 1px solid #27272A; border-radius: 0px; padding: 10px 12px; margin-bottom: 8px;\">"
            "    <span style=\"background-color: #0284C7; color: #FFFFFF; border-radius: 0px; padding: 2px 7px; font-weight: bold; font-size: 11px;\">2</span>"
            "    <strong style=\"color: #FFFFFF; font-size: 12.5px; margin-left: 6px;\">Включите отладку по USB</strong>"
            "    <p style=\"margin: 5px 0 0 24px; color: #A1A1AA; font-size: 11.5px; line-height: 1.45;\">"
            "      Перейдите в <b>Настройки</b> &rarr; <b>Для разработчиков</b> и активируйте тумблер <b>Отладка по USB</b> (для Xiaomi также «Установка через USB»)."
            "    </p>"
            "  </div>"
            "  <div style=\"background-color: #18181B; border: 1px solid #27272A; border-radius: 0px; padding: 10px 12px; margin-bottom: 8px;\">"
            "    <span style=\"background-color: #0284C7; color: #FFFFFF; border-radius: 0px; padding: 2px 7px; font-weight: bold; font-size: 11px;\">3</span>"
            "    <strong style=\"color: #FFFFFF; font-size: 12.5px; margin-left: 6px;\">Подключите кабель к ПК</strong>"
            "    <p style=\"margin: 5px 0 0 24px; color: #A1A1AA; font-size: 11.5px; line-height: 1.45;\">"
            "      Соедините устройство кабелем. На экране телефона появится запрос &mdash; отметьте <b>«Всегда разрешать с этого компьютера»</b> и нажмите <b>ОК</b>."
            "    </p>"
            "  </div>"
            "  <div style=\"background-color: #18181B; border: 1px dashed #0284C7; border-radius: 0px; padding: 9px 12px; margin-top: 6px;\">"
            "    <span style=\"color: #00E5FF; font-size: 11.5px; font-weight: 600;\"><img src=\":/svg/lightbulb\" width=\"13\" height=\"13\" style=\"vertical-align:middle;\"/> Телефон не определяется?</span>"
            "    <p style=\"margin: 4px 0 0 0; color: #71717A; font-size: 11px; line-height: 1.4;\">"
            "      Смените режим подключения USB на <b>«Передача файлов (MTP)»</b> либо подключите кабель в другой USB-порт на ПК."
            "    </p>"
            "  </div>"
            "</div>"
            "</body></html>");

        if(ui->label_3)
        {
            ui->label_3->setText(
                "<a style=\"color: #00E5FF; text-decoration: none; font-size: 12px; font-weight: 500;\" href=\"https://www.anymp4.com/ru/faq/enable-usb-debugging-for-android.html\"><img src=\":/svg/clipboard\" width=\"13\" height=\"13\" style=\"vertical-align:middle;\"/> Подробная пошаговая "
                "инструкция с иллюстрациями &rarr;</a>");
        }
        if(ui->label_5)
        {
            ui->label_5->setStyleSheet(
                "background-color: #18181B;"
                "border: 1px solid #0284C7;"
                "border-radius: 0px;"
                "color: #00E5FF;"
                "font-size: 12.5px;"
                "font-weight: 600;"
                "padding: 10px;");
            ui->label_5->setText(QString::fromUtf8("Поиск подключенного Android-устройства..."));
        }
    }
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
                if(connectPhone.connectionType == DeviceConnectType::Apple)
                    ui->label_8->setText(QString::fromUtf8("Подключение Apple-устройства (Lightning / Type-C)"));
                else
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
            case AppleIpswPage:
                ui->label_8->setText("Прошивка и восстановление Apple iOS");
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

            // Set custom instructions and text background according to connection type
            updateDevicePageInstructions(connectPhone.connectionType);

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
                        if(connectPhone.connectionType == DeviceConnectType::Apple)
                        {
                            QList<AppleDevice> devices = Apple::getDevices();
                            if(devices.isEmpty())
                            {
                                if(vis && vis->status() != UNKNOWN)
                                {
                                    vis->setStatus(UNKNOWN);
                                    ui->label_5->setStyleSheet(
                                        "background-color: #18181B;"
                                        "border: 1px solid #0284C7;"
                                        "border-radius: 0px;"
                                        "color: #00E5FF;"
                                        "font-size: 12.5px;"
                                        "font-weight: 600;"
                                        "padding: 10px;");
                                    ui->label_5->setText(QString::fromUtf8("Поиск подключенного устройства Apple (Lightning / Type-C)..."));
                                }
                            }
                            else
                            {
                                bool hasAuth = false;
                                bool hasUnauth = false;
                                AppleDevice authDev;
                                AppleDevice unauthDev;

                                for(const AppleDevice &device : std::as_const(devices))
                                {
                                    AppleConStatus status = Apple::deviceStatus(device);
                                    if(status == APPLE_DEVICE || status == APPLE_RECOVERY || status == APPLE_DFU)
                                    {
                                        hasAuth = true;
                                        authDev = device;
                                        break;
                                    }
                                    else if(status == APPLE_UNAUTH)
                                    {
                                        hasUnauth = true;
                                        unauthDev = device;
                                    }
                                }

                                if(hasAuth)
                                {
                                    connectPhone.isAuthed = true;
                                    connectPhone.appleDevice = authDev;
                                    ServiceProvider::currentService()->setAppleArgs(authDev);

                                    QString devName = !authDev.displayName.isEmpty() ? authDev.displayName : (!authDev.marketingName.isEmpty() ? authDev.marketingName : authDev.devId);
                                    QString modeStr = authDev.mode == AppleDeviceMode::Recovery ? QString("Recovery Mode") : (authDev.mode == AppleDeviceMode::DFU ? QString("DFU Mode") : (!authDev.productVersion.isEmpty() ? QString("iOS %1").arg(authDev.productVersion) : QString("Normal Mode")));
                                    QString devSub = QString("Apple • %1").arg(modeStr);

                                    if(vis && vis->status() != DEVICE)
                                    {
                                        vis->setStatus(DEVICE, devName, devSub);
                                    }
                                    ui->label_5->setStyleSheet(
                                        "background-color: #0D261A;"
                                        "border: 1px solid #059669;"
                                        "border-radius: 0px;"
                                        "color: #34D399;"
                                        "font-size: 12.5px;"
                                        "font-weight: 600;"
                                        "padding: 10px;");
                                    ui->label_5->setText(QString::fromUtf8("Устройство Apple подключено: %1! Запуск...").arg(devName));
                                }
                                else if(hasUnauth)
                                {
                                    QString devName = !unauthDev.displayName.isEmpty() ? unauthDev.displayName : (!unauthDev.marketingName.isEmpty() ? unauthDev.marketingName : unauthDev.devId);

                                    if(vis && vis->status() != UNAUTH)
                                    {
                                        vis->setStatus(UNAUTH, devName);
                                    }
                                    ui->label_5->setStyleSheet(
                                        "background-color: #241A08;"
                                        "border: 1px solid #D97706;"
                                        "border-radius: 0px;"
                                        "color: #FBBF24;"
                                        "font-size: 12.5px;"
                                        "font-weight: 600;"
                                        "padding: 10px;");
                                    ui->label_5->setText(QString::fromUtf8("Нажмите «Доверять этому компьютеру» на экране iPhone/iPad"));
                                }
                            }
                        }
                        else
                        {
                            QList<AdbDevice> devices = Adb::getDevices();
                            if(devices.isEmpty())
                            {
                                if(vis && vis->status() != UNKNOWN)
                                {
                                    vis->setStatus(UNKNOWN);
                                    ui->label_5->setStyleSheet(
                                        "background-color: #18181B;"
                                        "border: 1px solid #0284C7;"
                                        "border-radius: 0px;"
                                        "color: #00E5FF;"
                                        "font-size: 12.5px;"
                                        "font-weight: 600;"
                                        "padding: 10px;");
                                    ui->label_5->setText(QString::fromUtf8("Поиск подключенного Android-устройства..."));
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
                                    ServiceProvider::currentService()->setAndroidArgs(authDev);

                                    QString devName = !authDev.marketingName.isEmpty() ? authDev.marketingName : (!authDev.displayName.isEmpty() ? authDev.displayName : (!authDev.model.isEmpty() ? authDev.model : authDev.devId));
                                    QString devSub = !authDev.vendor.isEmpty() ? (authDev.vendor + " (" + authDev.model + ")") : authDev.devId;

                                    if(vis && vis->status() != DEVICE)
                                    {
                                        vis->setStatus(DEVICE, devName, devSub);
                                    }
                                    ui->label_5->setStyleSheet(
                                        "background-color: #0D261A;"
                                        "border: 1px solid #059669;"
                                        "border-radius: 0px;"
                                        "color: #34D399;"
                                        "font-size: 12.5px;"
                                        "font-weight: 600;"
                                        "padding: 10px;");
                                    ui->label_5->setText(QString::fromUtf8("Устройство подключено: %1! Запуск...").arg(devName));
                                }
                                else if(hasUnauth)
                                {
                                    QString devName = !unauthDev.marketingName.isEmpty() ? unauthDev.marketingName : (!unauthDev.displayName.isEmpty() ? unauthDev.displayName : (!unauthDev.model.isEmpty() ? unauthDev.model : unauthDev.devId));

                                    if(vis && vis->status() != UNAUTH)
                                    {
                                        vis->setStatus(UNAUTH, devName);
                                    }
                                    ui->label_5->setStyleSheet(
                                        "background-color: #241A08;"
                                        "border: 1px solid #D97706;"
                                        "border-radius: 0px;"
                                        "color: #FBBF24;"
                                        "font-size: 12.5px;"
                                        "font-weight: 600;"
                                        "padding: 10px;");
                                    ui->label_5->setText(QString::fromUtf8("Нажмите «Разрешить отладку по USB» на экране телефона"));
                                }
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
            if(ServiceProvider::currentService() && !ServiceProvider::currentService()->isStarted())
                ServiceProvider::currentService()->start();
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
            if(ServiceProvider::currentService() && !ServiceProvider::currentService()->isStarted())
                ServiceProvider::currentService()->start();
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
            if(ServiceProvider::currentService() && !ServiceProvider::currentService()->isStarted())
                ServiceProvider::currentService()->start();
            break;
        }
        case AppleIpswPage:
        {
            if(pages.contains(AppleIpswPage))
            {
                auto *widget = static_cast<AppleIpswWidget *>(pages.value(AppleIpswPage));
                if(widget)
                {
                    if(!connectPhone.appleDevice.isEmpty())
                        widget->setDevice(connectPhone.appleDevice);
                    widget->refreshDevice();
                    QTimer::singleShot(50, widget, [widget]() { widget->showBetaDisclaimer(); });
                }
            }
            if(ServiceProvider::currentService() && !ServiceProvider::currentService()->isStarted())
                ServiceProvider::currentService()->start();
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

    m_isServiceLaunching = false;
    ServiceTileButton::setAnyLaunching(false);
    if(m_shockwaveOverlay)
        m_shockwaveOverlay->hide();

    ui->aiChatEdit->clear();
    ui->aiChatSend->setText(QString());

    ui->labelLoginAuthed->setText("-");
    ui->labelCredits->setText(QString::fromUtf8("0\nБаланс"));
    ui->labelVipDays->setText(QString::fromUtf8("0 ДНЕЙ\nVIP статус"));

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
    ui->aiChatSend->setText(QString());

    ui->aiChatEdit->setDisabled(true);
    ui->aiChatSend->setDisabled(true);

    auto *chatView = findChild<AIChatView *>("aiChatView");
    if(chatView)
        chatView->showLocked();

    updateCabinetUserCard();
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
    ui->labelCredits->setText(QString("%1 %2\nБаланс").arg(network.authedId.credits).arg(network.authedId.currencyType));
    if(network.authedId.hasVipAccount())
        ui->labelVipDays->setText(QString("%1 ДНЕЙ\nVIP активен").arg(network.authedId.vipDays));
    else
        ui->labelVipDays->setText(QString("Нет VIP\nVIP статус"));

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
            sLbl->setText(QString("%1 дн.").arg(network.authedId.vipDays));
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
            sLbl->setText(QString::fromUtf8("Заблокирован"));
            sLbl->setStyleSheet("color: #F87171; font-size: 12px; font-weight: bold; background: transparent; border: none;");
        }
        else
        {
            sLbl->setText(QString::fromUtf8("Активен"));
            sLbl->setStyleSheet("color: #34D399; font-size: 12px; font-weight: bold; background: transparent; border: none;");
        }
    }

    updateCabinetUserCard();
    initServiceModules();
}

void MainWindow::updateCabinetUserCard()
{
    if(!ui->frame_7)
        return;

    bool isAuthed = !network.authedId.idName.isEmpty();
    bool isVip = isAuthed && network.authedId.hasVipAccount();
    int32_t credits = isAuthed ? static_cast<int32_t>(network.authedId.credits) : 0;
    bool isZeroCredits = (credits <= 0);

    QPushButton *btnAddCredits = ui->frame_7->findChild<QPushButton *>("buttonAddCredits");
    QPushButton *btnAddVip = ui->frame_7->findChild<QPushButton *>("buttonAddVip");
    QLabel *roleLbl = ui->authedMainWin ? ui->authedMainWin->findChild<QLabel *>("cabinetRoleBadge") : nullptr;
    QLabel *onlineBadge = ui->authedMainWin ? ui->authedMainWin->findChild<QLabel *>("cabinetOnlineBadge") : nullptr;
    QFrame *divider = ui->frame_7->findChild<QFrame *>("cabinetHorizontalDivider");
    QLabel *creditsLbl = findChild<QLabel *>("cabinetVal_credits");
    QLabel *vipLbl = findChild<QLabel *>("cabinetVal_vip");

    static const QString VIP_BUTTON_YELLOW_STYLE = QStringLiteral(
        "QPushButton {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #FDE047, stop:0.5 #F59E0B, stop:1 #D97706);"
        "    color: #000000;"
        "    font-size: 11px;"
        "    font-weight: 800;"
        "    border: 1px solid #FBBF24;"
        "    border-radius: 0px;"
        "    padding: 2px 10px;"
        "}"
        "QPushButton:hover {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #FEF08A, stop:0.5 #FBBF24, stop:1 #F59E0B);"
        "    border-color: #FDE047;"
        "    color: #000000;"
        "}"
        "QPushButton:pressed {"
        "    background: #D97706;"
        "    border-color: #B45309;"
        "    color: #000000;"
        "}");

    if(!isAuthed)
    {
        // Reset to default Obsidian dark state
        ui->frame_7->setStyleSheet(QStringLiteral(
            "QFrame#frame_7 {"
            "    background-color: #0B0F19;"
            "    border: 1px solid #1E293B;"
            "    border-radius: 0px;"
            "    padding: 6px 12px;"
            "}"
            "QFrame#cabinetSideRow {"
            "    background-color: #070B14;"
            "    border: 1px solid #1E293B;"
            "    border-radius: 0px;"
            "    padding: 2px 6px;"
            "}"
            "QFrame#cabinetSideRow:hover {"
            "    border-color: #334155;"
            "    background-color: #0E1526;"
            "}"));
        if(ui->labelLoginAuthed)
            ui->labelLoginAuthed->setStyleSheet("color: #F8FAFC; font-size: 14px; font-weight: bold; background: transparent; border: none;");
        if(roleLbl)
        {
            roleLbl->setText(QString::fromUtf8("Основной аккаунт"));
            roleLbl->setStyleSheet("color: #64748B; font-size: 9.5px; font-weight: 600; text-transform: uppercase; letter-spacing: 0.5px; background: transparent; border: none;");
        }
        if(onlineBadge)
            onlineBadge->setStyleSheet("color: #34D399; font-size: 9.5px; font-weight: bold; padding: 2px 7px; background-color: rgba(52, 211, 153, 0.12); border: 1px solid rgba(52, 211, 153, 0.30); border-radius: 0px;");
        if(btnAddCredits)
            btnAddCredits->setStyleSheet(QString());
        if(btnAddVip)
            btnAddVip->setStyleSheet(VIP_BUTTON_YELLOW_STYLE);
        if(divider)
            divider->setStyleSheet("background-color: #1E293B; max-height: 1px; border: none;");
        if(creditsLbl)
            creditsLbl->setStyleSheet("color: #34D399; font-size: 11px; font-weight: bold; background: transparent; border: none;");
        if(ui->labelCredits)
            ui->labelCredits->setStyleSheet("color: #34D399; font-weight: bold;");
        return;
    }

    if(isVip)
    {
        // 1. VIP -> Full Gradient Yellow
        ui->frame_7->setStyleSheet(QStringLiteral(
            "QFrame#frame_7 {"
            "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0.9, stop:0 #B45309, stop:0.25 #D97706, stop:0.55 #F59E0B, stop:0.8 #FBBF24, stop:1 #D97706);"
            "    border: 1.5px solid #FDE047;"
            "    border-radius: 0px;"
            "    padding: 6px 12px;"
            "}"
            "QFrame#cabinetSideRow {"
            "    background-color: rgba(15, 23, 42, 0.82);"
            "    border: 1px solid rgba(251, 191, 36, 0.55);"
            "    border-radius: 0px;"
            "    padding: 2px 6px;"
            "}"
            "QFrame#cabinetSideRow:hover {"
            "    border-color: #FDE047;"
            "    background-color: rgba(15, 23, 42, 0.95);"
            "}"));

        if(ui->labelLoginAuthed)
            ui->labelLoginAuthed->setStyleSheet("color: #0F172A; font-size: 14px; font-weight: 800; background: transparent; border: none;");

        if(roleLbl)
        {
            roleLbl->setText(QString::fromUtf8("★ VIP АККАУНТ"));
            roleLbl->setStyleSheet("color: #78350F; font-size: 9.5px; font-weight: 800; text-transform: uppercase; letter-spacing: 0.5px; background: transparent; border: none;");
        }

        if(onlineBadge)
            onlineBadge->setStyleSheet("color: #064E3B; font-size: 9.5px; font-weight: bold; padding: 2px 7px; background-color: rgba(6, 95, 70, 0.20); border: 1px solid rgba(6, 95, 70, 0.40); border-radius: 0px;");

        if(btnAddCredits)
            btnAddCredits->setStyleSheet("QPushButton { background-color: #0F172A; color: #F8FAFC; border: 1px solid #334155; font-size: 11px; font-weight: 600; padding: 2px 10px; border-radius: 0px; } QPushButton:hover { background-color: #1E293B; border-color: #FBBF24; color: #FBBF24; }");

        if(btnAddVip)
            btnAddVip->setStyleSheet(VIP_BUTTON_YELLOW_STYLE);

        if(divider)
            divider->setStyleSheet("background-color: rgba(120, 53, 15, 0.45); max-height: 1px; border: none;");

        if(vipLbl)
            vipLbl->setStyleSheet("color: #FBBF24; font-size: 11px; font-weight: bold; background: transparent; border: none;");

        // Sub-item balance: red if 0, green if >0
        if(creditsLbl)
        {
            if(isZeroCredits)
                creditsLbl->setStyleSheet("color: #EF4444; font-size: 11px; font-weight: bold; background: transparent; border: none;");
            else
                creditsLbl->setStyleSheet("color: #34D399; font-size: 11px; font-weight: bold; background: transparent; border: none;");
        }
        if(ui->labelCredits)
            ui->labelCredits->setStyleSheet(isZeroCredits ? "color: #EF4444; font-weight: bold;" : "color: #34D399; font-weight: bold;");
    }
    else if(isZeroCredits)
    {
        // 2. 0 Credits, No VIP -> Full Gradient Red (slightly transparent)
        ui->frame_7->setStyleSheet(QStringLiteral(
            "QFrame#frame_7 {"
            "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0.9, stop:0 rgba(127, 29, 29, 0.75), stop:0.35 rgba(185, 28, 28, 0.65), stop:0.7 rgba(220, 38, 38, 0.58), stop:1 rgba(153, 27, 27, 0.75));"
            "    border: 1.5px solid rgba(239, 68, 68, 0.85);"
            "    border-radius: 0px;"
            "    padding: 6px 12px;"
            "}"
            "QFrame#cabinetSideRow {"
            "    background-color: rgba(15, 23, 42, 0.72);"
            "    border: 1px solid rgba(239, 68, 68, 0.45);"
            "    border-radius: 0px;"
            "    padding: 2px 6px;"
            "}"
            "QFrame#cabinetSideRow:hover {"
            "    border-color: #EF4444;"
            "    background-color: rgba(15, 23, 42, 0.90);"
            "}"));

        if(ui->labelLoginAuthed)
            ui->labelLoginAuthed->setStyleSheet("color: #FFFFFF; font-size: 14px; font-weight: bold; background: transparent; border: none;");

        if(roleLbl)
        {
            roleLbl->setText(QString::fromUtf8("Основной аккаунт • 0 Кредитов"));
            roleLbl->setStyleSheet("color: #FECACA; font-size: 9.5px; font-weight: 600; text-transform: uppercase; letter-spacing: 0.5px; background: transparent; border: none;");
        }

        if(onlineBadge)
            onlineBadge->setStyleSheet("color: #FCA5A5; font-size: 9.5px; font-weight: bold; padding: 2px 7px; background-color: rgba(239, 68, 68, 0.22); border: 1px solid rgba(239, 68, 68, 0.45); border-radius: 0px;");

        if(btnAddCredits)
            btnAddCredits->setStyleSheet("QPushButton { background-color: #DC2626; color: #FFFFFF; border: 1px solid #EF4444; font-size: 11px; font-weight: bold; padding: 2px 10px; border-radius: 0px; } QPushButton:hover { background-color: #B91C1C; border-color: #FCA5A5; }");

        if(btnAddVip)
            btnAddVip->setStyleSheet(VIP_BUTTON_YELLOW_STYLE);

        if(divider)
            divider->setStyleSheet("background-color: rgba(239, 68, 68, 0.35); max-height: 1px; border: none;");

        if(vipLbl)
            vipLbl->setStyleSheet("color: #94A3B8; font-size: 11px; font-weight: 500; background: transparent; border: none;");

        // Sub-item balance: red
        if(creditsLbl)
            creditsLbl->setStyleSheet("color: #EF4444; font-size: 11px; font-weight: bold; background: transparent; border: none;");

        if(ui->labelCredits)
            ui->labelCredits->setStyleSheet("color: #EF4444; font-weight: bold;");
    }
    else
    {
        // 3. > 0 Credits, No VIP -> Default state as it is
        ui->frame_7->setStyleSheet(QStringLiteral(
            "QFrame#frame_7 {"
            "    background-color: #0B0F19;"
            "    border: 1px solid #1E293B;"
            "    border-radius: 0px;"
            "    padding: 6px 12px;"
            "}"
            "QFrame#cabinetSideRow {"
            "    background-color: #070B14;"
            "    border: 1px solid #1E293B;"
            "    border-radius: 0px;"
            "    padding: 2px 6px;"
            "}"
            "QFrame#cabinetSideRow:hover {"
            "    border-color: #334155;"
            "    background-color: #0E1526;"
            "}"));

        if(ui->labelLoginAuthed)
            ui->labelLoginAuthed->setStyleSheet("color: #F8FAFC; font-size: 14px; font-weight: bold; background: transparent; border: none;");

        if(roleLbl)
        {
            roleLbl->setText(QString::fromUtf8("Основной аккаунт"));
            roleLbl->setStyleSheet("color: #64748B; font-size: 9.5px; font-weight: 600; text-transform: uppercase; letter-spacing: 0.5px; background: transparent; border: none;");
        }

        if(onlineBadge)
            onlineBadge->setStyleSheet("color: #34D399; font-size: 9.5px; font-weight: bold; padding: 2px 7px; background-color: rgba(52, 211, 153, 0.12); border: 1px solid rgba(52, 211, 153, 0.30); border-radius: 0px;");

        if(btnAddCredits)
            btnAddCredits->setStyleSheet(QString());

        if(btnAddVip)
            btnAddVip->setStyleSheet(VIP_BUTTON_YELLOW_STYLE);

        if(divider)
            divider->setStyleSheet("background-color: #1E293B; max-height: 1px; border: none;");

        if(vipLbl)
            vipLbl->setStyleSheet("color: #94A3B8; font-size: 11px; font-weight: 500; background: transparent; border: none;");

        // Sub-item balance: green
        if(creditsLbl)
            creditsLbl->setStyleSheet("color: #34D399; font-size: 11px; font-weight: bold; background: transparent; border: none;");

        if(ui->labelCredits)
            ui->labelCredits->setStyleSheet("color: #34D399; font-weight: bold;");
    }
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

AppleDevice MainWindow::currentAppleDevice() const
{
    return connectPhone.appleDevice;
}

QWidget *MainWindow::pageWidget(PageIndex page) const
{
    if(pages.empty())
        return nullptr;
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
                    resText = "Авторизация прошла успешно. Добро пожаловать!";

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

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if(m_shockwaveOverlay)
        m_shockwaveOverlay->setGeometry(this->rect());
    // Recompute service tile column count when window is resized
    if(ui && ui->serviceContents && !services.isEmpty())
        applyServiceFilters();
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
    if(!app)
        app = qApp;
    if(app)
        app->setStyleSheet(styleSheet);
    if(qApp)
    {
        qApp->setStyleSheet(styleSheet);
        if(theme == Dark)
        {
            qApp->setStyle(QStyleFactory::create("Fusion"));
            QPalette darkPalette;
            darkPalette.setColor(QPalette::Window, QColor(10, 14, 26));             // #0A0E1A
            darkPalette.setColor(QPalette::WindowText, QColor(248, 250, 252));      // #F8FAFC
            darkPalette.setColor(QPalette::Base, QColor(7, 10, 18));                // #070A12
            darkPalette.setColor(QPalette::AlternateBase, QColor(15, 23, 42));      // #0F172A
            darkPalette.setColor(QPalette::ToolTipBase, QColor(15, 23, 42));        // #0F172A
            darkPalette.setColor(QPalette::ToolTipText, QColor(248, 250, 252));     // #F8FAFC
            darkPalette.setColor(QPalette::Text, QColor(248, 250, 252));            // #F8FAFC
            darkPalette.setColor(QPalette::Button, QColor(15, 23, 42));             // #0F172A
            darkPalette.setColor(QPalette::ButtonText, QColor(248, 250, 252));      // #F8FAFC
            darkPalette.setColor(QPalette::BrightText, QColor(56, 189, 248));       // #38BDF8
            darkPalette.setColor(QPalette::Link, QColor(56, 189, 248));             // #38BDF8
            darkPalette.setColor(QPalette::Highlight, QColor(2, 132, 199));         // #0284C7
            darkPalette.setColor(QPalette::HighlightedText, QColor(255, 255, 255)); // #FFFFFF
            darkPalette.setColor(QPalette::PlaceholderText, QColor(100, 116, 139)); // #64748B

            darkPalette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(71, 85, 105)); // #475569
            darkPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(71, 85, 105));
            darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(71, 85, 105));
            darkPalette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(30, 41, 59)); // #1E293B
            darkPalette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(71, 85, 105));
            qApp->setPalette(darkPalette);
        }
    }
    this->setStyleSheet(styleSheet);
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

void MainWindow::askAiQuestion(const QString &question)
{
    if(ui->aiToolBoxToggle && ui->aiToolBoxToggle->isVisible())
    {
        ui->aiToolBoxToggle->click();
    }

    QPushButton *tabChat = findChild<QPushButton *>("tabChatBtn");
    if(tabChat)
    {
        tabChat->click();
    }

    if(ui->aiChatEdit)
    {
        ui->aiChatEdit->setText(question);
    }
    if(ui->aiChatSend)
    {
        ui->aiChatSend->click();
    }
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
            else if(status && network.authedId.vipDays < 5 && network.authedId.vipDays > 0 && !isShownedVipExp)
            {
                delayUICall(300, [this]() { QMessageBox::warning(this, "Уведомление", infoVipExpire); });
                isShownedVipExp = true;
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
    if(cyberReactorLoader)
        cyberReactorLoader->setStatusText(text);

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
        ui->butShowPass->setText(QString());
        ui->butShowPass->setIcon(QIcon(":/svg/eye-off"));
        ui->butShowPass->setIconSize(QSize(16, 16));
        ui->butShowPass->setToolTip(tr("Скрыть пароль"));
    }
    else
    {
        ui->linePassEdit->setEchoMode(QLineEdit::EchoMode::Password);
        ui->butShowPass->setText(QString());
        ui->butShowPass->setIcon(QIcon(":/svg/eye"));
        ui->butShowPass->setIconSize(QSize(16, 16));
        ui->butShowPass->setToolTip(tr("Показать пароль"));
    }
}
