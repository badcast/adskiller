#include <QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSignalSpy>
#include "Services.h"
#include "mainwindow.h"

// Define MainWindow globally so tests can link against it
MainWindow* MainWindow::current = nullptr;

class TestMyDeviceService : public QObject
{
    Q_OBJECT

private slots:
    void testParseNetworkResponse()
    {
        MyDeviceService service;
        
        QJsonObject response;
        QJsonArray actualArray;
        QJsonObject actualObj;
        actualObj["devId"] = 101;
        actualObj["vendor"] = "Samsung";
        actualObj["model"] = "Galaxy S23";
        actualObj["logTime"] = 1672531200; // 2023-01-01
        actualObj["lastConnectTime"] = 1672531200;
        actualObj["expire"] = 1704067200; // 2024-01-01
        actualObj["purchased_type"] = 1; // VIP
        actualObj["connectionCount"] = 5;
        actualObj["packages"] = 120;
        actualArray.append(actualObj);
        
        response["actual"] = actualArray;
        response["expired"] = QJsonArray(); // Empty expired array
        
        QSignalSpy devicesSpy(&service, SIGNAL(devicesChanged()));
        
        // Simulate network response
        // void slotPullMyDeviceList(const QJsonObject responce, const QString guid, ServiceOperation so, bool ok);
        service.slotPullMyDeviceList(response, service.uuid(), ServiceOperation::Get, true);
        
        // Check if parsing worked
        QVariantList devices = service.devices();
        QCOMPARE(devices.size(), 1);
        
        QVariantMap map = devices.first().toMap();
        QCOMPARE(map["vendor"].toString(), QString("Samsung"));
        QCOMPARE(map["model"].toString(), QString("Galaxy S23"));
        QCOMPARE(map["id"].toString(), QString("101"));
        QCOMPARE(map["purchased"].toString(), QString("VIP"));
        QCOMPARE(map["guarantee"].toString(), QString("Да"));
        
        QCOMPARE(devicesSpy.count(), 1);
    }
};

QTEST_MAIN(TestMyDeviceService)
#include "TestMyDeviceService.moc"
