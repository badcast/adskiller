#include "Services.h"
#include "mainwindow.h"
#include <QEventLoop>
#include <QMessageBox>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <functional>
#include <QTimer>
#include <QTextEdit>


QString AIAgentService::uuid() const
{
    return IDServiceAIAgentString;
}

AIAgentService::AIAgentService(QObject *parent) : Service(DeviceConnectType::None, parent)
{
}

bool AIAgentService::canStart()
{
    return Service::canStart();
}

bool AIAgentService::isStarted()
{
    return false;
}

bool AIAgentService::isFinish()
{
    return false;
}

bool AIAgentService::start()
{
    // Wire UI: find widgets by object name
    auto *btn = MainWindow::current->findChild<QPushButton *>("aiChatSend");
    auto *edit = MainWindow::current->findChild<QTextEdit *>("aiChatEdit");
    auto *messages = MainWindow::current->findChild<QTextEdit *>("aiChatMessages");

    if(!btn || !edit || !messages)
        return false;

    btn->setEnabled(true);
    edit->setEnabled(true);
    messages->setText("");
    messages->setStyleSheet("color: black; background: white;");

    // Prepare a reusable send function so both button and Enter-key can use it
    std::function<void()> doSend = [this, btn, edit, messages]() {
        if(!btn->isEnabled())
            return;
        QString text = edit->toPlainText().trimmed();
        if(text.isEmpty())
            return;

        // Store message in conversation history as compact JSON string
        QJsonObject userObj;
        userObj["role"] = "user";
        userObj["content"] = text;
        QString serialized = QString::fromUtf8(QJsonDocument(userObj).toJson(QJsonDocument::Compact));
        aiMessages.append(serialized);

        // Append user message with time and prefix
        QString time = QDateTime::currentDateTime().toString("HH:mm");
        QString esc = text.toHtmlEscaped();
        QString userHtml = QString("<div style='margin:8px; text-align:right;'><div style='display:inline-block; max-width:220px; min-height:28px; background:#d4f1c4; color:#000; padding:8px; border-radius:10px; font-size:12px;'><b>Вы:</b> %1</div><div style='font-size:9px;color:#999;margin-top:2px;'>%2</div></div>")
                               .arg(esc)
                               .arg(time);
        messages->append(userHtml);

        // Disable button and show sending state
        btn->setEnabled(false);
        QString prevText = btn->text();
        btn->setProperty("__prev_text", prevText);
        btn->setText("Отправка...");

        // Prepare request object from full conversation
        QJsonObject serviceReq;
        if(aiSessionId >= 0)
            serviceReq["sessionId"] = aiSessionId;

        QJsonArray msgs;
        for(const QString &s : aiMessages)
        {
            QJsonDocument d = QJsonDocument::fromJson(s.toUtf8());
            if(!d.isNull() && d.isObject())
                msgs.append(d.object());
        }
        serviceReq["messages"] = msgs;

        // Create transient Network object parented to MainWindow so it lives asynchronously
        Network *net = new Network(MainWindow::current->network);

        QObject::connect(net, &Network::sPullServiceUUID, this, &AIAgentService::slotPullMessage);
        QObject::connect(net, &Network::sPullServiceUUID, net, &QObject::deleteLater);

        net->pullServiceUUID(uuid(), serviceReq, ServiceOperation::Get);

        // clear input
        edit->clear();
        
        // Add typing placeholder and start animation
        aiTypingId++;
        aiTypingSpanId = QString("AI_TYPING_%1").arg(aiTypingId);
        aiTypingDots = 1;
        QString typingSpan = QString("<span id='%1'>%2</span>").arg(aiTypingSpanId).arg("...");
        QString typingHtml = QString("<div style='margin:6px; text-align:left;'><div style='display:inline-block; background:#f1f0f0; padding:8px; border-radius:8px; font-style:italic; color:#666; min-height:20px;'><b>ИИ:</b> ") + typingSpan + "</div></div>";
        messages->append(typingHtml);

        // Start timer for dots animation
        QTimer *t = new QTimer(this);
        t->setInterval(500);
        QObject::connect(t, &QTimer::timeout, MainWindow::current, [messages, this]() {
            // rotate dots 1..3
            aiTypingDots = (aiTypingDots % 3) + 1;
            QString dots;
            for(int i = 0; i < aiTypingDots; ++i)
                dots += '.';
            QString startTag = QString("<span id='%1'>").arg(aiTypingSpanId);
            QString toHtml = messages->toHtml();
            int pos = toHtml.indexOf(startTag);
            if(pos != -1)
            {
                int startContent = pos + startTag.length();
                int endPos = toHtml.indexOf("</span>", startContent);
                if(endPos != -1)
                {
                    toHtml.replace(startContent, endPos - startContent, dots);
                    messages->setHtml(toHtml);
                }
            }
        });
        t->start();
        aiTypingTimer = t;
    };

    // Connect button click to send
    QObject::connect(btn, &QPushButton::clicked, [doSend]() { doSend(); });

    // Event filter to handle Enter key (without Shift/Ctrl/Alt) as send
    class EnterFilter : public QObject
    {
    public:
        EnterFilter(std::function<void()> f, QObject *parent = nullptr) : QObject(parent), fn(std::move(f))
        {
        }
        bool eventFilter(QObject *obj, QEvent *ev) override
        {
            if(ev->type() == QEvent::KeyPress)
            {
                QKeyEvent *ke = static_cast<QKeyEvent *>(ev);
                if((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) && !(ke->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier)))
                {
                    if(fn)
                        fn();
                    return true;
                }
            }
            return QObject::eventFilter(obj, ev);
        }

    private:
        std::function<void()> fn;
    };

    // Install filter on edit
    EnterFilter *filter = new EnterFilter(doSend, edit);
    edit->installEventFilter(filter);

    return true;
}

