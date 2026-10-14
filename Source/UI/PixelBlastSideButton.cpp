#include "PixelBlastSideButton.h"

#include <QIcon>
#include <QEasingCurve>
#include <QGraphicsDropShadowEffect>

PixelBlastSideButton::PixelBlastSideButton(Side side, QWidget *parent) : QWidget(parent), m_side(side)
{
    setObjectName("PixelBlastSideButton");
    setAttribute(Qt::WA_Hover, true);
    setCursor(Qt::PointingHandCursor);

    m_currentWidth = m_collapsedWidth;
    resize(m_currentWidth, m_height);

    m_anim = new QPropertyAnimation(this, "currentWidth", this);

    setupUi();
    updatePosition();
}

void PixelBlastSideButton::setSide(Side side)
{
    if(m_side == side)
        return;

    m_side = side;
    setupUi();
    updatePosition();
}

void PixelBlastSideButton::setupUi()
{
    // Clean up previous container if recreating for side change
    if(m_container)
    {
        delete m_container;
        m_container = nullptr;
    }

    m_container = new QFrame(this);
    m_container->setObjectName("pixelBlastSideContainer");
    m_container->resize(m_currentWidth, m_height);

    QHBoxLayout *layout = new QHBoxLayout(m_container);
    layout->setContentsMargins(6, 4, 8, 4);
    layout->setSpacing(8);

    m_iconLabel = new QLabel(m_container);
    m_iconLabel->setFixedSize(38, 38);
    m_iconLabel->setPixmap(QIcon(":/svg/services/pixelblast").pixmap(36, 36));
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setStyleSheet("background: transparent; border: none;");

    m_textLabel = new QLabel(m_container);
    m_textLabel->setTextFormat(Qt::RichText);
    m_textLabel->setText(
        QString::fromUtf8(
            "<div style='line-height: 110%;'>"
            "<span style='color: #38BDF8; font-family: \"Segoe UI Variable\", \"Segoe UI\", sans-serif; font-weight: 800; font-size: 12.5px; letter-spacing: 0.8px;'>PIXEL BLAST</span><br/>"
            "<span style='color: #FBBF24; font-family: \"Segoe UI Variable\", \"Segoe UI\", sans-serif; font-weight: 900; font-size: 11px; letter-spacing: 1px;'>ИГРАТЬ!!!</span>"
            "</div>"));
    m_textLabel->setStyleSheet("background: transparent; border: none;");
    m_textLabel->setAlignment(m_side == Left ? (Qt::AlignLeft | Qt::AlignVCenter) : (Qt::AlignRight | Qt::AlignVCenter));
    m_textLabel->setVisible(false);

    m_badgeLabel = new QLabel(m_container);
    m_badgeLabel->setText(m_side == Left ? QString::fromUtf8("▶") : QString::fromUtf8("◀"));
    m_badgeLabel->setStyleSheet("color: #FBBF24; font-size: 13px; font-weight: 900; background: transparent; border: none;");
    m_badgeLabel->setAlignment(Qt::AlignCenter);
    m_badgeLabel->setVisible(false);

    if(m_side == Left)
    {
        layout->addWidget(m_iconLabel);
        layout->addWidget(m_textLabel, 1);
        layout->addWidget(m_badgeLabel);
    }
    else
    {
        layout->addWidget(m_badgeLabel);
        layout->addWidget(m_textLabel, 1);
        layout->addWidget(m_iconLabel);
    }

    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(16);
    shadow->setColor(QColor(56, 189, 248, 80));
    shadow->setOffset(m_side == Left ? 2 : -2, 2);
    m_container->setGraphicsEffect(shadow);

    updateStyle();
}

void PixelBlastSideButton::updateStyle()
{
    if(!m_container)
        return;

    QString borderRadius;
    QString borderEdge;

    if(m_side == Left)
    {
        borderRadius = "border-top-right-radius: 0px; border-bottom-right-radius: 0px; border-left: none;";
        borderEdge = "border: 1px solid";
    }
    else
    {
        borderRadius = "border-top-left-radius: 0px; border-bottom-left-radius: 0px; border-right: none;";
        borderEdge = "border: 1px solid";
    }

    QString bgGradient;
    QString borderColor;

    if(m_isPressed)
    {
        bgGradient = "background-color: #0369A1;";
        borderColor = "#7DD3FC";
    }
    else if(m_isHovered)
    {
        bgGradient = "background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0F172A, stop:0.5 #1E293B, stop:1 #0284C7);";
        borderColor = "#38BDF8";
    }
    else
    {
        bgGradient = "background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #070A12, stop:0.7 #0B0F19, stop:1 #1E293B);";
        borderColor = "#1E293B";
    }

    m_container->setStyleSheet(QString(
                                   "QFrame#pixelBlastSideContainer {"
                                   "  %1"
                                   "  %2 %3;"
                                   "  %4"
                                   "}")
                                   .arg(bgGradient, borderEdge, borderColor, borderRadius));
}

void PixelBlastSideButton::setCurrentWidth(int w)
{
    m_currentWidth = w;

    if(m_side == Right && parentWidget())
    {
        move(parentWidget()->width() - m_currentWidth, y());
    }

    resize(m_currentWidth, m_height);

    if(m_container)
    {
        m_container->resize(m_currentWidth, m_height);
    }

    bool showText = (m_currentWidth > 85);
    if(m_textLabel)
        m_textLabel->setVisible(showText);
    if(m_badgeLabel)
        m_badgeLabel->setVisible(showText);

    update();
}

void PixelBlastSideButton::updatePosition()
{
    if(!parentWidget())
        return;

    int parentH = parentWidget()->height();
    int targetY = qMax(80, (parentH - m_height) / 2);

    if(m_side == Left)
    {
        move(0, targetY);
    }
    else
    {
        move(parentWidget()->width() - m_currentWidth, targetY);
    }

    raise();
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void PixelBlastSideButton::enterEvent(QEnterEvent *event)
#else
void PixelBlastSideButton::enterEvent(QEvent *event)
#endif
{
    m_isHovered = true;
    updateStyle();

    m_anim->stop();
    m_anim->setStartValue(m_currentWidth);
    m_anim->setEndValue(m_expandedWidth);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
    m_anim->setDuration(220);
    m_anim->start();

    QWidget::enterEvent(event);
}

void PixelBlastSideButton::leaveEvent(QEvent *event)
{
    m_isHovered = false;
    updateStyle();

    m_anim->stop();
    m_anim->setStartValue(m_currentWidth);
    m_anim->setEndValue(m_collapsedWidth);
    m_anim->setEasingCurve(QEasingCurve::InCubic);
    m_anim->setDuration(200);
    m_anim->start();

    QWidget::leaveEvent(event);
}

void PixelBlastSideButton::mousePressEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
        m_isPressed = true;
        updateStyle();
    }
    QWidget::mousePressEvent(event);
}

void PixelBlastSideButton::mouseReleaseEvent(QMouseEvent *event)
{
    if(m_isPressed && event->button() == Qt::LeftButton && rect().contains(event->pos()))
    {
        emit clicked();
    }
    m_isPressed = false;
    updateStyle();
    QWidget::mouseReleaseEvent(event);
}
