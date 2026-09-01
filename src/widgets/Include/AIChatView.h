#ifndef AICHATVIEW_H
#define AICHATVIEW_H

#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QTimer>
#include <QDateTime>
#include <QScrollBar>
#include <QClipboard>
#include <QApplication>

class AIChatBubble : public QWidget
{
    Q_OBJECT
public:
    enum Type
    {
        User,
        AI,
        Welcome,
        Locked
    };

    explicit AIChatBubble(Type type, const QString &text, const QString &timeStr = QString(), QWidget *parent = nullptr);

    Type type() const
    {
        return m_type;
    }

private:
    Type m_type;
    QFrame *m_cardFrame = nullptr;
    QLabel *m_textLabel = nullptr;
    QLabel *m_timeLabel = nullptr;
    QPushButton *m_copyButton = nullptr;
};

class AITypingIndicator : public QWidget
{
    Q_OBJECT
public:
    explicit AITypingIndicator(QWidget *parent = nullptr);
    ~AITypingIndicator() override;

    void start();
    void stop();

private slots:
    void onTick();

private:
    QFrame *m_cardFrame = nullptr;
    QLabel *m_dotsLabel = nullptr;
    QTimer *m_timer = nullptr;
    int m_step = 0;
};

class AIChatView : public QScrollArea
{
    Q_OBJECT
public:
    explicit AIChatView(QWidget *parent = nullptr);

    void addUserMessage(const QString &text, const QString &time = QString());
    void addAIMessage(const QString &text, const QString &time = QString());
    void showTyping(bool show);
    void showWelcome();
    void showLocked();
    void clearAll();
    void scrollToBottom();

private:
    QWidget *m_container = nullptr;
    QVBoxLayout *m_layout = nullptr;
    AITypingIndicator *m_typingIndicator = nullptr;
};

#endif // AICHATVIEW_H
