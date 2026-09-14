#include <functional>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <list>
#include <memory>

#include <QApplication>
#include <QButtonGroup>
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

#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "AdbDeviceVisualizer.h"
#include "AIChatView.h"
#include "Snowflake.h"
#include "Strings.h"
#include "Services.h"
#include "ProgressCircle.h"
#include "FileManagerWidget.h"
#include "ApkManagerWidget.h"
#include "ContactFixerWidget.h"
#include "AITranslaterWidget.h"
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

    AITranslaterWidget *atWidget = new AITranslaterWidget(this);
    atWidget->setObjectName("page_aitranslater");
    atWidget->setVisible(false);
    ui->contentLayout->layout()->addWidget(atWidget);
    pages.insert(AITranslaterPage, atWidget);

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

        QPushButton *tabChatBtn = new QPushButton("Чат", segmentedBar);
        tabChatBtn->setObjectName("tabChatBtn");
        tabChatBtn->setIcon(QIcon(":/svg/message-circle"));
        tabChatBtn->setIconSize(QSize(14, 14));

        QPushButton *tabInfoBtn = new QPushButton("Инфо", segmentedBar);
        tabInfoBtn->setObjectName("tabInfoBtn");
        tabInfoBtn->setIcon(QIcon(":/svg/clipboard"));
        tabInfoBtn->setIconSize(QSize(14, 14));

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
                ui->aiToolBoxToggle->setIcon(QIcon(":/svg/bot"));
                ui->aiToolBoxToggle->setIconSize(QSize(18, 18));
                ui->aiToolBoxToggle->setText(QString::fromUtf8("AI\n‹"));
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
                "<img src=\":/svg/bot\" width=\"36\" height=\"36\"/><br/>"
                "<b style=\"color:#38BDF8; font-size:13pt;\">AdsKiller AI Assistant</b><br/>"
                "<span style=\"color:#8E9297; font-size:9pt;\">Интеллектуальный помощник</span><br/>"
                "<span style=\"display:inline-block; margin-top:6px; background-color:#1E293B; color:#38BDF8; font-size:8.5pt; font-weight:600; padding:2px 8px; border-radius:0px;\">В сети</span>"
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

        QPushButton *shuffleBtn = new QPushButton(quickBarWidget);
        shuffleBtn->setObjectName("aiShuffleBtn");
        shuffleBtn->setIcon(QIcon(":/svg/shuffle"));
        shuffleBtn->setIconSize(QSize(14, 14));
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
            QString iconRes;
            QStringList variations;
        };

        QList<QuickQuestion> quickQuestions = {
            {":/svg/credit-card", {"Мои кредиты", "Сколько кредитов?", "Остаток баланса?", "Показать баланс"}},
            {":/svg/crown", {"VIP статус", "Остаток VIP дней", "Сколько VIP дней?", "Когда истекает VIP?"}},
            {":/svg/smartphone", {"Мои устройства", "Список устройств", "Активные девайсы", "Привязанные устройства"}},
            {":/svg/shield", {"Удаление рекламы", "Запусти удаление рекламы", "Какие есть сервисы?", "Открой окно покупки VIP"}},
            {":/svg/zap", {"Быстрая очистка", "Остановить приложения", "Очистить кэш", "Как закрыть вирусы?"}},
            {":/svg/rocket", {"Ускорить телефон", "Как очистить ОЗУ?", "Оптимизация системы", "Ускорить работу"}},
            {":/svg/lightbulb", {"Что ты умеешь?", "Возможности AdsKiller", "Справка по функциям", "Чем можешь помочь?"}},
            {":/svg/mail", {"Моя почта", "Мой email", "Какая у меня почта?", "Адрес эл. почты"}},
            {":/svg/shopping-cart", {"Купить кредиты", "Как купить VIP?", "Пополнение баланса", "Тарифы и цены"}},
            {":/svg/lock", {"Безопасность", "Безопасно ли это?", "Как включить отладку?", "Как подключить телефон?"}},
            {":/svg/bar-chart", {"Статистика", "Заблокированная реклама", "Отчет блокировки", "Сколько рекламы скрыто?"}},
            {":/svg/clipboard", {"Как пользоваться?", "Инструкция для новичка", "Быстрый старт", "Помощь по приложению"}}};

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

                QPushButton *btn = new QPushButton(initialText, quickButtonsWidget);
                btn->setObjectName("aiQuickBtn");
                btn->setIcon(QIcon(qData.iconRes));
                btn->setIconSize(QSize(13, 13));
                btn->setFixedHeight(26);
                btn->setCursor(Qt::PointingHandCursor);
                quickButtonsLayout->addWidget(btn);

                QObject::connect(
                    btn,
                    &QPushButton::clicked,
                    this,
                    [this, btn, iconRes = qData.iconRes, variations = qData.variations, lastIdx = initialIdx]() mutable
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
                            btn->setText(variations[r]);
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
        ui->aiChatSend->setText(QString());
        ui->aiChatSend->setIcon(QIcon(":/svg/send"));
        ui->aiChatSend->setIconSize(QSize(15, 15));
        ui->aiChatSend->setToolTip("Отправить сообщение (Enter)");
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
        "}");

    radioPlayer = new RadioPlayerWidget(radioToolBar);
    radioToolBar->addWidget(radioPlayer);

    // Place the toolbar at the top of the main window
    addToolBar(Qt::TopToolBarArea, radioToolBar);

    // Hide toolbar when close button is clicked
    connect(
        radioPlayer,
        &RadioPlayerWidget::requestClose,
        this,
        [this]()
        {
            if(radioToolBar)
            {
                radioToolBar->setVisible(false);
            }
        });

    // Add "Радио" menu to QMenuBar
    if(ui->menubar)
    {
        QMenu *radioMenu = ui->menubar->addMenu(QString::fromUtf8("Радио"));
        radioMenu->setIcon(QIcon(":/svg/radio"));

        QAction *toggleViewAct = radioToolBar->toggleViewAction();
        toggleViewAct->setText(QString::fromUtf8("Показать/скрыть панель радио"));
        toggleViewAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
        radioMenu->addAction(toggleViewAct);

        radioMenu->addSeparator();

        QAction *playPauseAct = radioMenu->addAction(QIcon(":/svg/play"), QString::fromUtf8("Воспроизведение / Пауза"), radioPlayer, &RadioPlayerWidget::togglePlay);
        playPauseAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Space));

        QAction *nextStationAct = radioMenu->addAction(QIcon(":/svg/skip-forward"), QString::fromUtf8("Следующая станция"), radioPlayer, &RadioPlayerWidget::nextStation);
        nextStationAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Right));

        QAction *prevStationAct = radioMenu->addAction(QIcon(":/svg/skip-back"), QString::fromUtf8("Предыдущая станция"), radioPlayer, &RadioPlayerWidget::previousStation);
        prevStationAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Left));

        radioMenu->addSeparator();

        QMenu *stationsSubMenu = radioMenu->addMenu(QString::fromUtf8("Выбрать станцию"));
        const auto &stations = radioPlayer->stations();
        for(int i = 0; i < stations.size(); ++i)
        {
            const auto &st = stations[i];
            QAction *stAct = stationsSubMenu->addAction(QStringLiteral("%1 (%2)").arg(st.name, st.genre));
            connect(
                stAct,
                &QAction::triggered,
                radioPlayer,
                [this, i]()
                {
                    radioPlayer->setStationIndex(i);
                    if(!radioPlayer->isPlaying())
                    {
                        radioPlayer->play();
                    }
                });
        }

        radioMenu->addSeparator();
        radioMenu->addAction(QIcon(":/svg/plus"), QString::fromUtf8("Добавить свою радиостанцию..."), radioPlayer, &RadioPlayerWidget::showAddStationDialog);
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
        if(qApp)
            qApp->setStyleSheet(styleSheetContent);
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
        ui->butShowPass->setText(QString());
        ui->butShowPass->setIcon(QIcon(":/svg/eye"));
        ui->butShowPass->setIconSize(QSize(18, 18));
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
    if(ui->frame_5)
        ui->frame_5->hide();
    if(ui->frame_6)
        ui->frame_6->hide();

    if(ui->horizontalLayout)
    {
        ui->horizontalLayout->setContentsMargins(14, 8, 14, 4);
        ui->horizontalLayout->setSpacing(0);
        if(ui->horizontalSpacer_8)
            ui->horizontalSpacer_8->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
        if(ui->horizontalSpacer_9)
            ui->horizontalSpacer_9->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    if(ui->authedMainWin)
    {
        if(ui->authedMainWin->layout())
        {
            if(ui->frame_3)
                ui->authedMainWin->layout()->removeWidget(ui->frame_3);
            if(ui->labelLoginAuthed)
                ui->authedMainWin->layout()->removeWidget(ui->labelLoginAuthed);
            delete ui->authedMainWin->layout();
        }

        ui->authedMainWin->setMaximumSize(16777215, 16777215);
        ui->authedMainWin->setMinimumHeight(0);

        QHBoxLayout *authLayout = new QHBoxLayout(ui->authedMainWin);
        authLayout->setContentsMargins(0, 0, 0, 0);
        authLayout->setSpacing(10);

        if(ui->frame_3)
        {
            ui->frame_3->setFixedSize(36, 36);
            authLayout->addWidget(ui->frame_3, 0, Qt::AlignVCenter);
        }

        QVBoxLayout *userCol = new QVBoxLayout();
        userCol->setContentsMargins(0, 0, 0, 0);
        userCol->setSpacing(1);

        if(ui->labelLoginAuthed)
        {
            QFont font = ui->labelLoginAuthed->font();
            font.setPointSize(12);
            font.setBold(true);
            font.setUnderline(false);
            ui->labelLoginAuthed->setFont(font);
            ui->labelLoginAuthed->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            ui->labelLoginAuthed->setStyleSheet("color: #F8FAFC; font-size: 14px; font-weight: bold; background: transparent; border: none;");
            userCol->addWidget(ui->labelLoginAuthed);
        }

        QLabel *roleLbl = ui->authedMainWin->findChild<QLabel *>("cabinetRoleBadge");
        if(!roleLbl)
        {
            roleLbl = new QLabel(QString::fromUtf8("Основной аккаунт"), ui->authedMainWin);
            roleLbl->setObjectName("cabinetRoleBadge");
            roleLbl->setStyleSheet("color: #64748B; font-size: 9.5px; font-weight: 600; text-transform: uppercase; letter-spacing: 0.5px; background: transparent; border: none;");
        }
        userCol->addWidget(roleLbl);

        authLayout->addLayout(userCol);

        QLabel *onlineBadge = ui->authedMainWin->findChild<QLabel *>("cabinetOnlineBadge");
        if(!onlineBadge)
        {
            onlineBadge = new QLabel(QString::fromUtf8("● В СЕТИ"), ui->authedMainWin);
            onlineBadge->setObjectName("cabinetOnlineBadge");
            onlineBadge->setStyleSheet("color: #34D399; font-size: 9.5px; font-weight: bold; padding: 2px 7px; background-color: rgba(52, 211, 153, 0.12); border: 1px solid rgba(52, 211, 153, 0.3); border-radius: 0px;");
        }
        authLayout->addWidget(onlineBadge, 0, Qt::AlignVCenter);
    }

    if(ui->frame_7)
    {
        if(!ui->frame_7->layout())
        {
            ui->frame_7->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            ui->frame_7->setMinimumHeight(0);

            QVBoxLayout *f7MainLayout = new QVBoxLayout(ui->frame_7);
            f7MainLayout->setContentsMargins(12, 8, 12, 8);
            f7MainLayout->setSpacing(6);

            // Top Row: Avatar & Name on the FAR LEFT (TURN LEFT), VIP action button on Right
            QHBoxLayout *topRow = new QHBoxLayout();
            topRow->setContentsMargins(0, 0, 0, 0);
            topRow->setSpacing(8);

            if(ui->authedMainWin)
            {
                topRow->addWidget(ui->authedMainWin, 0, Qt::AlignLeft | Qt::AlignVCenter);
            }

            topRow->addStretch(1);

            // Кнопка "Добавить кредиты"
            QPushButton *btnAddCredits = new QPushButton(QString::fromUtf8("+ Добавить кредиты"), ui->frame_7);
            btnAddCredits->setObjectName("buttonAddCredits");
            btnAddCredits->setCursor(Qt::PointingHandCursor);
            btnAddCredits->setFixedHeight(26);
            btnAddCredits->setToolTip(QString::fromUtf8("Пополнить баланс кредитов через администратора"));
            topRow->addWidget(btnAddCredits, 0, Qt::AlignRight | Qt::AlignVCenter);

            QObject::connect(
                btnAddCredits,
                &QPushButton::clicked,
                this,
                [this]()
                {
                    QMessageBox msgBox(this);
                    msgBox.setWindowTitle(QString::fromUtf8("Пополнение баланса"));
                    msgBox.setIcon(QMessageBox::Information);
                    msgBox.setText(QString::fromUtf8(
                        "<h3>Пополнение кредитов</h3>"
                        "<p>Для пополнения баланса кредитов напишите администратору программы в WhatsApp.</p>"
                        "<p style='color: #94A3B8; font-size: 11px;'>Нажмите кнопку <b>«Написать в WhatsApp»</b>, чтобы перейти к диалогу с администратором.</p>"));

                    QPushButton *btnWa = msgBox.addButton(QString::fromUtf8("Написать в WhatsApp"), QMessageBox::ActionRole);
                    btnWa->setIcon(QIcon(":/svg/message-circle"));
                    btnWa->setIconSize(QSize(16, 16));
                    btnWa->setCursor(Qt::PointingHandCursor);
                    btnWa->setStyleSheet(
                        "QPushButton {"
                        "   background-color: #059669;"
                        "   color: #FFFFFF;"
                        "   font-size: 12px;"
                        "   font-weight: bold;"
                        "   border: 1px solid #059669;"
                        "   border-radius: 0px;"
                        "   padding: 6px 16px;"
                        "   min-width: 160px;"
                        "}"
                        "QPushButton:hover { background-color: #10B981; border-color: #34D399; }"
                        "QPushButton:pressed { background-color: #047857; }");

                    QPushButton *btnClose = msgBox.addButton(QString::fromUtf8("Закрыть"), QMessageBox::RejectRole);
                    btnClose->setCursor(Qt::PointingHandCursor);
                    msgBox.setDefaultButton(btnWa);

                    msgBox.exec();

                    if(msgBox.clickedButton() == btnWa)
                    {
                        this->on_action_WhatsApp_triggered();
                    }
                });

            // Кнопка "Добавить VIP"
            QPushButton *btnAddVip = new QPushButton(QString::fromUtf8("+ Добавить VIP"), ui->frame_7);
            btnAddVip->setObjectName("buttonAddVip");
            btnAddVip->setCursor(Qt::PointingHandCursor);
            btnAddVip->setFixedHeight(26);
            btnAddVip->setToolTip(QString::fromUtf8("Оформить или продлить VIP-статус"));
            topRow->addWidget(btnAddVip, 0, Qt::AlignRight | Qt::AlignVCenter);

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

            f7MainLayout->addLayout(topRow);

            // Subtle 1px Horizontal Divider
            QFrame *divider = new QFrame(ui->frame_7);
            divider->setObjectName("cabinetHorizontalDivider");
            divider->setFrameShape(QFrame::HLine);
            divider->setFixedHeight(1);
            divider->setStyleSheet("background-color: #1E293B; max-height: 1px; border: none;");
            f7MainLayout->addWidget(divider);

            // Horizontal Account Reference Bar ("Справочник аккаунта")
            QFrame *refBar = new QFrame(ui->frame_7);
            refBar->setObjectName("cabinetHorizontalRefPanel");
            refBar->setStyleSheet("background: transparent; border: none;");

            QHBoxLayout *refLayout = new QHBoxLayout(refBar);
            refLayout->setContentsMargins(0, 0, 0, 0);
            refLayout->setSpacing(6);

            auto createChip = [refBar, refLayout](const QString &iconRes, const QString &caption, const QString &valObjName, const QString &initVal, const QString &valColor = "#F8FAFC")
            {
                QFrame *chip = new QFrame(refBar);
                chip->setObjectName("cabinetSideRow");
                chip->setProperty("cabinetChip", true);
                chip->setStyleSheet("QFrame#cabinetSideRow { background-color: #070B14; border: 1px solid #1E293B; border-radius: 0px; padding: 2px 6px; } QFrame#cabinetSideRow:hover { border-color: #334155; background-color: #0E1526; }");

                QHBoxLayout *cl = new QHBoxLayout(chip);
                cl->setContentsMargins(6, 3, 6, 3);
                cl->setSpacing(6);

                QLabel *iconLbl = new QLabel(chip);
                iconLbl->setObjectName("cabinetSideIcon");
                iconLbl->setFixedSize(18, 18);
                iconLbl->setAlignment(Qt::AlignCenter);
                iconLbl->setPixmap(QPixmap(iconRes).scaled(14, 14, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                iconLbl->setStyleSheet("background: transparent; border: none;");
                cl->addWidget(iconLbl);

                QVBoxLayout *vl = new QVBoxLayout();
                vl->setContentsMargins(0, 0, 0, 0);
                vl->setSpacing(1);

                QLabel *capLbl = new QLabel(caption, chip);
                capLbl->setObjectName("cabinetSideCaption");
                capLbl->setStyleSheet("color: #64748B; font-size: 8.5px; font-weight: 700; letter-spacing: 0.5px; text-transform: uppercase; background: transparent; border: none;");
                vl->addWidget(capLbl);

                QLabel *valLbl = new QLabel(initVal, chip);
                valLbl->setObjectName(valObjName);
                valLbl->setProperty("cabinetVal", true);
                valLbl->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold; background: transparent; border: none;").arg(valColor));
                vl->addWidget(valLbl);

                cl->addLayout(vl);
                refLayout->addWidget(chip, 1);
            };

            createChip(":/svg/credit-card", QString::fromUtf8("Баланс"), "cabinetVal_credits", "-", "#34D399");
            createChip(":/svg/crown", QString::fromUtf8("VIP-статус"), "cabinetVal_vip", "-", "#FBBF24");
            createChip(":/svg/smartphone", QString::fromUtf8("Устройства"), "cabinetVal_devices", "-", "#38BDF8");
            createChip(":/svg/globe", QString::fromUtf8("Локация"), "cabinetVal_location", "-", "#94A3B8");
            createChip(":/svg/shield", QString::fromUtf8("Безопасность"), "cabinetVal_status", "-", "#34D399");
            createChip(":/svg/refresh-cw", QString::fromUtf8("Время входа"), "cabinetVal_loginTime", "-", "#94A3B8");

            // Hidden login reference label for findChild<QLabel*>("cabinetVal_login")
            QLabel *hiddenLogin = new QLabel(ui->frame_7);
            hiddenLogin->setObjectName("cabinetVal_login");
            hiddenLogin->hide();

            f7MainLayout->addWidget(refBar);
        }
    }

    if(ui->label_7)
    {
        ui->label_7->setText(QString::fromUtf8("ДОСТУПНЫЕ СЕРВИСЫ"));
    }
    if(ui->toplevel_layout_auth_2 && !ui->toplevel_up_2->findChild<QLineEdit *>("serviceSearchEdit"))
    {
        ui->toplevel_layout_auth_2->setContentsMargins(14, 4, 14, 4);
        ui->toplevel_layout_auth_2->setSpacing(10);
        ui->toplevel_layout_auth_2->addStretch(1);

        QLineEdit *searchEdit = new QLineEdit(ui->toplevel_up_2);
        searchEdit->setObjectName("serviceSearchEdit");
        searchEdit->setPlaceholderText(QString::fromUtf8("Поиск сервисов..."));
        searchEdit->setClearButtonEnabled(true);
        searchEdit->setFixedWidth(240);
        searchEdit->setFixedHeight(26);
        searchEdit->setCursor(Qt::IBeamCursor);
        searchEdit->addAction(QIcon(":/svg/search"), QLineEdit::LeadingPosition);
        searchEdit->setStyleSheet(
            "QLineEdit#serviceSearchEdit {"
            "   background-color: #070B14;"
            "   border: 1px solid #1E293B;"
            "   border-radius: 0px;"
            "   color: #F8FAFC;"
            "   font-size: 11px;"
            "   padding: 2px 8px 2px 4px;"
            "}"
            "QLineEdit#serviceSearchEdit:hover { border-color: #334155; background-color: #0F172A; }"
            "QLineEdit#serviceSearchEdit:focus { border-color: #38BDF8; background-color: #0F172A; }");

        ui->toplevel_layout_auth_2->addWidget(searchEdit, 0, Qt::AlignRight | Qt::AlignVCenter);

        QObject::connect(searchEdit, &QLineEdit::textChanged, this, [this]() {
            this->applyServiceFilters();
        });
    }

    if(ui->authInfo)
    {
        ui->authInfo->setVisible(false);
    }

    if(ui->sss && ui->serviceContents)
    {
        if(services.isEmpty())
        {
            if(auto *btn18 = ui->serviceContents->findChild<QPushButton *>("button_RemoveADSMalware_18"))
            {
                ui->serviceContents->layout()->removeWidget(btn18);
                btn18->deleteLater();
            }
            if(auto *btn19 = ui->serviceContents->findChild<QPushButton *>("button_RemoveADSMalware_19"))
            {
                ui->serviceContents->layout()->removeWidget(btn19);
                btn19->deleteLater();
            }
        }

        while(ui->sss->count() > 0)
        {
            QLayoutItem *item = ui->sss->takeAt(0);
            if(item->widget() && item->widget() != ui->serviceContents)
            {
                item->widget()->deleteLater();
            }
            delete item;
        }

        ui->sss->setContentsMargins(14, 8, 14, 16);
        ui->sss->setSpacing(16);

        // Vertical Quick Filters Panel
        QFrame *filterPanel = new QFrame(ui->scrollAreaWidgetContents_3);
        filterPanel->setObjectName("cabinetFilterPanel");
        filterPanel->setFixedWidth(180);

        QVBoxLayout *fLayout = new QVBoxLayout(filterPanel);
        fLayout->setContentsMargins(8, 10, 8, 10);
        fLayout->setSpacing(6);

        QLabel *filterTitle = new QLabel(QString::fromUtf8("ФИЛЬТР СЕРВИСОВ"), filterPanel);
        filterTitle->setObjectName("cabinetFilterTitle");
        fLayout->addWidget(filterTitle);

        QButtonGroup *filterGroup = new QButtonGroup(this);
        filterGroup->setObjectName("serviceFilterGroup");
        filterGroup->setExclusive(true);

        auto addFilterBtn = [this, filterPanel, fLayout, filterGroup](const QString &mode, const QString &text, const QString &iconPath, bool checked = false)
        {
            QPushButton *btn = new QPushButton(text, filterPanel);
            btn->setObjectName("cabinetFilterBtn");
            btn->setProperty("filterMode", mode);
            btn->setCheckable(true);
            btn->setChecked(checked);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setFixedHeight(32);
            btn->setIcon(QIcon(iconPath));
            btn->setIconSize(QSize(16, 16));

            filterGroup->addButton(btn);
            fLayout->addWidget(btn);

            QObject::connect(btn, &QPushButton::toggled, this, [this](bool c) {
                if(c) this->applyServiceFilters();
            });
            return btn;
        };

        addFilterBtn("all", QString::fromUtf8("Все сервисы"), ":/svg/shuffle", true);
        addFilterBtn("available", QString::fromUtf8("Доступные"), ":/svg/check-circle");
        addFilterBtn("unavailable", QString::fromUtf8("Не доступные"), ":/svg/services/unavailable");
        addFilterBtn("free", QString::fromUtf8("Бесплатные"), ":/svg/tag");
        addFilterBtn("paid", QString::fromUtf8("Платные"), ":/svg/credit-card");

        fLayout->addStretch(1);

        ui->sss->insertStretch(0, 1);
        ui->sss->addWidget(ui->serviceContents, 0, Qt::AlignTop);
        ui->sss->addWidget(filterPanel, 0, Qt::AlignTop);
        ui->sss->addStretch(1);

        this->createAppleServiceButton();
        this->applyServiceFilters();
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
            "  <div style=\"background: #1E293B; border: 1px solid #334155; border-radius: 0px; padding: 9px 12px; margin-bottom: 8px;\">"
            "    <span style=\"background: #0284C7; color: #FFFFFF; border-radius: 0px; padding: 2px 8px; font-weight: bold; font-size: 11px;\">1</span>"
            "    <strong style=\"color: #F8FAFC; font-size: 12.5px; margin-left: 6px;\">Режим разработчика</strong>"
            "    <p style=\"margin: 4px 0 0 24px; color: #94A3B8; font-size: 11.5px; line-height: 1.4;\">"
            "      Откройте <b>Настройки</b> &rarr; <b>О телефоне</b>. Найдите <b>Номер сборки</b> (или версию MIUI/HyperOS) и нажмите на него <b>7 раз</b> подряд."
            "    </p>"
            "  </div>"
            "  <div style=\"background: #1E293B; border: 1px solid #334155; border-radius: 0px; padding: 9px 12px; margin-bottom: 8px;\">"
            "    <span style=\"background: #0284C7; color: #FFFFFF; border-radius: 0px; padding: 2px 8px; font-weight: bold; font-size: 11px;\">2</span>"
            "    <strong style=\"color: #F8FAFC; font-size: 12.5px; margin-left: 6px;\">Включите отладку по USB</strong>"
            "    <p style=\"margin: 4px 0 0 24px; color: #94A3B8; font-size: 11.5px; line-height: 1.4;\">"
            "      Перейдите в <b>Настройки</b> &rarr; <b>Для разработчиков</b> и активируйте тумблер <b>Отладка по USB</b> (для Xiaomi также «Установка через USB»)."
            "    </p>"
            "  </div>"
            "  <div style=\"background: #1E293B; border: 1px solid #334155; border-radius: 0px; padding: 9px 12px; margin-bottom: 8px;\">"
            "    <span style=\"background: #0284C7; color: #FFFFFF; border-radius: 0px; padding: 2px 8px; font-weight: bold; font-size: 11px;\">3</span>"
            "    <strong style=\"color: #F8FAFC; font-size: 12.5px; margin-left: 6px;\">Подключите кабель к ПК</strong>"
            "    <p style=\"margin: 4px 0 0 24px; color: #94A3B8; font-size: 11.5px; line-height: 1.4;\">"
            "      Соедините устройство кабелем. На экране телефона появится запрос &mdash; отметьте <b>«Всегда разрешать с этого компьютера»</b> и нажмите <b>ОК</b>."
            "    </p>"
            "  </div>"
            "  <div style=\"background: rgba(30, 41, 59, 0.4); border: 1px dashed #334155; border-radius: 0px; padding: 8px 12px; margin-top: 6px;\">"
            "    <span style=\"color: #38BDF8; font-size: 11.5px; font-weight: 600;\"><img src=\":/svg/lightbulb\" width=\"13\" height=\"13\" style=\"vertical-align:middle;\"/> Телефон не определяется?</span>"
            "    <p style=\"margin: 3px 0 0 0; color: #64748B; font-size: 11px; line-height: 1.35;\">"
            "      Смените режим подключения USB на <b>«Передача файлов (MTP)»</b> либо подключите кабель в другой USB-порт на ПК."
            "    </p>"
            "  </div>"
            "</div>"
            "</body></html>");
    }
    if(ui->label_3)
    {
        ui->label_3->setText("<a style=\"color: #38BDF8; text-decoration: none; font-size: 12px; font-weight: 500;\" href=\"https://www.anymp4.com/ru/faq/enable-usb-debugging-for-android.html\"><img src=\":/svg/clipboard\" width=\"13\" height=\"13\" style=\"vertical-align:middle;\"/> Подробная пошаговая инструкция с иллюстрациями &rarr;</a>");
    }
    if(ui->label_5)
    {
        ui->label_5->setText(QString::fromUtf8("Поиск подключенного Android-устройства..."));
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
