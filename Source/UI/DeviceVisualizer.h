#ifndef DEVICEVISUALIZER_H
#define DEVICEVISUALIZER_H

#include <QWidget>
#include <QTimer>
#include <QPainter>
#include <QString>

#include "AdbFront.h"
#include "AppleFront.h"

class DeviceVisualizer : public QWidget
{
public:
    inline static DeviceVisualizer *s_adbVisualizer = nullptr;

    explicit DeviceVisualizer(QWidget *parent = nullptr);

    ~DeviceVisualizer() override;

    void setIsApple(bool apple);

    bool isApple() const;

    void setStatus(AdbConStatus s, const QString &name = QString(), const QString &sub = QString());

    void setStatus(AppleConStatus s, const QString &name = QString(), const QString &sub = QString());

    AdbConStatus status() const;

    void startAnimation();

    void stopAnimation();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QTimer *m_animTimer;
    AdbConStatus m_status;
    QString m_devName;
    QString m_devSub;
    float m_time;
    float m_connectedTime;
    bool m_isApple = false;
};

inline DeviceVisualizer *&s_adbVisualizer = DeviceVisualizer::s_adbVisualizer;

#endif // DEVICEVISUALIZER_H
