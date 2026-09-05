#include <functional>
#include <algorithm>
#include <cctype>
#include <list>
#include <memory>

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

    // Apply convenient modern design for all pages
    setupPagesDesign();

    // AI toolbox toggle button and panel configuration
    if(ui->aiToolBoxToggle)
    {
        ui->aiToolBoxToggle->setText(QString::fromUtf8("›"));
        ui->aiToolBoxToggle->setToolTip("Свернуть панель ИИ");
        ui->aiToolBoxToggle->setCursor(Qt::PointingHandCursor);
        ui->aiToolBoxToggle->setStyleSheet(
            "QPushButton#aiToolBoxToggle {"
            "   background-color: #1A1C21;"
            "   color: #8E9297;"
            "   border: 1px solid #2D313A;"
            "   border-radius: 4px;"
            "   font-size: 13px;"
            "   font-weight: bold;"
            "   padding: 0px;"
            "}"
            "QPushButton#aiToolBoxToggle:hover {"
            "   background-color: #262932;"
            "   border-color: #4CC2FF;"
            "   color: #4CC2FF;"
            "}"
            "QPushButton#aiToolBoxToggle:pressed {"
            "   background-color: #141518;"
            "}");
        ui->aiToolBoxContainer->setMaximumWidth(324);
        ui->aiToolBoxContainer->setStyleSheet(
            "QFrame#aiToolBoxContainer {"
            "   background-color: #141518;"
            "   border: none;"
            "}");

        if(ui->aiToolBox)
        {
            ui->aiToolBox->setItemText(0, "✦ ИИ Ассистент - AdsKiller");
            ui->aiToolBox->setItemText(1, "ℹ О модуле ИИ");
            ui->aiToolBox->setStyleSheet(
                "QToolBox#aiToolBox {"
                "   background-color: #141518;"
                "   border: 1px solid #252830;"
                "   border-radius: 10px;"
                "}"
                "QToolBox#aiToolBox::tab {"
                "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #22252C, stop:1 #1A1C21);"
                "   color: #9CA3AF;"
                "   border: 1px solid #2F333E;"
                "   border-radius: 7px;"
                "   padding: 5px 12px;"
                "   font-size: 11px;"
                "   font-weight: 600;"
                "   margin: 2px 2px;"
                "}"
                "QToolBox#aiToolBox::tab:selected {"
                "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #252D3B, stop:1 #1C2330);"
                "   color: #4CC2FF;"
                "   border: 1px solid #0078D4;"
                "   font-weight: bold;"
                "}"
                "QToolBox#aiToolBox::tab:hover {"
                "   background: #282D38;"
                "   color: #FFFFFF;"
                "   border-color: #4CC2FF;"
                "}");
        }

        if(ui->aboutAi_edit)
        {
            ui->aboutAi_edit->setStyleSheet(
                "QTextEdit#aboutAi_edit {"
                "   background-color: #16181D;"
                "   color: #D1D5DB;"
                "   border: 1px solid #262930;"
                "   border-radius: 8px;"
                "   padding: 10px;"
                "   font-size: 11px;"
                "}");
            ui->aboutAi_edit->setHtml(
                "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">"
                "<html><head><meta name=\"qrichtext\" content=\"1\" /><meta charset=\"utf-8\" />"
                "<style type=\"text/css\">p, li { white-space: pre-wrap; }</style></head>"
                "<body style=\"font-family:'Segoe UI', 'Noto Sans', sans-serif; font-size:10pt; color:#D1D5DB;\">"
                "<div style=\"text-align:center; padding-bottom:8px;\">"
                "<span style=\"font-size:20px;\">🤖</span><br/>"
                "<b style=\"color:#4CC2FF; font-size:12pt;\">AdsKiller AI Assistant</b><br/>"
                "<span style=\"color:#8E9297; font-size:9pt;\">Интеллектуальный помощник</span>"
                "</div>"
                "<hr style=\"border:none; border-top:1px solid #2B2F38; margin:8px 0;\"/>"
                "<p style=\"line-height:1.6; font-size:9.5pt;\">"
                "<b style=\"color:#FFFFFF;\">Автор модуля ИИ:</b><br/>"
                "&nbsp;&nbsp;Команда <span style=\"color:#4CC2FF;\">imister.tech</span><br/><br/>"
                "<b style=\"color:#FFFFFF;\">Разработчик:</b><br/>"
                "&nbsp;&nbsp;Нурсеит К. (<span style=\"color:#4CC2FF;\">badcast</span>)<br/><br/>"
                "<b style=\"color:#FFFFFF;\">Дизайн:</b><br/>"
                "&nbsp;&nbsp;Владимир (<span style=\"color:#4CC2FF;\">LeoJames</span>)"
                "</p>"
                "</body></html>");
        }

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
        chatView->showLocked();

        // Quick buttons scroll area + shuffle button bar
        QWidget *quickBarWidget = new QWidget(ui->aiToolBoxPage1);
        quickBarWidget->setStyleSheet("background: transparent;");
        QHBoxLayout *quickBarLayout = new QHBoxLayout(quickBarWidget);
        quickBarLayout->setContentsMargins(0, 0, 0, 0);
        quickBarLayout->setSpacing(4);

        QScrollArea *scrollArea = new QScrollArea(quickBarWidget);
        scrollArea->setObjectName("aiQuickScrollArea");
        scrollArea->setWidgetResizable(true);
        scrollArea->setFixedHeight(56);
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
            "   background: rgba(0, 0, 0, 25);"
            "   margin: 0px;"
            "   border-radius: 1px;"
            "}"
            "QScrollArea#aiQuickScrollArea QScrollBar::handle:horizontal {"
            "   background: #44474F;"
            "   min-width: 16px;"
            "   border-radius: 1px;"
            "}"
            "QScrollArea#aiQuickScrollArea QScrollBar::handle:horizontal:hover {"
            "   background: #4CC2FF;"
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
        quickButtonsLayout->setContentsMargins(2, 2, 2, 2);
        quickButtonsLayout->setSpacing(4);

        struct QuickQuestion
        {
            QString icon;
            QStringList variations;
        };

        QList<QuickQuestion> quickQuestions = {
            {"💳", {"Мои кредиты", "Сколько кредитов?", "Остаток баланса?", "Показать баланс"}},
            {"👑", {"VIP статус", "Остаток VIP дней", "Сколько VIP дней?", "Когда истекает VIP?"}},
            {"📱", {"Мои устройства", "Список устройств", "Активные девайсы", "Привязанные устройства"}},
            {"🛡️", {"Удаление рекламы", "Запусти удаление рекламы", "Какие есть сервисы?", "Открой окно покупки VIP"}},
            {"⚡", {"Быстрая очистка", "Остановить приложения", "Очистить кэш", "Как закрыть вирусы?"}},
            {"🚀", {"Ускорить телефон", "Как очистить ОЗУ?", "Оптимизация системы", "Ускорить работу"}},
            {"💡", {"Что ты умеешь?", "Возможности AdsKiller", "Справка по функциям", "Чем можешь помочь?"}},
            {"📧", {"Моя почта", "Мой email", "Какая у меня почта?", "Адрес эл. почты"}},
            {"🛒", {"Купить кредиты", "Как купить VIP?", "Пополнение баланса", "Тарифы и цены"}},
            {"🔒", {"Безопасность", "Безопасно ли это?", "Как включить отладку?", "Как подключить телефон?"}},
            {"📊", {"Статистика", "Заблокированная реклама", "Отчет блокировки", "Сколько рекламы скрыто?"}},
            {"❓", {"Как пользоваться?", "Инструкция для новичка", "Быстрый старт", "Помощь по приложению"}}};

        auto questionsPtr = std::make_shared<QList<QuickQuestion>>(quickQuestions);

        auto populateRandomButtons = [this, quickButtonsWidget, quickButtonsLayout, questionsPtr]()
        {
            // Clear existing buttons from layout
            QLayoutItem *child;
            while((child = quickButtonsLayout->takeAt(0)) != nullptr)
            {
                if(child->widget())
                    delete child->widget();
                delete child;
            }

            // Shuffle questions randomly across the grid
            std::shuffle(questionsPtr->begin(), questionsPtr->end(), *QRandomGenerator::global());

            for(int i = 0; i < questionsPtr->size(); ++i)
            {
                const auto &qData = (*questionsPtr)[i];
                int initialIdx = QRandomGenerator::global()->bounded(qData.variations.size());
                QString initialText = qData.variations[initialIdx];

                QPushButton *btn = new QPushButton(QString("%1  %2").arg(qData.icon, initialText), quickButtonsWidget);
                btn->setStyleSheet(
                    "QPushButton {"
                    "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #252830, stop:1 #1D2026);"
                    "   color: #D1D5DB;"
                    "   border: 1px solid #333742;"
                    "   border-radius: 8px;"
                    "   padding: 3px 9px;"
                    "   font-size: 10.5px;"
                    "   font-weight: 500;"
                    "   white-space: nowrap;"
                    "}"
                    "QPushButton:hover {"
                    "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #2D3340, stop:1 #232934);"
                    "   border: 1px solid #4CC2FF;"
                    "   color: #4CC2FF;"
                    "}"
                    "QPushButton:pressed {"
                    "   background-color: #16181C;"
                    "   color: #FFFFFF;"
                    "}");
                btn->setCursor(Qt::PointingHandCursor);
                quickButtonsLayout->addWidget(btn, i % 2, i / 2);

                QObject::connect(
                    btn,
                    &QPushButton::clicked,
                    this,
                    [this, btn, icon = qData.icon, variations = qData.variations, lastIdx = initialIdx]() mutable
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
        };

        // Populate buttons with initial random arrangement
        populateRandomButtons();
        scrollArea->setWidget(quickButtonsWidget);

        QPushButton *shuffleBtn = new QPushButton("🔀", quickBarWidget);
        shuffleBtn->setToolTip("Перемешать подсказки (случайный порядок)");
        shuffleBtn->setFixedSize(26, 54);
        shuffleBtn->setCursor(Qt::PointingHandCursor);
        shuffleBtn->setStyleSheet(
            "QPushButton {"
            "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #22252C, stop:1 #1A1C22);"
            "   color: #8E9297;"
            "   border: 1px solid #2D313A;"
            "   border-radius: 8px;"
            "   font-size: 11px;"
            "   padding: 0px;"
            "}"
            "QPushButton:hover {"
            "   background: #282D36;"
            "   border-color: #4CC2FF;"
            "   color: #4CC2FF;"
            "}"
            "QPushButton:pressed {"
            "   background: #141518;"
            "}");

        QObject::connect(shuffleBtn, &QPushButton::clicked, this, populateRandomButtons);

        quickBarLayout->addWidget(scrollArea, 1);
        quickBarLayout->addWidget(shuffleBtn, 0);

        ui->aiChatEdit->setStyleSheet(
            "QTextEdit#aiChatEdit {"
            "   background-color: #1A1C22;"
            "   color: #FFFFFF;"
            "   border: 1px solid #2E333D;"
            "   border-radius: 9px;"
            "   padding: 6px 10px;"
            "   font-size: 11.5px;"
            "   selection-background-color: #0078D4;"
            "}"
            "QTextEdit#aiChatEdit:focus {"
            "   border: 1px solid #4CC2FF;"
            "   background-color: #16181D;"
            "}");
        ui->aiChatEdit->setPlaceholderText("Задайте вопрос ИИ...");
        ui->aiChatEdit->setMinimumHeight(40);
        ui->aiChatEdit->setMaximumHeight(40);

        ui->aiChatSend->setStyleSheet(
            "QPushButton#aiChatSend {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #0078D4, stop:1 #005A9E);"
            "   color: #FFFFFF;"
            "   border: 1px solid #1A8CE6;"
            "   border-radius: 9px;"
            "   padding: 6px 12px;"
            "   font-weight: bold;"
            "   font-size: 11.5px;"
            "}"
            "QPushButton#aiChatSend:hover {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #1088E8, stop:1 #0066BA);"
            "   border-color: #4CC2FF;"
            "}"
            "QPushButton#aiChatSend:pressed {"
            "   background: #004D80;"
            "}"
            "QPushButton#aiChatSend:disabled {"
            "   background: #252830;"
            "   color: #555A64;"
            "   border-color: #2D313A;"
            "}");
        ui->aiChatSend->setCursor(Qt::PointingHandCursor);
        ui->aiChatSend->setMinimumHeight(40);
        ui->aiChatSend->setMaximumHeight(40);

        QGridLayout *aiLayout = qobject_cast<QGridLayout *>(ui->aiChatMessages->parentWidget()->layout());
        if(aiLayout)
        {
            aiLayout->setContentsMargins(4, 4, 4, 4);
            aiLayout->setSpacing(6);
            aiLayout->removeWidget(ui->aiChatMessages);
            aiLayout->removeWidget(ui->aiChatEdit);
            aiLayout->removeWidget(ui->aiChatSend);
            aiLayout->addWidget(chatView, 0, 0, 1, 2);
            aiLayout->addWidget(quickBarWidget, 1, 0, 1, 2);
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

void MainWindow::setupPagesDesign()
{
    // ==========================================
    // 0. Global Window and Controls Styling
    // ==========================================
    this->setStyleSheet(
        "QMainWindow {"
        "   background-color: #141517;"
        "}"
        "QWidget#centralwidget {"
        "   background-color: #141517;"
        "}"
        "QScrollBar:vertical {"
        "   background: transparent;"
        "   width: 8px;"
        "   margin: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "   background: #363940;"
        "   border-radius: 4px;"
        "   min-height: 24px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "   background: #4E525C;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "   height: 0px;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "   background: transparent;"
        "}"
        "QScrollBar:horizontal {"
        "   background: transparent;"
        "   height: 8px;"
        "   margin: 0px;"
        "}"
        "QScrollBar::handle:horizontal {"
        "   background: #363940;"
        "   border-radius: 4px;"
        "   min-width: 24px;"
        "}"
        "QScrollBar::handle:horizontal:hover {"
        "   background: #4E525C;"
        "}"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
        "   width: 0px;"
        "}"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {"
        "   background: transparent;"
        "}"
    );

    // ==========================================
    // 1. Auth Page (page_auth)
    // ==========================================
    if(ui->frame_4)
    {
        ui->frame_4->setStyleSheet(
            "QFrame#frame_4 {"
            "   background-color: #1E2024;"
            "   border: 1px solid #2D3139;"
            "   border-radius: 12px;"
            "}"
        );
    }
    if(ui->label_4)
    {
        ui->label_4->setStyleSheet("color: #9CA3AF; font-size: 13px; font-weight: 500;");
        ui->label_4->setText("Введите логин и пароль для авторизации");
    }
    if(ui->label_12)
    {
        ui->label_12->setStyleSheet("color: #8E9297; font-size: 11px; font-weight: 700; letter-spacing: 0.5px;");
    }
    if(ui->label_14)
    {
        ui->label_14->setStyleSheet("color: #8E9297; font-size: 11px; font-weight: 700; letter-spacing: 0.5px;");
    }
    if(ui->lineLoginEdit)
    {
        ui->lineLoginEdit->setPlaceholderText("Логин или имя пользователя");
        ui->lineLoginEdit->setStyleSheet(
            "QLineEdit {"
            "   background-color: #141518;"
            "   color: #F3F4F6;"
            "   border: 1px solid #363A44;"
            "   border-radius: 6px;"
            "   padding: 5px 10px;"
            "   font-size: 13px;"
            "}"
            "QLineEdit:focus {"
            "   border: 1px solid #4CC2FF;"
            "   background-color: #16181C;"
            "}"
        );
    }
    if(ui->linePassEdit)
    {
        ui->linePassEdit->setPlaceholderText("Пароль или токен");
        ui->linePassEdit->setStyleSheet(
            "QLineEdit {"
            "   background-color: #141518;"
            "   color: #F3F4F6;"
            "   border: 1px solid #363A44;"
            "   border-radius: 6px;"
            "   padding: 5px 10px;"
            "   font-size: 13px;"
            "}"
            "QLineEdit:focus {"
            "   border: 1px solid #4CC2FF;"
            "   background-color: #16181C;"
            "}"
        );
    }
    if(ui->butShowPass)
    {
        ui->butShowPass->setText("👁");
        ui->butShowPass->setToolTip("Показать / скрыть пароль");
        ui->butShowPass->setCursor(Qt::PointingHandCursor);
        ui->butShowPass->setStyleSheet(
            "QPushButton {"
            "   background-color: #26292F;"
            "   color: #D1D5DB;"
            "   border: 1px solid #363A44;"
            "   border-radius: 6px;"
            "   font-size: 13px;"
            "   padding: 2px;"
            "}"
            "QPushButton:hover {"
            "   background-color: #32363E;"
            "   border-color: #4CC2FF;"
            "   color: #4CC2FF;"
            "}"
            "QPushButton:pressed {"
            "   background-color: #1A1B1E;"
            "}"
        );
    }
    if(ui->checkAutoLogin)
    {
        ui->checkAutoLogin->setCursor(Qt::PointingHandCursor);
        ui->checkAutoLogin->setStyleSheet(
            "QCheckBox {"
            "   color: #C9CCD1;"
            "   font-size: 12px;"
            "   spacing: 6px;"
            "}"
            "QCheckBox::indicator {"
            "   width: 16px;"
            "   height: 16px;"
            "   border: 1px solid #3E434D;"
            "   border-radius: 4px;"
            "   background-color: #1E2024;"
            "}"
            "QCheckBox::indicator:hover {"
            "   border-color: #4CC2FF;"
            "}"
            "QCheckBox::indicator:checked {"
            "   background-color: #0078D4;"
            "   border-color: #0078D4;"
            "}"
        );
    }
    if(ui->label_2)
    {
        ui->label_2->setStyleSheet(
            "QLabel {"
            "   color: #4CC2FF;"
            "   font-size: 12px;"
            "}"
            "QLabel a {"
            "   color: #4CC2FF;"
            "   text-decoration: none;"
            "}"
            "QLabel a:hover {"
            "   color: #70D0FF;"
            "   text-decoration: underline;"
            "}"
        );
    }
    if(ui->authButton)
    {
        ui->authButton->setCursor(Qt::PointingHandCursor);
        ui->authButton->setStyleSheet(
            "QPushButton {"
            "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #007ACC, stop:1 #005A9E);"
            "   color: #FFFFFF;"
            "   border: 1px solid #005A9E;"
            "   border-radius: 6px;"
            "   font-size: 13px;"
            "   font-weight: bold;"
            "   padding: 5px 16px;"
            "}"
            "QPushButton:hover {"
            "   background: #1084D9;"
            "   border-color: #4CC2FF;"
            "}"
            "QPushButton:pressed {"
            "   background: #004C87;"
            "}"
            "QPushButton:disabled {"
            "   background-color: #2D3139;"
            "   border-color: #363A44;"
            "   color: #6B7280;"
            "}"
        );
    }
    if(ui->statusAuthText)
    {
        ui->statusAuthText->setStyleSheet("color: #F87171; font-size: 12px; font-weight: 500;");
    }
    if(ui->label_auth_ver)
    {
        ui->label_auth_ver->setStyleSheet("color: #5C6067; font-size: 11px;");
        ui->label_auth_ver->setText("версия: " + runtimeVersion.mVersion.toString());
    }

    // ==========================================
    // 2. Cabinet Page (page_cabinet)
    // ==========================================
    if(ui->toplevel_up)
    {
        ui->toplevel_up->setStyleSheet(
            "QFrame#toplevel_up {"
            "   background-color: #1A1D21;"
            "   border-bottom: 1px solid #282B30;"
            "}"
        );
    }
    if(ui->label_6)
    {
        ui->label_6->setStyleSheet(
            "color: #FFFFFF;"
            "font-size: 14px;"
            "font-weight: bold;"
            "letter-spacing: 0.5px;"
            "font-style: normal;"
            "background: transparent;"
        );
    }
    if(ui->logoutButton)
    {
        ui->logoutButton->setCursor(Qt::PointingHandCursor);
        ui->logoutButton->setStyleSheet(
            "QPushButton {"
            "   background-color: rgba(239, 68, 68, 0.12);"
            "   color: #F87171;"
            "   border: 1px solid rgba(239, 68, 68, 0.3);"
            "   border-radius: 6px;"
            "   font-size: 12px;"
            "   font-weight: 600;"
            "   padding: 4px 12px;"
            "}"
            "QPushButton:hover {"
            "   background-color: rgba(239, 68, 68, 0.22);"
            "   color: #FFA3A3;"
            "   border-color: rgba(239, 68, 68, 0.5);"
            "}"
            "QPushButton:pressed {"
            "   background-color: rgba(239, 68, 68, 0.35);"
            "}"
        );
    }
    if(ui->frame_7)
    {
        ui->frame_7->setMaximumSize(16777215, 16777215);
        ui->frame_7->setMinimumHeight(164);
        ui->frame_7->setStyleSheet(
            "QFrame#frame_7 {"
            "   background-color: #1E2024;"
            "   border: 1px solid #2D3139;"
            "   border-radius: 12px;"
            "}"
        );
    }
    if(ui->frame_3)
    {
        ui->frame_3->setStyleSheet(
            "QFrame#frame_3 {"
            "   border: 2px solid #363940;"
            "   border-radius: 8px;"
            "   background-color: #141517;"
            "}"
        );
    }
    if(ui->labelLoginAuthed)
    {
        ui->labelLoginAuthed->setStyleSheet(
            "color: #FFFFFF;"
            "font-size: 18px;"
            "font-weight: bold;"
            "text-decoration: none;"
        );
    }
    if(ui->frame_6)
    {
        ui->frame_6->setStyleSheet(
            "QFrame#frame_6 {"
            "   background-color: #26292F;"
            "   border: 1px solid #363940;"
            "   border-radius: 8px;"
            "}"
        );
    }
    if(ui->labelVipDays)
    {
        ui->labelVipDays->setStyleSheet(
            "color: #FBBF24;"
            "font-size: 13px;"
            "font-weight: bold;"
            "text-decoration: none;"
        );
    }
    if(ui->frame_5)
    {
        ui->frame_5->setStyleSheet(
            "QFrame#frame_5 {"
            "   background-color: #26292F;"
            "   border: 1px solid #363940;"
            "   border-radius: 8px;"
            "}"
        );
    }
    if(ui->labelCredits)
    {
        ui->labelCredits->setStyleSheet(
            "color: #34D399;"
            "font-size: 13px;"
            "font-weight: bold;"
            "text-decoration: none;"
        );
    }
    if(ui->toplevel_up_2)
    {
        ui->toplevel_up_2->setStyleSheet(
            "QFrame#toplevel_up_2 {"
            "   background-color: #18191C;"
            "   border-top: 1px solid #26282E;"
            "   border-bottom: 1px solid #26282E;"
            "}"
        );
    }
    if(ui->label_7)
    {
        ui->label_7->setStyleSheet(
            "color: #9CA3AF;"
            "font-size: 11px;"
            "font-weight: 700;"
            "letter-spacing: 1.5px;"
            "font-style: normal;"
            "background: transparent;"
        );
    }
    if(ui->serviceContents)
    {
        ui->serviceContents->setStyleSheet(
            "QFrame#serviceContents QPushButton {"
            "   background-color: #22252B;"
            "   color: #FFFFFF;"
            "   border: 1px solid #32363E;"
            "   border-radius: 10px;"
            "   padding: 12px 8px 12px 8px;"
            "   font-weight: bold;"
            "   font-size: 12px;"
            "   text-align: bottom center;"
            "}"
            "QFrame#serviceContents QPushButton:hover {"
            "   background-color: #2A2E36;"
            "   border-color: #4CC2FF;"
            "}"
            "QFrame#serviceContents QPushButton:pressed {"
            "   background-color: #1B1D22;"
            "   border-color: #007ACC;"
            "}"
        );
    }
    if(ui->authInfo)
    {
        ui->authInfo->setMinimumHeight(130);
        ui->authInfo->setMaximumHeight(230);
        ui->authInfo->horizontalHeader()->setStretchLastSection(true);
        ui->authInfo->setStyleSheet(
            "QTableView {"
            "   background-color: #1A1C20;"
            "   alternate-background-color: #16181B;"
            "   gridline-color: #282A2E;"
            "   border: 1px solid #282A2E;"
            "   border-radius: 8px;"
            "   color: #D1D5DB;"
            "   font-size: 12px;"
            "   selection-background-color: #0078D4;"
            "   selection-color: #FFFFFF;"
            "}"
            "QHeaderView::section {"
            "   background-color: #22252B;"
            "   color: #9CA3AF;"
            "   font-size: 12px;"
            "   font-weight: bold;"
            "   border: none;"
            "   border-bottom: 1px solid #2E3238;"
            "   border-right: 1px solid #282A2E;"
            "   padding: 6px 8px;"
            "}"
        );
    }

    // ==========================================
    // 3. Devices Connection Page (page_devices)
    // ==========================================
    if(ui->scrollArea_2)
    {
        ui->scrollArea_2->setFrameShape(QFrame::NoFrame);
        ui->scrollArea_2->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        ui->scrollArea_2->setStyleSheet("QScrollArea { background: transparent; border: none; } QWidget#scrollAreaWidgetContents_2 { background: transparent; }");
    }
    if(ui->label)
    {
        ui->label->setText(
            "<html><head/><body>"
            "<div style=\"font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; color: #D1D5DB; padding: 4px;\">"
            "  <h2 style=\"color: #FFFFFF; font-size: 17px; margin-top: 8px; margin-bottom: 16px; font-weight: 700;\">"
            "    Как включить отладку по USB на Android"
            "  </h2>"
            "  <div style=\"background-color: #21242A; border: 1px solid #2F333B; border-radius: 8px; padding: 12px 14px; margin-bottom: 10px;\">"
            "    <span style=\"background: #0078D4; color: white; border-radius: 12px; padding: 2px 9px; font-weight: bold; font-size: 12px;\">1</span>"
            "    <strong style=\"color: #FFFFFF; font-size: 13px; margin-left: 6px;\">Включите режим разработчика</strong>"
            "    <p style=\"margin: 6px 0 0 26px; color: #9CA3AF; font-size: 12px; line-height: 1.4;\">"
            "      Откройте <b>Настройки</b> &rarr; <b>О телефоне</b>. Найдите <b>Номер сборки</b> и нажмите на него <b>7 раз</b> подряд до появления уведомления."
            "    </p>"
            "  </div>"
            "  <div style=\"background-color: #21242A; border: 1px solid #2F333B; border-radius: 8px; padding: 12px 14px; margin-bottom: 10px;\">"
            "    <span style=\"background: #0078D4; color: white; border-radius: 12px; padding: 2px 9px; font-weight: bold; font-size: 12px;\">2</span>"
            "    <strong style=\"color: #FFFFFF; font-size: 13px; margin-left: 6px;\">Включите отладку по USB</strong>"
            "    <p style=\"margin: 6px 0 0 26px; color: #9CA3AF; font-size: 12px; line-height: 1.4;\">"
            "      Перейдите в <b>Настройки</b> &rarr; <b>Для разработчиков</b> и активируйте переключатель <b>Отладка по USB</b>."
            "    </p>"
            "  </div>"
            "  <div style=\"background-color: #21242A; border: 1px solid #2F333B; border-radius: 8px; padding: 12px 14px; margin-bottom: 10px;\">"
            "    <span style=\"background: #0078D4; color: white; border-radius: 12px; padding: 2px 9px; font-weight: bold; font-size: 12px;\">3</span>"
            "    <strong style=\"color: #FFFFFF; font-size: 13px; margin-left: 6px;\">Подключите кабель к ПК</strong>"
            "    <p style=\"margin: 6px 0 0 26px; color: #9CA3AF; font-size: 12px; line-height: 1.4;\">"
            "      Соедините устройство кабелем. На экране телефона появится запрос &mdash; подтвердите <b>«Всегда разрешать с этого компьютера»</b>."
            "    </p>"
            "  </div>"
            "</div>"
            "</body></html>"
        );
    }
    if(ui->label_3)
    {
        ui->label_3->setText("<a style=\"color: #4CC2FF; text-decoration: none; font-size: 12px;\" href=\"https://www.anymp4.com/ru/faq/enable-usb-debugging-for-android.html\">📖 Подробная пошаговая инструкция с иллюстрациями</a>");
    }
    if(ui->label_5)
    {
        ui->label_5->setStyleSheet(
            "background-color: #172338;"
            "border: 1px solid #1E3A5F;"
            "border-radius: 8px;"
            "color: #38BDF8;"
            "font-size: 13px;"
            "font-weight: 600;"
            "padding: 12px;"
        );
        ui->label_5->setText("⏳ Ожидание подключения Android-устройства по USB...");
    }

    // ==========================================
    // 4. Procedures & Scan Execution Page (page_adsmalware)
    // ==========================================
    if(ui->deviceLabelName)
    {
        ui->deviceLabelName->setStyleSheet(
            "background-color: #1E2229;"
            "border: 1px solid #2E333D;"
            "border-radius: 8px;"
            "color: #4CC2FF;"
            "font-size: 14px;"
            "font-weight: bold;"
            "font-style: normal;"
            "padding: 8px 16px;"
        );
    }
    if(ui->processLogStatus)
    {
        ui->processLogStatus->setStyleSheet(
            "QListView {"
            "   background-color: #121316;"
            "   border: 1px solid #282A2E;"
            "   border-radius: 8px;"
            "   color: #9CA3AF;"
            "   font-family: 'Consolas', 'DejaVu Sans Mono', 'Courier New', monospace;"
            "   font-size: 11px;"
            "   padding: 8px;"
            "}"
            "QListView::item:selected {"
            "   background-color: #26292F;"
            "   color: #4CC2FF;"
            "}"
        );
    }
    if(ui->processBarStatus)
    {
        ui->processBarStatus->setStyleSheet(
            "QProgressBar {"
            "   background-color: #1A1C20;"
            "   border: 1px solid #2E3238;"
            "   border-radius: 5px;"
            "   height: 16px;"
            "   text-align: center;"
            "   color: #FFFFFF;"
            "   font-size: 11px;"
            "   font-weight: bold;"
            "}"
            "QProgressBar::chunk {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0078D4, stop:1 #4CC2FF);"
            "   border-radius: 4px;"
            "}"
        );
    }
    if(ui->malwareStatusText0)
    {
        ui->malwareStatusText0->setStyleSheet(
            "color: #E5E7EB;"
            "font-size: 14px;"
            "font-weight: 600;"
            "padding: 6px;"
            "font-style: normal;"
            "text-decoration: none;"
        );
    }
    if(ui->malwareReRun)
    {
        ui->malwareReRun->setCursor(Qt::PointingHandCursor);
        ui->malwareReRun->setStyleSheet(
            "QPushButton {"
            "   background-color: #0078D4;"
            "   border: 1px solid #005A9E;"
            "   border-radius: 8px;"
            "   color: #FFFFFF;"
            "   font-size: 13px;"
            "   font-weight: bold;"
            "   padding: 10px 20px;"
            "}"
            "QPushButton:hover {"
            "   background-color: #1084D9;"
            "   border-color: #4CC2FF;"
            "}"
            "QPushButton:pressed {"
            "   background-color: #004C87;"
            "}"
        );
    }

    // ==========================================
    // 5. Loader Page (page_loader)
    // ==========================================
    if(ui->frame_loader)
    {
        ui->frame_loader->setStyleSheet(
            "QFrame#frame_loader {"
            "   background-color: #1E2024;"
            "   border: 1px solid #2D3139;"
            "   border-radius: 12px;"
            "}"
        );
    }
    if(ui->loaderPageText)
    {
        ui->loaderPageText->setStyleSheet(
            "color: #9CA3AF;"
            "font-size: 14px;"
            "font-weight: 600;"
            "letter-spacing: 0.5px;"
        );
    }

    // ==========================================
    // 6. My Devices & Warranty Page (page_mydevices)
    // ==========================================
    if(ui->label_9)
    {
        ui->label_9->setStyleSheet("color: #9CA3AF; font-size: 12px; font-weight: 600;");
    }
    if(ui->label_10)
    {
        ui->label_10->setStyleSheet("color: #9CA3AF; font-size: 12px; font-weight: 600;");
    }
    if(ui->myDeviceFilterDateStart)
    {
        ui->myDeviceFilterDateStart->setStyleSheet(
            "QDateEdit {"
            "   background-color: #1E2228;"
            "   border: 1px solid #363A42;"
            "   border-radius: 6px;"
            "   color: #FFFFFF;"
            "   font-size: 12px;"
            "   padding: 4px 8px;"
            "}"
            "QDateEdit:focus {"
            "   border: 1px solid #4CC2FF;"
            "}"
        );
    }
    if(ui->myDeviceFilterDateEnd)
    {
        ui->myDeviceFilterDateEnd->setStyleSheet(
            "QDateEdit {"
            "   background-color: #1E2228;"
            "   border: 1px solid #363A42;"
            "   border-radius: 6px;"
            "   color: #FFFFFF;"
            "   font-size: 12px;"
            "   padding: 4px 8px;"
            "}"
            "QDateEdit:focus {"
            "   border: 1px solid #4CC2FF;"
            "}"
        );
    }
    if(ui->myDeviceQuaranteeFilter)
    {
        ui->myDeviceQuaranteeFilter->setCursor(Qt::PointingHandCursor);
        ui->myDeviceQuaranteeFilter->setStyleSheet(
            "QCheckBox {"
            "   color: #D1D5DB;"
            "   font-size: 12px;"
            "   font-weight: 500;"
            "   spacing: 6px;"
            "}"
            "QCheckBox::indicator {"
            "   width: 16px;"
            "   height: 16px;"
            "   border: 1px solid #3E434D;"
            "   border-radius: 4px;"
            "   background-color: #1E2024;"
            "}"
            "QCheckBox::indicator:hover {"
            "   border-color: #4CC2FF;"
            "}"
            "QCheckBox::indicator:checked {"
            "   background-color: #0078D4;"
            "   border-color: #0078D4;"
            "}"
        );
    }
    if(ui->myDeviceSend)
    {
        ui->myDeviceSend->setCursor(Qt::PointingHandCursor);
        ui->myDeviceSend->setStyleSheet(
            "QPushButton {"
            "   background-color: #0078D4;"
            "   border: 1px solid #005A9E;"
            "   border-radius: 6px;"
            "   color: #FFFFFF;"
            "   font-size: 12px;"
            "   font-weight: 600;"
            "   padding: 5px 16px;"
            "}"
            "QPushButton:hover {"
            "   background-color: #1084D9;"
            "   border-color: #4CC2FF;"
            "}"
            "QPushButton:pressed {"
            "   background-color: #004C87;"
            "}"
        );
    }
    if(ui->myDeviceActual)
    {
        ui->myDeviceActual->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        ui->myDeviceActual->horizontalHeader()->setStretchLastSection(true);
        ui->myDeviceActual->setStyleSheet(
            "QTableView {"
            "   background-color: #1A1C20;"
            "   alternate-background-color: #16181B;"
            "   gridline-color: #282A2E;"
            "   border: 1px solid #282A2E;"
            "   border-radius: 8px;"
            "   color: #D1D5DB;"
            "   font-size: 12px;"
            "   selection-background-color: #0078D4;"
            "   selection-color: #FFFFFF;"
            "}"
            "QHeaderView::section {"
            "   background-color: #22252B;"
            "   color: #9CA3AF;"
            "   font-size: 12px;"
            "   font-weight: bold;"
            "   border: none;"
            "   border-bottom: 1px solid #2E3238;"
            "   border-right: 1px solid #282A2E;"
            "   padding: 6px 8px;"
            "}"
        );
    }
    if(ui->myDevicePageLabel)
    {
        ui->myDevicePageLabel->setStyleSheet("color: #6B7280; font-size: 11px; padding: 4px;");
    }

    // ==========================================
    // 7. VIP Subscription Page (page_buyvip)
    // ==========================================
    if(ui->groupBox)
    {
        ui->groupBox->setStyleSheet(
            "QGroupBox {"
            "   background-color: #1E2024;"
            "   border: 1px solid #2D3139;"
            "   border-radius: 12px;"
            "   margin-top: 24px;"
            "   padding: 24px 20px 20px 20px;"
            "   font-size: 15px;"
            "   font-weight: bold;"
            "   color: #FFFFFF;"
            "}"
            "QGroupBox::title {"
            "   subcontrol-origin: margin;"
            "   subcontrol-position: top center;"
            "   padding: 4px 16px;"
            "   background-color: #26292F;"
            "   border: 1px solid #363940;"
            "   border-radius: 6px;"
            "   color: #FFFFFF;"
            "}"
        );
    }
    if(ui->label_13)
    {
        ui->label_13->setStyleSheet("color: #9CA3AF; font-size: 13px; font-weight: 500; margin-bottom: 4px;");
    }
    if(ui->labelVipBalance)
    {
        ui->labelVipBalance->setStyleSheet(
            "background-color: #13271D;"
            "border: 1px solid #16532E;"
            "border-radius: 8px;"
            "color: #4ADE80;"
            "font-size: 14px;"
            "font-weight: bold;"
            "padding: 10px 14px;"
        );
    }
    if(ui->comboBoxSelectVIPDays)
    {
        ui->comboBoxSelectVIPDays->setStyleSheet(
            "QComboBox {"
            "   background-color: #18191C;"
            "   border: 1px solid #32353B;"
            "   border-radius: 6px;"
            "   color: #FFFFFF;"
            "   font-size: 13px;"
            "   padding: 6px 12px;"
            "   min-height: 24px;"
            "}"
            "QComboBox:hover {"
            "   border-color: #4CC2FF;"
            "}"
            "QComboBox::drop-down {"
            "   border: none;"
            "   width: 24px;"
            "}"
            "QComboBox QAbstractItemView {"
            "   background-color: #1E2024;"
            "   border: 1px solid #32353B;"
            "   selection-background-color: #0078D4;"
            "   selection-color: #FFFFFF;"
            "   color: #FFFFFF;"
            "   padding: 4px;"
            "}"
        );
    }
    if(ui->frame)
    {
        ui->frame->setStyleSheet(
            "QFrame#frame {"
            "   background-color: #18191C;"
            "   border: 1px solid #2A2D33;"
            "   border-radius: 8px;"
            "   padding: 10px;"
            "}"
        );
    }
    if(ui->labelInfoVip)
    {
        ui->labelInfoVip->setStyleSheet("color: #F3F4F6; font-size: 13px; font-weight: 600;");
    }
    if(ui->buttonBuyVip)
    {
        ui->buttonBuyVip->setCursor(Qt::PointingHandCursor);
        ui->buttonBuyVip->setStyleSheet(
            "QPushButton {"
            "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #10B981, stop:1 #059669);"
            "   border: 1px solid #059669;"
            "   border-radius: 6px;"
            "   color: #FFFFFF;"
            "   font-size: 13px;"
            "   font-weight: bold;"
            "   padding: 8px 20px;"
            "}"
            "QPushButton:hover {"
            "   background: #10B981;"
            "   border-color: #34D399;"
            "}"
            "QPushButton:pressed {"
            "   background: #047857;"
            "}"
        );
    }

    // ==========================================
    // 8. Top Back Bar (toplevel_backpage)
    // ==========================================
    if(ui->toplevel_backpage)
    {
        ui->toplevel_backpage->setStyleSheet(
            "QFrame#toplevel_backpage {"
            "   background-color: #1A1D21;"
            "   border-bottom: 1px solid #282B30;"
            "}"
        );
    }
    if(ui->buttonBackTo)
    {
        ui->buttonBackTo->setText("‹ Назад");
        ui->buttonBackTo->setCursor(Qt::PointingHandCursor);
        ui->buttonBackTo->setStyleSheet(
            "QPushButton {"
            "   background-color: #26292F;"
            "   border: 1px solid #363940;"
            "   border-radius: 6px;"
            "   color: #E5E7EB;"
            "   font-size: 12px;"
            "   font-weight: 600;"
            "   padding: 5px 14px;"
            "}"
            "QPushButton:hover {"
            "   background-color: #32363E;"
            "   border-color: #4CC2FF;"
            "   color: #4CC2FF;"
            "}"
            "QPushButton:pressed {"
            "   background-color: #1A1B1E;"
            "}"
        );
    }
    if(ui->label_8)
    {
        ui->label_8->setStyleSheet(
            "color: #FFFFFF;"
            "font-size: 14px;"
            "font-weight: bold;"
            "letter-spacing: 0.5px;"
            "font-style: normal;"
            "background: transparent;"
        );
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
