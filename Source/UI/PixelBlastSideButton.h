#pragma once

#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QTimer>

class QPaintEvent;

class PixelBlastSideButton : public QWidget
{
    Q_OBJECT

public:
    explicit PixelBlastSideButton(QWidget *parent = nullptr);
    ~PixelBlastSideButton() override = default;

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void setupUi();

    QLabel *m_iconLabel = nullptr;
    QLabel *m_textLabel = nullptr;
    QLabel *m_actionLabel = nullptr;
    QTimer *m_borderTimer = nullptr;
    int m_borderAngle = 0;
    bool m_isPressed = false;
};
