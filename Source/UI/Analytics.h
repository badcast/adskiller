#ifndef ANALYTICS_H
#define ANALYTICS_H

#include <QObject>
#include <QStringList>

class MainWindow;
class QEvent;

class Analytics final : public QObject
{
    friend class MainWindow;

public:
    explicit Analytics(MainWindow *window);
    ~Analytics() override;

private:
    QString collectData();
    bool eventFilter(QObject *watched, QEvent *event) override;

    MainWindow *m_window;
    QStringList m_events;
    qint64 m_lastMouseEventMs = 0;
};

#endif