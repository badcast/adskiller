#include "MiAccountWidget.h"
#include "MainWindow.h"
#include "Services.h"

#include <QFrame>
#include <QIcon>

// ============================================================================
// MiAccountWidget Implementation
// ============================================================================

MiAccountWidget::MiAccountWidget(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet("MiAccountWidget { background-color: #0A0E1A; }");
    setupUi();
}

void MiAccountWidget::setDevice(const AdbDevice &device)
{
    Q_UNUSED(device);
}

void MiAccountWidget::resetSession()
{
}

void MiAccountWidget::setupUi()
{
    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(20, 20, 20, 20);
    rootLayout->setAlignment(Qt::AlignCenter);

    QFrame *centerCard = new QFrame(this);
    centerCard->setStyleSheet(
        "QFrame {"
        "  background-color: #0F172A;"
        "  border: 1px solid #1E293B;"
        "  border-radius: 0px;"
        "  padding: 40px 60px;"
        "}");
    QVBoxLayout *cardLayout = new QVBoxLayout(centerCard);
    cardLayout->setSpacing(16);
    cardLayout->setAlignment(Qt::AlignCenter);

    // Warning icon from SVG
    m_iconLabel = new QLabel(centerCard);
    m_iconLabel->setPixmap(QIcon(":/svg/alert-triangle").pixmap(64, 64));
    m_iconLabel->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(m_iconLabel);

    // Text: "Данный модуль находится в разработке."
    m_titleLabel = new QLabel(QString::fromUtf8("Данный модуль находится в разработке."), centerCard);
    m_titleLabel->setStyleSheet("font-size: 18px; font-weight: 700; color: #F8FAFC; letter-spacing: 0.5px;");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(m_titleLabel);

    m_subLabel = new QLabel(QString::fromUtf8("Сервис MI Account временно недоступен."), centerCard);
    m_subLabel->setStyleSheet("font-size: 12px; color: #94A3B8;");
    m_subLabel->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(m_subLabel);

    m_btnBack = new QPushButton(QString::fromUtf8("Назад в личный кабинет"), centerCard);
    m_btnBack->setIcon(QIcon(":/svg/arrow-left"));
    m_btnBack->setMinimumHeight(36);
    m_btnBack->setStyleSheet(
        "QPushButton {"
        "  background-color: #0284C7;"
        "  color: #FFFFFF;"
        "  font-weight: 700;"
        "  border: 1px solid #38BDF8;"
        "  padding: 6px 24px;"
        "  margin-top: 10px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #0369A1;"
        "  border-color: #7DD3FC;"
        "}");
    connect(m_btnBack, &QPushButton::clicked, this, &MiAccountWidget::onBackToCabinet);
    cardLayout->addWidget(m_btnBack, 0, Qt::AlignCenter);

    rootLayout->addWidget(centerCard, 0, Qt::AlignCenter);
}

void MiAccountWidget::onBackToCabinet()
{
    if(MainWindow::current)
        MainWindow::current->closeService(ServiceProvider::currentService());
}

// ============================================================================
// MiDeviceUnlockService (MI Account) Implementation
// ============================================================================

MiDeviceUnlockService::MiDeviceUnlockService(QObject *parent) : Service(DeviceConnectType::None, parent)
{
    title = QString::fromUtf8("MI Account");
    m_sortScore = 200;
    m_flag = ServiceFlag::NewBeta;
}

MiDeviceUnlockService::~MiDeviceUnlockService()
{
    stop();
}

QString MiDeviceUnlockService::uuid() const
{
    return IDServiceMiAccountString;
}

QString MiDeviceUnlockService::getInternalData()
{
    return Service::getInternalData() + ":" + QStringList {"Device = " + mAdbDevice.devId}.join('\t');
}

PageIndex MiDeviceUnlockService::targetPage()
{
    return MiAccountPage;
}

bool MiDeviceUnlockService::canStart()
{
    return Service::canStart();
}

bool MiDeviceUnlockService::isStarted()
{
    return m_started;
}

bool MiDeviceUnlockService::isFinish()
{
    return m_finished;
}

QString MiDeviceUnlockService::widgetIconName()
{
    return "mi-unlock";
}

bool MiDeviceUnlockService::start()
{
    sendCheckPull();
    if(!canStart())
        return false;

    m_started = true;
    m_finished = false;

    if(MainWindow::current)
    {
        auto *widget = static_cast<MiAccountWidget *>(MainWindow::current->pageWidget(MiAccountPage));
        if(widget)
        {
            widget->resetSession();
            if(!mAdbDevice.isEmpty())
                widget->setDevice(mAdbDevice);
        }
    }

    return true;
}

void MiDeviceUnlockService::stop()
{
    m_started = false;
    m_finished = true;

    if(MainWindow::current)
    {
        auto *widget = static_cast<MiAccountWidget *>(MainWindow::current->pageWidget(MiAccountPage));
        if(widget)
            widget->resetSession();
    }
}
