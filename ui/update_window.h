#ifndef UPDATE_WINDOW_H
#define UPDATE_WINDOW_H

#include <QMainWindow>
#include <QTimer>

QT_BEGIN_NAMESPACE
namespace Ui {
class update_window;
}
QT_END_NAMESPACE

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
    Ui::update_window *ui;
};

#endif // UPDATE_WINDOW_H
