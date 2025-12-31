#include <QPainter>

#include "icontextbutton.h"

IconTextButton::IconTextButton(const QIcon &icon, const QString &text, QWidget *parent) : QPushButton(parent), _icon(icon), _text(text)
{
    setFixedSize(400, 100);
}

QSize IconTextButton::sizeHint() const
{
    return QSize(400, 100);
}

void IconTextButton::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QSize iconSize(height() * .8, height() * .8);
    QRect iconRect(10, (height() - iconSize.height()) / 2, iconSize.width(), iconSize.height());
    _icon.paint(&painter, iconRect);

    QRect textRect(iconRect.right() + 10, 0, width() - iconRect.width() - 20, height());
    painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, _text);
}
