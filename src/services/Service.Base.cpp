#include "Services.h"

void Service::setDevice(const AdbDevice& adbDevice){
    mAdbDevice = adbDevice;
}

PageIndex Service::targetPage()
{
    return LongInfoPage;
}


bool Service::canStart()
{
    return mDeviceType == DeviceConnectType::ADB && !mAdbDevice.isEmpty() || mDeviceType == DeviceConnectType::None;
}

DeviceConnectType Service::deviceType() const
{
    return mDeviceType;
}
