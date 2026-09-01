#include "AIChatView.h"
#include <QRegularExpression>
#include <QScrollBar>

static QString formatMarkdown(const QString &raw)
{
    QString s = raw.toHtmlEscaped();
    s.replace("\r\n", "<br/>");
    s.replace("\n", "<br/>");
    static QRegularExpression boldRe(R"(\*\*(.+?)\*\*)");
    s.replace(boldRe, "<b style='color:#FFFFFF;'>\\1</b>");
    static QRegularExpression codeRe(R"(`([^`]+)`)");
    s.replace(codeRe, "<code style='background:rgba(0,0,0,0.35); color:#4CC2FF; padding:1px 4px; border-radius:3px; font-family:Consolas, monospace;'>\\1</code>");
    return s;
}

// -------------------------------------------------------------------
// AIChatBubble Implementation
// -------------------------------------------------------------------
AIChatBubble::AIChatBubble(Type type, const QString &text, const QString &timeStr, QWidget *parent) : QWidget(parent), m_type(type)
{
    QHBoxLayout *rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(4, 3, 4, 3);
    rootLayout->setSpacing(0);

    m_cardFrame = new QFrame(this);

    if(type == User)
    {
        m_cardFrame->setStyleSheet(
            "QFrame {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #0078D4, stop:1 #005A9E);"
            "   border: 1px solid #1084E3;"
            "   border-radius: 13px;"
            "   border-bottom-right-radius: 2px;"
            "}");

        QVBoxLayout *cardLayout = new QVBoxLayout(m_cardFrame);
        cardLayout->setContentsMargins(12, 9, 12, 7);
        cardLayout->setSpacing(4);

        m_textLabel = new QLabel(this);
        m_textLabel->setTextFormat(Qt::RichText);
        QString esc = text.toHtmlEscaped();
        esc.replace("\r\n", "<br/>");
        esc.replace("\n", "<br/>");
        m_textLabel->setText(QString("<span style='color:#FFFFFF; font-size:11.5px; line-height:1.4;'>%1</span>").arg(esc));
        m_textLabel->setWordWrap(true);
        m_textLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
        m_textLabel->setStyleSheet("background: transparent; border: none;");
        m_textLabel->setMaximumWidth(225);
        cardLayout->addWidget(m_textLabel);

        QString time = timeStr.isEmpty() ? QDateTime::currentDateTime().toString("HH:mm") : timeStr;
        m_timeLabel = new QLabel(time, this);
        m_timeLabel->setStyleSheet("color: rgba(255,255,255,0.65); font-size: 8.5px; background: transparent; border: none;");
        m_timeLabel->setAlignment(Qt::AlignRight);
        cardLayout->addWidget(m_timeLabel);

        rootLayout->addStretch();
        rootLayout->addWidget(m_cardFrame);
    }
    else if(type == AI)
    {
        m_cardFrame->setStyleSheet(
            "QFrame {"
            "   background-color: #26282D;"
            "   border: 1px solid #383A40;"
            "   border-radius: 13px;"
            "   border-bottom-left-radius: 2px;"
            "}");

        QVBoxLayout *cardLayout = new QVBoxLayout(m_cardFrame);
        cardLayout->setContentsMargins(12, 10, 12, 8);
        cardLayout->setSpacing(5);

        // Header with AI avatar badge and Copy button
        QHBoxLayout *headerLayout = new QHBoxLayout();
        headerLayout->setContentsMargins(0, 0, 0, 0);
        headerLayout->setSpacing(4);

        QLabel *badge = new QLabel("✦ AdsKiller AI", this);
        badge->setStyleSheet("color: #4CC2FF; font-weight: bold; font-size: 10.5px; background: transparent; border: none;");
        headerLayout->addWidget(badge);

        headerLayout->addStretch();

        m_copyButton = new QPushButton("📋", this);
        m_copyButton->setToolTip("Скопировать ответ");
        m_copyButton->setFixedSize(22, 19);
        m_copyButton->setCursor(Qt::PointingHandCursor);
        m_copyButton->setStyleSheet(
            "QPushButton {"
            "   background: transparent;"
            "   color: #8E9297;"
            "   border: none;"
            "   font-size: 10.5px;"
            "   padding: 1px;"
            "}"
            "QPushButton:hover {"
            "   color: #4CC2FF;"
            "   background: rgba(255,255,255,0.08);"
            "   border-radius: 3px;"
            "}");

        connect(
            m_copyButton,
            &QPushButton::clicked,
            this,
            [this, text]()
            {
                QClipboard *clipboard = QApplication::clipboard();
                if(clipboard)
                    clipboard->setText(text);
                m_copyButton->setText("✓");
                QTimer::singleShot(
                    1500,
                    this,
                    [this]()
                    {
                        if(m_copyButton)
                            m_copyButton->setText("📋");
                    });
            });
        headerLayout->addWidget(m_copyButton);

        cardLayout->addLayout(headerLayout);

        m_textLabel = new QLabel(this);
        m_textLabel->setTextFormat(Qt::RichText);
        m_textLabel->setText(QString("<div style='color:#E3E5E8; font-size:11.5px; line-height:1.45;'>%1</div>").arg(formatMarkdown(text)));
        m_textLabel->setWordWrap(true);
        m_textLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
        m_textLabel->setStyleSheet("background: transparent; border: none;");
        m_textLabel->setMaximumWidth(230);
        cardLayout->addWidget(m_textLabel);

        QString time = timeStr.isEmpty() ? QDateTime::currentDateTime().toString("HH:mm") : timeStr;
        m_timeLabel = new QLabel(time, this);
        m_timeLabel->setStyleSheet("color: #72767D; font-size: 8.5px; background: transparent; border: none;");
        m_timeLabel->setAlignment(Qt::AlignRight);
        cardLayout->addWidget(m_timeLabel);

        rootLayout->addWidget(m_cardFrame);
        rootLayout->addStretch();
    }
    else if(type == Welcome)
    {
        m_cardFrame->setStyleSheet(
            "QFrame {"
            "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(76,194,255,0.1), stop:1 rgba(76,194,255,0.02));"
            "   border: 1px dashed rgba(76,194,255,0.3);"
            "   border-radius: 10px;"
            "}");

        QVBoxLayout *cardLayout = new QVBoxLayout(m_cardFrame);
        cardLayout->setContentsMargins(14, 12, 14, 12);
        cardLayout->setSpacing(3);

        QLabel *iconLabel = new QLabel("✨", this);
        iconLabel->setAlignment(Qt::AlignCenter);
        iconLabel->setStyleSheet("font-size: 18px; background: transparent; border: none;");
        cardLayout->addWidget(iconLabel);

        QLabel *titleLabel = new QLabel("AdsKiller AI Assistant", this);
        titleLabel->setAlignment(Qt::AlignCenter);
        titleLabel->setStyleSheet("color: #4CC2FF; font-weight: bold; font-size: 12px; background: transparent; border: none;");
        cardLayout->addWidget(titleLabel);

        QLabel *descLabel = new QLabel("Задайте вопрос или выберите быструю команду ниже", this);
        descLabel->setAlignment(Qt::AlignCenter);
        descLabel->setStyleSheet("color: #8E9297; font-size: 10px; background: transparent; border: none;");
        descLabel->setWordWrap(true);
        cardLayout->addWidget(descLabel);

        rootLayout->addWidget(m_cardFrame);
    }
    else if(type == Locked)
    {
        m_cardFrame->setStyleSheet(
            "QFrame {"
            "   background: rgba(0,0,0,0.15);"
            "   border: 1px solid #33363D;"
            "   border-radius: 10px;"
            "}");

        QVBoxLayout *cardLayout = new QVBoxLayout(m_cardFrame);
        cardLayout->setContentsMargins(14, 16, 14, 16);
        cardLayout->setSpacing(5);

        QLabel *iconLabel = new QLabel("🔒", this);
        iconLabel->setAlignment(Qt::AlignCenter);
        iconLabel->setStyleSheet("font-size: 20px; background: transparent; border: none;");
        cardLayout->addWidget(iconLabel);

        QLabel *descLabel = new QLabel("Войдите в систему для доступа к ИИ", this);
        descLabel->setAlignment(Qt::AlignCenter);
        descLabel->setStyleSheet("color: #8E9297; font-size: 11px; background: transparent; border: none;");
        cardLayout->addWidget(descLabel);

        rootLayout->addWidget(m_cardFrame);
    }
}

