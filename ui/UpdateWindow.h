#ifndef UPDATE_WINDOW_H
#define UPDATE_WINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QLabel>
#include <QProgressBar>
#include <functional>

#include <QMouseEvent>

#include <QPainter>
#include <QVector>

class RandomBlockProgress : public QWidget
{
    Q_OBJECT
public:
    explicit RandomBlockProgress(QWidget *parent = nullptr);
    void setValue(int value);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_value;
    QVector<int> m_randomIndices;
};

class UpdateWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit UpdateWindow(QWidget *parent = nullptr);
    ~UpdateWindow();

    void setText(const QString &value);
    void setStats(const QString &stats);
    void setProgress(int currentProgress, int totalProgress);
    void delayPush(int ms, std::function<void()> call, bool loop = false);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QLabel *titleLabel;
    QLabel *label;
    QLabel *jokeLabel;
    QLabel *statsLabel;
    QProgressBar *progressBarCurrent;
    RandomBlockProgress *blockProgressTotal;

    bool m_drag;
    QPoint m_dragPosition;
};

#endif // UPDATE_WINDOW_H
