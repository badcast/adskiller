#include <functional>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <list>
#include <memory>

#include <QCloseEvent>
#include <QCheckBox>
#include <QDate>
#include <QDesktopServices>
#include <QEasingCurve>
#include <QFontDatabase>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStringListModel>
#include <QTableView>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QRandomGenerator>

#include "PurchaseConfirmDialog.h"

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "AdbDeviceVisualizer.h"
#include "AIChatView.h"
#include "Snowflake.h"
#include "Strings.h"
#include "Services.h"
#include "ProgressCircle.h"

constexpr struct
{
    PageIndex index;
    const char *widgetName;
} PageConstNames[LengthPages] = {
    {AuthPage, "page_auth"},
    {CabinetPage, "page_cabinet"},
    {LongInfoPage, "page_adsmalware"},
    {LoaderPage, "page_loader"},
    {DevicesPage, "page_devices"},
    {MyDevicesPage, "page_mydevices"},
    {BuyVIPPage, "page_buyvip"}
};

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

    class CapsuleFocusFilter : public QObject
    {
    public:
        explicit CapsuleFocusFilter(QWidget *capsule) : QObject(capsule), m_capsule(capsule)
        {
        }

    protected:
        bool eventFilter(QObject *obj, QEvent *event) override
        {
            if(!m_capsule)
                return QObject::eventFilter(obj, event);

            if(event->type() == QEvent::FocusIn)
            {
                m_capsule->setStyleSheet(
                    "QFrame#aiInputCapsule {"
                    "   background-color: #191B21;"
                    "   border: 1px solid #38BDF8;"
                    "   border-radius: 12px;"
                    "}");
            }
            else if(event->type() == QEvent::FocusOut)
            {
                m_capsule->setStyleSheet(
                    "QFrame#aiInputCapsule {"
                    "   background-color: #16181D;"
                    "   border: 1px solid #2B2F38;"
                    "   border-radius: 12px;"
                    "}");
            }
            return QObject::eventFilter(obj, event);
        }

    private:
        QWidget *m_capsule;
    };
} // namespace

void MainWindow::setupWindowLayoutAndAnim()
{
    // Refresh TabPages to Content widget (Selective)
    QList<QWidget *> _w;
    for(int x = 0; x < ui->tabWidget->count(); ++x)
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
    deviceLeftAnimator->setEasingCurve(QEasingCurve::InOutCubic);

    // Top header back to main page.
    ui->contentLayout->layout()->addWidget(ui->toplevel_backpage);

    for(int x = 0; x < _w.count(); ++x)
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

    // Font init
    int fontId = QFontDatabase::addApplicationFont(":/resources/font-DigitalNumbers");
    QStringList fontFamils = QFontDatabase::applicationFontFamilies(fontId);
    if(!fontFamils.isEmpty())
    {
        QString fontFamily = fontFamils.first();
        malwareProgressCircle->setStyleSheet(QString("QWidget { Font-family: '%1'; }").arg(fontFamily));
    }

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
}