// -------------------------------------------------------------------
// AITypingIndicator Implementation
// -------------------------------------------------------------------
AITypingIndicator::AITypingIndicator(QWidget *parent) : QWidget(parent)
{
    QHBoxLayout *rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(4, 3, 4, 3);
    rootLayout->setSpacing(0);

    m_cardFrame = new QFrame(this);
    m_cardFrame->setStyleSheet(
        "QFrame {"
        "   background-color: #26282D;"
        "   border: 1px solid #383A40;"
        "   border-radius: 12px;"
        "   border-bottom-left-radius: 2px;"
        "}");

    QHBoxLayout *cardLayout = new QHBoxLayout(m_cardFrame);
    cardLayout->setContentsMargins(12, 7, 14, 7);
    cardLayout->setSpacing(6);

    QLabel *badge = new QLabel("✦ AI", this);
    badge->setStyleSheet("color: #4CC2FF; font-weight: bold; font-size: 10.5px; background: transparent; border: none;");
    cardLayout->addWidget(badge);

    m_dotsLabel = new QLabel("думает ● • •", this);
    m_dotsLabel->setStyleSheet("color: #8E9297; font-size: 10px; background: transparent; border: none;");
    cardLayout->addWidget(m_dotsLabel);

    rootLayout->addWidget(m_cardFrame);
    rootLayout->addStretch();

    m_timer = new QTimer(this);
    m_timer->setInterval(350);
    connect(m_timer, &QTimer::timeout, this, &AITypingIndicator::onTick);

    hide();
}

