#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include "AdbFront.h"

class SamsungFrpWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SamsungFrpWidget(QWidget *parent = nullptr);
    ~SamsungFrpWidget() override = default;

    void setDevice(const AdbDevice &device);
    void resetSession();

private slots:
    void onBackToCabinet();

private:
    void setupUi();

    QLabel *m_iconLabel = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_subLabel = nullptr;
    QPushButton *m_btnBack = nullptr;
};
