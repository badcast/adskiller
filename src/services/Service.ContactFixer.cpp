#include <memory>
#include <fstream>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <NumberPreview.h>
#include <vcard.h>
#include <text_io.h>

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

PageIndex ContactFixerService::targetPage()
{
    return PageIndex::ContactFixerPage;
}

QString ContactFixerService::widgetIconName()
{
    return "white-transfer";
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
    return true; // We don't have long-running background task here
}

void ContactFixerService::stop()
{
}

QVariantList ContactFixerService::loadVcf(const QString &path)
{
    QVariantList list;
    QString localPath = QUrl(path).toLocalFile();
    if(localPath.isEmpty())
        localPath = path;

    std::ifstream ifs(localPath.toStdString());
    if(!ifs.is_open())
        return list;

    TextReader reader(ifs);
    std::vector<vCard> cards = reader.parseCards();

    for(vCard &card : cards)
    {
        QVariantMap cmap;
        // Find FN (Formatted Name)
        try
        {
            vCardProperty fnProp = card.at(VC_FORMATTED_NAME);
            cmap["name"] = QString::fromStdString(fnProp.getValue());
        }
        catch(...)
        {
            cmap["name"] = "Unknown Contact";
        }

        // Find TEL
        QVariantList numbers;
        for(size_t i = 0; i < card.count(); ++i)
        {
            vCardProperty prop = card.properties()[i];
            if(prop.getName() == VC_TELEPHONE)
            {
                numbers.append(QString::fromStdString(prop.getValue()));
            }
        }
        cmap["numbers"] = numbers;
        list.append(cmap);
    }
    return list;
}

bool ContactFixerService::exportVcf(const QVariantList &contacts, const QString &path)
{
    QString localPath = QUrl(path).toLocalFile();
    if(localPath.isEmpty())
        localPath = path;

    std::ofstream ofs(localPath.toStdString());
    if(!ofs.is_open())
        return false;

    TextWriter writer(ofs);
    std::vector<vCard> cards;

    for(const QVariant &cvar : contacts)
    {
        QVariantMap cmap = cvar.toMap();
        vCard card(VC_VER_3_0);

        card.addProperty(vCardProperty::createName("", cmap["name"].toString().toStdString()));
        card.addProperty(vCardProperty(VC_FORMATTED_NAME, cmap["name"].toString().toStdString()));

        QVariantList numbers = cmap["numbers"].toList();
        for(const QVariant &numvar : numbers)
        {
            // Fix the number before exporting
            NumberPreview np(numvar.toString().toStdString());
            std::string fixed = np.format(NumberFormat::Beauty | NumberFormat::Global);
            if(fixed.empty())
                fixed = numvar.toString().toStdString();

            card.addProperty(vCardProperty(VC_TELEPHONE, fixed));
        }
        cards.push_back(card);
    }

    writer << cards;
    return true;
}

QVariantMap ContactFixerService::parseNumber(const QString &number)
{
    QVariantMap result;
    NumberPreview np(number.toStdString());

    result["isEmpty"] = np.isEmpty();
    if(np.isEmpty())
        return result;

    result["isGeneric"] = np.isGenericNumber();
    result["country"] = QString::fromStdString(np.country());
    result["dialCode"] = QString::fromStdString(np.dialCode());
    result["beautyGlobal"] = QString::fromStdString(np.format(NumberFormat::Beauty | NumberFormat::Global));
    result["beautyLocal"] = QString::fromStdString(np.format(NumberFormat::Beauty | NumberFormat::Local));
    result["compactGlobal"] = QString::fromStdString(np.format(NumberFormat::Compact | NumberFormat::Global));

    return result;
}
