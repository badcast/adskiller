#pragma once

#include <memory>
#include <QByteArray>
#include <QList>
#include <QString>
#include "adbfront.h"

class AdbShellDexApp
{
public:
    explicit AdbShellDexApp(AdbShell &shell);
    ~AdbShellDexApp() = default;

    // Checks whether the dex agent is available / deployed on the device
    bool isAvailable();

    // Ensures the dex agent is pushed to the device (/data/local/tmp/adskiller_dexagent.dex)
    bool ensureDeployed();

    // Retrieves list of all packages directly from Android PackageManager (real names, flags, paths)
    QList<AdbPackageInfo> getPackageList();

    // Retrieves pixel-perfect app icon rendered by Android framework as PNG byte array
    QByteArray getAppIcon(const QString &packageName, int size = 96);

    // Retrieves package details directly from Android PackageManager
    AdbPackageDetails getPackageDetails(const QString &packageName);

    // Remote path where the dex is deployed on the device
    static QString remoteDexPath();

private:
    AdbShell &m_shell;
    bool m_deployed = false;
};
