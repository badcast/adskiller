#ifndef CIRCULARCLEANVISUALIZER_H
#define CIRCULARCLEANVISUALIZER_H

#include <QString>
#include <QTimer>
#include <QPixmap>
#include <QWidget>

// ============================================================================
// CircularCleanVisualizer
// Circular running ticker  with circular progress
// arc and dynamically updating app icon in the center.
// ============================================================================
class CircularCleanVisualizer : public QWidget
{
    Q_OBJECT

public:
    explicit CircularCleanVisualizer(QWidget *parent = nullptr);
    ~CircularCleanVisualizer() override;

    void setProgress(int percent);
    void setCurrentApp(const QString &appName, const QString &pkgName, const QPixmap &icon, const QString &statusText);
    void setRunning(bool running);
    void setCompleted(bool completed, const QString &summaryText);
    void setStopped(const QString &stoppedText = "Очистка остановлена");
    void reset();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onAnimationTick();

private:
    int m_progress = 0;
    qreal m_tickerAngle = 0.0;
    qreal m_particleAngle = 0.0;
    qreal m_iconScale = 1.0;
    qreal m_pulsePhase = 0.0;

    QString m_appName;
    QString m_pkgName;
    QPixmap m_appIcon;
    QString m_statusText;

    bool m_isRunning = false;
    bool m_isCompleted = false;

    QTimer *m_animTimer = nullptr;
    QString m_tickerText;
};

#endif // CIRCULARCLEANVISUALIZER_H
