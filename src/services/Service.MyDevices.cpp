#include "Services.h"

void MyDeviceService::slotPullMyDeviceList(const QList<DeviceItemInfo> actual, const QList<DeviceItemInfo> expired, bool ok)
{
    this->actual = std::make_shared<QList<DeviceItemInfo>>(actual);
    this->expired = std::make_shared<QList<DeviceItemInfo>>(expired);

    QList<DeviceItemInfo> items;
    items << actual;
    items << expired;
    std::sort(items.begin(), items.end());
    MainWindow::current->fillMyDevicesPage(items);
}

MyDeviceService::MyDeviceService(QObject *parent) : Service(None, parent), mInternalData(0), table(nullptr)
{
    QObject::connect(&MainWindow::current->network, &Network::sPullDeviceList, this, &MyDeviceService::slotPullMyDeviceList);
}

bool MyDeviceService::canStart()
{
    return !(mInternalData & 1);
}

bool MyDeviceService::isStarted()
{
    return (mInternalData & 1);
}

bool MyDeviceService::isFinish()
{
    return false;
}

bool MyDeviceService::start()
{
    if(!canStart() || isStarted() || MainWindow::current->accessUi_myDevices(table) && table == nullptr)
        return false;

    mInternalData |= 1;


    return true;
}

void MyDeviceService::stop()
{
    mInternalData = 0;
}

void MyDeviceService::reset()
{
    stop();
}
