#ifndef CYBERREACTORLOADER_H
#define CYBERREACTORLOADER_H

#include <QWidget>
#include <QTimer>
#include <QElapsedTimer>
#include <QColor>
#include <QString>
#include <QStringList>
#include <QList>

class CyberReactorLoader : public QWidget
{
    Q_OBJECT

public:
    explicit CyberReactorLoader(QWidget *parent = nullptr);
    ~CyberReactorLoader() override;

    void setStatusText(const QString &text);
    QString statusText() const { return m_statusText; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private slots:
    void onAnimationTick();

private:
    QTimer *m_timer = nullptr;
    QElapsedTimer m_elapsed;

    qreal m_outerAngle = 0.0;
    qreal m_middleAngle = 0.0;
    qreal m_radarAngle = 0.0;
    qreal m_pulsePhase = 0.0;
    qreal m_shockwaveRadius = 15.0;
    qreal m_streamProgress = 0.0;

    int m_telemetryIndex = 0;
    qint64 m_lastTelemetrySwitch = 0;

    QString m_statusText;
    QStringList m_telemetryLines;

    struct Particle
    {
        qreal orbitRadius;
        qreal angle;
        qreal speed;
        qreal size;
        QColor color;
    };
    QList<Particle> m_particles;

    void initParticles();
};

#endif // CYBERREACTORLOADER_H
