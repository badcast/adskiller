#include <functional>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <list>
#include <memory>

#include <QCloseEvent>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDate>
#include <QDesktopServices>
#include <QEasingCurve>
#include <QFile>
#include <QFontDatabase>
#include <QStyle>
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
#include "FileManagerWidget.h"
#include "ApkManagerWidget.h"
#include "ContactFixerWidget.h"
#include <QToolBar>
#include "RadioPlayerWidget.h"

constexpr struct
{
    PageIndex index;
    const char *widgetName;
} PageConstNames[7] = {{AuthPage, "page_auth"}, {CabinetPage, "page_cabinet"}, {LongInfoPage, "page_adsmalware"}, {LoaderPage, "page_loader"}, {DevicesPage, "page_devices"}, {MyDevicesPage, "page_mydevices"}, {BuyVIPPage, "page_buyvip"}};

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
                m_capsule->setProperty("focused", true);
                m_capsule->style()->unpolish(m_capsule);
                m_capsule->style()->polish(m_capsule);
            }
            else if(event->type() == QEvent::FocusOut)
            {
                m_capsule->setProperty("focused", false);
                m_capsule->style()->unpolish(m_capsule);
                m_capsule->style()->polish(m_capsule);
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

    // Embedded pages for File Manager and APK Manager
    FileManagerWidget *fmWidget = new FileManagerWidget(this);
    fmWidget->setObjectName("page_filemanager");
    fmWidget->setVisible(false);
    ui->contentLayout->layout()->addWidget(fmWidget);
    pages.insert(FileManagerPage, fmWidget);

    ApkManagerWidget *apkWidget = new ApkManagerWidget(this);
    apkWidget->setObjectName("page_apkmanager");
    apkWidget->setVisible(false);
    ui->contentLayout->layout()->addWidget(apkWidget);
    pages.insert(ApkManagerPage, apkWidget);

    ContactFixerWidget *cfWidget = new ContactFixerWidget(this);
    cfWidget->setObjectName("page_contactfixer");
    cfWidget->setVisible(false);
    ui->contentLayout->layout()->addWidget(cfWidget);
    pages.insert(ContactFixerPage, cfWidget);

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
        ui->mainapplogo->setProperty("holiday", true);
        ui->mainapplogo->style()->unpolish(ui->mainapplogo);
        ui->mainapplogo->style()->polish(ui->mainapplogo);
        this->setWindowIcon(QIcon(":/resources/app-logo-merry"));
    }
    else
    {
        ui->mainapplogo->setProperty("holiday", false);
        ui->mainapplogo->style()->unpolish(ui->mainapplogo);
        ui->mainapplogo->style()->polish(ui->mainapplogo);
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
            ui->horizontalLayout_ai->setSpacing(0);
            ui->horizontalLayout_ai->setAlignment(ui->aiToolBoxToggle, Qt::AlignVCenter);
        }

        ui->aiToolBoxToggle->setIcon(QIcon());
        ui->aiToolBoxToggle->setCursor(Qt::PointingHandCursor);

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

        QHBoxLayout *headerLayout = new QHBoxLayout(aiHeaderBar);
        headerLayout->setContentsMargins(10, 0, 6, 0);
        headerLayout->setSpacing(6);

        QLabel *aiTitle = new QLabel(aiHeaderBar);
        aiTitle->setObjectName("aiTitle");
        aiTitle->setText("<b>AdsKiller AI</b>");

        QLabel *aiStatus = new QLabel(aiHeaderBar);
        aiStatus->setObjectName("aiStatus");
        aiStatus->setText(QString::fromUtf8("●"));
        aiStatus->setToolTip("Ассистент активен");

        headerLayout->addWidget(aiTitle);
        headerLayout->addWidget(aiStatus);
        headerLayout->addStretch(1);

        QFrame *segmentedBar = new QFrame(aiHeaderBar);
        segmentedBar->setObjectName("aiSegmentedBar");
        segmentedBar->setFixedHeight(26);
        QHBoxLayout *segLayout = new QHBoxLayout(segmentedBar);
        segLayout->setContentsMargins(2, 2, 2, 2);
        segLayout->setSpacing(2);

        QPushButton *tabChatBtn = new QPushButton("💬 Чат", segmentedBar);
        tabChatBtn->setObjectName("tabChatBtn");
        QPushButton *tabInfoBtn = new QPushButton("ℹ Инфо", segmentedBar);
        tabInfoBtn->setObjectName("tabInfoBtn");

        tabChatBtn->setCheckable(true);
        tabInfoBtn->setCheckable(true);
        tabChatBtn->setChecked(true);
        tabChatBtn->setCursor(Qt::PointingHandCursor);
        tabInfoBtn->setCursor(Qt::PointingHandCursor);

        segLayout->addWidget(tabChatBtn);
        segLayout->addWidget(tabInfoBtn);
        headerLayout->addWidget(segmentedBar);

        QPushButton *headerCollapseBtn = new QPushButton(QString::fromUtf8("›"), aiHeaderBar);
        headerCollapseBtn->setObjectName("aiHeaderCollapseBtn");
        headerCollapseBtn->setToolTip(QString::fromUtf8("Свернуть панель AdsKiller AI"));
        headerCollapseBtn->setFixedSize(24, 24);
        headerCollapseBtn->setCursor(Qt::PointingHandCursor);
        headerLayout->addWidget(headerCollapseBtn);

        aiPanelLayout->addWidget(aiHeaderBar);

        // QStackedWidget for switching between Chat and Info
        QStackedWidget *aiStack = new QStackedWidget(aiPanel);
        aiStack->setObjectName("aiStack");
        aiStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

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
        auto setAiPanelExpanded = [this, aiPanel](bool expanded)
        {
            if(expanded)
            {
                ui->aiToolBoxToggle->setVisible(false);
                aiPanel->setVisible(true);
                ui->aiToolBoxContainer->setFixedWidth(350);
            }
            else
            {
                aiPanel->setVisible(false);
                ui->aiToolBoxToggle->setVisible(true);
                ui->aiToolBoxToggle->setFixedSize(36, 100);
                ui->aiToolBoxToggle->setText(QString::fromUtf8("🤖\nAI\n‹"));
                ui->aiToolBoxToggle->setToolTip(QString::fromUtf8("Развернуть панель AdsKiller AI"));
                ui->aiToolBoxContainer->setFixedWidth(36);
            }
        };

        QObject::connect(headerCollapseBtn, &QPushButton::clicked, this, [setAiPanelExpanded]() { setAiPanelExpanded(false); });

        QObject::connect(ui->aiToolBoxToggle, &QPushButton::clicked, this, [setAiPanelExpanded, aiPanel]() { setAiPanelExpanded(!aiPanel->isVisible()); });

        // Setup Page 2: About AI info
        if(ui->aboutAi_edit)
        {
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
        quickBarWidget->setObjectName("aiQuickBarWidget");
        quickBarWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        quickBarWidget->setFixedHeight(38);
        page1Layout->addWidget(quickBarWidget, 0);
        QHBoxLayout *quickBarLayout = new QHBoxLayout(quickBarWidget);
        quickBarLayout->setContentsMargins(0, 2, 0, 2);
        quickBarLayout->setSpacing(4);

        QPushButton *shuffleBtn = new QPushButton(QString::fromUtf8("🔀"), quickBarWidget);
        shuffleBtn->setObjectName("aiShuffleBtn");
        shuffleBtn->setToolTip("Перемешать подсказки");
        shuffleBtn->setFixedSize(26, 26);
        shuffleBtn->setCursor(Qt::PointingHandCursor);

        QScrollArea *scrollArea = new QScrollArea(quickBarWidget);
        scrollArea->setObjectName("aiQuickScrollArea");
        scrollArea->setWidgetResizable(true);
        scrollArea->setFixedHeight(34);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        scrollArea->viewport()->installEventFilter(new HorizontalWheelFilter(scrollArea));
        scrollArea->installEventFilter(new HorizontalWheelFilter(scrollArea));

        QWidget *quickButtonsWidget = new QWidget(scrollArea);
        quickButtonsWidget->setObjectName("aiQuickButtonsWidget");
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
                btn->setObjectName("aiQuickBtn");
                btn->setFixedHeight(26);
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

        QHBoxLayout *capsuleLayout = new QHBoxLayout(inputCapsule);
        capsuleLayout->setContentsMargins(10, 4, 6, 4);
        capsuleLayout->setSpacing(6);

        ui->aiChatEdit->setPlaceholderText("Спросите у AdsKiller AI...");
        ui->aiChatEdit->setMinimumHeight(32);
        ui->aiChatEdit->setMaximumHeight(48);
        ui->aiChatEdit->installEventFilter(new CapsuleFocusFilter(inputCapsule));

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
        setAiPanelExpanded(true);
    }
}

