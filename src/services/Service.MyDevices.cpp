#include <QBrush>
#include <QCheckBox>
#include <QColor>
#include <QFont>
#include <QHeaderView>
#include <QIcon>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QStandardItemModel>

#include "Services.h"
#include "mainwindow.h"

static QIcon createBrandBadgeIcon(const QString &vendorRaw)
{
    QString vendor = vendorRaw.trimmed().toLower();
    QString letter = "📱";
    QColor bgTop, bgBot;
    QColor textColor = Qt::white;

    if(vendor.contains("xiaomi") || vendor.contains("redmi") || vendor.contains("poco") || vendor.contains("mi"))
    {
        letter = "X";
        bgTop = QColor("#FF4D4D");
        bgBot = QColor("#D32F2F"); // Red background for Xiaomi as requested
    }
    else if(vendor.contains("samsung"))
    {
        letter = "S";
        bgTop = QColor("#1E88E5");
        bgBot = QColor("#0D47A1");
    }
    else if(vendor.contains("huawei") || vendor.contains("honor"))
    {
        letter = "H";
        bgTop = QColor("#EF5350");
        bgBot = QColor("#B71C1C");
    }
    else if(vendor.contains("oneplus"))
    {
        letter = "1+";
        bgTop = QColor("#E53935");
        bgBot = QColor("#B71C1C");
    }
    else if(vendor.contains("oppo") || vendor.contains("realme"))
    {
        letter = vendor.contains("realme") ? "R" : "O";
        bgTop = QColor("#43A047");
        bgBot = QColor("#1B5E20");
    }
    else if(vendor.contains("vivo") || vendor.contains("iqoo"))
    {
        letter = "V";
        bgTop = QColor("#00ACC1");
        bgBot = QColor("#006064");
    }
    else if(vendor.contains("google") || vendor.contains("pixel"))
    {
        letter = "G";
        bgTop = QColor("#4285F4");
        bgBot = QColor("#1A73E8");
    }
    else if(vendor.contains("apple"))
    {
        letter = "A";
        bgTop = QColor("#757575");
        bgBot = QColor("#303030");
    }
    else
    {
        letter = vendorRaw.isEmpty() ? "D" : vendorRaw.left(1).toUpper();
        bgTop = QColor("#0078D4");
        bgBot = QColor("#004E8C");
    }

    constexpr int size = 26;
    QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    QPainter p;
    if(p.begin(&img))
    {
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::TextAntialiasing);

        // Draw rounded badge rectangle
        QPainterPath path;
        path.addRoundedRect(QRectF(1, 1, size - 2, size - 2), 6, 6);

        QLinearGradient grad(0, 0, 0, size);
        grad.setColorAt(0.0, bgTop);
        grad.setColorAt(1.0, bgBot);
        p.fillPath(path, grad);

        p.setPen(QPen(QColor(255, 255, 255, 70), 1));
        p.drawPath(path);

        // Draw text letter
        p.setPen(textColor);
        QFont f = p.font();
        f.setBold(true);
        f.setPixelSize(letter.length() > 1 ? 10 : 13);
        p.setFont(f);
        p.drawText(QRectF(0, 0, size, size), Qt::AlignCenter, letter);

        p.end();
    }
    return QIcon(QPixmap::fromImage(img));
}

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

    MainWindow::current->network.pullServiceUUID(uuid(), request, ServiceOperation::Get);
}

class DeviceSortItem : public QStandardItem
{
public:
    DeviceSortItem() : QStandardItem()
    {
    }
    explicit DeviceSortItem(const QString &text) : QStandardItem(text)
    {
    }
    DeviceSortItem(const QIcon &icon, const QString &text) : QStandardItem(icon, text)
    {
    }

