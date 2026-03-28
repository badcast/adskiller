#include <QCheckBox>
#include <QHeaderView>
#include <QStandardItemModel>

#include "Services.h"
#include "mainwindow.h"

QString MyDeviceService::guid() const
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

void MyDeviceService::slotRefresh()
{
    clearMyDevicesPage("Запрос...");
    if(mInternalData & 0x2)
        return;
    mInternalData |= 0x2;
    QDateTime dtStart = dateEditBegin->dateTime();
    QDateTime dtEnd = dateEditEnd->dateTime();
    if(actual)
        actual->clear();
    if(expired)
        expired->clear();
    quaranteeFilter->setEnabled(false);
    refreshButton->setEnabled(false);

    // Take fake delay
    MainWindow::current->delayUI(2000);

    QJsonObject request;
    request["rangeStart"] = dtStart.toString(Qt::ISODate);
    request["rangeEnd"] = dtEnd.toString(Qt::ISODate);
    request["showFlag"] = 0x3;

    MainWindow::current->network.pullServiceUUID(guid(), request, ServiceOperation::Get);
}

void MyDeviceService::clearMyDevicesPage(QString text)
{
    QStandardItemModel *model = new QStandardItemModel(table);
    table->setModel(model);

    model->setRowCount(1);
    model->setColumnCount(7);

    model->setHorizontalHeaderItem(0, new QStandardItem("id"));
    model->setHorizontalHeaderItem(1, new QStandardItem("Производитель"));
    model->setHorizontalHeaderItem(2, new QStandardItem("Модель"));
    model->setHorizontalHeaderItem(3, new QStandardItem("Дата регистраций"));
    model->setHorizontalHeaderItem(4, new QStandardItem("Послед. подключение"));
    model->setHorizontalHeaderItem(5, new QStandardItem("Срок истечения"));
    model->setHorizontalHeaderItem(6, new QStandardItem("Пакетов"));
    model->setHorizontalHeaderItem(7, new QStandardItem("Подключений"));
    model->setHorizontalHeaderItem(8, new QStandardItem("Оплачено"));
    model->setHorizontalHeaderItem(9, new QStandardItem("Есть гарантия?"));
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    if(text.isEmpty())
        return;
    model->setItem(0, 0, new QStandardItem(text));
    model->setItem(0, 1, new QStandardItem(text));
    model->setItem(0, 2, new QStandardItem(text));
    model->setItem(0, 3, new QStandardItem(text));
    model->setItem(0, 4, new QStandardItem(text));
    model->setItem(0, 5, new QStandardItem(text));
    model->setItem(0, 6, new QStandardItem(text));
    model->setItem(0, 7, new QStandardItem(text));
    model->setItem(0, 8, new QStandardItem(text));
    model->setItem(0, 9, new QStandardItem(text));
}

void MyDeviceService::fillMyDevicesPage()
{
    QStandardItemModel *model = qobject_cast<QStandardItemModel *>(table->model());
    QList<DeviceItemInfo> items;

    items << *actual;
    items << *expired;

    int idx = 0;
    for(const DeviceItemInfo &item : std::as_const(items))
    {
        if(quaranteeFilter->isChecked() && !item.serverQuarantee)
            continue;
        model->setItem(idx, 0, new QStandardItem(QString::number(item.deviceId)));
        model->setItem(idx, 1, new QStandardItem(item.vendor));
        model->setItem(idx, 2, new QStandardItem(item.model));
        model->setItem(idx, 3, new QStandardItem(item.logTime.toString(Qt::RFC2822Date)));
        model->setItem(idx, 4, new QStandardItem(item.lastConnectTime.toString(Qt::RFC2822Date)));
        model->setItem(idx, 5, new QStandardItem(item.expire.toString(Qt::RFC2822Date)));
        model->setItem(idx, 6, new QStandardItem(QString::number(item.packages)));
        model->setItem(idx, 7, new QStandardItem(QString::number(item.connectionCount)));
        model->setItem(idx, 8, new QStandardItem(QString("%1").arg(item.purchasedType == 1 ? ("VIP") : (item.purchasedType == 2 ? (QString::number(item.purchasedValue)) : ("отсутствует")))));
        model->setItem(idx, 9, new QStandardItem(QString("%1").arg(item.serverQuarantee == 1 ? "Да" : "Нет")));
        ++idx;
    }
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

void MyDeviceService::slotPullMyDeviceList(const QJsonObject responce, const QString guid, bool ok)
{
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
    }

    this->actual = std::make_shared<QList<DeviceItemInfo>>(actual);
    this->expired = std::make_shared<QList<DeviceItemInfo>>(expired);
    mInternalData &= ~2;
    if(ok)
    {
        if(actual.empty() && expired.empty())
            clearMyDevicesPage({});
        else
            fillMyDevicesPage();
    }
    else
    {
        clearMyDevicesPage("Ошибка при загрузке.");
    }

    quaranteeFilter->setEnabled(true);
    refreshButton->setEnabled(true);
}

void MyDeviceService::slotQuaranteeUpdate()
{
    QStandardItemModel *model = qobject_cast<QStandardItemModel *>(table->model());
    if(model == nullptr || mInternalData & 2)
        return;
    clearMyDevicesPage({});
    fillMyDevicesPage();
}

MyDeviceService::MyDeviceService(QObject *parent) : Service(None, parent), mInternalData(0), table(nullptr), dateEditBegin(nullptr), dateEditEnd(nullptr), refreshButton(nullptr), quaranteeFilter(nullptr)
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
    if(!canStart() || isStarted() || !MainWindow::current->accessUi_page_devices(table, dateEditBegin, dateEditEnd, refreshButton, quaranteeFilter) || !(table && dateEditBegin && dateEditEnd && refreshButton && quaranteeFilter))
        return false;

    mInternalData |= 1;
    // Set minimum as default.
    dateEditBegin->setDate(QDate(2024, 1, 1));
    // Set maximum as default current date.
    dateEditEnd->setDate(QDate::currentDate());

    quaranteeFilter->setChecked(true);

    QObject::disconnect(&MainWindow::current->network, &Network::sPullServiceUUID, this, &MyDeviceService::slotPullMyDeviceList);
    QObject::connect(&MainWindow::current->network, &Network::sPullServiceUUID, this, &MyDeviceService::slotPullMyDeviceList);

    QObject::disconnect(refreshButton, &QPushButton::clicked, this, &MyDeviceService::slotRefresh);
    QObject::connect(refreshButton, &QPushButton::clicked, this, &MyDeviceService::slotRefresh);
    QObject::disconnect(quaranteeFilter, &QCheckBox::clicked, this, &MyDeviceService::slotQuaranteeUpdate);
    QObject::connect(quaranteeFilter, &QCheckBox::clicked, this, &MyDeviceService::slotQuaranteeUpdate);
    slotRefresh();
    return true;
}

void MyDeviceService::stop()
{
    mInternalData = 0;
    actual.reset();
    expired.reset();
}
