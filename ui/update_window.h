#ifndef UPDATE_WINDOW_H
#define UPDATE_WINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QLabel>
#include <QProgressBar>
#include <functional>

class UpdateWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit UpdateWindow(QWidget *parent = nullptr);
    ~UpdateWindow();

    void setText(const QString &value);
    void setProgress(int v1, int v2);
    void delayPush(int ms, std::function<void()> call, bool loop = false);

private:
    QLabel *label;
    QProgressBar *progressBarCurrent;
    QProgressBar *progressBarTotal;
};

#endif // UPDATE_WINDOW_H
