#include <functional>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <list>
#include <memory>

#include <QButtonGroup>
#include <QCloseEvent>
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
#include "Extension.h"
#include "MainWindow.h"
#include "Network.h"
#include "AIChatView.h"
#include "AboutDialog.h"
#include "ui_MainWindow.h"
#include "FileManagerWidget.h"
#include "ApkManagerWidget.h"
#include "ContactFixerWidget.h"
#include "AdbDeviceVisualizer.h"

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
// ServiceTileButton Implementation (Metro UI Tile Card with "NEW" Red Ribbon)
// ============================================================================

ServiceTileButton::ServiceTileButton(const QIcon &icon,
                                     const QString &title,
                                     const QString &badgeText,
                                     Tier tier,
                                     bool showRibbon,
                                     const QString &ribbonText,
                                     QWidget *parent)
    : QPushButton(parent),
      m_title(title),
      m_badgeText(badgeText),
      m_tier(tier),
      m_showRibbon(showRibbon),
      m_ribbonText(ribbonText)
{
    setIcon(icon);
    setAttribute(Qt::WA_Hover, true);
    setFixedSize(260, 82);
    setCursor(tier == Tier::Disabled ? Qt::ForbiddenCursor : Qt::PointingHandCursor);
    setStyleSheet("ServiceTileButton { background: transparent; border: none; outline: none; padding: 0px; margin: 0px; }");
}

void ServiceTileButton::setTier(Tier tier)
{
    m_tier = tier;
    setCursor(tier == Tier::Disabled ? Qt::ForbiddenCursor : Qt::PointingHandCursor);
    update();
}

void ServiceTileButton::setTitle(const QString &title)
{
    m_title = title;
    update();
}

