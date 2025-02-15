#ifndef PACKAGES_H
#define PACKAGES_H

#include <QString>
#include <QList>

#include "network.h"

struct PackageIO
{
    QString applicationName;
    QString packageName;
};

QList<PackageIO> adsFilterBy(Network &net, const QList<PackageIO> & packages);

#endif // PACKAGES_H