void MainWindow::setupAiPanel()
{
    // Modern AI Panel configuration
    if(ui->aiToolBoxToggle && ui->aiToolBoxContainer)
    {
        if(ui->horizontalLayout_ai)
        {
            ui->horizontalLayout_ai->setContentsMargins(0, 0, 0, 0);
            ui->horizontalLayout_ai->setSpacing(2);
        }

        ui->aiToolBoxContainer->setStyleSheet(
            "QFrame#aiToolBoxContainer {"
            "   background-color: #141518;"
            "   border: none;"
            "}");

        // Dynamic toggle button styling lambda
        auto updateToggleButtonStyle = [this](bool expanded)
        {
            if(expanded)
            {
                ui->aiToolBoxToggle->setText(QString::fromUtf8("›"));
                ui->aiToolBoxToggle->setToolTip("Свернуть панель ИИ");
                ui->aiToolBoxToggle->setFixedWidth(18);
                ui->aiToolBoxToggle->setStyleSheet(
                    "QPushButton#aiToolBoxToggle {"
                    "   background: #181A20;"
                    "   color: #64748B;"
                    "   border: 1px solid #232730;"
                    "   border-radius: 4px;"
                    "   font-size: 14px;"
                    "   font-weight: bold;"
                    "   padding: 0px;"
                    "}"
                    "QPushButton#aiToolBoxToggle:hover {"
                    "   background: #222631;"
                    "   border-color: #38BDF8;"
                    "   color: #38BDF8;"
                    "}"
                    "QPushButton#aiToolBoxToggle:pressed {"
                    "   background: #121418;"
                    "}");
            }
            else
            {
                ui->aiToolBoxToggle->setText(QString::fromUtf8("И\nИ\n\n‹"));
                ui->aiToolBoxToggle->setToolTip("Развернуть панель AdsKiller AI");
                ui->aiToolBoxToggle->setFixedWidth(32);
                ui->aiToolBoxToggle->setStyleSheet(
                    "QPushButton#aiToolBoxToggle {"
                    "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #1E2330, stop:1 #141720);"
                    "   color: #38BDF8;"
                    "   border: 1px solid #2B3950;"
                    "   border-top-left-radius: 8px;"
                    "   border-bottom-left-radius: 8px;"
                    "   border-top-right-radius: 0px;"
                    "   border-bottom-right-radius: 0px;"
                    "   font-size: 11px;"
                    "   font-weight: bold;"
                    "   padding: 6px 0px;"
                    "}"
                    "QPushButton#aiToolBoxToggle:hover {"
                    "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #26334A, stop:1 #1A2233);"
                    "   border: 1px solid #38BDF8;"
                    "   color: #FFFFFF;"
                    "}"
                    "QPushButton#aiToolBoxToggle:pressed {"
                    "   background: #0F172A;"
                    "}");
            }
        };

        ui->aiToolBoxToggle->setCursor(Qt::PointingHandCursor);
        ui->aiToolBoxContainer->setFixedWidth(350);
        updateToggleButtonStyle(true);

        // Detach pages from legacy QToolBox and embed modern aiPanel into horizontalLayout_ai
        if(ui->aiToolBox)
        {
            while(ui->aiToolBox->count() > 0)
            {
                ui->aiToolBox->removeItem(0);
            }
            if(ui->horizontalLayout_ai)
            {
                ui->horizontalLayout_ai->removeWidget(ui->aiToolBox);
                ui->aiToolBox->hide();
            }
        }

        // Modern unified panel wrapper (replaces clunky QToolBox accordion)
        QWidget *aiPanel = new QWidget(ui->aiToolBoxContainer);
        aiPanel->setObjectName("aiMainPanel");
        aiPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        aiPanel->setMinimumHeight(520);
        aiPanel->setStyleSheet(
            "QWidget#aiMainPanel {"
            "   background-color: #141518;"
            "   border: 1px solid #23262E;"
            "   border-radius: 10px;"
            "}");

        if(ui->horizontalLayout_ai)
        {
            ui->horizontalLayout_ai->insertWidget(0, aiPanel, 1);
            ui->horizontalLayout_ai->setStretch(0, 1);
            ui->horizontalLayout_ai->setStretch(1, 0);
        }

        QVBoxLayout *aiPanelLayout = new QVBoxLayout(aiPanel);
        aiPanelLayout->setContentsMargins(0, 0, 0, 0);
        aiPanelLayout->setSpacing(0);

        // Top Header Bar with title, online indicator, and segmented tab pill switcher
        QWidget *aiHeaderBar = new QWidget(aiPanel);
        aiHeaderBar->setObjectName("aiHeaderBar");
        aiHeaderBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        aiHeaderBar->setFixedHeight(40);
        aiHeaderBar->setStyleSheet(
            "QWidget#aiHeaderBar {"
            "   background-color: #17191E;"
            "   border-top-left-radius: 9px;"
            "   border-top-right-radius: 9px;"
            "   border-bottom: 1px solid #23262E;"
            "}");

        QHBoxLayout *headerLayout = new QHBoxLayout(aiHeaderBar);
        headerLayout->setContentsMargins(10, 0, 6, 0);
        headerLayout->setSpacing(6);

        QLabel *aiTitle = new QLabel(aiHeaderBar);
        aiTitle->setText("<b>AdsKiller AI</b>");
        aiTitle->setStyleSheet("color: #FFFFFF; font-size: 12px; font-weight: 600;");

        QLabel *aiStatus = new QLabel(aiHeaderBar);
        aiStatus->setText(QString::fromUtf8("●"));
        aiStatus->setToolTip("Ассистент активен");
        aiStatus->setStyleSheet("color: #10B981; font-size: 8px; margin-top: 1px;");

        headerLayout->addWidget(aiTitle);
        headerLayout->addWidget(aiStatus);
        headerLayout->addStretch(1);

        QFrame *segmentedBar = new QFrame(aiHeaderBar);
        segmentedBar->setObjectName("aiSegmentedBar");
        segmentedBar->setFixedHeight(26);
        segmentedBar->setStyleSheet(
            "QFrame#aiSegmentedBar {"
            "   background-color: #1E2128;"
            "   border: 1px solid #2B2F38;"
            "   border-radius: 6px;"
            "}");
        QHBoxLayout *segLayout = new QHBoxLayout(segmentedBar);
        segLayout->setContentsMargins(2, 2, 2, 2);
        segLayout->setSpacing(2);

        QPushButton *tabChatBtn = new QPushButton("💬 Чат", segmentedBar);
        QPushButton *tabInfoBtn = new QPushButton("ℹ Инфо", segmentedBar);

        const QString segBtnStyle = "QPushButton {"
                                    "   background: transparent;"
                                    "   color: #8E9297;"
                                    "   border: none;"
                                    "   border-radius: 4px;"
                                    "   padding: 2px 7px;"
                                    "   font-size: 10.5px;"
                                    "   font-weight: 500;"
                                    "}"
                                    "QPushButton:hover {"
                                    "   color: #FFFFFF;"
                                    "   background: rgba(255, 255, 255, 0.05);"
                                    "}"
                                    "QPushButton:checked {"
                                    "   background: #2D3340;"
                                    "   color: #38BDF8;"
                                    "   font-weight: bold;"
                                    "}";

        tabChatBtn->setStyleSheet(segBtnStyle);
        tabInfoBtn->setStyleSheet(segBtnStyle);
        tabChatBtn->setCheckable(true);
        tabInfoBtn->setCheckable(true);
        tabChatBtn->setChecked(true);
        tabChatBtn->setCursor(Qt::PointingHandCursor);
        tabInfoBtn->setCursor(Qt::PointingHandCursor);

        segLayout->addWidget(tabChatBtn);
        segLayout->addWidget(tabInfoBtn);
        headerLayout->addWidget(segmentedBar);

        QPushButton *headerCollapseBtn = new QPushButton(QString::fromUtf8("✕"), aiHeaderBar);
        headerCollapseBtn->setToolTip("Свернуть панель ИИ");
        headerCollapseBtn->setFixedSize(22, 22);
        headerCollapseBtn->setCursor(Qt::PointingHandCursor);
        headerCollapseBtn->setStyleSheet(
            "QPushButton {"
            "   background: transparent;"
            "   color: #64748B;"
            "   border: none;"
            "   border-radius: 4px;"
            "   font-size: 11px;"
            "   font-weight: bold;"
            "}"
            "QPushButton:hover {"
            "   background: rgba(239, 68, 68, 0.15);"
            "   color: #F87171;"
            "}"
            "QPushButton:pressed {"
            "   background: rgba(239, 68, 68, 0.25);"
            "}");
        headerLayout->addWidget(headerCollapseBtn);
        QObject::connect(headerCollapseBtn, &QPushButton::clicked, ui->aiToolBoxToggle, &QPushButton::click);

        aiPanelLayout->addWidget(aiHeaderBar);

        // QStackedWidget for switching between Chat and Info
        QStackedWidget *aiStack = new QStackedWidget(aiPanel);
        aiStack->setObjectName("aiStack");
        aiStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        aiStack->setStyleSheet("QStackedWidget#aiStack { background: transparent; border: none; }");

        aiStack->addWidget(ui->aiToolBoxPage1);
        aiStack->addWidget(ui->aiToolBoxPage2);
        aiPanelLayout->addWidget(aiStack, 1);

        QObject::connect(
            tabChatBtn,
            &QPushButton::clicked,
            this,
            [aiStack, tabChatBtn, tabInfoBtn]()
            {
                aiStack->setCurrentIndex(0);
                tabChatBtn->setChecked(true);
                tabInfoBtn->setChecked(false);
            });
        QObject::connect(
            tabInfoBtn,
            &QPushButton::clicked,
            this,
            [aiStack, tabChatBtn, tabInfoBtn]()
            {
                aiStack->setCurrentIndex(1);
                tabChatBtn->setChecked(false);
                tabInfoBtn->setChecked(true);
            });

        // Collapse / Expand toggle logic
        QObject::connect(
            ui->aiToolBoxToggle,
            &QPushButton::clicked,
            this,
            [this, aiPanel, updateToggleButtonStyle]()
            {
                if(aiPanel->isVisible())
                {
                    aiPanel->setVisible(false);
                    updateToggleButtonStyle(false);
                    ui->aiToolBoxContainer->setFixedWidth(36);
                }
                else
                {
                    aiPanel->setVisible(true);
                    updateToggleButtonStyle(true);
                    ui->aiToolBoxContainer->setFixedWidth(350);
                }
            });

        // Setup Page 2: About AI info
        ui->aiToolBoxPage2->setStyleSheet("QWidget#aiToolBoxPage2 { background-color: #141518; border: none; }");
        if(ui->aboutAi_edit)
        {
            ui->aboutAi_edit->setStyleSheet(
                "QTextEdit#aboutAi_edit {"
                "   background-color: #141518;"
                "   color: #D1D5DB;"
                "   border: none;"
                "   padding: 14px 14px;"
                "   font-size: 11px;"
                "}");
            ui->aboutAi_edit->setHtml(
                "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">"
                "<html><head><meta name=\"qrichtext\" content=\"1\" /><meta charset=\"utf-8\" />"
                "<style type=\"text/css\">p, li { white-space: pre-wrap; line-height: 1.5; }</style></head>"
                "<body style=\"font-family:'Segoe UI', 'Noto Sans', sans-serif; font-size:10pt; color:#D1D5DB;\">"
                "<div style=\"text-align:center; padding:12px 0 8px 0;\">"
                "<span style=\"font-size:30px;\">🤖</span><br/>"
                "<b style=\"color:#38BDF8; font-size:13pt;\">AdsKiller AI Assistant</b><br/>"
                "<span style=\"color:#8E9297; font-size:9pt;\">Интеллектуальный помощник</span><br/>"
                "<span style=\"display:inline-block; margin-top:6px; background-color:#1E293B; color:#38BDF8; font-size:8.5pt; font-weight:600; padding:2px 8px; border-radius:10px;\">● В сети</span>"
                "</div>"
                "<hr style=\"border:none; border-top:1px solid #252830; margin:10px 0;\"/>"
                "<p style=\"font-size:9.5pt;\">"
                "<b style=\"color:#FFFFFF;\">Возможности модуля:</b><br/>"
                "&nbsp;• Диагностика и блокировка рекламы<br/>"
                "&nbsp;• Управление подключенными устройствами<br/>"
                "&nbsp;• Проверка статуса подписки и кредитов<br/>"
                "&nbsp;• Быстрые ответы и оптимизация ОС"
                "</p>"
                "<hr style=\"border:none; border-top:1px solid #252830; margin:10px 0;\"/>"
                "<p style=\"font-size:9.5pt;\">"
                "<b style=\"color:#FFFFFF;\">Автор модуля ИИ:</b><br/>"
                "&nbsp;&nbsp;Команда <span style=\"color:#38BDF8;\">imister.tech</span><br/><br/>"
                "<b style=\"color:#FFFFFF;\">Разработчик:</b><br/>"
                "&nbsp;&nbsp;Нурсеит К. (<span style=\"color:#38BDF8;\">badcast</span>)<br/><br/>"
                "<b style=\"color:#FFFFFF;\">Дизайн:</b><br/>"
                "&nbsp;&nbsp;Владимир (<span style=\"color:#38BDF8;\">LeoJames</span>)"
                "</p>"
                "</body></html>");
        }

        // Setup Page 1: Chat interface
        ui->aiToolBoxPage1->setStyleSheet("QWidget#aiToolBoxPage1 { background-color: #141518; border: none; }");
        ui->aiToolBoxPage1->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        // Clean out legacy designer layout immediately before creating Page 1 widgets
        QLayout *oldPage1Layout = ui->aiToolBoxPage1->layout();
        if(oldPage1Layout)
        {
            QLayoutItem *item;
            while((item = oldPage1Layout->takeAt(0)) != nullptr)
            {
                delete item;
            }
            delete oldPage1Layout;
        }

        QVBoxLayout *page1Layout = new QVBoxLayout(ui->aiToolBoxPage1);
        page1Layout->setContentsMargins(6, 6, 6, 6);
        page1Layout->setSpacing(6);

        // Create custom widget-based AIChatView
        AIChatView *chatView = new AIChatView(ui->aiToolBoxPage1);
        chatView->setObjectName("aiChatView");
        chatView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        chatView->setMinimumWidth(0);
        chatView->setMaximumWidth(16777215);
        chatView->setMinimumHeight(300);
        chatView->setMaximumHeight(16777215);
        page1Layout->addWidget(chatView, 1);
        ui->aiChatMessages->hide();
        chatView->showLocked();

        // Quick suggestions single-row carousel + shuffle button
        QWidget *quickBarWidget = new QWidget(ui->aiToolBoxPage1);
        quickBarWidget->setStyleSheet("background: transparent;");
        quickBarWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        quickBarWidget->setFixedHeight(38);
        page1Layout->addWidget(quickBarWidget, 0);
        QHBoxLayout *quickBarLayout = new QHBoxLayout(quickBarWidget);
        quickBarLayout->setContentsMargins(0, 2, 0, 2);
        quickBarLayout->setSpacing(4);

        QPushButton *shuffleBtn = new QPushButton(QString::fromUtf8("🔀"), quickBarWidget);
        shuffleBtn->setToolTip("Перемешать подсказки");
        shuffleBtn->setFixedSize(26, 26);
        shuffleBtn->setCursor(Qt::PointingHandCursor);
        shuffleBtn->setStyleSheet(
            "QPushButton {"
            "   background: #1C1E24;"
            "   color: #8E9297;"
            "   border: 1px solid #2B2F38;"
            "   border-radius: 13px;"
            "   font-size: 11px;"
            "   padding: 0px;"
            "}"
            "QPushButton:hover {"
            "   background: #252A34;"
            "   border-color: #38BDF8;"
            "   color: #38BDF8;"
            "}"
            "QPushButton:pressed {"
            "   background: #131417;"
            "}");

        QScrollArea *scrollArea = new QScrollArea(quickBarWidget);
        scrollArea->setObjectName("aiQuickScrollArea");
        scrollArea->setWidgetResizable(true);
        scrollArea->setFixedHeight(34);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scrollArea->setStyleSheet(
            "QScrollArea#aiQuickScrollArea {"
            "   border: none;"
            "   background: transparent;"
            "}");

        scrollArea->viewport()->installEventFilter(new HorizontalWheelFilter(scrollArea));
        scrollArea->installEventFilter(new HorizontalWheelFilter(scrollArea));

        QWidget *quickButtonsWidget = new QWidget(scrollArea);
        quickButtonsWidget->setStyleSheet("background: transparent;");
        QHBoxLayout *quickButtonsLayout = new QHBoxLayout(quickButtonsWidget);
        quickButtonsLayout->setContentsMargins(0, 2, 0, 2);
        quickButtonsLayout->setSpacing(5);

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

        auto populateRandomButtons = [this, quickButtonsWidget, quickButtonsLayout, scrollArea, questionsPtr]()
        {
            // Clear existing buttons from layout
            QLayoutItem *child;
            while((child = quickButtonsLayout->takeAt(0)) != nullptr)
            {
                if(child->widget())
                    delete child->widget();
                delete child;
            }

            // Shuffle questions randomly
            std::shuffle(questionsPtr->begin(), questionsPtr->end(), *QRandomGenerator::global());

            for(int i = 0; i < questionsPtr->size(); ++i)
            {
                const auto &qData = (*questionsPtr)[i];
                int initialIdx = QRandomGenerator::global()->bounded(qData.variations.size());
                QString initialText = qData.variations[initialIdx];

                QPushButton *btn = new QPushButton(QString("%1 %2").arg(qData.icon, initialText), quickButtonsWidget);
                btn->setFixedHeight(26);
                btn->setStyleSheet(
                    "QPushButton {"
                    "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #22252C, stop:1 #1A1C22);"
                    "   color: #D1D5DB;"
                    "   border: 1px solid #2F333E;"
                    "   border-radius: 13px;"
                    "   padding: 2px 10px;"
                    "   font-size: 11px;"
                    "   font-weight: 500;"
                    "}"
                    "QPushButton:hover {"
                    "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #2A303C, stop:1 #202630);"
                    "   border: 1px solid #38BDF8;"
                    "   color: #38BDF8;"
                    "}"
                    "QPushButton:pressed {"
                    "   background-color: #141518;"
                    "   color: #FFFFFF;"
                    "}");
                btn->setCursor(Qt::PointingHandCursor);
                quickButtonsLayout->addWidget(btn);

                QObject::connect(
                    btn,
                    &QPushButton::clicked,
                    this,
                    [this, btn, icon = qData.icon, variations = qData.variations, lastIdx = initialIdx]() mutable
                    {
                        if(!ui->aiChatSend->isEnabled())
                            return;
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
                            btn->setText(QString("%1 %2").arg(icon, variations[r]));
                        }
                    });
            }
            if(scrollArea->horizontalScrollBar())
                scrollArea->horizontalScrollBar()->setValue(0);
        };

        populateRandomButtons();
        scrollArea->setWidget(quickButtonsWidget);

        QObject::connect(shuffleBtn, &QPushButton::clicked, this, populateRandomButtons);

        quickBarLayout->addWidget(shuffleBtn, 0);
        quickBarLayout->addWidget(scrollArea, 1);

        // Integrated modern input capsule
        QFrame *inputCapsule = new QFrame(ui->aiToolBoxPage1);
        inputCapsule->setObjectName("aiInputCapsule");
        inputCapsule->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        inputCapsule->setMinimumHeight(44);
        inputCapsule->setMaximumHeight(56);
        inputCapsule->setStyleSheet(
            "QFrame#aiInputCapsule {"
            "   background-color: #16181D;"
            "   border: 1px solid #2B2F38;"
            "   border-radius: 12px;"
            "}");

        QHBoxLayout *capsuleLayout = new QHBoxLayout(inputCapsule);
        capsuleLayout->setContentsMargins(10, 4, 6, 4);
        capsuleLayout->setSpacing(6);

        ui->aiChatEdit->setStyleSheet(
            "QTextEdit#aiChatEdit {"
            "   background: transparent;"
            "   color: #F3F4F6;"
            "   border: none;"
            "   padding: 4px 2px;"
            "   font-size: 11.5px;"
            "   selection-background-color: #0078D4;"
            "}");
        ui->aiChatEdit->setPlaceholderText("Спросите у AdsKiller AI...");
        ui->aiChatEdit->setMinimumHeight(32);
        ui->aiChatEdit->setMaximumHeight(48);
        ui->aiChatEdit->installEventFilter(new CapsuleFocusFilter(inputCapsule));

        ui->aiChatSend->setStyleSheet(
            "QPushButton#aiChatSend {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #0078D4, stop:1 #005A9E);"
            "   color: #FFFFFF;"
            "   border: none;"
            "   border-radius: 15px;"
            "   font-size: 13px;"
            "   font-weight: bold;"
            "}"
            "QPushButton#aiChatSend:hover {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #1088E8, stop:1 #0066BA);"
            "}"
            "QPushButton#aiChatSend:pressed {"
            "   background: #004D80;"
            "}"
            "QPushButton#aiChatSend:disabled {"
            "   background: #23262E;"
            "   color: #4B515D;"
            "}");
        ui->aiChatSend->setFixedSize(30, 30);
        ui->aiChatSend->setCursor(Qt::PointingHandCursor);
        ui->aiChatSend->setText(QString::fromUtf8("➤"));
        ui->aiChatSend->setToolTip("Отправить сообщение (Enter)");

        capsuleLayout->addWidget(ui->aiChatEdit, 1);
        capsuleLayout->addWidget(ui->aiChatSend, 0, Qt::AlignVCenter);

        page1Layout->addWidget(inputCapsule, 0);

        // Activate container layout and ensure all components are visible
        if(ui->aiToolBoxContainer->layout())
            ui->aiToolBoxContainer->layout()->activate();

        // Explicitly show all components to ensure nothing remains hidden
        chatView->show();
        quickBarWidget->show();
        inputCapsule->show();
        ui->aiToolBoxPage1->show();
        aiStack->show();
        aiPanel->show();
    }
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
        "}");

    // ==========================================
    // 1. Auth Page (page_auth)
    // ==========================================
    if(ui->page_auth)
    {
        ui->page_auth->setAttribute(Qt::WA_StyledBackground, true);
        ui->page_auth->setStyleSheet(
            "QWidget#page_auth {"
            "   background: qradialgradient(cx:0.5, cy:0.45, radius:0.8, fx:0.5, fy:0.4, "
            "       stop:0 #111A2E, stop:0.55 #0A0E1A, stop:1 #04060A);"
            "}");
    }
    if(ui->gridLayout_asd2)
    {
        ui->gridLayout_asd2->setAlignment(Qt::AlignCenter);
    }

    if(ui->frame_4)
    {
        ui->frame_4->setAttribute(Qt::WA_StyledBackground, true);
        ui->frame_4->setFixedSize(440, 610);
        ui->frame_4->setStyleSheet(
            "QFrame#frame_4 {"
            "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #141B2D, stop:1 #0B0F19);"
            "   border: 1px solid rgba(56, 189, 248, 0.28);"
            "   border-radius: 20px;"
            "}");
    }

    if(ui->mainapplogo)
    {
        ui->mainapplogo->setFixedSize(68, 68);
        QString logoUrl = (QDate::currentDate().month() == 12 || QDate::currentDate().month() == 1) ? ":/resources/app-logo-merry" : ":/resources/app-logo";
        ui->mainapplogo->setStyleSheet(QString(
                                           "QFrame#mainapplogo {"
                                           "   image: url(%1);"
                                           "   background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #1E293B, stop:1 #0F172A);"
                                           "   border: 1.5px solid rgba(56, 189, 248, 0.35);"
                                           "   border-radius: 20px;"
                                           "   padding: 10px;"
                                           "}")
                                           .arg(logoUrl));
    }

    if(ui->label_4)
    {
        ui->label_4->setTextFormat(Qt::RichText);
        ui->label_4->setAlignment(Qt::AlignCenter);
        ui->label_4->setStyleSheet("background: transparent;");
        ui->label_4->setText(
            "<div align='center'>"
            "<span style='font-size: 20px; font-weight: 700; color: #F8FAFC; letter-spacing: 0.5px;'>AdsKiller Desktop</span><br>"
            "<span style='font-size: 13px; font-weight: 400; color: #94A3B8;'>Авторизуйтесь для доступа к сервисам</span>"
            "</div>");
    }

    if(ui->label_12)
    {
        ui->label_12->setText("ЛОГИН или EMAIL");
        ui->label_12->setStyleSheet("color: #94A3B8; font-size: 11px; font-weight: 700; letter-spacing: 0.8px; background: transparent;");
    }

    if(ui->lineLoginEdit)
    {
        ui->lineLoginEdit->setFixedHeight(42);
        ui->lineLoginEdit->setPlaceholderText("Логин или электронная почта");
        ui->lineLoginEdit->setStyleSheet(
            "QLineEdit {"
            "   background-color: #0F172A;"
            "   color: #F8FAFC;"
            "   border: 1.5px solid #1E293B;"
            "   border-radius: 10px;"
            "   padding: 0 14px;"
            "   font-size: 13px;"
            "   selection-background-color: #0284C7;"
            "}"
            "QLineEdit:hover {"
            "   border: 1.5px solid #334155;"
            "   background-color: #131E35;"
            "}"
            "QLineEdit:focus {"
            "   border: 1.5px solid #38BDF8;"
            "   background-color: #0F172A;"
            "}"
            "QLineEdit:disabled {"
            "   background-color: #0B101D;"
            "   color: #475569;"
            "   border-color: #1E293B;"
            "}");
    }

    if(ui->label_14)
    {
        ui->label_14->setText("ПАРОЛЬ");
        ui->label_14->setStyleSheet("color: #94A3B8; font-size: 11px; font-weight: 700; letter-spacing: 0.8px; background: transparent;");
    }

    if(ui->linePassEdit)
    {
        ui->linePassEdit->setFixedHeight(42);
        ui->linePassEdit->setPlaceholderText("Пароль");
        ui->linePassEdit->setStyleSheet(
            "QLineEdit {"
            "   background-color: #0F172A;"
            "   color: #F8FAFC;"
            "   border: 1.5px solid #1E293B;"
            "   border-radius: 10px;"
            "   padding: 0 14px;"
            "   font-size: 13px;"
            "   selection-background-color: #0284C7;"
            "}"
            "QLineEdit:hover {"
            "   border: 1.5px solid #334155;"
            "   background-color: #131E35;"
            "}"
            "QLineEdit:focus {"
            "   border: 1.5px solid #38BDF8;"
            "   background-color: #0F172A;"
            "}"
            "QLineEdit:disabled {"
            "   background-color: #0B101D;"
            "   color: #475569;"
            "   border-color: #1E293B;"
            "}");
    }

    if(ui->butShowPass)
    {
        ui->butShowPass->setText("👁");
        ui->butShowPass->setToolTip("Показать / скрыть пароль");
        ui->butShowPass->setCursor(Qt::PointingHandCursor);
        ui->butShowPass->setFixedSize(42, 42);
        ui->butShowPass->setStyleSheet(
            "QPushButton {"
            "   background-color: #0F172A;"
            "   color: #94A3B8;"
            "   border: 1.5px solid #1E293B;"
            "   border-radius: 10px;"
            "   font-size: 15px;"
            "}"
            "QPushButton:hover {"
            "   background-color: #1E293B;"
            "   border-color: #38BDF8;"
            "   color: #38BDF8;"
            "}"
            "QPushButton:pressed {"
            "   background-color: #0B101D;"
            "   border-color: #0284C7;"
            "}");
    }

    if(ui->checkAutoLogin)
    {
        ui->checkAutoLogin->setCursor(Qt::PointingHandCursor);
        ui->checkAutoLogin->setText("Войти при запуске");
        ui->checkAutoLogin->setStyleSheet(
            "QCheckBox {"
            "   color: #94A3B8;"
            "   font-size: 12px;"
            "   font-weight: 500;"
            "   spacing: 8px;"
            "   background: transparent;"
            "}"
            "QCheckBox:hover {"
            "   color: #E2E8F0;"
            "}"
            "QCheckBox::indicator {"
            "   width: 17px;"
            "   height: 17px;"
            "   border: 1.5px solid #334155;"
            "   border-radius: 5px;"
            "   background-color: #0F172A;"
            "}"
            "QCheckBox::indicator:hover {"
            "   border-color: #38BDF8;"
            "   background-color: #131E35;"
            "}"
            "QCheckBox::indicator:checked {"
            "   background-color: #0284C7;"
            "   border-color: #38BDF8;"
            "   image: url(:/resources/checkbox-checked);"
            "}");
    }

    if(ui->label_2)
    {
        ui->label_2->setCursor(Qt::PointingHandCursor);
        ui->label_2->setText("<a href=\"index\" style=\"color: #38BDF8; text-decoration: none;\">Забыли логин или пароль?</a>");
        ui->label_2->setStyleSheet("QLabel#label_2 { color: #38BDF8; font-size: 12px; font-weight: 500; background: transparent; }");
    }

    if(ui->authButton)
    {
        ui->authButton->setText("Войти в систему");
        ui->authButton->setFixedHeight(44);
        ui->authButton->setCursor(Qt::PointingHandCursor);
        ui->authButton->setStyleSheet(
            "QPushButton {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0284C7, stop:1 #0EA5E9);"
            "   color: #FFFFFF;"
            "   border: 1px solid rgba(56, 189, 248, 0.4);"
            "   border-radius: 10px;"
            "   font-size: 14px;"
            "   font-weight: 700;"
            "   letter-spacing: 0.5px;"
            "}"
            "QPushButton:hover {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0369A1, stop:1 #38BDF8);"
            "   border-color: #7DD3FC;"
            "}"
            "QPushButton:pressed {"
            "   background: #0284C7;"
            "   border-color: #0284C7;"
            "}"
            "QPushButton:disabled {"
            "   background-color: #1E293B;"
            "   border-color: #334155;"
            "   color: #64748B;"
            "}");
    }

    if(ui->statusAuthText)
    {
        ui->statusAuthText->setAlignment(Qt::AlignCenter);
        ui->statusAuthText->setWordWrap(true);
        ui->statusAuthText->setStyleSheet(
            "QLabel#statusAuthText {"
            "   color: #38BDF8;"
            "   font-size: 12px;"
            "   font-weight: 500;"
            "   background: transparent;"
            "   padding: 2px 8px;"
            "}");
    }

    if(ui->label_auth_ver)
    {
        ui->label_auth_ver->setAlignment(Qt::AlignCenter);
        ui->label_auth_ver->setStyleSheet("color: #475569; font-size: 11px; font-weight: 500; letter-spacing: 0.3px; background: transparent;");
        ui->label_auth_ver->setText("Версия: " + runtimeVersion.mVersion.toString());
    }

    // Assemble modern responsive layout for frame_4
    if(ui->frame_4 && !ui->frame_4->layout())
    {
        QVBoxLayout *cardLayout = new QVBoxLayout(ui->frame_4);
        cardLayout->setContentsMargins(36, 32, 36, 26);
        cardLayout->setSpacing(0);

        // 1. App logo badge
        if(ui->mainapplogo)
            cardLayout->addWidget(ui->mainapplogo, 0, Qt::AlignHCenter);

        cardLayout->addSpacing(14);

        // 2. Title and Subtitle
        if(ui->label_4)
            cardLayout->addWidget(ui->label_4);

        cardLayout->addSpacing(22);

        // 3. Login label
        if(ui->label_12)
            cardLayout->addWidget(ui->label_12);

        cardLayout->addSpacing(6);

        // 4. Login input
        if(ui->lineLoginEdit)
            cardLayout->addWidget(ui->lineLoginEdit);

        cardLayout->addSpacing(14);

        // 5. Password label
        if(ui->label_14)
            cardLayout->addWidget(ui->label_14);

        cardLayout->addSpacing(6);

        // 6. Password row (input + show pass button)
        QHBoxLayout *passRow = new QHBoxLayout();
        passRow->setContentsMargins(0, 0, 0, 0);
        passRow->setSpacing(8);
        if(ui->linePassEdit)
            passRow->addWidget(ui->linePassEdit, 1);
        if(ui->butShowPass)
            passRow->addWidget(ui->butShowPass, 0);
        cardLayout->addLayout(passRow);

        cardLayout->addSpacing(14);

        // 7. Options row (Auto login + Forgot token)
        QHBoxLayout *optRow = new QHBoxLayout();
        optRow->setContentsMargins(0, 0, 0, 0);
        if(ui->checkAutoLogin)
            optRow->addWidget(ui->checkAutoLogin, 0, Qt::AlignVCenter);
        optRow->addStretch(1);
        if(ui->label_2)
            optRow->addWidget(ui->label_2, 0, Qt::AlignVCenter);
        cardLayout->addLayout(optRow);

        cardLayout->addSpacing(16);

        // 8. Consent checkbox (Terms / Privacy / License)
        QCheckBox *checkConsent = new QCheckBox(ui->frame_4);
        checkConsent->setChecked(true);
        checkConsent->setCursor(Qt::PointingHandCursor);
        checkConsent->setStyleSheet(
            "QCheckBox {"
            "   color: #94A3B8;"
            "   font-size: 11px;"
            "   font-weight: 400;"
            "   spacing: 8px;"
            "   background: transparent;"
            "}"
            "QCheckBox:hover {"
            "   color: #E2E8F0;"
            "}"
            "QCheckBox::indicator {"
            "   width: 16px;"
            "   height: 16px;"
            "   border: 1.5px solid #334155;"
            "   border-radius: 4px;"
            "   background-color: #0F172A;"
            "}"
            "QCheckBox::indicator:hover {"
            "   border-color: #38BDF8;"
            "   background-color: #131E35;"
            "}"
            "QCheckBox::indicator:checked {"
            "   background-color: #0284C7;"
            "   border-color: #38BDF8;"
            "   image: url(:/resources/checkbox-checked);"
            "}");

        // Build rich-text label with clickable links
        QLabel *consentLabel = new QLabel(ui->frame_4);
        consentLabel->setTextFormat(Qt::RichText);
        consentLabel->setOpenExternalLinks(true);
        consentLabel->setWordWrap(true);
        consentLabel->setStyleSheet("background: transparent; font-size: 11px;");
        consentLabel->setText(
            "<span style='color:#94A3B8;'>"
            "Я согласен с "
            "<a href='https://adskiller.imister.tech#TERMS' style='color:#38BDF8; text-decoration:none;'>условиями соглашения</a>"
            ", "
            "<a href='https://adskiller.imister.tech#PRIVACY' style='color:#38BDF8; text-decoration:none;'>политикой конфиден.</a>"
            " и "
            "<a href='https://adskiller.imister.tech#LICENSE' style='color:#38BDF8; text-decoration:none;'>лицензией</a>"
            "</span>");

        QHBoxLayout *consentRow = new QHBoxLayout();
        consentRow->setContentsMargins(0, 0, 0, 0);
        consentRow->setSpacing(6);
        consentRow->addWidget(checkConsent, 0, Qt::AlignTop);
        consentRow->addWidget(consentLabel, 1);
        cardLayout->addLayout(consentRow);

        cardLayout->addSpacing(16);

        // 9. Sign In CTA button
        if(ui->authButton)
        {
            ui->authButton->setEnabled(false);
            cardLayout->addWidget(ui->authButton);
            connect(checkConsent, &QCheckBox::toggled, ui->authButton, &QPushButton::setEnabled);
        }

        cardLayout->addSpacing(10);

        // 10. Status text banner
        if(ui->statusAuthText)
            cardLayout->addWidget(ui->statusAuthText);

        cardLayout->addStretch(1);

        // 11. Version footer
        if(ui->label_auth_ver)
            cardLayout->addWidget(ui->label_auth_ver);

        // Connect Enter key navigation
        if(ui->lineLoginEdit && ui->linePassEdit)
            connect(ui->lineLoginEdit, &QLineEdit::returnPressed, [this]() { ui->linePassEdit->setFocus(); });
        if(ui->linePassEdit && ui->authButton)
            connect(ui->linePassEdit, &QLineEdit::returnPressed, ui->authButton, &QPushButton::click);
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
            "}");
    }
    if(ui->label_6)
    {
        ui->label_6->setStyleSheet(
            "color: #FFFFFF;"
            "font-size: 14px;"
            "font-weight: bold;"
            "letter-spacing: 0.5px;"
            "font-style: normal;"
            "background: transparent;");
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
            "}");
    }
    if(ui->authpageUpdate)
    {
        ui->authpageUpdate->setText(QString::fromUtf8("Обновить"));
        ui->authpageUpdate->setMinimumSize(105, 30);
        ui->authpageUpdate->setMaximumSize(125, 30);
        ui->authpageUpdate->setCursor(Qt::PointingHandCursor);
        ui->authpageUpdate->setToolTip(QString::fromUtf8("Обновить данные кабинета и доступность сервисов"));
        ui->authpageUpdate->setStyleSheet(
            "QPushButton {"
            "   background-color: #222630;"
            "   border: 1px solid #363D4E;"
            "   border-radius: 6px;"
            "   color: #38BDF8;"
            "   font-size: 12px;"
            "   font-weight: 600;"
            "   padding: 4px 12px;"
            "}"
            "QPushButton:hover {"
            "   background-color: #2D3342;"
            "   border-color: #38BDF8;"
            "   color: #FFFFFF;"
            "}"
            "QPushButton:pressed {"
            "   background-color: #171A21;"
            "   border-color: #0284C7;"
            "}");
    }
    if(ui->frame_7)
    {
        ui->frame_7->setMaximumSize(16777215, 16777215);
        ui->frame_7->setMinimumHeight(140);
        ui->frame_7->setStyleSheet(
            "QFrame#frame_7 {"
            "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #1C1E24, stop:1 #131519);"
            "   border: 1px solid #2B2E36;"
            "   border-radius: 14px;"
            "}");

        if(!ui->frame_7->layout() && ui->frame_6 && ui->authedMainWin && ui->frame_5)
        {
            QHBoxLayout *f7Layout = new QHBoxLayout(ui->frame_7);
            f7Layout->setContentsMargins(16, 12, 16, 12);
            f7Layout->setSpacing(14);
            f7Layout->addWidget(ui->frame_6, 1);
            f7Layout->addWidget(ui->authedMainWin, 1);
            f7Layout->addWidget(ui->frame_5, 1);
        }
    }
    if(ui->authedMainWin)
    {
        ui->authedMainWin->setMaximumSize(16777215, 16777215);
        ui->authedMainWin->setMinimumHeight(110);
        ui->authedMainWin->setStyleSheet(
            "QFrame#authedMainWin {"
            "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #222630, stop:1 #171A21);"
            "   border: 1px solid #2F3543;"
            "   border-radius: 12px;"
            "}");
        if(ui->authedMainWin->layout())
        {
            ui->authedMainWin->layout()->setContentsMargins(10, 8, 10, 8);
            ui->authedMainWin->layout()->setSpacing(4);
            if(ui->frame_3)
                ui->authedMainWin->layout()->setAlignment(ui->frame_3, Qt::AlignCenter);
            if(ui->labelLoginAuthed)
                ui->authedMainWin->layout()->setAlignment(ui->labelLoginAuthed, Qt::AlignCenter);
        }
    }
    if(ui->frame_3)
    {
        ui->frame_3->setFixedSize(58, 58);
        ui->frame_3->setStyleSheet(
            "QFrame#frame_3 {"
            "   image: url(:/resources/no-avatar);"
            "   border: 2px solid #38BDF8;"
            "   border-radius: 29px;"
            "   background-color: #0F1216;"
            "   padding: 3px;"
            "}");
    }
    if(ui->labelLoginAuthed)
    {
        QFont font = ui->labelLoginAuthed->font();
        font.setUnderline(false);
        ui->labelLoginAuthed->setFont(font);
        ui->labelLoginAuthed->setStyleSheet(
            "color: #FFFFFF;"
            "font-size: 15px;"
            "font-weight: bold;"
            "text-decoration: none;"
            "background: transparent;");
        ui->labelLoginAuthed->setAlignment(Qt::AlignCenter);
    }
    if(ui->frame_6)
    {
        ui->frame_6->setMaximumSize(16777215, 16777215);
        ui->frame_6->setMinimumHeight(110);
        ui->frame_6->setStyleSheet(
            "QFrame#frame_6 {"
            "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #2A2113, stop:1 #1A150D);"
            "   border: 1px solid rgba(245, 158, 11, 0.4);"
            "   border-radius: 12px;"
            "}");

        if(!ui->frame_6->findChild<QPushButton *>("buttonAddVip"))
        {
            if(ui->frame_6->layout())
            {
                if(ui->labelVipDays)
                    ui->frame_6->layout()->removeWidget(ui->labelVipDays);
                delete ui->frame_6->layout();
            }

            QVBoxLayout *f6Layout = new QVBoxLayout(ui->frame_6);
            f6Layout->setContentsMargins(10, 8, 10, 8);
            f6Layout->setSpacing(6);
            f6Layout->setAlignment(Qt::AlignCenter);

            if(ui->labelVipDays)
            {
                f6Layout->addWidget(ui->labelVipDays, 0, Qt::AlignCenter);
            }

            QPushButton *btnAddVip = new QPushButton(QString::fromUtf8("+ Добавить"), ui->frame_6);
            btnAddVip->setObjectName("buttonAddVip");
            btnAddVip->setCursor(Qt::PointingHandCursor);
            btnAddVip->setFixedSize(115, 25);
            btnAddVip->setToolTip(QString::fromUtf8("Пополнить или продлить VIP-статус"));
            btnAddVip->setStyleSheet(
                "QPushButton {"
                "   background: qlineargradient(x1:0, y1:0, x2:0, y2:0, stop:0 #D97706, stop:1 #F59E0B);"
                "   color: #FFFFFF;"
                "   font-size: 11px;"
                "   font-weight: bold;"
                "   border: none;"
                "   border-radius: 6px;"
                "   padding: 2px 8px;"
                "}"
                "QPushButton:hover {"
                "   background: qlineargradient(x1:0, y1:0, x2:0, y2:0, stop:0 #F59E0B, stop:1 #FBBF24);"
                "}"
                "QPushButton:pressed {"
                "   background: #B45309;"
                "}");
            f6Layout->addWidget(btnAddVip, 0, Qt::AlignCenter);

            QObject::connect(
                btnAddVip,
                &QPushButton::clicked,
                this,
                [this]()
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
                            return;
                        }
                    }
                    QMessageBox::warning(this, QString::fromUtf8("Пополнение VIP"), QString::fromUtf8("Сервис пополнения VIP недоступен."));
                });
        }
    }
    if(ui->labelVipDays)
    {
        QFont font = ui->labelVipDays->font();
        font.setUnderline(false);
        ui->labelVipDays->setFont(font);
        ui->labelVipDays->setStyleSheet(
            "color: #FBBF24;"
            "font-size: 13.5px;"
            "font-weight: bold;"
            "text-decoration: none;"
            "line-height: 1.4;"
            "background: transparent;");
        ui->labelVipDays->setAlignment(Qt::AlignCenter);
    }
    if(ui->frame_5)
    {
        ui->frame_5->setMaximumSize(16777215, 16777215);
        ui->frame_5->setMinimumHeight(110);
        ui->frame_5->setStyleSheet(
            "QFrame#frame_5 {"
            "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #14241B, stop:1 #0E1A13);"
            "   border: 1px solid rgba(52, 211, 153, 0.4);"
            "   border-radius: 12px;"
            "}");
    }
    if(ui->labelCredits)
    {
        QFont font = ui->labelCredits->font();
        font.setUnderline(false);
        ui->labelCredits->setFont(font);
        ui->labelCredits->setStyleSheet(
            "color: #34D399;"
            "font-size: 13.5px;"
            "font-weight: bold;"
            "text-decoration: none;"
            "line-height: 1.4;"
            "background: transparent;");
        ui->labelCredits->setAlignment(Qt::AlignCenter);
    }
    if(ui->toplevel_up_2)
    {
        ui->toplevel_up_2->setStyleSheet(
            "QFrame#toplevel_up_2 {"
            "   background-color: #17191E;"
            "   border-top: 1px solid #262930;"
            "   border-bottom: 1px solid #262930;"
            "   border-radius: 6px;"
            "}");
    }
    if(ui->label_7)
    {
        ui->label_7->setText(QString::fromUtf8("ДОСТУПНЫЕ СЕРВИСЫ"));
        ui->label_7->setStyleSheet(
            "color: #38BDF8;"
            "font-size: 12px;"
            "font-weight: bold;"
            "letter-spacing: 1.2px;"
            "background: transparent;");
    }
    if(ui->serviceContents)
    {
        ui->serviceContents->setStyleSheet("QFrame#serviceContents { background: transparent; border: none; }");
    }
    if(ui->authInfo)
    {
        ui->authInfo->setVisible(false);
    }
    if(ui->sss && ui->serviceContents && !ui->scrollAreaWidgetContents_3->findChild<QFrame *>("cabinetSideInfoPanel"))
    {
        for(int i = ui->sss->count() - 1; i >= 0; --i)
        {
            QLayoutItem *item = ui->sss->itemAt(i);
            if(item && item->widget() != ui->serviceContents)
            {
                ui->sss->takeAt(i);
                delete item;
            }
        }

        // Side reference card / Справочник аккаунта
        QFrame *sidePanel = new QFrame(ui->scrollAreaWidgetContents_3);
        sidePanel->setObjectName("cabinetSideInfoPanel");
        sidePanel->setFixedWidth(290);
        sidePanel->setStyleSheet(
            "QFrame#cabinetSideInfoPanel {"
            "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #1D2028, stop:1 #13151B);"
            "   border: 1px solid #2C313E;"
            "   border-radius: 14px;"
            "}");

        QVBoxLayout *sideLayout = new QVBoxLayout(sidePanel);
        sideLayout->setContentsMargins(14, 14, 14, 14);
        sideLayout->setSpacing(7);

        QLabel *sideTitle = new QLabel(QString::fromUtf8("📋 СПРАВОЧНИК АККАУНТА"), sidePanel);
        sideTitle->setStyleSheet(
            "color: #38BDF8;"
            "font-size: 11px;"
            "font-weight: bold;"
            "letter-spacing: 1.1px;"
            "background: transparent;"
            "padding-bottom: 6px;"
            "border-bottom: 1px solid #282C38;");
        sideLayout->addWidget(sideTitle);

        auto createRow = [sidePanel, sideLayout](const QString &icon, const QString &caption, const QString &valObjName)
        {
            QFrame *row = new QFrame(sidePanel);
            row->setStyleSheet(
                "QFrame {"
                "   background-color: #171922;"
                "   border: 1px solid #242836;"
                "   border-radius: 8px;"
                "}");
            QHBoxLayout *rl = new QHBoxLayout(row);
            rl->setContentsMargins(8, 5, 10, 5);
            rl->setSpacing(8);

            QLabel *iconLbl = new QLabel(icon, row);
            iconLbl->setFixedSize(26, 26);
            iconLbl->setAlignment(Qt::AlignCenter);
            iconLbl->setStyleSheet(
                "background-color: #212635;"
                "border-radius: 6px;"
                "font-size: 13px;");
            rl->addWidget(iconLbl);

            QVBoxLayout *col = new QVBoxLayout();
            col->setContentsMargins(0, 0, 0, 0);
            col->setSpacing(1);

            QLabel *capLbl = new QLabel(caption, row);
            capLbl->setStyleSheet("color: #64748B; font-size: 10px; font-weight: 500; background: transparent; border: none;");
            col->addWidget(capLbl);

            QLabel *valLbl = new QLabel("-", row);
            valLbl->setObjectName(valObjName);
            valLbl->setStyleSheet("color: #F1F5F9; font-size: 12px; font-weight: bold; background: transparent; border: none;");
            col->addWidget(valLbl);

            rl->addLayout(col, 1);
            sideLayout->addWidget(row);
        };

        createRow("👤", QString::fromUtf8("Логин аккаунта"), "cabinetVal_login");
        createRow("🕒", QString::fromUtf8("Время входа"), "cabinetVal_loginTime");
        createRow("💳", QString::fromUtf8("Баланс кредитов"), "cabinetVal_credits");
        createRow("👑", QString::fromUtf8("VIP-статус"), "cabinetVal_vip");
        createRow("📱", QString::fromUtf8("Подключено устройств"), "cabinetVal_devices");
        createRow("🌐", QString::fromUtf8("Локация"), "cabinetVal_location");
        createRow("🛡️", QString::fromUtf8("Статус безопасности"), "cabinetVal_status");

        sideLayout->addStretch(1);

        ui->sss->setContentsMargins(10, 8, 10, 14);
        ui->sss->setSpacing(16);
        ui->sss->setAlignment(ui->serviceContents, Qt::AlignTop);
        ui->sss->insertStretch(0, 1);
        ui->sss->addWidget(sidePanel, 0, Qt::AlignTop);
        ui->sss->addStretch(1);
    }

    // ==========================================
    // 3. Devices Connection Page (page_devices)
    // ==========================================
    if(auto *hLayout = qobject_cast<QHBoxLayout *>(ui->page_devices->layout()))
    {
        hLayout->setContentsMargins(18, 14, 18, 14);
        hLayout->setSpacing(20);
        hLayout->setStretch(0, 0); // horizontalSpacer_2
        hLayout->setStretch(1, 5); // device_left_group
        hLayout->setStretch(2, 6); // device_right_group
        hLayout->setStretch(3, 0); // horizontalSpacer
    }

    if(ui->scrollArea_2)
    {
        ui->scrollArea_2->setFrameShape(QFrame::NoFrame);
        ui->scrollArea_2->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        ui->scrollArea_2->setStyleSheet("QScrollArea { background: transparent; border: none; } QWidget#scrollAreaWidgetContents_2 { background: transparent; }");
    }

    if(ui->device_left_group)
    {
        ui->device_left_group->setStyleSheet(
            "QFrame#device_left_group {"
            "   background-color: #0F172A;"
            "   border: 1px solid #1E293B;"
            "   border-radius: 16px;"
            "}");
    }

    if(ui->label)
    {
        ui->label->setText(
            "<html><head/><body>"
            "<div style=\"font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; color: #E2E8F0; padding: 2px;\">"
            "  <div style=\"margin-bottom: 12px;\">"
            "    <span style=\"font-size: 16px; font-weight: 700; color: #F8FAFC;\">Подключение устройства (ADB)</span>"
            "  </div>"
            "  <p style=\"color: #94A3B8; font-size: 11.5px; margin: 0 0 12px 0; line-height: 1.4;\">"
            "    Для выполнения процедур активируйте <b>Отладку по USB</b> на вашем Android-смартфоне:"
            "  </p>"
            "  <div style=\"background: #1E293B; border: 1px solid #334155; border-radius: 9px; padding: 9px 12px; margin-bottom: 8px;\">"
            "    <span style=\"background: #0284C7; color: #FFFFFF; border-radius: 10px; padding: 2px 8px; font-weight: bold; font-size: 11px;\">1</span>"
            "    <strong style=\"color: #F8FAFC; font-size: 12.5px; margin-left: 6px;\">Режим разработчика</strong>"
            "    <p style=\"margin: 4px 0 0 24px; color: #94A3B8; font-size: 11.5px; line-height: 1.4;\">"
            "      Откройте <b>Настройки</b> &rarr; <b>О телефоне</b>. Найдите <b>Номер сборки</b> (или версию MIUI/HyperOS) и нажмите на него <b>7 раз</b> подряд."
            "    </p>"
            "  </div>"
            "  <div style=\"background: #1E293B; border: 1px solid #334155; border-radius: 9px; padding: 9px 12px; margin-bottom: 8px;\">"
            "    <span style=\"background: #0284C7; color: #FFFFFF; border-radius: 10px; padding: 2px 8px; font-weight: bold; font-size: 11px;\">2</span>"
            "    <strong style=\"color: #F8FAFC; font-size: 12.5px; margin-left: 6px;\">Включите отладку по USB</strong>"
            "    <p style=\"margin: 4px 0 0 24px; color: #94A3B8; font-size: 11.5px; line-height: 1.4;\">"
            "      Перейдите в <b>Настройки</b> &rarr; <b>Для разработчиков</b> и активируйте тумблер <b>Отладка по USB</b> (для Xiaomi также «Установка через USB»)."
            "    </p>"
            "  </div>"
            "  <div style=\"background: #1E293B; border: 1px solid #334155; border-radius: 9px; padding: 9px 12px; margin-bottom: 8px;\">"
            "    <span style=\"background: #0284C7; color: #FFFFFF; border-radius: 10px; padding: 2px 8px; font-weight: bold; font-size: 11px;\">3</span>"
            "    <strong style=\"color: #F8FAFC; font-size: 12.5px; margin-left: 6px;\">Подключите кабель к ПК</strong>"
            "    <p style=\"margin: 4px 0 0 24px; color: #94A3B8; font-size: 11.5px; line-height: 1.4;\">"
            "      Соедините устройство кабелем. На экране телефона появится запрос &mdash; отметьте <b>«Всегда разрешать с этого компьютера»</b> и нажмите <b>ОК</b>."
            "    </p>"
            "  </div>"
            "  <div style=\"background: rgba(30, 41, 59, 0.4); border: 1px dashed #334155; border-radius: 8px; padding: 8px 12px; margin-top: 6px;\">"
            "    <span style=\"color: #38BDF8; font-size: 11.5px; font-weight: 600;\">💡 Телефон не определяется?</span>"
            "    <p style=\"margin: 3px 0 0 0; color: #64748B; font-size: 11px; line-height: 1.35;\">"
            "      Смените режим подключения USB на <b>«Передача файлов (MTP)»</b> либо подключите кабель в другой USB-порт на ПК."
            "    </p>"
            "  </div>"
            "</div>"
            "</body></html>");
    }
    if(ui->label_3)
    {
        ui->label_3->setText("<a style=\"color: #38BDF8; text-decoration: none; font-size: 12px; font-weight: 500;\" href=\"https://www.anymp4.com/ru/faq/enable-usb-debugging-for-android.html\">📖 Подробная пошаговая инструкция с иллюстрациями &rarr;</a>");
    }
    if(ui->label_5)
    {
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

    if(ui->device_right_group && !adbVisualizer)
    {
        while(auto item = ui->device_right_group->takeAt(0))
        {
            if(item->widget())
            {
                item->widget()->hide();
                item->widget()->deleteLater();
            }
            delete item;
        }

        AdbDeviceVisualizer *visualizer = new AdbDeviceVisualizer(ui->page_devices);
        visualizer->setObjectName("adbDeviceVisualizer");
        ui->device_right_group->addWidget(visualizer);
        adbVisualizer = visualizer;
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
            "padding: 8px 16px;");
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
            "}");
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
            "}");
    }
    if(ui->malwareStatusText0)
    {
        ui->malwareStatusText0->setStyleSheet(
            "color: #E5E7EB;"
            "font-size: 14px;"
            "font-weight: 600;"
            "padding: 6px;"
            "font-style: normal;"
            "text-decoration: none;");
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
            "}");
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
            "}");
    }
    if(ui->loaderPageText)
    {
        ui->loaderPageText->setStyleSheet(
            "color: #9CA3AF;"
            "font-size: 14px;"
            "font-weight: 600;"
            "letter-spacing: 0.5px;");
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
            "}");
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
            "}");
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
            "   image: url(:/resources/checkbox-checked);"
            "}");
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
            "}");
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
            "}");
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
            "}");
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
            "padding: 10px 14px;");
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
            "}");
    }
    if(ui->frame)
    {
        ui->frame->setStyleSheet(
            "QFrame#frame {"
            "   background-color: #18191C;"
            "   border: 1px solid #2A2D33;"
            "   border-radius: 8px;"
            "   padding: 10px;"
            "}");
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
            "}");
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
            "}");
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
            "}");
    }
    if(ui->label_8)
    {
        ui->label_8->setStyleSheet(
            "color: #FFFFFF;"
            "font-size: 14px;"
            "font-weight: bold;"
            "letter-spacing: 0.5px;"
            "font-style: normal;"
            "background: transparent;");
    }
}
