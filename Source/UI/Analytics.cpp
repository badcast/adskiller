#include "Analytics.h"

#include <QApplication>
#include <QCursor>
#include <QDateTime>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSysInfo>
#include <QWheelEvent>

#include "MainWindow.h"
#include "Services.h"

Analytics::Analytics(MainWindow *window) : QObject(window), m_window(window)
{
    if(qApp)
        qApp->installEventFilter(this);
}

Analytics::~Analytics()
{
    if(qApp)
        qApp->removeEventFilter(this);
}

QString Analytics::collectData()
{
    QStringList data;
    const QDateTime now = QDateTime::currentDateTime();
    data << "Local timestamp = " + now.toString(Qt::ISODateWithMs) << "Operating system = " + QSysInfo::prettyProductName() << "OS type = " + QSysInfo::productType() << "OS version = " + QSysInfo::productVersion() << "Kernel = " + QSysInfo::kernelType() + " " + QSysInfo::kernelVersion()
         << "Architecture = " + QSysInfo::currentCpuArchitecture();

    const std::shared_ptr<Service> service = ServiceProvider::currentService();
    if(service)
    {
        data << "Service info:";
        data << service->getInternalData();
    }
    else
    {
        data << "No service is running; the user is on the main page (Cabinet) and viewing the service list with account information.";
    }

    if(!m_events.isEmpty())
    {
        data << "UI events:";
        data.append(m_events);
        m_events.clear();
    }

    return data.join('\n');
}

bool Analytics::eventFilter(QObject *watched, QEvent *event)
{
    if(!event)
        return false;

    if(!AppSetting::analyticsEnabled())
    {
        m_events.clear();
        return false;
    }

    QWidget *widget = qobject_cast<QWidget *>(watched);
    if(!widget || (widget != m_window && !m_window->isAncestorOf(widget)))
        return false;

    QString eventName;
    switch(event->type())
    {
        case QEvent::MouseMove:
        {
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            if(now - m_lastMouseEventMs < 100)
                return false;
            m_lastMouseEventMs = now;
            eventName = "mouse_move";
            break;
        }
        case QEvent::MouseButtonPress:
            eventName = "mouse_press";
            break;
        case QEvent::MouseButtonRelease:
            eventName = "mouse_release";
            break;
        case QEvent::MouseButtonDblClick:
            eventName = "mouse_double_click";
            break;
        case QEvent::Wheel:
            eventName = "mouse_wheel";
            break;
        case QEvent::KeyPress:
            eventName = "key_press";
            break;
        case QEvent::KeyRelease:
            eventName = "key_release";
            break;
        case QEvent::FocusIn:
            eventName = "focus_in";
            break;
        case QEvent::FocusOut:
            eventName = "focus_out";
            break;
        case QEvent::Enter:
            eventName = "pointer_enter";
            break;
        case QEvent::Leave:
            eventName = "pointer_leave";
            break;
        case QEvent::Move:
            if(widget == m_window)
                eventName = "window_move";
            break;
        case QEvent::Resize:
            if(widget == m_window)
                eventName = "window_resize";
            break;
        case QEvent::Show:
            for(auto iter = m_window->pages.cbegin(); iter != m_window->pages.cend(); ++iter)
            {
                if(iter.value() == widget)
                {
                    eventName = "page_show";
                    break;
                }
            }
            break;
        default:
            return false;
    }

    if(eventName.isEmpty())
        return false;

    const QPoint cursor = QCursor::pos();
    const QPoint widgetCursor = widget->mapFromGlobal(cursor);
    const QPoint windowCursor = widget->window()->mapFromGlobal(cursor);
    QJsonObject record;
    record["timestamp_local"] = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    record["event"] = eventName;
    record["widget"] = widget->objectName();
    record["widget_type"] = QString::fromLatin1(widget->metaObject()->className());
    record["cursor_screen_x"] = cursor.x();
    record["cursor_screen_y"] = cursor.y();
    record["cursor_window_x"] = windowCursor.x();
    record["cursor_window_y"] = windowCursor.y();
    record["cursor_widget_x"] = widgetCursor.x();
    record["cursor_widget_y"] = widgetCursor.y();
    record["window_x"] = m_window->geometry().x();
    record["window_y"] = m_window->geometry().y();
    record["window_width"] = m_window->width();
    record["window_height"] = m_window->height();

    if(event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)
    {
        const auto *keyEvent = static_cast<QKeyEvent *>(event);
        const QString keyText = keyEvent->text();
        if(keyText.isEmpty() || !keyText.at(0).isPrint())
            record["key_code"] = keyEvent->key();
        record["modifiers"] = static_cast<int>(keyEvent->modifiers());
    }
    else if(event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::MouseButtonDblClick)
    {
        const auto *mouseEvent = static_cast<QMouseEvent *>(event);
        record["button"] = static_cast<int>(mouseEvent->button());
        record["buttons"] = static_cast<int>(mouseEvent->buttons());
    }
    else if(event->type() == QEvent::Wheel)
    {
        const auto *wheelEvent = static_cast<QWheelEvent *>(event);
        record["wheel_x"] = wheelEvent->angleDelta().x();
        record["wheel_y"] = wheelEvent->angleDelta().y();
    }

    if(m_events.size() == 512)
        m_events.removeFirst();
    m_events.append(QString::fromUtf8(QJsonDocument(record).toJson(QJsonDocument::Compact)));
    return false;
}