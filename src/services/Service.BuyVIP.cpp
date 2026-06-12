#include <tuple>

#include <QJsonObject>

#include "Services.h"
#include "mainwindow.h"

constexpr auto BalanceStrFormat = "Ваш баланс: %1";

QList<std::tuple<QString, int>> dayPresetsFilter(int min, int max)
{
    QList<std::tuple<QString, int>> v = {{"1 день", 1}, {"2 дня", 2}, {"3 дня", 3}, {"4 дня", 4}, {"5 дней", 5}, {"1 неделя", 7}, {"2 недели", 14}, {"3 недели", 21}, {"4 недели", 28}};
    QList<std::tuple<QString, int>> n;

    for(int x = 0; x < v.size(); ++x)
    {
        if(std::get<int>(v[x]) >= min && std::get<int>(v[x]) <= max)
        {
            n << v[x];
        }
    }

    return n;
}

QString BuyVIPService::uuid() const
{
    return IDServiceVIPBuyString;
}

QString BuyVIPService::widgetIconName()
{
    return "white-transfer";
}

BuyVIPService::BuyVIPService(QObject *parent) : Service(DeviceConnectType::None, parent), network(nullptr)
{
    mind = -1;
    maxd = -1;
    dailyRate = 0;
}

BuyVIPService::~BuyVIPService()
{
    if(network)
    {
        delete network;
        network = nullptr;
    }
}

bool BuyVIPService::canStart()
{
    return Service::canStart();
}

bool BuyVIPService::isStarted()
{
    return network != nullptr;
}

PageIndex BuyVIPService::targetPage()
{
    return PageIndex::BuyVIPPage;
}

bool BuyVIPService::isFinish()
{
    return false;
}

bool BuyVIPService::start()
{
    if(isStarted())
        return false;

    network = new Network(MainWindow::current->network);
    network->authedId = MainWindow::current->network.authedId;
    setBalanceText(QString(BalanceStrFormat).arg(network->authedId.credits));

    QObject::connect(network, &Network::sPullServiceUUID, this, &BuyVIPService::service_uuid_responce);

    network->pullServiceUUID(uuid(), QJsonObject {}, ServiceOperation::Get);

    return true;
}

void BuyVIPService::stop()
{
    mPresets.clear();
    mind = -1;
    maxd = -1;
    dailyRate = 0;
    setVariants(QStringList {});
}

void BuyVIPService::buyVip(int index)
{
    QString error_msg;

    if(index == -1 || index == 0)
        error_msg = "Выберите вариант из списка.";
    else if(network->authedId.credits == 0 || network->authedId.credits < std::get<int>(mPresets[index - 1]) * dailyRate)
        error_msg = "Недостаточна средств на вашем балансе, для начало пополните ее через Поддержка->связаться.";

    if(!error_msg.isEmpty())
    {
        // In QML, we can just set infoText or emit a signal. For now, since QML can't natively show QMessageBox directly from C++ without blocking,
        // we can set infoText to error msg. But we will keep QMessageBox since QML supports native dialogs popping up from C++ just fine!
        QMessageBox::warning(nullptr, "Попытка покупки не удалась", error_msg);
        return;
    }

    QJsonObject request;
    request["days"] = std::get<int>(mPresets[index - 1]);

    network->pullServiceUUID(uuid(), request, ServiceOperation::Set);
}

void BuyVIPService::selectVariant(int index)
{
    if(!isStarted())
        return;

    QString message;
    if(!mPresets.isEmpty())
    {
        if(index > 0)
            message = QString("Стоимость вашей заявки будет %1").arg(dailyRate * (std::get<1>(mPresets[index - 1])));
        else
            message = "Выберите доступный вариант.";
    }
    else
    {
        message = "Нет вариантов для покупки.";
    }

    setInfoText(message);
}

void BuyVIPService::service_uuid_responce(const QJsonObject responce, const QString uuid, ServiceOperation so, bool ok)
{
    mind = -1;
    maxd = -1;
    dailyRate = 0;
    if(!ok)
    {
        int i = QMessageBox::warning(nullptr, "Ошибка сети", "Обнаружена проблема с подключением к сети, что делать дальше, вам потребуется перезапустить данный сервис или выйти в личный кабинет.", "Выйти", "Перезапустить");
        if(i == 0)
        {
            close();
        }
        else
        {
            restart();
        }
        return;
    }

    if(so == ServiceOperation::Get)
    {
        mind = responce["min_days"].toInt();
        maxd = responce["max_days"].toInt();
        dailyRate = responce["dailyRate"].toInt();

        mPresets = dayPresetsFilter(mind, maxd);

        QStringList varList;
        varList << "Выберите в списке";
        for(int x = 0; x < mPresets.size(); ++x)
        {
            varList << std::get<0>(mPresets[x]);
        }
        setVariants(varList);
    }
    else if(so == ServiceOperation::Set)
    {
        if(responce["subtracked"].toInt() > 0)
            QMessageBox::information(nullptr, "Уведомление", "VIP успешно куплен.");
        else
            QMessageBox::warning(nullptr, "Уведомление", "Ошибка транзакций.");
        close();
    }
}
