#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QMessageBox>

#include "PurchaseConfirmDialog.h"

PurchaseConfirmDialog::PurchaseConfirmDialog(QWidget *parent, const QString &deviceName, const UserDataInfo &data) : QDialog(parent)
{
    setWindowTitle("Подтверждение покупки — AdsKiller");
    setWindowIcon(QIcon(":/resources/app-logo"));
    setModal(true);
    setFixedWidth(460);

    setStyleSheet(
        "QDialog {"
        "   background-color: #1A1C20;"
        "   color: #F3F4F6;"
        "}");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(22, 20, 22, 20);
    mainLayout->setSpacing(16);

    // Header with icon and title
    QHBoxLayout *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(14);

    QLabel *iconLabel = new QLabel(this);
    iconLabel->setFixedSize(48, 48);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setPixmap(QPixmap(":/svg/shield").scaled(26, 26, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    iconLabel->setStyleSheet(
        "background-color: #1E3A5F;"
        "border: 1px solid #2563EB;"
        "border-radius: 0px;");
    headerLayout->addWidget(iconLabel);

    QVBoxLayout *titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(3);

    QLabel *titleLabel = new QLabel("Подтверждение оплаты", this);
    titleLabel->setStyleSheet("color: #FFFFFF; font-size: 16px; font-weight: bold;");
    titleLayout->addWidget(titleLabel);

    QLabel *subtitleLabel = new QLabel("Очистка и обезвреживание рекламы", this);
    subtitleLabel->setStyleSheet("color: #9CA3AF; font-size: 12px; font-weight: 500;");
    titleLayout->addWidget(subtitleLabel);

    headerLayout->addLayout(titleLayout);
    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);

    // Details Card
    QFrame *cardFrame = new QFrame(this);
    cardFrame->setObjectName("purchaseDetailsCard");
    cardFrame->setStyleSheet(
        "QFrame#purchaseDetailsCard {"
        "   background-color: #141518;"
        "   border: 1px solid #282B32;"
        "   border-radius: 0px;"
        "}");

    QGridLayout *cardGrid = new QGridLayout(cardFrame);
    cardGrid->setContentsMargins(16, 14, 16, 14);
    cardGrid->setVerticalSpacing(10);
    cardGrid->setHorizontalSpacing(16);

    QString cleanDevName = deviceName;
    const QString prefix = "Выбранное устройство (";
    if(cleanDevName.startsWith(prefix) && cleanDevName.endsWith(")"))
    {
        cleanDevName = cleanDevName.mid(prefix.length(), cleanDevName.length() - prefix.length() - 1).trimmed();
    }
    if(cleanDevName.isEmpty())
        cleanDevName = deviceName;

    // Row 0: Device
    QLabel *lblDevTitle = new QLabel("Устройство:", cardFrame);
    lblDevTitle->setStyleSheet("color: #9CA3AF; font-size: 12px;");
    cardGrid->addWidget(lblDevTitle, 0, 0);

    QLabel *lblDevVal = new QLabel(cleanDevName, cardFrame);
    lblDevVal->setStyleSheet("color: #FFFFFF; font-size: 12px; font-weight: bold;");
    lblDevVal->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    cardGrid->addWidget(lblDevVal, 0, 1);

    // Row 1: Procedure
    QLabel *lblProcTitle = new QLabel("Процедура:", cardFrame);
    lblProcTitle->setStyleSheet("color: #9CA3AF; font-size: 12px;");
    cardGrid->addWidget(lblProcTitle, 1, 0);

    QLabel *lblProcVal = new QLabel("Удаление рекламы и вредоносов", cardFrame);
    lblProcVal->setStyleSheet("color: #E5E7EB; font-size: 12px; font-weight: 500;");
    lblProcVal->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    cardGrid->addWidget(lblProcVal, 1, 1);

    // Row 2: Divider
    QFrame *divider = new QFrame(cardFrame);
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet("background-color: #262930; max-height: 1px;");
    cardGrid->addWidget(divider, 2, 0, 1, 2);

    // Row 3: Price
    QLabel *lblPriceTitle = new QLabel("Стоимость:", cardFrame);
    lblPriceTitle->setStyleSheet("color: #9CA3AF; font-size: 12px;");
    cardGrid->addWidget(lblPriceTitle, 3, 0);

    QLabel *lblPriceVal = new QLabel(QString("%1 %2").arg(data.basePrice).arg(data.currencyType), cardFrame);
    lblPriceVal->setStyleSheet("color: #4CC2FF; font-size: 14px; font-weight: bold;");
    lblPriceVal->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    cardGrid->addWidget(lblPriceVal, 3, 1);

    // Row 4: Current balance
    QLabel *lblBalanceTitle = new QLabel("Текущий баланс:", cardFrame);
    lblBalanceTitle->setStyleSheet("color: #9CA3AF; font-size: 12px;");
    cardGrid->addWidget(lblBalanceTitle, 4, 0);

    QLabel *lblBalanceVal = new QLabel(QString("%1 %2").arg(data.credits).arg(data.currencyType), cardFrame);
    lblBalanceVal->setStyleSheet("color: #FBBF24; font-size: 13px; font-weight: bold;");
    lblBalanceVal->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    cardGrid->addWidget(lblBalanceVal, 4, 1);

    // Row 5: Balance after purchase
    int remainingCredits = qMax<int>(0, static_cast<int>(data.credits) - static_cast<int>(data.basePrice));
    bool hasEnough = (data.credits >= data.basePrice);

    QLabel *lblAfterTitle = new QLabel("Баланс после списания:", cardFrame);
    lblAfterTitle->setStyleSheet("color: #9CA3AF; font-size: 12px;");
    cardGrid->addWidget(lblAfterTitle, 5, 0);

    QLabel *lblAfterVal = new QLabel(cardFrame);
    if(hasEnough)
    {
        lblAfterVal->setText(QString("%1 %2").arg(remainingCredits).arg(data.currencyType));
        lblAfterVal->setStyleSheet("color: #34D399; font-size: 13px; font-weight: bold;");
    }
    else
    {
        lblAfterVal->setText("Недостаточно средств");
        lblAfterVal->setStyleSheet("color: #F87171; font-size: 12px; font-weight: bold;");
    }
    lblAfterVal->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    cardGrid->addWidget(lblAfterVal, 5, 1);

    mainLayout->addWidget(cardFrame);

    // Warning banner if insufficient funds
    if(!hasEnough)
    {
        QLabel *warnLabel = new QLabel(this);
        warnLabel->setWordWrap(true);
        warnLabel->setTextFormat(Qt::RichText);
        warnLabel->setText("<img src=\":/svg/alert-triangle\" width=\"14\" height=\"14\" style=\"vertical-align: middle;\"/> Недостаточно средств на балансе для оплаты процедуры. Пожалуйста, пополните баланс через меню «Поддержка → Связаться».");
        warnLabel->setStyleSheet(
            "background-color: rgba(239, 68, 68, 0.12);"
            "border: 1px solid rgba(239, 68, 68, 0.35);"
            "border-radius: 0px;"
            "color: #FCA5A5;"
            "font-size: 11px;"
            "padding: 8px 10px;");
        mainLayout->addWidget(warnLabel);
    }

    // Action Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(12);

    QPushButton *cancelBtn = new QPushButton("Отмена", this);
    cancelBtn->setCursor(Qt::PointingHandCursor);
    cancelBtn->setMinimumHeight(38);
    cancelBtn->setStyleSheet(
        "QPushButton {"
        "   background-color: #22252B;"
        "   border: 1px solid #32363E;"
        "   border-radius: 0px;"
        "   color: #D1D5DB;"
        "   font-size: 13px;"
        "   font-weight: 600;"
        "   padding: 0px 20px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #2D3139;"
        "   border-color: #4B5563;"
        "   color: #FFFFFF;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #1A1C20;"
        "}");
    btnLayout->addWidget(cancelBtn);

    QPushButton *confirmBtn = new QPushButton(this);
    confirmBtn->setCursor(Qt::PointingHandCursor);
    confirmBtn->setMinimumHeight(38);
    if(hasEnough)
    {
        confirmBtn->setIcon(QIcon(":/svg/credit-card"));
        confirmBtn->setIconSize(QSize(16, 16));
        confirmBtn->setText(QString(" Оплатить %1 %2").arg(data.basePrice).arg(data.currencyType));
        confirmBtn->setEnabled(true);
    }
    else
    {
        confirmBtn->setText("Недостаточно средств");
        confirmBtn->setEnabled(false);
    }
    confirmBtn->setStyleSheet(
        "QPushButton {"
        "   background-color: #10B981;"
        "   border: 1px solid #059669;"
        "   border-radius: 0px;"
        "   color: #FFFFFF;"
        "   font-size: 13px;"
        "   font-weight: bold;"
        "   padding: 0px 24px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #059669;"
        "   border-color: #34D399;"
        "}"
        "QPushButton:pressed {"
        "   background: #047857;"
        "}"
        "QPushButton:disabled {"
        "   background-color: #2A2E35;"
        "   border-color: #363A42;"
        "   color: #6B7280;"
        "}");
    confirmBtn->setDefault(true);
    btnLayout->addWidget(confirmBtn);

    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(confirmBtn, &QPushButton::clicked, this, &QDialog::accept);

    mainLayout->addLayout(btnLayout);
}

int PurchaseConfirmDialog::confirm(QWidget *parent, const QString &deviceName, const UserDataInfo &data)
{
    PurchaseConfirmDialog dlg(parent, deviceName, data);
    if(dlg.exec() == QDialog::Accepted)
        return QMessageBox::StandardButton::Yes;
    return QMessageBox::StandardButton::No;
}
