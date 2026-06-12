#include <QCheckBox>
#include <QHeaderView>
#include <QStandardItemModel>

#include "Services.h"
#include "mainwindow.h"

QString MyDeviceService::uuid() const
{
    return IDServiceMyDeviceString;
}

QString MyDeviceService::widgetIconName()
{
    return "white-devices-list";
}

PageIndex MyDeviceService::targetPage()
{
    return PageIndex::MyDevicesPage;
}

void MyDeviceService::refreshData(const QString &dateStartISO, const QString &dateEndISO, bool guaranteeFilter)
{
    if(mInternalData & 0x2)
        return;
    mInternalData |= 0x2;
    setIsRefreshing(true);
    m_lastGuaranteeFilter = guaranteeFilter;

    // Use asynchronous timer instead of blocking delayUI
    QTimer::singleShot(
        2000,
        [this, dateStartISO, dateEndISO]()
        {
            QJsonObject request;
            request["rangeStart"] = dateStartISO;
            request["rangeEnd"] = dateEndISO;
            request["showFlag"] = 0x3;

            MainWindow::current->network.pullServiceUUID(uuid(), request, ServiceOperation::Get);
        });
}

void MyDeviceService::filterData(bool guaranteeFilter)
{
    m_lastGuaranteeFilter = guaranteeFilter;
    if(actual && expired)
    {
        buildDeviceList(m_lastGuaranteeFilter);
    }
}

void MyDeviceService::buildDeviceList(bool guaranteeFilter)
{
    QVariantList list;
    QList<DeviceItemInfo> items;

    if(actual)
        items << *actual;
    if(expired)
        items << *expired;

    for(const DeviceItemInfo &item : std::as_const(items))
    {
        if(guaranteeFilter && !item.serverQuarantee)
            continue;

        QVariantMap map;
        map["id"] = QString::number(item.deviceId);
        map["vendor"] = item.vendor;
        map["model"] = item.model;
        map["registrationDate"] = item.logTime.toString(Qt::RFC2822Date);
        map["lastConnection"] = item.lastConnectTime.toString(Qt::RFC2822Date);
        map["expiration"] = item.expire.toString(Qt::RFC2822Date);
        map["packages"] = QString::number(item.packages);
        map["connections"] = QString::number(item.connectionCount);
        map["purchased"] = item.purchasedType == 1 ? "VIP" : (item.purchasedType == 2 ? QString::number(item.purchasedValue) : "отсутствует");
        map["guarantee"] = item.serverQuarantee == 1 ? "Да" : "Нет";

        list << map;
    }

    m_devices = list;
    emit devicesChanged();
}

void MyDeviceService::slotPullMyDeviceList(const QJsonObject responce, const QString guid, ServiceOperation so, bool ok)
{
    qDebug() << "slotPullMyDeviceList guid:" << guid << "ok:" << ok << "responce:" << responce;
    QList<DeviceItemInfo> actual {}, expired {};
    if(ok && !responce.isEmpty() && responce["actual"].isArray() && responce["expired"].isArray())
    {
        std::function<DeviceItemInfo(const QJsonObject &)> convertToObj = [](const QJsonObject &obj)
        {
            DeviceItemInfo dit;
            dit.mdkey = obj["mdkey"].toString();
            dit.logTime = QDateTime::fromSecsSinceEpoch(obj["logTime"].toVariant().toULongLong());
            dit.lastConnectTime = QDateTime::fromSecsSinceEpoch(obj["lastConnectTime"].toVariant().toULongLong());
            dit.expire = QDateTime::fromSecsSinceEpoch(obj["expire"].toVariant().toULongLong());
            dit.vendor = obj["vendor"].toString();
            dit.model = obj["model"].toString();
            dit.purchasedType = obj["purchased_type"].toInt();
            dit.purchasedValue = obj["purchased_value"].toInt();
            dit.connectionCount = obj["connectionCount"].toInt();
            dit.packages = obj["packages"].toInt();
            dit.deviceId = obj["devId"].toInt();
            return dit;
        };

        QJsonArray arr = responce["actual"].toArray();
        for(auto iter = arr.begin(); iter != arr.end(); ++iter)
        {
            actual << convertToObj(iter->toObject());
            actual.last().serverQuarantee = 1;
        }

        arr = responce["expired"].toArray();
        for(auto iter = arr.begin(); iter != arr.end(); ++iter)
        {
            expired << convertToObj(iter->toObject());
            expired.last().serverQuarantee = 0;
        }

        this->actual = std::make_shared<QList<DeviceItemInfo>>(actual);
        this->expired = std::make_shared<QList<DeviceItemInfo>>(expired);

        buildDeviceList(m_lastGuaranteeFilter);
    }

    if(mInternalData & 0x2)
    {
        setIsRefreshing(false);
    }

    mInternalData &= ~(0x2);
}

MyDeviceService::MyDeviceService(QObject *parent) : Service(None, parent), mInternalData(0)
{
}

MyDeviceService::~MyDeviceService()
{
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
    if(!canStart() || isStarted())
        return false;

    mInternalData |= 1;

    QObject::disconnect(&MainWindow::current->network, &Network::sPullServiceUUID, this, &MyDeviceService::slotPullMyDeviceList);
    QObject::connect(&MainWindow::current->network, &Network::sPullServiceUUID, this, &MyDeviceService::slotPullMyDeviceList);

    return true;
}

void MyDeviceService::stop()
{
    mInternalData = 0;
    actual.reset();
    expired.reset();
}
