#pragma once

#include <QWidget>
#include <QFrame>
#include <QLabel>
#include <QHBoxLayout>
#include <QPropertyAnimation>
#include <QGraphicsDropShadowEffect>
#include <QMouseEvent>
#include <QPainter>
#include <QEnterEvent>

class PixelBlastSideButton : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int currentWidth READ currentWidth WRITE setCurrentWidth)

public:
    enum Side
    {
        Left,
        Right
    };

    explicit PixelBlastSideButton(Side side = Left, QWidget *parent = nullptr);
    ~PixelBlastSideButton() override = default;

    int currentWidth() const
    {
        return m_currentWidth;
    }
    void setCurrentWidth(int w);

    void setSide(Side side);
    Side side() const
    {
        return m_side;
    }

    void updatePosition();

signals:
    void clicked();

protected:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent *event) override;
#else
    void enterEvent(QEvent *event) override;
#endif
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void setupUi();
    void updateStyle();

    Side m_side = Left;
    int m_collapsedWidth = 52;
    int m_expandedWidth = 310;
    int m_height = 54;
    int m_currentWidth = 52;

    QFrame *m_container = nullptr;
    QLabel *m_iconLabel = nullptr;
    QLabel *m_textLabel = nullptr;
    QLabel *m_badgeLabel = nullptr;
    QPropertyAnimation *m_anim = nullptr;
    bool m_isHovered = false;
    bool m_isPressed = false;
};
