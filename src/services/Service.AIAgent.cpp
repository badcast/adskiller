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

AIAgentService::~AIAgentService()
{
    stop();
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

bool AIAgentService::eventFilter(QObject *obj, QEvent *ev)
{
    if(ev->type() == QEvent::KeyPress)
    {
        QKeyEvent *ke = static_cast<QKeyEvent *>(ev);
        if((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) && !(ke->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier)))
        {
            sendCurrentMessage();
            return true;
        }
    }
    return Service::eventFilter(obj, ev);
}

#include "AIChatView.h"

void AIAgentService::sendCurrentMessage()
{
    if(!MainWindow::current)
        return;

    auto *btn = MainWindow::current->findChild<QPushButton *>("aiChatSend");
    auto *edit = MainWindow::current->findChild<QTextEdit *>("aiChatEdit");
    auto *chatView = MainWindow::current->findChild<AIChatView *>("aiChatView");

    if(!btn || !edit || !btn->isEnabled())
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

    // Add user message bubble widget
    if(chatView)
    {
        chatView->addUserMessage(text);
        chatView->showTyping(true);
    }

    // Disable button and show sending state
    btn->setEnabled(false);
    QString prevText = btn->text();
    btn->setProperty("__prev_text", prevText);
    btn->setText("•••");

    // Prepare request object from full conversation
    QJsonObject serviceReq;
    if(aiSessionId >= 0)
        serviceReq["session_id"] = aiSessionId;

    serviceReq["message"] = aiMessages.last();

    // Create transient Network object parented to MainWindow so it lives asynchronously
    Network *net = new Network(MainWindow::current->network);

    QObject::connect(net, &Network::sPullServiceUUID, this, &AIAgentService::slotPullMessage);
    QObject::connect(net, &Network::sPullServiceUUID, net, &QObject::deleteLater);

    net->pullServiceUUID(uuid(), serviceReq, ServiceOperation::Get);

    // clear input
    edit->clear();
}

bool AIAgentService::start()
{
    if(!MainWindow::current)
        return false;

    auto *btn = MainWindow::current->findChild<QPushButton *>("aiChatSend");
    auto *edit = MainWindow::current->findChild<QTextEdit *>("aiChatEdit");
    auto *chatView = MainWindow::current->findChild<AIChatView *>("aiChatView");

    aiMessages.clear();

    if(!btn || !edit)
        return false;

    // Disconnect any existing connection to this slot to prevent duplicates
    btn->disconnect(this);
    edit->removeEventFilter(this);

    btn->setEnabled(true);
    edit->setEnabled(true);

    if(chatView)
        chatView->showWelcome();

    // Connect button click directly to slot with 'this' receiver context (auto-disconnected when this is destroyed)
    QObject::connect(btn, &QPushButton::clicked, this, &AIAgentService::sendCurrentMessage);

    // Install filter on edit
    edit->installEventFilter(this);

    return true;
}

void AIAgentService::stop()
{
    if(MainWindow::current)
    {
        auto *btn = MainWindow::current->findChild<QPushButton *>("aiChatSend");
        if(btn)
            btn->disconnect(this);
        auto *edit = MainWindow::current->findChild<QTextEdit *>("aiChatEdit");
        if(edit)
            edit->removeEventFilter(this);
        auto *chatView = MainWindow::current->findChild<AIChatView *>("aiChatView");
        if(chatView)
            chatView->showTyping(false);
    }
}

void AIAgentService::slotPullMessage(const QJsonObject responce, const QString guid, ServiceOperation so, bool ok)
{
    Q_UNUSED(guid)
    Q_UNUSED(so)
    auto *chatView = MainWindow::current ? MainWindow::current->findChild<AIChatView *>("aiChatView") : nullptr;
    if(chatView)
        chatView->showTyping(false);

    if(!ok || (!responce["status"].isUndefined() && responce["status"].toInteger() > 0))
    {
        QMessageBox::warning(MainWindow::current, "AI Agent", "Ошибка сети или сервера при обращении к AI сервису.");
        // restore button
        auto *btn = MainWindow::current ? MainWindow::current->findChild<QPushButton *>("aiChatSend") : nullptr;
        if(btn)
        {
            QString prev = btn->property("__prev_text").toString();
            if(!prev.isEmpty())
                btn->setText(prev);
            else
                btn->setText("Отправить");
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

    // Update sessionId if provided
    if(!responce["session_id"].isNull())
    {
        aiSessionId = responce["session_id"].toInt();
    }

    // Store assistant message in history
    QJsonObject aiObj;
    aiObj["role"] = "assistant";
    aiObj["content"] = text;
    QString serializedAi = QString::fromUtf8(QJsonDocument(aiObj).toJson(QJsonDocument::Compact));
    aiMessages.append(serializedAi);

    // Append AI message bubble widget
    if(chatView)
    {
        chatView->addAIMessage(text);
    }

    // Restore send button state
    auto *btn = MainWindow::current ? MainWindow::current->findChild<QPushButton *>("aiChatSend") : nullptr;
    if(btn)
    {
        QString prev = btn->property("__prev_text").toString();
        if(!prev.isEmpty())
            btn->setText(prev);
        else
            btn->setText("Отправить");
        btn->setEnabled(true);
    }
}
