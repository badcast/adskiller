#ifndef ADBDEVICEVISUALIZER_H
#define ADBDEVICEVISUALIZER_H

#include <QWidget>
#include <QTimer>
#include <QPainter>
#include <QString>

#include "adbfront.h"
#include "applefront.h"

class AdbDeviceVisualizer : public QWidget
{
public:
    inline static AdbDeviceVisualizer *s_adbVisualizer = nullptr;

    explicit AdbDeviceVisualizer(QWidget *parent = nullptr);

    ~AdbDeviceVisualizer() override;

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

inline AdbDeviceVisualizer *&s_adbVisualizer = AdbDeviceVisualizer::s_adbVisualizer;

#endif // ADBDEVICEVISUALIZER_H
