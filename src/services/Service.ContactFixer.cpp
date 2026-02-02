#include <memory>

#include <NumberPreview.h>
#include <vcard.h>

#include "Services.h"

struct CFSInternalData
{
};

ContactFixerService::ContactFixerService(QObject *parent) : Service(DeviceConnectType::None, parent), mInternal(new CFSInternalData)
{
}

ContactFixerService::~ContactFixerService()
{
    if(mInternal)
    {
        delete mInternal;
        mInternal = nullptr;
    }
}

QString ContactFixerService::uuid() const
{
    return IDServiceContactFixerString;
}

bool ContactFixerService::canStart()
{
    return Service::canStart();
}

bool ContactFixerService::isStarted()
{
    return false;
}

bool ContactFixerService::isFinish()
{
    return false;
}

bool ContactFixerService::start()
{
    NumberPreview np("+890000000000");
    QString value0 = QString::fromStdString(np.format(NumberFormat::Beauty | NumberFormat::Global));
    QString value1 = QString::fromStdString(np.format(NumberFormat::Beauty | NumberFormat::Local));
    QString value2 = QString::fromStdString(np.format(NumberFormat::Compact | NumberFormat::Global));
    QString value3 = QString::fromStdString(np.format(NumberFormat::Compact | NumberFormat::Local));
    return false;
}

void ContactFixerService::stop()
{
}

void ContactFixerService::reset()
{
    stop();
}
