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
    return network != nullptr || listVariants != nullptr || balanceText != nullptr || buyButton != nullptr || infoAfterPeriod != nullptr;
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

    QObject::connect(network, &Network::sPullServiceUUID, this, &BuyVIPService::service_uuid_responce);
    QObject::connect(buyButton, &QPushButton::clicked, this, &BuyVIPService::click_buy_vip);
    QObject::connect(listVariants, &QComboBox::currentIndexChanged, this, &BuyVIPService::variant_selected);

    network->pullServiceUUID(guid(), QJsonObject {}, ServiceOperation::Get);

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
}

void BuyVIPService::click_buy_vip()
{
    QString error_msg;
    int i = listVariants->currentIndex();

    if(i == -1 || i == 0)
        error_msg = "Выберите вариант из списка.";
    else if(network->authedId.credits == 0 || network->authedId.credits < std::get<int>(mPresets[i - 1]) * dailyRate)
        error_msg = "Недостаточна средств на вашем балансе, для начало пополните ее через Поддержка->связаться.";
    if(!error_msg.isEmpty())
    {
        QMessageBox::warning(MainWindow::current, "Попытка покупки не удалась", error_msg);
        return;
    }

    QJsonObject request;
    request["days"] = std::get<int>(mPresets[i - 1]);

    network->pullServiceUUID(guid(), request, ServiceOperation::Set);
}

void BuyVIPService::variant_selected()
{
    if(!isStarted())
        return;
    buyButton->setEnabled(listVariants->currentIndex() > 0);

    QString message;
    if(!mPresets.isEmpty())
    {
        if(listVariants->currentIndex() > 0)
            message = QString("Стоимость вашей заявки будет %1").arg(dailyRate * (std::get<1>(mPresets[listVariants->currentIndex() - 1])));
        else
            message = "Выберите доступный вариант.";
    }
    else
    {
        message = "Нет вариантов для покупки.";
    }

    infoAfterPeriod->setText(message);
}

void BuyVIPService::service_uuid_responce(const QJsonObject responce, const QString uuid, ServiceOperation so, bool ok)
{
    listVariants->clear();
    mind = -1;
    maxd = -1;
    dailyRate = 0;
    if(!ok)
    {
        int i = QMessageBox::warning(MainWindow::current, "Ошибка сети", "Обнаружена проблема с подключением к сети, что делать дальше, вам потребуется перезапустить данный сервис или выйти в личный кабинет.", "Выйти", "Перезапустить");
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

        listVariants->addItem("Выберите в списке");
        for(int x = 0; x < mPresets.size(); ++x)
        {
            listVariants->addItem(std::get<0>(mPresets[x]));
        }
    }
    else if(so == ServiceOperation::Set)
    {
        if(responce["subtracked"].toInt() > 0)
            QMessageBox::information(MainWindow::current, "Уведомление", "VIP успешно куплен.");
        else
            QMessageBox::warning(MainWindow::current, "Уведомление", "Ошибка транзакций.");
        close();
    }
}