AITypingIndicator::~AITypingIndicator()
{
    if(m_timer)
        m_timer->stop();
}

void AITypingIndicator::start()
{
    m_step = 0;
    show();
    if(m_timer && !m_timer->isActive())
        m_timer->start();
}

void AITypingIndicator::stop()
{
    if(m_timer && m_timer->isActive())
        m_timer->stop();
    hide();
}

void AITypingIndicator::onTick()
{
    m_step = (m_step + 1) % 3;
    if(m_step == 0)
        m_dotsLabel->setText("думает ● • •");
    else if(m_step == 1)
        m_dotsLabel->setText("думает • ● •");
    else
        m_dotsLabel->setText("думает • • ●");
}

// -------------------------------------------------------------------
// AIChatView Implementation
// -------------------------------------------------------------------
AIChatView::AIChatView(QWidget *parent) : QScrollArea(parent)
{
    setObjectName("aiChatMessagesArea");
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    setStyleSheet(
        "QScrollArea#aiChatMessagesArea {"
        "   background-color: #1A1B1E;"
        "   border: 1px solid #2E3035;"
        "   border-radius: 8px;"
        "}"
        "QScrollArea#aiChatMessagesArea QScrollBar:vertical {"
        "   width: 4px;"
        "   background: transparent;"
        "   margin: 0px;"
        "}"
        "QScrollArea#aiChatMessagesArea QScrollBar::handle:vertical {"
        "   background: #484B52;"
        "   border-radius: 2px;"
        "   min-height: 16px;"
        "}"
        "QScrollArea#aiChatMessagesArea QScrollBar::handle:vertical:hover {"
        "   background: #6D7179;"
        "}"
        "QScrollArea#aiChatMessagesArea QScrollBar::add-line:vertical, "
        "QScrollArea#aiChatMessagesArea QScrollBar::sub-line:vertical {"
        "   height: 0px;"
        "}");

    m_container = new QWidget(this);
    m_container->setStyleSheet("background: transparent;");
    m_layout = new QVBoxLayout(m_container);
    m_layout->setContentsMargins(8, 8, 8, 8);
    m_layout->setSpacing(8);

    m_typingIndicator = new AITypingIndicator(m_container);
    m_layout->addWidget(m_typingIndicator);

    m_layout->addStretch();
    setWidget(m_container);
}

void AIChatView::addUserMessage(const QString &text, const QString &time)
{
    AIChatBubble *bubble = new AIChatBubble(AIChatBubble::User, text, time, m_container);
    // Insert before typing indicator and stretch
    int insertIndex = qMax(0, m_layout->count() - 2);
    m_layout->insertWidget(insertIndex, bubble);
    scrollToBottom();
}

void AIChatView::addAIMessage(const QString &text, const QString &time)
{
    AIChatBubble *bubble = new AIChatBubble(AIChatBubble::AI, text, time, m_container);
    int insertIndex = qMax(0, m_layout->count() - 2);
    m_layout->insertWidget(insertIndex, bubble);
    scrollToBottom();
}

void AIChatView::showTyping(bool show)
{
    if(m_typingIndicator)
    {
        if(show)
            m_typingIndicator->start();
        else
            m_typingIndicator->stop();
    }
    scrollToBottom();
}

void AIChatView::showWelcome()
{
    clearAll();
    AIChatBubble *welcome = new AIChatBubble(AIChatBubble::Welcome, "", "", m_container);
    int insertIndex = qMax(0, m_layout->count() - 2);
    m_layout->insertWidget(insertIndex, welcome);
    scrollToBottom();
}

void AIChatView::showLocked()
{
    clearAll();
    AIChatBubble *locked = new AIChatBubble(AIChatBubble::Locked, "", "", m_container);
    int insertIndex = qMax(0, m_layout->count() - 2);
    m_layout->insertWidget(insertIndex, locked);
    scrollToBottom();
}

void AIChatView::clearAll()
{
    if(m_typingIndicator)
        m_typingIndicator->stop();

    // Remove all widgets except typing indicator and spacer stretch
    for(int i = m_layout->count() - 1; i >= 0; --i)
    {
        QLayoutItem *item = m_layout->itemAt(i);
        if(item && item->widget() && item->widget() != m_typingIndicator)
        {
            QWidget *w = item->widget();
            m_layout->removeWidget(w);
            w->deleteLater();
        }
    }
}

void AIChatView::scrollToBottom()
{
    QTimer::singleShot(
        50,
        this,
        [this]()
        {
            if(verticalScrollBar())
                verticalScrollBar()->setValue(verticalScrollBar()->maximum());
        });
}