void ServiceTileButton::setBadgeText(const QString &text)
{
    m_badgeText = text;
    update();
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

QSize ServiceTileButton::sizeHint() const
{
    return QSize(260, 82);
}

QSize ServiceTileButton::minimumSizeHint() const
{
    return QSize(220, 76);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void ServiceTileButton::enterEvent(QEnterEvent *event)
{
    QPushButton::enterEvent(event);
    update();
}
#else
void ServiceTileButton::enterEvent(QEvent *event)
{
    QPushButton::enterEvent(event);
    update();
}
#endif

void ServiceTileButton::leaveEvent(QEvent *event)
{
    QPushButton::leaveEvent(event);
    update();
}

void ServiceTileButton::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const int w = width();
    const int h = height();
    const bool enabled = isEnabled();
    const bool hovered = enabled && underMouse();
    const bool pressed = enabled && isDown();

    // 1. Metro Card Shell: Crisp Flat Dark Obsidian Background & Outline
    QColor bgColor;
    QColor borderColor;
    if(!enabled)
    {
        bgColor = QColor(11, 15, 25);       // #0B0F19
        borderColor = QColor(30, 41, 59);   // #1E293B
    }
    else if(pressed)
    {
        bgColor = QColor(8, 14, 26);        // #080E1A
        borderColor = QColor(2, 132, 199);  // #0284C7
    }
    else if(hovered)
    {
        bgColor = QColor(19, 31, 55);       // #131F37
        borderColor = QColor(56, 189, 248); // #38BDF8
    }
    else
    {
        bgColor = QColor(15, 23, 42);       // #0F172A
        borderColor = QColor(30, 41, 59);   // #1E293B
    }

    painter.fillRect(QRect(0, 0, w, h), bgColor);
    painter.setPen(QPen(borderColor, 1));
    painter.drawRect(QRect(0, 0, w - 1, h - 1));

    // 2. Left Status Accent Stripe (4px width)
    QColor accentColor;
    if(!enabled || m_tier == Tier::Disabled)
    {
        accentColor = QColor(71, 85, 105);   // #475569 Slate
    }
    else
    {
        switch(m_tier)
        {
        case Tier::Vip:
            accentColor = hovered ? QColor(251, 191, 36) : QColor(245, 158, 11);  // Gold #F59E0B / #FBBF24
            break;
        case Tier::Free:
            accentColor = hovered ? QColor(52, 211, 153) : QColor(16, 185, 129);  // Emerald #10B981 / #34D399
            break;
        case Tier::Dynamic:
            accentColor = hovered ? QColor(129, 140, 248) : QColor(99, 102, 241); // Indigo #818CF8
            break;
        case Tier::Credit:
        default:
            accentColor = hovered ? QColor(56, 189, 248) : QColor(14, 165, 233);  // Sky Cyan #38BDF8
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
        titleColor = QColor(56, 189, 248);  // #38BDF8
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
        const int maxPillW = w - textX - 12;
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

    // 6. Top-Right Red Ribbon Banner with "NEW" Inscription
    if(m_showRibbon && enabled)
    {
        const int ribbonW = 58;
        const int ribbonH = 21;
        const int notch = 8;
        const int rx = w - ribbonW;
        const int ry = 0;

        // Physical drop shadow behind ribbon
        QPolygon shadowPoly;
        shadowPoly << QPoint(w, ry + 2)
                   << QPoint(rx + 1, ry + 2)
                   << QPoint(rx + notch + 1, ry + ribbonH / 2 + 1)
                   << QPoint(rx + 1, ry + ribbonH + 2)
                   << QPoint(w, ry + ribbonH + 2);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 110));
        painter.drawPolygon(shadowPoly);

        // Ribbon polygon with decorative swallowtail notch cut on the left
        QPolygon ribbonPoly;
        ribbonPoly << QPoint(w, ry)
                   << QPoint(rx, ry)
                   << QPoint(rx + notch, ry + ribbonH / 2)
                   << QPoint(rx, ry + ribbonH)
                   << QPoint(w, ry + ribbonH);

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

            ServiceTileButton *button = new ServiceTileButton(
                svcIcon,
                remoteService->name,
                badgeText,
                tier,
                instance->active, // Show red ribbon "NEW" for active services
                QString::fromUtf8("NEW"),
                ui->serviceContents);
            button->setFixedSize(260, 82);
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
            button->setProperty("serviceUuid", remoteService->uuid);
            button->setProperty("isAdsKiller", remoteService->uuid == IDServiceAdsString);
            button->setProperty("isAppleFirmware", false);
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
    std::sort(std::begin(services), std::end(services), [](const std::shared_ptr<Service> &lhs, const std::shared_ptr<Service> &rhs) {
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

    createAppleServiceButton();
    applyServiceFilters();
}

void MainWindow::createAppleServiceButton()
{
    if(!ui || !ui->serviceContents || !ui->serviceContents->layout())
        return;

    if(ui->serviceContents->findChild<ServiceTileButton *>("serviceButton_AppleFirmware"))
        return;

    QIcon appleIcon(":/svg/services/apple");
    if(appleIcon.isNull())
        appleIcon = QIcon(":/service-icons/apple");
    if(appleIcon.isNull())
        appleIcon = QIcon("res/svg/services/apple.svg");
    if(appleIcon.isNull())
        appleIcon = QIcon("svg/services/apple.svg");

    ServiceTileButton *appleBtn = new ServiceTileButton(
        appleIcon,
        QString::fromUtf8("Прошивки APPLE"),
        QString::fromUtf8("БЕСПЛАТНО"),
        ServiceTileButton::Tier::Free,
        true,
        QString::fromUtf8("SOON"),
        ui->serviceContents);
    appleBtn->setObjectName("serviceButton_AppleFirmware");
    appleBtn->setFixedSize(260, 82);
    appleBtn->setEnabled(true);
    appleBtn->setCursor(Qt::PointingHandCursor);
    appleBtn->setProperty("serviceUuid", "apple_firmware");
    appleBtn->setProperty("isAppleFirmware", true);
    appleBtn->setProperty("isAdsKiller", false);
    appleBtn->setProperty("isFree", true);
    appleBtn->setProperty("isPaid", false);
    appleBtn->setProperty("isActive", false);
    appleBtn->setProperty("isUnderDev", true);
    appleBtn->setProperty("serviceName", QString::fromUtf8("Прошивки APPLE"));
    appleBtn->setProperty("serviceDesc", QString::fromUtf8("Прошивка, восстановление и обход блокировок Apple iOS устройств"));
    appleBtn->setToolTip(QString::fromUtf8("Прошивки APPLE\nПрошивка, восстановление и обход блокировок Apple iOS устройств\n\n• Бесплатная услуга (в разработке)"));

    QObject::connect(
        appleBtn,
        &QPushButton::clicked,
        this,
        [this]()
        {
            QMessageBox msgBox(this);
            msgBox.setWindowTitle(QString::fromUtf8("Прошивки APPLE"));
            msgBox.setIcon(QMessageBox::Information);
            msgBox.setText(QString::fromUtf8(
                "<h3>Сервис «Прошивки APPLE»</h3>"
                "<p>Данный модуль в настоящее время находится <b>в активной разработке</b>.</p>"
                "<p style='color: #94A3B8; font-size: 11px;'>Функционал загрузки официальных и кастомных IPSW-прошивок, восстановления из режима DFU/Recovery, а также инструменты работы с устройствами Apple iOS будут доступны в ближайшем обновлении.</p>"));

            QPushButton *btnOk = msgBox.addButton(QString::fromUtf8("Понятно"), QMessageBox::AcceptRole);
            btnOk->setCursor(Qt::PointingHandCursor);
            btnOk->setStyleSheet(
                "QPushButton {"
                "   background-color: #0284C7;"
                "   color: #FFFFFF;"
                "   font-size: 12px;"
                "   font-weight: bold;"
                "   border: 1px solid #0284C7;"
                "   border-radius: 0px;"
                "   padding: 6px 20px;"
                "   min-width: 100px;"
                "}"
                "QPushButton:hover { background-color: #0369A1; border-color: #38BDF8; }"
                "QPushButton:pressed { background-color: #075985; }");
            msgBox.setDefaultButton(btnOk);
            msgBox.exec();
        });

    if(auto *grid = qobject_cast<QGridLayout *>(ui->serviceContents->layout()))
    {
        int count = ui->serviceContents->findChildren<ServiceTileButton *>(QString(), Qt::FindDirectChildrenOnly).size();
        grid->addWidget(appleBtn, count / 2, count % 2);
    }
}

void MainWindow::applyServiceFilters()
{
    if(!ui || !ui->serviceContents || !ui->serviceContents->layout())
        return;

    QGridLayout *grid = qobject_cast<QGridLayout *>(ui->serviceContents->layout());
    if(!grid)
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

    auto getServicePriority = [](ServiceTileButton *btn) -> int {
        if(btn->property("isAdsKiller").toBool()
           || btn->property("serviceUuid").toString() == IDServiceAdsString
           || btn->title().contains(QString::fromUtf8("реклам"), Qt::CaseInsensitive)
           || btn->title().contains(QString::fromUtf8("Ads"), Qt::CaseInsensitive))
        {
            return 1; // 1. Удаление рекламы (Ads Killer) ALWAYS FIRST
        }
        if(btn->property("isAppleFirmware").toBool()
           || btn->objectName() == "serviceButton_AppleFirmware"
           || btn->title().contains(QString::fromUtf8("APPLE"), Qt::CaseInsensitive))
        {
            return 2; // 2. Прошивки APPLE ALWAYS SECOND
        }
        return 10;
    };

    // Sort order: Ads Killer ALWAYS 1st, Apple Firmware ALWAYS 2nd, then active services, then rest
    std::sort(allButtons.begin(), allButtons.end(), [getServicePriority](ServiceTileButton *a, ServiceTileButton *b) {
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

        if(isAvail) ++availCount;
        if(isUnavail) ++unavailCount;
        if(isFree) ++freeCount;
        if(isPaid) ++paidCount;

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
            matchesSearch = btn->title().contains(query, Qt::CaseInsensitive)
                            || btn->toolTip().contains(query, Qt::CaseInsensitive)
                            || btn->property("serviceDesc").toString().contains(query, Qt::CaseInsensitive)
                            || btn->badgeText().contains(query, Qt::CaseInsensitive);
        }

        bool visible = matchesFilter && matchesSearch;
        grid->removeWidget(btn);
        btn->setVisible(visible);

        if(visible)
        {
            grid->addWidget(btn, visibleIndex / 2, visibleIndex % 2);
            ++visibleIndex;
        }
    }

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
    if(visibleIndex == 0)
    {
        if(!emptyLbl)
        {
            emptyLbl = new QLabel(ui->serviceContents);
            emptyLbl->setObjectName("servicesEmptyLabel");
            emptyLbl->setAlignment(Qt::AlignCenter);
            emptyLbl->setStyleSheet("color: #64748B; font-size: 13px; font-weight: 600; padding: 40px; background: transparent;");
        }
        emptyLbl->setText(query.isEmpty() 
            ? QString::fromUtf8("В данной категории нет сервисов")
            : QString::fromUtf8("По запросу «%1» ничего не найдено").arg(query));
        grid->addWidget(emptyLbl, 0, 0, 1, 2, Qt::AlignCenter);
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
                                    "border-radius: 0px;"
                                    "color: #38BDF8;"
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
                                    "background-color: #451A03;"
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
            darkPalette.setColor(QPalette::Window, QColor(10, 14, 26));            // #0A0E1A
            darkPalette.setColor(QPalette::WindowText, QColor(248, 250, 252));    // #F8FAFC
            darkPalette.setColor(QPalette::Base, QColor(7, 10, 18));              // #070A12
            darkPalette.setColor(QPalette::AlternateBase, QColor(15, 23, 42));    // #0F172A
            darkPalette.setColor(QPalette::ToolTipBase, QColor(15, 23, 42));      // #0F172A
            darkPalette.setColor(QPalette::ToolTipText, QColor(248, 250, 252));   // #F8FAFC
            darkPalette.setColor(QPalette::Text, QColor(248, 250, 252));          // #F8FAFC
            darkPalette.setColor(QPalette::Button, QColor(15, 23, 42));           // #0F172A
            darkPalette.setColor(QPalette::ButtonText, QColor(248, 250, 252));    // #F8FAFC
            darkPalette.setColor(QPalette::BrightText, QColor(56, 189, 248));     // #38BDF8
            darkPalette.setColor(QPalette::Link, QColor(56, 189, 248));           // #38BDF8
            darkPalette.setColor(QPalette::Highlight, QColor(2, 132, 199));       // #0284C7
            darkPalette.setColor(QPalette::HighlightedText, QColor(255, 255, 255)); // #FFFFFF
            darkPalette.setColor(QPalette::PlaceholderText, QColor(100, 116, 139)); // #64748B

            darkPalette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(71, 85, 105)); // #475569
            darkPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(71, 85, 105));
            darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(71, 85, 105));
            darkPalette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(30, 41, 59));   // #1E293B
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
