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
        if(std::get<1>(v[x]) >= min && std::get<1>(v[x]) <= max)
        {
            n << v[x];
        }
    }

    return n;
}

QString BuyVIPService::guid() const
{
    return IDServiceVIPBuyString;
}

QString BuyVIPService::widgetIconName()
{
    return "white-transfer";
}

BuyVIPService::BuyVIPService(QObject *parent) : Service(DeviceConnectType::None, parent), network(nullptr), listVariants(nullptr), balanceText(nullptr), buyButton(nullptr), infoAfterPeriod(nullptr)
{
    mind = -1;
    maxd = -1;
    dailyRate = 0;
}

bool BuyVIPService::canStart()
{
    return Service::canStart();
}

bool BuyVIPService::isStarted()
{
    return listVariants != nullptr || balanceText != nullptr || buyButton != nullptr || infoAfterPeriod != nullptr;
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
    MainWindow::current->accessUi_page_buyvip(listVariants, balanceText, infoAfterPeriod, buyButton);
    network = new Network(MainWindow::current);
    network->authedId = MainWindow::current->network.authedId;
    balanceText->setText(QString(BalanceStrFormat).arg(network->authedId.credits));

    QObject::connect(network, &Network::sPullServiceGUID, this, &BuyVIPService::service_guid_responce);

    QObject::disconnect(buyButton, &QPushButton::clicked, this, &BuyVIPService::click_buy_vip);
    QObject::connect(buyButton, &QPushButton::clicked, this, &BuyVIPService::click_buy_vip);

    QObject::disconnect(listVariants, &QComboBox::currentIndexChanged, this, &BuyVIPService::variant_selected);
    QObject::connect(listVariants, &QComboBox::currentIndexChanged, this, &BuyVIPService::variant_selected);

    QJsonObject request;
    request["type"] = "get";
    network->pullServiceGUID(guid(), request);

    return true;
}

void BuyVIPService::stop()
{
    listVariants = nullptr;
    balanceText = nullptr;
    buyButton = nullptr;
    infoAfterPeriod = nullptr;
    mPresets.clear();
    mind = -1;
    maxd = -1;
    dailyRate = 0;
    if(network)
    {
        delete network;
        network = nullptr;
    }
}

void BuyVIPService::reset()
{
    stop();
}

void BuyVIPService::click_buy_vip()
{
    QString error_msg;

    if(listVariants->currentIndex() == -1 || listVariants->currentIndex() == 0)
        error_msg = "Выберите вариант из списка.";
    else if(network->authedId.credits == 0)
        error_msg = "Недостаточна средств на вашем балансе, для начало пополните ее через Поддержка->связаться.";

    if(!error_msg.isEmpty())
        QMessageBox::warning(MainWindow::current, "Попытка покупки не удалась", error_msg);
}

void BuyVIPService::variant_selected()
{
    buyButton->setEnabled(listVariants->currentIndex() > 0);

    QString message;
    if(!mPresets.isEmpty())
    {
        if(listVariants->currentIndex() > 0)
            message = QString("Стоимость вашей заявки будет %1").arg(dailyRate * (std::get<1>(mPresets[listVariants->currentIndex()-1])));
        else
            message = "Выберите доступный вариант.";
    }
    else
    {
        message = "Нет вариантов для покупки.";
    }

    infoAfterPeriod->setText(message);
}

void BuyVIPService::service_guid_responce(const QJsonObject responce, const QString guid, bool ok)
{
    listVariants->clear();
    mind = -1;
    maxd = -1;
    dailyRate = 0;
    if(!ok)
        return;

    mind = responce["min_days"].toInt();
    maxd = responce["max_days"].toInt();
    dailyRate = responce["dailyRate"].toInt();

    mPresets = dayPresetsFilter(mind, maxd);

    listVariants->addItem("Выберите в списке");
    for(int x = 0; x < mPresets.size(); ++x)
    {
        listVariants->addItem(std::get<0>(mPresets[x]));
    }
}
