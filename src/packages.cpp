
#include "packages.h"

QList<PackageIO> adsFilterBy(Network & net, const QList<PackageIO> &packages)
{
    QList<PackageIO> _filteredPackages;
    QStringList adsList;

    for(const PackageIO& item : packages)
    {
        if(std::any_of(adsList.begin(), adsList.end(), [&item](const QString& val){return val==item.packageName;}))
        {
            _filteredPackages.append(item);
        }
    }

    return _filteredPackages;
}
