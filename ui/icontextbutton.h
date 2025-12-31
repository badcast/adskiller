#pragma once

#include <QPushButton>
#include <QWidget>

class IconTextButton : public QPushButton
{
    Q_OBJECT

public:
    IconTextButton(const QIcon &icon, const QString &text, QWidget *parent = nullptr);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QIcon _icon;
    QString _text;
};