    bool operator<(const QStandardItem &other) const override
    {
        const QVariant v1 = data(Qt::UserRole);
        const QVariant v2 = other.data(Qt::UserRole);
        if(v1.isValid() && v2.isValid())
        {
            if(v1.userType() == QMetaType::QDateTime || v2.userType() == QMetaType::QDateTime)
                return v1.toDateTime() < v2.toDateTime();
            if(v1.userType() == QMetaType::Int || v1.userType() == QMetaType::LongLong || v1.userType() == QMetaType::Double)
                return v1.toDouble() < v2.toDouble();
            if(v1.userType() == QMetaType::Bool)
                return static_cast<int>(v1.toBool()) < static_cast<int>(v2.toBool());
            return v1.toString().localeAwareCompare(v2.toString()) < 0;
        }

        const QVariant d1 = data(Qt::DisplayRole);
        const QVariant d2 = other.data(Qt::DisplayRole);
        if(d1.userType() == QMetaType::Int || d1.userType() == QMetaType::LongLong || d1.userType() == QMetaType::Double)
            return d1.toDouble() < d2.toDouble();
        return text().localeAwareCompare(other.text()) < 0;
    }
};

void MyDeviceService::clearMyDevicesPage(QString text)
{
    delete table->model(); // fix: delete old model before replacing to avoid accumulation
    QStandardItemModel *model = new QStandardItemModel(table);
    table->setModel(model);

    model->setColumnCount(10);

    model->setHorizontalHeaderItem(0, new QStandardItem("ID"));
    model->setHorizontalHeaderItem(1, new QStandardItem("Производитель"));
    model->setHorizontalHeaderItem(2, new QStandardItem("Модель"));
    model->setHorizontalHeaderItem(3, new QStandardItem("Регистрация"));
    model->setHorizontalHeaderItem(4, new QStandardItem("Посл. подключение"));
    model->setHorizontalHeaderItem(5, new QStandardItem("Срок истечения"));
    model->setHorizontalHeaderItem(6, new QStandardItem("Пакетов"));
    model->setHorizontalHeaderItem(7, new QStandardItem("Подключений"));
    model->setHorizontalHeaderItem(8, new QStandardItem("Оплачено"));
    model->setHorizontalHeaderItem(9, new QStandardItem("Гарантия"));

    table->setIconSize(QSize(22, 22));
    table->verticalHeader()->setDefaultSectionSize(34);
    table->verticalHeader()->hide();
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->horizontalHeader()->setSortIndicatorShown(true);
    table->horizontalHeader()->setSectionsClickable(true);
    table->setSortingEnabled(true);

    if(text.isEmpty())
        return;

    model->setRowCount(1);
    for(int col = 0; col < 10; ++col)
    {
        QStandardItem *it = new QStandardItem(col == 1 ? text : "");
        it->setTextAlignment(Qt::AlignCenter);
        it->setForeground(QBrush(QColor("#8E9297")));
        model->setItem(0, col, it);
    }
}