void MainWindow::setupRadioPlayer()
{
    radioToolBar = new QToolBar(tr("Онлайн Радио"), this);
    radioToolBar->setObjectName("radioToolBar");
    radioToolBar->setMovable(false);
    radioToolBar->setFloatable(false);
    radioToolBar->setStyleSheet(
        "QToolBar#radioToolBar {"
        "   background-color: #0B1120;"
        "   border: none;"
        "   border-bottom: 1px solid #1E293B;"
        "   margin: 0px;"
        "   padding: 0px;"
        "}"
    );

    radioPlayer = new RadioPlayerWidget(radioToolBar);
    radioToolBar->addWidget(radioPlayer);

    // Place the toolbar at the top of the main window
    addToolBar(Qt::TopToolBarArea, radioToolBar);

    // Hide toolbar when close button is clicked
    connect(radioPlayer, &RadioPlayerWidget::requestClose, this, [this]() {
        if(radioToolBar)
        {
            radioToolBar->setVisible(false);
        }
    });

    // Add "Радио" menu to QMenuBar
    if(ui->menubar)
    {
        QMenu *radioMenu = ui->menubar->addMenu(QString::fromUtf8("📻 Радио"));

        QAction *toggleViewAct = radioToolBar->toggleViewAction();
        toggleViewAct->setText(QString::fromUtf8("Показать/скрыть панель радио"));
        toggleViewAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
        radioMenu->addAction(toggleViewAct);

        radioMenu->addSeparator();

        QAction *playPauseAct = radioMenu->addAction(QString::fromUtf8("▶ / ⏸ Воспроизведение / Пауза"), radioPlayer, &RadioPlayerWidget::togglePlay);
        playPauseAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Space));

        QAction *nextStationAct = radioMenu->addAction(QString::fromUtf8("⏭ Следующая станция"), radioPlayer, &RadioPlayerWidget::nextStation);
        nextStationAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Right));

        QAction *prevStationAct = radioMenu->addAction(QString::fromUtf8("⏮ Предыдущая станция"), radioPlayer, &RadioPlayerWidget::previousStation);
        prevStationAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Left));

        radioMenu->addSeparator();

        QMenu *stationsSubMenu = radioMenu->addMenu(QString::fromUtf8("Выбрать станцию"));
        const auto &stations = radioPlayer->stations();
        for(int i = 0; i < stations.size(); ++i)
        {
            const auto &st = stations[i];
            QAction *stAct = stationsSubMenu->addAction(QStringLiteral("%1 (%2)").arg(st.name, st.genre));
            connect(stAct, &QAction::triggered, radioPlayer, [this, i]() {
                radioPlayer->setStationIndex(i);
                if(!radioPlayer->isPlaying())
                {
                    radioPlayer->play();
                }
            });
        }

        radioMenu->addSeparator();
        radioMenu->addAction(QString::fromUtf8("➕ Добавить свою радиостанцию..."), radioPlayer, &RadioPlayerWidget::showAddStationDialog);
    }
}

