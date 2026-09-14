#pragma once

#include <QDialog>

#include "Network.h"

class PurchaseConfirmDialog : public QDialog
{
public:
    PurchaseConfirmDialog(QWidget *parent, const QString &deviceName, const UserDataInfo &data);

    static int confirm(QWidget *parent, const QString &deviceName, const UserDataInfo &data);
};
