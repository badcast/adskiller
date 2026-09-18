#include <algorithm>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryFile>

#include "AdbShellDexApp.h"
#include "DexAgentBytes.h"

namespace
{
    const char *REMOTE_DEX_PATH = "/data/local/tmp/adskiller_dexagent.dex";
}

AdbShellDexApp::AdbShellDexApp(AdbShell &shell) : m_shell(shell)
{
}

QString AdbShellDexApp::remoteDexPath()
{
    return REMOTE_DEX_PATH;
}

bool AdbShellDexApp::ensureDeployed()
{
    if(!m_shell.isConnect())
        return false;

    if(m_deployed)
        return true;

    // Check if dex file exists on device with matching size
    auto statReply = m_shell.commandQueueWait(QStringList() << "stat" << "-c" << "%s" << REMOTE_DEX_PATH << "2>/dev/null");
    if(statReply.first && statReply.second.trimmed().toUInt() == s_dexAgentBytesLen)
    {
        m_deployed = true;
        return true;
    }

    // Write embedded dex to a local temp file and push to device
    QTemporaryFile tempFile;
    if(!tempFile.open())
        return false;

    tempFile.write(reinterpret_cast<const char *>(s_dexAgentBytes), s_dexAgentBytesLen);
    tempFile.flush();
    QString tempPath = tempFile.fileName();
    tempFile.close();

    bool pushed = m_shell.pushFile(tempPath, REMOTE_DEX_PATH);
    QFile::remove(tempPath);

    if(!pushed)
        return false;

    // Ensure permissions
    m_shell.commandQueueWait(QStringList() << "chmod" << "644" << REMOTE_DEX_PATH);

    m_deployed = true;
    return true;
}

bool AdbShellDexApp::isAvailable()
{
    if(!m_shell.isConnect())
        return false;

    if(!ensureDeployed())
        return false;

    auto testReply = m_shell.commandQueueWait(
        QStringList() << "CLASSPATH=" + QString(REMOTE_DEX_PATH)
                      << "app_process" << "/system/bin"
                      << "com.adskiller.agent.DexAgent" << "test" << "2>/dev/null");

    return testReply.first || testReply.second.contains("ERR:");
}

QList<AdbPackageInfo> AdbShellDexApp::getPackageList()
{
    QList<AdbPackageInfo> result;
    if(!ensureDeployed())
        return result;

    auto reply = m_shell.commandQueueWait(
        QStringList() << "CLASSPATH=" + QString(REMOTE_DEX_PATH)
                      << "app_process" << "/system/bin"
                      << "com.adskiller.agent.DexAgent" << "list" << "2>/dev/null");

    if(!reply.first || reply.second.trimmed().isEmpty())
        return result;

    QByteArray rawJson = reply.second.trimmed().toUtf8();
    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(rawJson, &parseErr);
    if(doc.isNull() || !doc.isArray())
    {
        qWarning() << "AdbShellDexApp::getPackageList: JSON parse error:" << parseErr.errorString();
        return result;
    }

    QJsonArray array = doc.array();
    for(const QJsonValue &val : array)
    {
        if(!val.isObject())
            continue;

        QJsonObject obj = val.toObject();
        AdbPackageInfo info;
        info.packageName = obj.value("package").toString();
        info.appName = obj.value("name").toString();
        info.isSystem = obj.value("isSystem").toBool();
        info.isDisabled = obj.value("disabled").toBool();
        info.apkPath = obj.value("apkPath").toString();
        info.versionName = obj.value("versionName").toString();

        if(info.appName.isEmpty())
            info.appName = AdbShell::friendlyAppName(info.packageName);

        result.append(std::move(info));
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const AdbPackageInfo &a, const AdbPackageInfo &b)
        {
            if(a.isSystem != b.isSystem)
                return !a.isSystem;
            return a.appName.localeAwareCompare(b.appName) < 0;
        });

    return result;
}

