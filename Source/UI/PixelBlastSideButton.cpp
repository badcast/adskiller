#include "PixelBlastSideButton.h"

#include <QIcon>
#include <QConicalGradient>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QSizePolicy>

PixelBlastSideButton::PixelBlastSideButton(QWidget *parent) : QWidget(parent)
{
    setObjectName("PixelBlastSideButton");
    setAttribute(Qt::WA_Hover, true);
    setCursor(Qt::PointingHandCursor);
    setAccessibleName(QString::fromUtf8("Запустить Pixel Blast"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(82);

    setupUi();

    m_borderTimer = new QTimer(this);
    connect(m_borderTimer, &QTimer::timeout, this, [this]()
            {
                m_borderAngle = (m_borderAngle + 1) % 360;
                update();
            });
    m_borderTimer->start(25);
}

void PixelBlastSideButton::setupUi()
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(22, 12, 22, 12);
    layout->setSpacing(16);

    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(48, 48);
    m_iconLabel->setPixmap(QIcon(":/svg/services/pixelblast").pixmap(36, 36));
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setStyleSheet("background: transparent; border: none;");
    m_iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    m_textLabel = new QLabel(this);
    m_textLabel->setTextFormat(Qt::RichText);
    m_textLabel->setText(
        QString::fromUtf8(
            "<div style='line-height: 110%;'>"
            "<span style='color: #FFF7D6; font-family: \"Segoe UI Variable\", \"Segoe UI\", sans-serif; font-weight: 900; font-size: 16px;'>PIXEL BLAST</span><br/>"
            "<span style='color: #D6C7A1; font-family: \"Segoe UI Variable\", \"Segoe UI\", sans-serif; font-weight: 600; font-size: 11px;'>Аркадная игра</span>"
            "</div>"));
    m_textLabel->setStyleSheet("background: transparent; border: none;");
    m_textLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_textLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    m_actionLabel = new QLabel(QString::fromUtf8("ИГРАТЬ"), this);
    m_actionLabel->setStyleSheet(
        "color: #1C1912; font-size: 12px; font-weight: 900; background: #FACC15; "
        "border: 1px solid #FDE68A; border-radius: 3px; padding: 9px 14px;");
    m_actionLabel->setAlignment(Qt::AlignCenter);
    m_actionLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    layout->addWidget(m_iconLabel);
    layout->addWidget(m_textLabel, 1);
    layout->addWidget(m_actionLabel);
}

void PixelBlastSideButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF panelRect = QRectF(rect()).adjusted(2, 2, -2, -2);
    QPainterPath panelPath;
    panelPath.addRoundedRect(panelRect, 8, 8);

    QLinearGradient background(panelRect.topLeft(), panelRect.bottomRight());
    background.setColorAt(0.0, QColor("#211B0D"));
    background.setColorAt(0.55, QColor("#17150F"));
    background.setColorAt(1.0, QColor("#30230A"));
    painter.fillPath(panelPath, background);

    QConicalGradient border(panelRect.center(), m_borderAngle);
    border.setColorAt(0.0, QColor("#FFF1A8"));
    border.setColorAt(0.12, QColor("#A16207"));
    border.setColorAt(0.28, QColor("#FDE68A"));
    border.setColorAt(0.48, QColor("#713F12"));
    border.setColorAt(0.68, QColor("#FACC15"));
    border.setColorAt(0.86, QColor("#A16207"));
    border.setColorAt(1.0, QColor("#FFF1A8"));

    QPen borderPen(border, m_isPressed ? 4 : 3);
    painter.setPen(borderPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(panelPath);
}

void PixelBlastSideButton::mousePressEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
        m_isPressed = true;
        update();
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void PixelBlastSideButton::mouseReleaseEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
        if(m_isPressed && rect().contains(event->pos()))
            emit clicked();
        m_isPressed = false;
        update();
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}