void AIAgentService::stop()
{
    if(aiTypingTimer)
    {
        aiTypingTimer->stop();
        aiTypingTimer->deleteLater();
        aiTypingTimer = nullptr;
    }
}

void AIAgentService::slotPullMessage(const QJsonObject responce, const QString guid, ServiceOperation so, bool ok)
{
    Q_UNUSED(guid)
    Q_UNUSED(so)
    if(!ok)
    {
        // stop typing timer and remove placeholder
        if(aiTypingTimer)
        {
            aiTypingTimer->stop();
            aiTypingTimer->deleteLater();
            aiTypingTimer = nullptr;
        }
        // remove typing placeholder HTML
        auto *messagesWidget = MainWindow::current->findChild<QTextEdit *>("aiChatMessages");
        if(messagesWidget && !aiTypingSpanId.isEmpty())
        {
            QString toHtml = messagesWidget->toHtml();
            QString startTag = QString("<span id='%1'>").arg(aiTypingSpanId);
            int pos = toHtml.indexOf(startTag);
            if(pos != -1)
            {
                int startOuter = toHtml.lastIndexOf("<div style='margin:6px; text-align:left;'", pos);
                int endPos = toHtml.indexOf("</div></div>", pos);
                if(startOuter != -1 && endPos != -1)
                {
                    toHtml.remove(startOuter, endPos + QString("</div></div>").length() - startOuter);
                    messagesWidget->setHtml(toHtml);
                }
            }
        }

        QMessageBox::warning(MainWindow::current, "AI Agent", "Ошибка сети или сервера при обращении к AI сервису.");
        // restore button
        auto *btn = MainWindow::current->findChild<QPushButton *>("aiChatSend");
        if(btn)
        {
            QString prev = btn->property("__prev_text").toString();
            if(!prev.isEmpty())
                btn->setText(prev);
            btn->setEnabled(true);
        }
        return;
    }

    // Expecting result object with fields like ai_status, response, sessionId
    QString text;
    if(!responce["response"].isNull())
        text = responce["response"].toString();
    else if(!responce["answer"].isNull())
        text = responce["answer"].toString();

    if(text.isEmpty())
        text = "(пустой ответ)";

    // stop typing timer and remove placeholder
    if(aiTypingTimer)
    {
        aiTypingTimer->stop();
        aiTypingTimer->deleteLater();
        aiTypingTimer = nullptr;
    }
    // remove typing placeholder HTML
    if(!aiTypingSpanId.isEmpty())
    {
        auto *messagesWidget = MainWindow::current->findChild<QTextEdit *>("aiChatMessages");
        if(messagesWidget)
        {
            QString toHtml = messagesWidget->toHtml();
            QString startTag = QString("<span id='%1'>").arg(aiTypingSpanId);
            int pos = toHtml.indexOf(startTag);
            if(pos != -1)
            {
                int startOuter = toHtml.lastIndexOf("<div style='margin:6px; text-align:left;'", pos);
                int endPos = toHtml.indexOf("</div></div>", pos);
                if(startOuter != -1 && endPos != -1)
                {
                    toHtml.remove(startOuter, endPos + QString("</div></div>").length() - startOuter);
                    messagesWidget->setHtml(toHtml);
                }
            }
        }
    }

    // Update sessionId if provided
    if(!responce["sessionId"].isNull())
    {
        aiSessionId = responce["sessionId"].toInt();
    }

    // Store assistant message in history
    QJsonObject aiObj;
    aiObj["role"] = "assistant";
    aiObj["content"] = text;
    QString serializedAi = QString::fromUtf8(QJsonDocument(aiObj).toJson(QJsonDocument::Compact));
    aiMessages.append(serializedAi);

    // Append AI message to chat view if available
    auto *messages = MainWindow::current->findChild<QTextEdit *>("aiChatMessages");
    if(messages)
    {
        QString time = QDateTime::currentDateTime().toString("HH:mm");
        QString esc = text.toHtmlEscaped();
        QString aiHtml = QString("<div style='margin:6px; text-align:left;'><div style='display:inline-block; background:#f1f0f0; padding:8px; border-radius:8px;'>🤖 %1</div><div style='font-size:9px;color:#999;'>%2</div></div>")
                            .arg(esc)
                            .arg(time);
        messages->append(aiHtml);
    }

    // Restore send button state
    auto *btn = MainWindow::current->findChild<QPushButton *>("aiChatSend");
    if(btn)
    {
        QString prev = btn->property("__prev_text").toString();
        if(!prev.isEmpty())
            btn->setText(prev);
        btn->setEnabled(true);
    }
}