QByteArray AdbShellDexApp::getAppIcon(const QString &packageName, int size)
{
    if(packageName.isEmpty() || !ensureDeployed())
        return {};

    auto reply = m_shell.commandQueueWait(
        QStringList() << "CLASSPATH=" + QString(REMOTE_DEX_PATH)
                      << "app_process" << "/system/bin"
                      << "com.adskiller.agent.DexAgent" << "icon" << packageName << QString::number(size) << "2>/dev/null");

    if(!reply.first || reply.second.trimmed().isEmpty())
        return {};

    QString out = reply.second.trimmed();
    if(out.startsWith("ERR:") || out.isEmpty())
        return {};

    return QByteArray::fromBase64(out.toLatin1());
}

AdbPackageDetails AdbShellDexApp::getPackageDetails(const QString &packageName)
{
    AdbPackageDetails details;
    if(packageName.isEmpty() || !ensureDeployed())
        return details;

    auto reply = m_shell.commandQueueWait(
        QStringList() << "CLASSPATH=" + QString(REMOTE_DEX_PATH)
                      << "app_process" << "/system/bin"
                      << "com.adskiller.agent.DexAgent" << "info" << packageName << "2>/dev/null");

    if(!reply.first || reply.second.trimmed().isEmpty())
        return details;

    QByteArray rawJson = reply.second.trimmed().toUtf8();
    QJsonDocument doc = QJsonDocument::fromJson(rawJson);
    if(doc.isNull() || !doc.isObject())
        return details;

    QJsonObject obj = doc.object();
    details.packageName = obj.value("package").toString();
    details.appName = obj.value("name").toString();
    details.versionName = obj.value("versionName").toString();
    details.versionCode = QString::number(obj.value("versionCode").toInteger());
    if(obj.contains("minSdk"))
        details.minSdk = QString::number(obj.value("minSdk").toInteger());
    if(obj.contains("targetSdk"))
        details.targetSdk = QString::number(obj.value("targetSdk").toInteger());
    details.codePath = obj.value("codePath").toString();
    details.dataDir = obj.value("dataDir").toString();
    details.isSystem = obj.value("isSystem").toBool();
    details.isDisabled = obj.value("disabled").toBool();

    qint64 fInstall = obj.value("firstInstallTime").toVariant().toLongLong();
    if(fInstall > 0)
        details.firstInstallTime = QDateTime::fromMSecsSinceEpoch(fInstall).toString("yyyy-MM-dd HH:mm:ss");

    qint64 lUpdate = obj.value("lastUpdateTime").toVariant().toLongLong();
    if(lUpdate > 0)
        details.lastUpdateTime = QDateTime::fromMSecsSinceEpoch(lUpdate).toString("yyyy-MM-dd HH:mm:ss");

    QJsonArray perms = obj.value("permissions").toArray();
    for(const QJsonValue &p : perms)
        details.requestedPermissions.append(p.toString());

    QJsonArray acts = obj.value("activities").toArray();
    for(const QJsonValue &a : acts)
        details.activities.append(a.toString());
    if(!details.activities.isEmpty())
        details.mainActivity = details.activities.first();

    QJsonArray srvs = obj.value("services").toArray();
    for(const QJsonValue &s : srvs)
        details.services.append(s.toString());

    QJsonArray recs = obj.value("receivers").toArray();
    for(const QJsonValue &r : recs)
        details.receivers.append(r.toString());

    QJsonArray prvs = obj.value("providers").toArray();
    for(const QJsonValue &p : prvs)
        details.providers.append(p.toString());

    if(!details.codePath.isEmpty())
    {
        auto sizeReply = m_shell.commandQueueWait(QStringList() << "stat" << "-c" << "%s" << "\"" + details.codePath + "\"" << "2>/dev/null");
        if(sizeReply.first && !sizeReply.second.trimmed().isEmpty())
        {
            qint64 sz = sizeReply.second.trimmed().toLongLong();
            if(sz > 0)
                details.apkSize = sz;
        }
    }

    return details;
}
