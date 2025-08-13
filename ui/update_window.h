#ifndef UPDATE_WINDOW_H
#define UPDATE_WINDOW_H

#include <QMainWindow>
#include <QTimer>

namespace Ui {
class UpdateWindow;
}

class update_window : public QMainWindow
{
    Q_OBJECT

public:
    explicit update_window(QWidget *parent = nullptr);
    ~update_window();

    void setText(const QString &value);
    void setProgress(int v1, int v2);
    void delayPush(int ms, std::function<void()> call, bool loop = false);
private:
    Ui::UpdateWindow *ui;
};

#endif // UPDATE_WINDOW_H