void MyDeviceService::fillMyDevicesPage()
{
    QStandardItemModel *model = qobject_cast<QStandardItemModel *>(table->model());
    if(!model)
        return;
    QList<DeviceItemInfo> items;

    if(actual)
        items << *actual;
    if(expired)
        items << *expired;

    model->removeRows(0, model->rowCount());

    int idx = 0;
    for(const DeviceItemInfo &item : std::as_const(items))
    {
        if(quaranteeFilter->isChecked() && !item.serverQuarantee)
            continue;

        model->insertRow(idx);

        DeviceSortItem *idItem = new DeviceSortItem(QString::number(item.deviceId));
        idItem->setData(item.deviceId, Qt::UserRole);
        idItem->setTextAlignment(Qt::AlignCenter);
        idItem->setForeground(QBrush(QColor("#8E9297")));

        DeviceSortItem *vendorItem = new DeviceSortItem(createBrandBadgeIcon(item.vendor), item.vendor);
        vendorItem->setData(item.vendor.trimmed().toLower(), Qt::UserRole);
        vendorItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        vendorItem->setForeground(QBrush(QColor("#FFFFFF")));

        DeviceSortItem *modelItem = new DeviceSortItem(item.model);
        modelItem->setData(item.model.trimmed().toLower(), Qt::UserRole);
        modelItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        modelItem->setForeground(QBrush(QColor("#E3E5E8")));

        DeviceSortItem *logTimeItem = new DeviceSortItem(item.logTime.isValid() ? item.logTime.toString("yyyy-MM-dd HH:mm") : "—");
        logTimeItem->setData(item.logTime, Qt::UserRole);
        logTimeItem->setTextAlignment(Qt::AlignCenter);
        logTimeItem->setForeground(QBrush(QColor("#BAC0CB")));

        DeviceSortItem *lastConnItem = new DeviceSortItem(item.lastConnectTime.isValid() ? item.lastConnectTime.toString("yyyy-MM-dd HH:mm") : "—");
        lastConnItem->setData(item.lastConnectTime, Qt::UserRole);
        lastConnItem->setTextAlignment(Qt::AlignCenter);
        lastConnItem->setForeground(QBrush(QColor("#BAC0CB")));

        DeviceSortItem *expireItem = new DeviceSortItem(item.expire.isValid() ? item.expire.toString("yyyy-MM-dd HH:mm") : "—");
        expireItem->setData(item.expire, Qt::UserRole);
        expireItem->setTextAlignment(Qt::AlignCenter);
        expireItem->setForeground(QBrush(QColor("#BAC0CB")));

        DeviceSortItem *pkgItem = new DeviceSortItem(QString::number(item.packages));
        pkgItem->setData(item.packages, Qt::UserRole);
        pkgItem->setTextAlignment(Qt::AlignCenter);
        pkgItem->setForeground(QBrush(QColor("#4CC2FF")));

        DeviceSortItem *connCountItem = new DeviceSortItem(QString::number(item.connectionCount));
        connCountItem->setData(item.connectionCount, Qt::UserRole);
        connCountItem->setTextAlignment(Qt::AlignCenter);
        connCountItem->setForeground(QBrush(QColor("#E3E5E8")));

        QString payStr = (item.purchasedType == 1) ? "👑 VIP" : ((item.purchasedType == 2) ? QString::number(item.purchasedValue) : "—");
        DeviceSortItem *payItem = new DeviceSortItem(payStr);
        int payRank = (item.purchasedType == 1) ? 999999 : ((item.purchasedType == 2) ? item.purchasedValue : 0);
        payItem->setData(payRank, Qt::UserRole);
        payItem->setTextAlignment(Qt::AlignCenter);
        if(item.purchasedType == 1)
            payItem->setForeground(QBrush(QColor("#FFD700")));
        else
            payItem->setForeground(QBrush(QColor("#8E9297")));

        DeviceSortItem *guarItem = new DeviceSortItem(item.serverQuarantee == 1 ? "✓ Активна" : "✗ Истекла");
        guarItem->setData(item.serverQuarantee == 1 ? 1 : 0, Qt::UserRole);
        guarItem->setTextAlignment(Qt::AlignCenter);
        if(item.serverQuarantee == 1)
            guarItem->setForeground(QBrush(QColor("#00E676")));
        else
            guarItem->setForeground(QBrush(QColor("#8E9297")));

        model->setItem(idx, 0, idItem);
        model->setItem(idx, 1, vendorItem);
        model->setItem(idx, 2, modelItem);
        model->setItem(idx, 3, logTimeItem);
        model->setItem(idx, 4, lastConnItem);
        model->setItem(idx, 5, expireItem);
        model->setItem(idx, 6, pkgItem);
        model->setItem(idx, 7, connCountItem);
        model->setItem(idx, 8, payItem);
        model->setItem(idx, 9, guarItem);
        ++idx;
    }

    table->setIconSize(QSize(22, 22));
    table->verticalHeader()->setDefaultSectionSize(34);
    table->verticalHeader()->hide();
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->horizontalHeader()->setSortIndicatorShown(true);
    table->horizontalHeader()->setSectionsClickable(true);
    table->setSortingEnabled(true);
}

void MyDeviceService::slotPullMyDeviceList(const QJsonObject responce, const QString guid, ServiceOperation so, bool ok)
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