void MainWindow::setupPagesDesign()
{
    // ==========================================
    // 0. Load Unified Application Stylesheet
    // ==========================================
    QFile styleFile(":/resources/ApplicationStyle.qss");
    if(!styleFile.open(QFile::ReadOnly | QFile::Text))
    {
        styleFile.setFileName(QCoreApplication::applicationDirPath() + "/ApplicationStyle.qss");
        if(!styleFile.open(QFile::ReadOnly | QFile::Text))
        {
            styleFile.setFileName(QCoreApplication::applicationDirPath() + "/res/style/ApplicationStyle.qss");
            if(!styleFile.open(QFile::ReadOnly | QFile::Text))
            {
                styleFile.setFileName("res/style/ApplicationStyle.qss");
                if(!styleFile.open(QFile::ReadOnly | QFile::Text))
                    styleFile.setFileName("ApplicationStyle.qss");
            }
        }
    }
    if(styleFile.isOpen() || styleFile.open(QFile::ReadOnly | QFile::Text))
    {
        QString styleSheetContent = QString::fromUtf8(styleFile.readAll());
        styleFile.close();
        this->setStyleSheet(styleSheetContent);
    }

    if(ui->contentLayout)
        ui->contentLayout->setAttribute(Qt::WA_StyledBackground, true);
    if(ui->toplevel_up)
        ui->toplevel_up->setAttribute(Qt::WA_StyledBackground, true);
    if(ui->toplevel_backpage)
        ui->toplevel_backpage->setAttribute(Qt::WA_StyledBackground, true);

    for(auto *page : pages.values())
    {
        if(page)
            page->setAttribute(Qt::WA_StyledBackground, true);
    }

    // ==========================================
    // 1. Auth Page (page_auth)
    // ==========================================
    if(ui->page_auth)
    {
        ui->page_auth->setAttribute(Qt::WA_StyledBackground, true);
    }
    if(ui->gridLayout_asd2)
    {
        ui->gridLayout_asd2->setAlignment(Qt::AlignCenter);
    }

    if(ui->frame_4)
    {
        ui->frame_4->setAttribute(Qt::WA_StyledBackground, true);
        ui->frame_4->setFixedSize(440, 610);
    }

    if(ui->mainapplogo)
    {
        ui->mainapplogo->setFixedSize(68, 68);
        bool isHoliday = (QDate::currentDate().month() == 12 || QDate::currentDate().month() == 1);
        ui->mainapplogo->setProperty("holiday", isHoliday);
    }

    if(ui->label_4)
    {
        ui->label_4->setTextFormat(Qt::RichText);
        ui->label_4->setAlignment(Qt::AlignCenter);
        ui->label_4->setText(
            "<div align='center'>"
            "<span style='font-size: 20px; font-weight: 700; color: #F8FAFC; letter-spacing: 0.5px;'>AdsKiller Desktop</span><br>"
            "<span style='font-size: 13px; font-weight: 400; color: #94A3B8;'>Авторизуйтесь для доступа к сервисам</span>"
            "</div>");
    }

    if(ui->label_12)
    {
        ui->label_12->setText("ЛОГИН или EMAIL");
    }

    if(ui->lineLoginEdit)
    {
        ui->lineLoginEdit->setFixedHeight(42);
        ui->lineLoginEdit->setPlaceholderText("Логин или электронная почта");
    }

    if(ui->label_14)
    {
        ui->label_14->setText("ПАРОЛЬ");
    }

    if(ui->linePassEdit)
    {
        ui->linePassEdit->setFixedHeight(42);
        ui->linePassEdit->setPlaceholderText("Пароль");
    }

    if(ui->butShowPass)
    {
        ui->butShowPass->setText("👁");
        ui->butShowPass->setToolTip("Показать / скрыть пароль");
        ui->butShowPass->setCursor(Qt::PointingHandCursor);
        ui->butShowPass->setFixedSize(42, 42);
    }

    if(ui->checkAutoLogin)
    {
        ui->checkAutoLogin->setCursor(Qt::PointingHandCursor);
        ui->checkAutoLogin->setText("Войти при запуске");
    }

    if(ui->label_2)
    {
        ui->label_2->setCursor(Qt::PointingHandCursor);
        ui->label_2->setText("<a href=\"index\" style=\"color: #38BDF8; text-decoration: none;\">Забыли логин или пароль?</a>");
    }

    if(ui->authButton)
    {
        ui->authButton->setText("Войти в систему");
        ui->authButton->setFixedHeight(44);
        ui->authButton->setCursor(Qt::PointingHandCursor);
    }

    if(ui->statusAuthText)
    {
        ui->statusAuthText->setAlignment(Qt::AlignCenter);
        ui->statusAuthText->setWordWrap(true);
    }

    if(ui->label_auth_ver)
    {
        ui->label_auth_ver->setAlignment(Qt::AlignCenter);
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
        checkConsent->setObjectName("checkConsent");
        checkConsent->setChecked(true);
        checkConsent->setCursor(Qt::PointingHandCursor);

        // Build rich-text label with clickable links
        QLabel *consentLabel = new QLabel(ui->frame_4);
        consentLabel->setObjectName("consentLabel");
        consentLabel->setTextFormat(Qt::RichText);
        consentLabel->setOpenExternalLinks(true);
        consentLabel->setWordWrap(true);
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
    if(ui->logoutButton)
    {
        ui->logoutButton->setCursor(Qt::PointingHandCursor);
    }
    if(ui->authpageUpdate)
    {
        ui->authpageUpdate->setText(QString::fromUtf8("Обновить"));
        ui->authpageUpdate->setMinimumSize(105, 30);
        ui->authpageUpdate->setMaximumSize(125, 30);
        ui->authpageUpdate->setCursor(Qt::PointingHandCursor);
        ui->authpageUpdate->setToolTip(QString::fromUtf8("Обновить данные кабинета и доступность сервисов"));
    }
    if(ui->frame_7)
    {
        ui->frame_7->setMaximumSize(16777215, 16777215);
        ui->frame_7->setMinimumHeight(140);

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
    }
    if(ui->labelLoginAuthed)
    {
        QFont font = ui->labelLoginAuthed->font();
        font.setUnderline(false);
        ui->labelLoginAuthed->setFont(font);
        ui->labelLoginAuthed->setAlignment(Qt::AlignCenter);
    }
    if(ui->frame_6)
    {
        ui->frame_6->setMaximumSize(16777215, 16777215);
        ui->frame_6->setMinimumHeight(110);

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
        ui->labelVipDays->setAlignment(Qt::AlignCenter);
    }
    if(ui->frame_5)
    {
        ui->frame_5->setMaximumSize(16777215, 16777215);
        ui->frame_5->setMinimumHeight(110);
    }
    if(ui->labelCredits)
    {
        QFont font = ui->labelCredits->font();
        font.setUnderline(false);
        ui->labelCredits->setFont(font);
        ui->labelCredits->setAlignment(Qt::AlignCenter);
    }
    if(ui->label_7)
    {
        ui->label_7->setText(QString::fromUtf8("ДОСТУПНЫЕ СЕРВИСЫ"));
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

        QVBoxLayout *sideLayout = new QVBoxLayout(sidePanel);
        sideLayout->setContentsMargins(14, 14, 14, 14);
        sideLayout->setSpacing(7);

        QLabel *sideTitle = new QLabel(QString::fromUtf8("📋 СПРАВОЧНИК АККАУНТА"), sidePanel);
        sideTitle->setObjectName("cabinetSideTitle");
        sideLayout->addWidget(sideTitle);

        auto createRow = [sidePanel, sideLayout](const QString &icon, const QString &caption, const QString &valObjName)
        {
            QFrame *row = new QFrame(sidePanel);
            row->setObjectName("cabinetSideRow");
            QHBoxLayout *rl = new QHBoxLayout(row);
            rl->setContentsMargins(8, 5, 10, 5);
            rl->setSpacing(8);

            QLabel *iconLbl = new QLabel(icon, row);
            iconLbl->setObjectName("cabinetSideIcon");
            iconLbl->setFixedSize(26, 26);
            iconLbl->setAlignment(Qt::AlignCenter);
            rl->addWidget(iconLbl);

            QVBoxLayout *col = new QVBoxLayout();
            col->setContentsMargins(0, 0, 0, 0);
            col->setSpacing(1);

            QLabel *capLbl = new QLabel(caption, row);
            capLbl->setObjectName("cabinetSideCaption");
            col->addWidget(capLbl);

            QLabel *valLbl = new QLabel("-", row);
            valLbl->setObjectName(valObjName);
            valLbl->setProperty("cabinetVal", true);
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
    if(ui->malwareReRun)
    {
        ui->malwareReRun->setCursor(Qt::PointingHandCursor);
    }

    // ==========================================
    // 5. Loader Page (page_loader)
    // ==========================================

    // ==========================================
    // 6. My Devices & Warranty Page (page_mydevices)
    // ==========================================
    if(ui->myDeviceQuaranteeFilter)
    {
        ui->myDeviceQuaranteeFilter->setCursor(Qt::PointingHandCursor);
    }
    if(ui->myDeviceSend)
    {
        ui->myDeviceSend->setCursor(Qt::PointingHandCursor);
    }
    if(ui->myDeviceActual)
    {
        ui->myDeviceActual->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        ui->myDeviceActual->horizontalHeader()->setStretchLastSection(true);
    }

    // ==========================================
    // 7. VIP Subscription Page (page_buyvip)
    // ==========================================
    if(ui->buttonBuyVip)
    {
        ui->buttonBuyVip->setCursor(Qt::PointingHandCursor);
    }

    // ==========================================
    // 8. Top Back Bar (toplevel_backpage)
    // ==========================================
    if(ui->buttonBackTo)
    {
        ui->buttonBackTo->setText("‹ Назад");
        ui->buttonBackTo->setCursor(Qt::PointingHandCursor);
    }
}
