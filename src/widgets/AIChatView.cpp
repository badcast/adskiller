#include "AIChatView.h"
#include <QRegularExpression>
#include <QScrollBar>

static QString formatMarkdown(const QString &raw)
{
    QString s = raw.toHtmlEscaped();
    s.replace("\r\n", "\n");

    // Code blocks ```...```
    static QRegularExpression blockCodeRe(R"(```(?:\w+)?\n?([\s\S]*?)```)");
    s.replace(blockCodeRe, "<pre style='background:#14161A; color:#4CC2FF; padding:6px 8px; border-radius:6px; border:1px solid #282B32; font-family:Consolas, monospace; font-size:10.5px; margin:4px 0;'>\\1</pre>");

    // Inline code `...`
    static QRegularExpression codeRe(R"(`([^`]+)`)");
    s.replace(codeRe, "<code style='background:rgba(0,0,0,0.35); color:#4CC2FF; padding:1px 5px; border-radius:3px; font-family:Consolas, monospace; font-size:11px;'>\\1</code>");

    // Bold **...**
    static QRegularExpression boldRe(R"(\*\*(.+?)\*\*)");
    s.replace(boldRe, "<b style='color:#FFFFFF;'>\\1</b>");

    // Italic *...*
    static QRegularExpression italicRe(R"((?<!\*)\*([^*]+?)\*(?!\*))");
    s.replace(italicRe, "<i style='color:#BAC0CB;'>\\1</i>");

    // Bullet points (- or * at start of line)
    static QRegularExpression bulletRe(R"((?:^|\n)[-*]\s+(.+))");
    s.replace(bulletRe, "<br/>&nbsp;&nbsp;<span style='color:#4CC2FF;'>•</span> \\1");

    // Numbered lists (1. , 2. at start of line)
    static QRegularExpression numRe(R"((?:^|\n)(\d+)\.\s+(.+))");
    s.replace(numRe, "<br/>&nbsp;&nbsp;<b style='color:#4CC2FF;'>\\1.</b> \\2");

    s.replace("\n", "<br/>");
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
            "   border: 1px solid #1A8CE6;"
            "   border-radius: 14px;"
            "   border-bottom-right-radius: 3px;"
            "}");

        QVBoxLayout *cardLayout = new QVBoxLayout(m_cardFrame);
        cardLayout->setContentsMargins(12, 8, 12, 7);
        cardLayout->setSpacing(3);

        m_textLabel = new QLabel(this);
        m_textLabel->setTextFormat(Qt::RichText);
        QString esc = text.toHtmlEscaped();
        esc.replace("\r\n", "<br/>");
        esc.replace("\n", "<br/>");
        m_textLabel->setText(QString("<span style='color:#FFFFFF; font-size:11.5px; line-height:1.45;'>%1</span>").arg(esc));
        m_textLabel->setWordWrap(true);
        m_textLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
        m_textLabel->setStyleSheet("background: transparent; border: none;");
        m_textLabel->setMaximumWidth(225);
        cardLayout->addWidget(m_textLabel);

        QString time = timeStr.isEmpty() ? QDateTime::currentDateTime().toString("HH:mm") : timeStr;
        m_timeLabel = new QLabel(time, this);
        m_timeLabel->setStyleSheet("color: rgba(255,255,255,0.7); font-size: 8.5px; font-weight: 500; background: transparent; border: none;");
        m_timeLabel->setAlignment(Qt::AlignRight);
        cardLayout->addWidget(m_timeLabel);

        rootLayout->addStretch();
        rootLayout->addWidget(m_cardFrame);
    }
    else if(type == AI)
    {
        m_cardFrame->setStyleSheet(
            "QFrame {"
            "   background-color: #1F2228;"
            "   border: 1px solid #2F333D;"
            "   border-radius: 14px;"
            "   border-bottom-left-radius: 3px;"
            "}");

        QVBoxLayout *cardLayout = new QVBoxLayout(m_cardFrame);
        cardLayout->setContentsMargins(12, 9, 12, 8);
        cardLayout->setSpacing(5);

        // Header with AI avatar badge and Copy button
        QHBoxLayout *headerLayout = new QHBoxLayout();
        headerLayout->setContentsMargins(0, 0, 0, 0);
        headerLayout->setSpacing(4);

        QLabel *badge = new QLabel(this);
        badge->setText("<span style='background: rgba(76,194,255,0.12); color: #4CC2FF; padding: 2px 7px; border-radius: 8px; font-weight: bold; font-size: 10px; border: 1px solid rgba(76,194,255,0.22);'>✦ AdsKiller AI</span>");
        badge->setTextFormat(Qt::RichText);
        badge->setStyleSheet("background: transparent; border: none;");
        headerLayout->addWidget(badge);

        headerLayout->addStretch();

        m_copyButton = new QPushButton("📋", this);
        m_copyButton->setToolTip("Скопировать ответ");
        m_copyButton->setFixedSize(24, 20);
        m_copyButton->setCursor(Qt::PointingHandCursor);
        m_copyButton->setStyleSheet(
            "QPushButton {"
            "   background: rgba(255,255,255,0.05);"
            "   color: #8E9297;"
            "   border: 1px solid #363940;"
            "   border-radius: 4px;"
            "   font-size: 11px;"
            "   padding: 1px;"
            "}"
            "QPushButton:hover {"
            "   color: #4CC2FF;"
            "   background: rgba(76,194,255,0.15);"
            "   border-color: #4CC2FF;"
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
        m_textLabel->setText(QString("<div style='color:#E5E7EB; font-size:11.5px; line-height:1.5;'>%1</div>").arg(formatMarkdown(text)));
        m_textLabel->setWordWrap(true);
        m_textLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
        m_textLabel->setStyleSheet("background: transparent; border: none;");
        m_textLabel->setMaximumWidth(235);
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
            "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #1E2530, stop:1 #171A20);"
            "   border: 1px solid #2B3A4E;"
            "   border-radius: 12px;"
            "}");

        QVBoxLayout *cardLayout = new QVBoxLayout(m_cardFrame);
        cardLayout->setContentsMargins(14, 14, 14, 14);
        cardLayout->setSpacing(4);

        QLabel *iconLabel = new QLabel("🤖", this);
        iconLabel->setAlignment(Qt::AlignCenter);
        iconLabel->setStyleSheet("font-size: 22px; background: transparent; border: none;");
        cardLayout->addWidget(iconLabel);

        QLabel *titleLabel = new QLabel("AdsKiller AI Assistant", this);
        titleLabel->setAlignment(Qt::AlignCenter);
        titleLabel->setStyleSheet("color: #4CC2FF; font-weight: bold; font-size: 13px; background: transparent; border: none;");
        cardLayout->addWidget(titleLabel);

        QLabel *descLabel = new QLabel("Задайте вопрос или нажмите любую кнопку-подсказку ниже", this);
        descLabel->setAlignment(Qt::AlignCenter);
        descLabel->setStyleSheet("color: #9CA3AF; font-size: 11px; background: transparent; border: none;");
        descLabel->setWordWrap(true);
        cardLayout->addWidget(descLabel);

        rootLayout->addWidget(m_cardFrame);
    }
    else if(type == Locked)
    {
        m_cardFrame->setStyleSheet(
            "QFrame {"
            "   background: rgba(0,0,0,0.2);"
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
        "   background-color: #1F2228;"
        "   border: 1px solid #2F333D;"
        "   border-radius: 12px;"
        "   border-bottom-left-radius: 3px;"
        "}");

    QHBoxLayout *cardLayout = new QHBoxLayout(m_cardFrame);
    cardLayout->setContentsMargins(12, 7, 14, 7);
    cardLayout->setSpacing(6);

    QLabel *badge = new QLabel("✦ AI", this);
    badge->setStyleSheet("color: #4CC2FF; font-weight: bold; font-size: 10.5px; background: transparent; border: none;");
    cardLayout->addWidget(badge);

    m_dotsLabel = new QLabel(this);
    m_dotsLabel->setTextFormat(Qt::RichText);
    m_dotsLabel->setText("<span style='color:#9CA3AF; font-size:10.5px;'>печатает <b style='color:#4CC2FF;'>●</b> • •</span>");
    m_dotsLabel->setStyleSheet("background: transparent; border: none;");
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
        m_dotsLabel->setText("<span style='color:#9CA3AF; font-size:10.5px;'>печатает <b style='color:#4CC2FF;'>●</b> • •</span>");
    else if(m_step == 1)
        m_dotsLabel->setText("<span style='color:#9CA3AF; font-size:10.5px;'>печатает • <b style='color:#4CC2FF;'>●</b> •</span>");
    else
        m_dotsLabel->setText("<span style='color:#9CA3AF; font-size:10.5px;'>печатает • • <b style='color:#4CC2FF;'>●</b></span>");
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
        "   background-color: #131417;"
        "   border: 1px solid #262930;"
        "   border-radius: 10px;"
        "}"
        "QScrollArea#aiChatMessagesArea QScrollBar:vertical {"
        "   width: 4px;"
        "   background: transparent;"
        "   margin: 0px;"
        "}"
        "QScrollArea#aiChatMessagesArea QScrollBar::handle:vertical {"
        "   background: #363940;"
        "   border-radius: 2px;"
        "   min-height: 18px;"
        "}"
        "QScrollArea#aiChatMessagesArea QScrollBar::handle:vertical:hover {"
        "   background: #4CC2FF;"
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
