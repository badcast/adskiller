#include <algorithm>
#include <memory>

#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSet>
#include <QSplitter>
#include <QStringListModel>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QVBoxLayout>

#include "ApkManagerWidget.h"
#include "Services.h"
#include "mainwindow.h"

namespace
{
    QString formatBytes(qint64 bytes)
    {
        if(bytes >= 1024ULL * 1024ULL * 1024ULL)
            return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " ГБ";
        if(bytes >= 1024ULL * 1024ULL)
            return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " МБ";
        if(bytes >= 1024ULL)
            return QString::number(bytes / 1024.0, 'f', 1) + " КБ";
        if(bytes > 0)
            return QString::number(bytes) + " Б";
        return "0 Б";
    }

    QString sdkToAndroidVersion(int sdk)
    {
        switch(sdk)
        {
            case 35:
                return "Android 15";
            case 34:
                return "Android 14 (Upside Down Cake)";
            case 33:
                return "Android 13 (Tiramisu)";
            case 32:
                return "Android 12L";
            case 31:
                return "Android 12 (Snow Cone)";
            case 30:
                return "Android 11 (Red Velvet Cake)";
            case 29:
                return "Android 10 (Quince Tart)";
            case 28:
                return "Android 9.0 (Pie)";
            case 27:
                return "Android 8.1 (Oreo MR1)";
            case 26:
                return "Android 8.0 (Oreo)";
            case 25:
                return "Android 7.1 (Nougat MR1)";
            case 24:
                return "Android 7.0 (Nougat)";
            case 23:
                return "Android 6.0 (Marshmallow)";
            case 22:
                return "Android 5.1 (Lollipop MR1)";
            case 21:
                return "Android 5.0 (Lollipop)";
            case 19:
                return "Android 4.4 (KitKat)";
            default:
                if(sdk > 35)
                    return QString("Android API %1").arg(sdk);
                if(sdk > 0)
                    return QString("Android (API %1)").arg(sdk);
                return "Не указан";
        }
    }

    QString friendlyAppName(const QString &pkgName)
    {
        static const QHash<QString, QString> knownApps = {
            {"com.whatsapp", "WhatsApp"},
            {"com.whatsapp.w4b", "WhatsApp Business"},
            {"org.telegram.messenger", "Telegram"},
            {"org.telegram.plus", "Telegram Plus"},
            {"com.instagram.android", "Instagram"},
            {"com.facebook.katana", "Facebook"},
            {"com.facebook.orca", "Messenger"},
            {"com.facebook.lite", "Facebook Lite"},
            {"com.google.android.youtube", "YouTube"},
            {"com.google.android.apps.youtube.music", "YouTube Music"},
            {"com.android.chrome", "Google Chrome"},
            {"com.google.android.gm", "Gmail"},
            {"com.google.android.apps.maps", "Google Карты"},
            {"com.google.android.apps.photos", "Google Фото"},
            {"com.google.android.googlequicksearchbox", "Google Поиск"},
            {"com.google.android.play.games", "Google Play Игры"},
            {"com.android.vending", "Google Play Маркет"},
            {"com.google.android.calendar", "Google Календарь"},
            {"com.google.android.contacts", "Google Контакты"},
            {"com.google.android.dialer", "Google Телефон"},
            {"com.google.android.apps.messaging", "Google Сообщения"},
            {"com.google.android.keep", "Google Заметки"},
            {"com.google.android.apps.docs", "Google Документы"},
            {"com.spotify.music", "Spotify"},
            {"com.zhiliaoapp.musically", "TikTok"},
            {"com.ss.android.ugc.trill", "TikTok"},
            {"com.vkontakte.android", "ВКонтакте"},
            {"ru.yandex.searchplugin", "Яндекс с Алисой"},
            {"com.yandex.browser", "Яндекс Браузер"},
            {"ru.yandex.taxi", "Яндекс Go"},
            {"ru.yandex.music", "Яндекс Музыка"},
            {"ru.yandex.yandexmaps", "Яндекс Карты"},
            {"ru.sberbankmobile", "СберБанк Онлайн"},
            {"ru.tinkoff.activities", "Т-Банк (Тинькофф)"},
            {"com.viber.voip", "Viber"},
            {"com.discord", "Discord"},
            {"com.opera.browser", "Opera"},
            {"com.opera.mini.native", "Opera Mini"},
            {"org.mozilla.firefox", "Firefox"},
            {"com.miui.gallery", "Галерея Xiaomi"},
            {"com.miui.cleanmaster", "Безопасность Xiaomi"},
            {"com.sec.android.app.camera", "Камера Samsung"},
            {"com.sec.android.gallery3d", "Галерея Samsung"},
            {"com.android.settings", "Настройки"},
            {"com.android.camera", "Камера"},
            {"com.android.gallery3d", "Галерея"},
            {"com.android.contacts", "Контакты"},
            {"com.android.mms", "Сообщения"},
            {"com.android.dialer", "Телефон"},
            {"com.android.providers.media", "Хранилище мультимедиа"},
            {"com.android.providers.downloads", "Загрузки"},
            {"com.android.packageinstaller", "Установщик пакетов"}};

        if(knownApps.contains(pkgName))
            return knownApps.value(pkgName);

        QStringList parts = pkgName.split('.', Qt::SkipEmptyParts);
        if(parts.isEmpty())
            return pkgName;

        QString cand = parts.last();
        if((cand == "android" || cand == "app" || cand == "mobile" || cand == "client") && parts.size() > 1)
            cand = parts[parts.size() - 2];

        if(!cand.isEmpty())
        {
            cand[0] = cand[0].toUpper();
            return cand;
        }

        return pkgName;
    }

    QIcon generateFallbackIcon(const QString &appName, const QString &pkgName, bool isSystem)
    {
        const int size = 64;
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);

        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);

        QRectF rect(1, 1, size - 2, size - 2);
        QPainterPath path;
        path.addRoundedRect(rect, 15, 15);

        QLinearGradient grad(0, 0, 0, size);
        if(isSystem)
        {
            grad.setColorAt(0.0, QColor(51, 65, 85));
            grad.setColorAt(1.0, QColor(30, 41, 59));
        }
        else
        {
            uint h = qHash(pkgName);
            int hue = h % 360;
            grad.setColorAt(0.0, QColor::fromHsv(hue, 180, 220));
            grad.setColorAt(1.0, QColor::fromHsv((hue + 45) % 360, 210, 150));
        }

        p.fillPath(path, grad);

        QPen borderPen(QColor(255, 255, 255, 45), 1.5);
        p.setPen(borderPen);
        p.drawPath(path);

        QString monogram = "?";
        if(!appName.trimmed().isEmpty())
        {
            QString clean = appName.trimmed();
            if(clean.size() >= 2 && clean[0].isLetter() && clean[1].isLetter())
                monogram = clean.left(2).toUpper();
            else
                monogram = clean.left(1).toUpper();
        }

        QFont font("Segoe UI", 16, QFont::Bold);
        p.setFont(font);
        p.setPen(Qt::white);
        p.drawText(rect, Qt::AlignCenter, monogram);

        p.end();
        return QIcon(pixmap);
    }

    QIcon extractAppIcon(AdbShell &shell, const QString &apkPath, const QString &pkgName, const QString &appName, bool isSystem, QMap<QString, QIcon> &cache)
    {
        if(cache.contains(pkgName))
            return cache.value(pkgName);

        QString cacheDir = QDir::tempPath() + "/adskiller_apk_icons";
        QString cacheFile = cacheDir + "/" + pkgName + ".png";
        if(QFile::exists(cacheFile))
        {
            QPixmap diskPix(cacheFile);
            if(!diskPix.isNull())
            {
                QIcon icon(diskPix);
                cache[pkgName] = icon;
                return icon;
            }
        }

        if(!apkPath.isEmpty())
        {
            auto listReply = shell.commandQueueWait(
                QStringList() << "unzip" << "-l" << "\"" + apkPath + "\"" << "res/*ic_launcher*.png" << "res/*launcher*.png"
                              << "res/*icon*.png" << "res/*logo*.png"
                              << "*.webp" << "2>/dev/null");

            if(listReply.first && !listReply.second.isEmpty())
            {
                QString bestEntry;
                int bestScore = -1;

                QStringList lines = listReply.second.split('\n', Qt::SkipEmptyParts);
                for(const QString &line : lines)
                {
                    QString trimmed = line.trimmed();
                    int resIdx = trimmed.indexOf("res/");
                    if(resIdx == -1)
                        continue;

                    QString entry = trimmed.mid(resIdx).trimmed();
                    if(!entry.endsWith(".png", Qt::CaseInsensitive) && !entry.endsWith(".webp", Qt::CaseInsensitive))
                        continue;

                    int score = 0;
                    if(entry.contains("xxxhdpi"))
                        score += 50;
                    else if(entry.contains("xxhdpi"))
                        score += 40;
                    else if(entry.contains("xhdpi"))
                        score += 30;
                    else if(entry.contains("hdpi"))
                        score += 20;
                    else if(entry.contains("mdpi"))
                        score += 10;

                    if(entry.contains("ic_launcher"))
                        score += 15;
                    else if(entry.contains("icon"))
                        score += 10;

                    if(score > bestScore)
                    {
                        bestScore = score;
                        bestEntry = entry;
                    }
                }

                if(!bestEntry.isEmpty())
                {
                    auto dumpReply = shell.commandQueueWait(QStringList() << "unzip" << "-p" << "\"" + apkPath + "\"" << "\"" + bestEntry + "\"" << "2>/dev/null" << "|" << "base64");

                    if(dumpReply.first && !dumpReply.second.isEmpty())
                    {
                        QByteArray rawData = QByteArray::fromBase64(dumpReply.second.toLatin1());
                        if(!rawData.isEmpty())
                        {
                            QPixmap pix;
                            if(pix.loadFromData(rawData))
                            {
                                QDir().mkpath(cacheDir);
                                pix.save(cacheFile, "PNG");
                                QIcon icon(pix);
                                cache[pkgName] = icon;
                                return icon;
                            }
                        }
                    }
                }
            }
        }

        QIcon fallback = generateFallbackIcon(appName, pkgName, isSystem);
        cache[pkgName] = fallback;
        return fallback;
    }

    AppDetails parseDumpsys(const QString &dumpsysOutput, const QString &pkgName)
    {
        AppDetails details;
        details.packageName = pkgName;
        details.rawDumpsys = dumpsysOutput;

        static QRegularExpression reVerName("versionName=([^\r\n\\s]+)");
        auto mVerName = reVerName.match(dumpsysOutput);
        if(mVerName.hasMatch())
            details.versionName = mVerName.captured(1);

        static QRegularExpression reVerCode("versionCode=(\\d+)");
        auto mVerCode = reVerCode.match(dumpsysOutput);
        if(mVerCode.hasMatch())
            details.versionCode = mVerCode.captured(1);

        static QRegularExpression reMinSdk("minSdk=(\\d+)");
        auto mMinSdk = reMinSdk.match(dumpsysOutput);
        if(mMinSdk.hasMatch())
            details.minSdk = mMinSdk.captured(1);

        static QRegularExpression reTargetSdk("targetSdk=(\\d+)");
        auto mTargetSdk = reTargetSdk.match(dumpsysOutput);
        if(mTargetSdk.hasMatch())
            details.targetSdk = mTargetSdk.captured(1);

        static QRegularExpression reCodePath("codePath=([^\r\n\\s]+)");
        auto mCodePath = reCodePath.match(dumpsysOutput);
        if(mCodePath.hasMatch())
            details.codePath = mCodePath.captured(1);

        static QRegularExpression reDataDir("dataDir=([^\r\n\\s]+)");
        auto mDataDir = reDataDir.match(dumpsysOutput);
        if(mDataDir.hasMatch())
            details.dataDir = mDataDir.captured(1);

        static QRegularExpression reFirstInstall("firstInstallTime=([0-9\\-]+\\s+[0-9:]+)");
        auto mFirstInstall = reFirstInstall.match(dumpsysOutput);
        if(mFirstInstall.hasMatch())
            details.firstInstallTime = mFirstInstall.captured(1);

        static QRegularExpression reLastUpdate("lastUpdateTime=([0-9\\-]+\\s+[0-9:]+)");
        auto mLastUpdate = reLastUpdate.match(dumpsysOutput);
        if(mLastUpdate.hasMatch())
            details.lastUpdateTime = mLastUpdate.captured(1);

        static QRegularExpression reInstaller("installerPackageName=([^\r\n\\s]+)");
        auto mInstaller = reInstaller.match(dumpsysOutput);
        if(mInstaller.hasMatch())
            details.installer = mInstaller.captured(1);

        static QRegularExpression reAbi("primaryCpuAbi=([^\r\n\\s]+)");
        auto mAbi = reAbi.match(dumpsysOutput);
        if(mAbi.hasMatch())
            details.primaryCpuAbi = mAbi.captured(1);

        static QRegularExpression reSignatures("signatures=PackageSignatures\\{([^\\}]+)\\}");
        auto mSignatures = reSignatures.match(dumpsysOutput);
        if(mSignatures.hasMatch())
            details.signatures = mSignatures.captured(1);

        int reqIdx = dumpsysOutput.indexOf("requested permissions:");
        if(reqIdx != -1)
        {
            int endIdx = dumpsysOutput.indexOf("\n    install permissions:", reqIdx);
            if(endIdx == -1)
                endIdx = dumpsysOutput.indexOf("\n    runtime permissions:", reqIdx);
            if(endIdx == -1)
                endIdx = dumpsysOutput.indexOf("\n  User 0:", reqIdx);

            QString permBlock = (endIdx != -1) ? dumpsysOutput.mid(reqIdx, endIdx - reqIdx) : dumpsysOutput.mid(reqIdx, 4000);
            QStringList lines = permBlock.split('\n');
            for(const QString &l : lines)
            {
                QString trimmed = l.trimmed();
                if(trimmed.startsWith("android.permission.") || trimmed.contains(".permission."))
                {
                    if(!details.requestedPermissions.contains(trimmed))
                        details.requestedPermissions.append(trimmed);
                }
            }
        }

        static QRegularExpression reGranted("([a-zA-Z0-9_\\.]+):\\s*granted=true");
        auto matchIter = reGranted.globalMatch(dumpsysOutput);
        while(matchIter.hasNext())
        {
            auto match = matchIter.next();
            details.grantedPermissions.insert(match.captured(1));
        }

        int actIdx = dumpsysOutput.indexOf("Activity Resolver Table:");
        if(actIdx != -1)
        {
            int actEnd = dumpsysOutput.indexOf("Receiver Resolver Table:", actIdx);
            if(actEnd == -1)
                actEnd = dumpsysOutput.indexOf("Service Resolver Table:", actIdx);
            QString actBlock = (actEnd != -1) ? dumpsysOutput.mid(actIdx, actEnd - actIdx) : dumpsysOutput.mid(actIdx, 3000);
            static QRegularExpression reComp(pkgName + "/([a-zA-Z0-9_\\.]+)");
            auto actMatch = reComp.globalMatch(actBlock);
            while(actMatch.hasNext())
            {
                QString act = actMatch.next().captured(1);
                if(!details.activities.contains(act))
                    details.activities.append(act);
            }
        }
        if(!details.activities.isEmpty())
            details.mainActivity = details.activities.first();

        int srvIdx = dumpsysOutput.indexOf("Service Resolver Table:");
        if(srvIdx != -1)
        {
            int srvEnd = dumpsysOutput.indexOf("Provider Resolver Table:", srvIdx);
            QString srvBlock = (srvEnd != -1) ? dumpsysOutput.mid(srvIdx, srvEnd - srvIdx) : dumpsysOutput.mid(srvIdx, 3000);
            static QRegularExpression reComp(pkgName + "/([a-zA-Z0-9_\\.]+)");
            auto srvMatch = reComp.globalMatch(srvBlock);
            while(srvMatch.hasNext())
            {
                QString srv = srvMatch.next().captured(1);
                if(!details.services.contains(srv))
                    details.services.append(srv);
            }
        }

        int recIdx = dumpsysOutput.indexOf("Receiver Resolver Table:");
        if(recIdx != -1)
        {
            int recEnd = dumpsysOutput.indexOf("Service Resolver Table:", recIdx);
            QString recBlock = (recEnd != -1) ? dumpsysOutput.mid(recIdx, recEnd - recIdx) : dumpsysOutput.mid(recIdx, 3000);
            static QRegularExpression reComp(pkgName + "/([a-zA-Z0-9_\\.]+)");
            auto recMatch = reComp.globalMatch(recBlock);
            while(recMatch.hasNext())
            {
                QString rec = recMatch.next().captured(1);
                if(!details.receivers.contains(rec))
                    details.receivers.append(rec);
            }
        }

        int prvIdx = dumpsysOutput.indexOf("Provider Resolver Table:");
        if(prvIdx != -1)
        {
            int prvEnd = dumpsysOutput.indexOf("\n\n", prvIdx);
            QString prvBlock = (prvEnd != -1) ? dumpsysOutput.mid(prvIdx, prvEnd - prvIdx) : dumpsysOutput.mid(prvIdx, 3000);
            static QRegularExpression reComp(pkgName + "/([a-zA-Z0-9_\\.]+)");
            auto prvMatch = reComp.globalMatch(prvBlock);
            while(prvMatch.hasNext())
            {
                QString prv = prvMatch.next().captured(1);
                if(!details.providers.contains(prv))
                    details.providers.append(prv);
            }
        }

        return details;
    }
} // namespace

ApkManagerWidget::ApkManagerWidget(QWidget *parent) : QWidget(parent)
{
    setupUi();
}

void ApkManagerWidget::setDevice(const AdbDevice &device)
{
    m_device = device;
    m_shell.connect(device.devId);
}

void ApkManagerWidget::setupUi()
{
    setStyleSheet(
        "QWidget { background-color: #0A0E1A; color: #F8FAFC; font-family: 'Segoe UI', 'Noto Sans', sans-serif; }"
        "QTableWidget { background-color: #0B0F19; alternate-background-color: #0F172A; color: #F8FAFC; border: 1px solid #1E293B; border-radius: 8px; gridline-color: #161F33; selection-background-color: #0284C7; selection-color: #FFFFFF; font-size: 12px; }"
        "QHeaderView::section { background-color: #0F172A; color: #94A3B8; font-weight: bold; font-size: 11px; border: none; border-bottom: 1px solid #1E293B; padding: 6px 8px; }"
        "QLineEdit { background-color: #0F172A; color: #F8FAFC; border: 1.5px solid #1E293B; border-radius: 8px; padding: 5px 10px; font-size: 12px; }"
        "QLineEdit:focus { border-color: #38BDF8; }"
        "QComboBox { background-color: #0F172A; color: #F8FAFC; border: 1.5px solid #1E293B; border-radius: 8px; padding: 5px 10px; font-size: 11.5px; font-weight: 500; }"
        "QComboBox:hover { border-color: #38BDF8; background-color: #131E35; }"
        "QComboBox::drop-down { border: none; width: 22px; }"
        "QComboBox QAbstractItemView { background-color: #0F172A; color: #F8FAFC; border: 1px solid #1E293B; selection-background-color: #0284C7; selection-color: #FFFFFF; padding: 4px; }"
        "QPushButton { background-color: #0F172A; color: #E2E8F0; border: 1.5px solid #1E293B; border-radius: 8px; padding: 5px 12px; font-size: 11.5px; font-weight: 600; }"
        "QPushButton:hover { background-color: #131E35; border-color: #38BDF8; color: #38BDF8; }"
        "QPushButton:pressed { background-color: #0B101D; border-color: #0284C7; }"
        "QPushButton:disabled { background-color: #0B101D; color: #475569; border-color: #1E293B; }"
        "QTabWidget::pane { border: 1px solid #1E293B; background-color: #0B0F19; border-radius: 8px; }"
        "QTabBar::tab { background: #0F172A; color: #94A3B8; padding: 6px 14px; font-size: 11.5px; font-weight: 600; border-top-left-radius: 6px; border-top-right-radius: 6px; margin-right: 2px; }"
        "QTabBar::tab:selected { background: #0B0F19; color: #38BDF8; border-bottom: 2px solid #38BDF8; }"
        "QTabBar::tab:hover:!selected { background: #131E35; color: #FFFFFF; }"
        "QTextEdit { background-color: #080C14; color: #E2E8F0; border: 1px solid #1E293B; border-radius: 8px; font-family: monospace; font-size: 11.5px; }"
        "QLabel { color: #94A3B8; font-size: 11.5px; }");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(14, 10, 14, 14);
    mainLayout->setSpacing(10);

    // Top Control Bar
    QHBoxLayout *topBar = new QHBoxLayout();
    topBar->setSpacing(8);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("🔍 Поиск по названию приложения или packageId...");
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &ApkManagerWidget::onSearchOrFilterChanged);

    m_filterCombo = new QComboBox(this);
    m_filterCombo->addItem("📱 Все приложения", "all");
    m_filterCombo->addItem("👤 Пользовательские (3rd-party)", "user");
    m_filterCombo->addItem("⚙ Системные (System)", "system");
    m_filterCombo->addItem("❄ Отключенные (Disabled)", "disabled");
    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ApkManagerWidget::onSearchOrFilterChanged);

    QPushButton *btnRefresh = new QPushButton("🔄 Обновить", this);
    connect(btnRefresh, &QPushButton::clicked, this, &ApkManagerWidget::loadPackages);

    QPushButton *btnInstall = new QPushButton("➕ Установить APK", this);
    btnInstall->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0284C7, stop:1 #0EA5E9); color: white; font-weight: bold; padding: 6px 14px; border: none; border-radius: 6px; }"
        "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0369A1, stop:1 #38BDF8); }");
    connect(btnInstall, &QPushButton::clicked, this, &ApkManagerWidget::installApk);

    topBar->addWidget(m_searchEdit, 2);
    topBar->addWidget(m_filterCombo, 1);
    topBar->addWidget(btnRefresh);
    topBar->addWidget(btnInstall);
    mainLayout->addLayout(topBar);

    // Central Splitter: Left is Package Table, Right is Package Inspector
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(4);
    splitter->setStyleSheet("QSplitter::handle { background-color: #1E293B; border-radius: 2px; }");

    // Left Pane (Package List)
    QWidget *leftContainer = new QWidget(splitter);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftContainer);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(6);

    m_table = new QTableWidget(leftContainer);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels(QStringList() << "Иконка" << "Приложение" << "Package ID" << "Тип" << "Состояние");
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->setShowGrid(false);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(42);
    m_table->setIconSize(QSize(28, 28));

    connect(m_table, &QTableWidget::itemSelectionChanged, this, &ApkManagerWidget::onPackageSelected);
    leftLayout->addWidget(m_table);
    splitter->addWidget(leftContainer);

    // Right Pane (Inspector)
    QWidget *rightContainer = new QWidget(splitter);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightContainer);
    rightLayout->setContentsMargins(6, 0, 0, 0);
    rightLayout->setSpacing(10);

    // Top App Info Card
    QFrame *appCard = new QFrame(rightContainer);
    appCard->setStyleSheet("QFrame { background-color: #0F172A; border: 1px solid #1E293B; border-radius: 12px; padding: 6px; }");
    QHBoxLayout *cardLayout = new QHBoxLayout(appCard);
    cardLayout->setContentsMargins(10, 8, 10, 8);
    cardLayout->setSpacing(14);

    m_appIconLabel = new QLabel(appCard);
    m_appIconLabel->setFixedSize(64, 64);
    m_appIconLabel->setAlignment(Qt::AlignCenter);

    QVBoxLayout *cardInfoLayout = new QVBoxLayout();
    cardInfoLayout->setSpacing(4);

    m_appNameLabel = new QLabel("Выберите приложение", appCard);
    m_appNameLabel->setStyleSheet("color: #FFFFFF; font-size: 15px; font-weight: bold; border: none;");

    QHBoxLayout *pkgRow = new QHBoxLayout();
    pkgRow->setSpacing(6);
    m_packageIdLabel = new QLabel("—", appCard);
    m_packageIdLabel->setStyleSheet("color: #38BDF8; font-size: 11.5px; font-weight: 600; border: none;");
    m_packageIdLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    QPushButton *btnCopyPkg = new QPushButton("📋", appCard);
    btnCopyPkg->setToolTip("Скопировать packageId");
    btnCopyPkg->setFixedSize(22, 22);
    connect(btnCopyPkg, &QPushButton::clicked, this, &ApkManagerWidget::copyPackageId);

    pkgRow->addWidget(m_packageIdLabel);
    pkgRow->addWidget(btnCopyPkg);
    pkgRow->addStretch(1);

    QHBoxLayout *badgesLayout = new QHBoxLayout();
    badgesLayout->setSpacing(6);

    m_typeBadge = new QLabel("—", appCard);
    m_typeBadge->setStyleSheet("background-color: #1E293B; color: #94A3B8; border: 1px solid #334155; border-radius: 4px; padding: 2px 8px; font-size: 10.5px; font-weight: 600;");

    m_statusBadge = new QLabel("—", appCard);
    m_statusBadge->setStyleSheet("background-color: #1E293B; color: #94A3B8; border: 1px solid #334155; border-radius: 4px; padding: 2px 8px; font-size: 10.5px; font-weight: 600;");

    m_installerBadge = new QLabel("—", appCard);
    m_installerBadge->setStyleSheet("background-color: #1E293B; color: #94A3B8; border: 1px solid #334155; border-radius: 4px; padding: 2px 8px; font-size: 10.5px; font-weight: 600;");

    badgesLayout->addWidget(m_typeBadge);
    badgesLayout->addWidget(m_statusBadge);
    badgesLayout->addWidget(m_installerBadge);
    badgesLayout->addStretch(1);

    cardInfoLayout->addWidget(m_appNameLabel);
    cardInfoLayout->addLayout(pkgRow);
    cardInfoLayout->addLayout(badgesLayout);

    cardLayout->addWidget(m_appIconLabel);
    cardLayout->addLayout(cardInfoLayout, 1);
    rightLayout->addWidget(appCard);

    // Action Buttons Row
    QHBoxLayout *actionsLayout = new QHBoxLayout();
    actionsLayout->setSpacing(6);

    m_btnLaunch = new QPushButton("▶ Запустить", rightContainer);
    m_btnLaunch->setStyleSheet("QPushButton { background-color: #065F46; color: #34D399; border: 1px solid #059669; } QPushButton:hover { background-color: #059669; color: #FFFFFF; }");
    connect(m_btnLaunch, &QPushButton::clicked, this, &ApkManagerWidget::launchApp);

    m_btnStop = new QPushButton("⏹ Остановить", rightContainer);
    m_btnStop->setStyleSheet("QPushButton { background-color: #78350F; color: #FBBF24; border: 1px solid #D97706; } QPushButton:hover { background-color: #D97706; color: #FFFFFF; }");
    connect(m_btnStop, &QPushButton::clicked, this, &ApkManagerWidget::forceStopApp);

    m_btnToggleFreeze = new QPushButton("❄ Отключить", rightContainer);
    connect(m_btnToggleFreeze, &QPushButton::clicked, this, &ApkManagerWidget::toggleFreezeApp);

    m_btnClearData = new QPushButton("🧹 Сброс данных", rightContainer);
    connect(m_btnClearData, &QPushButton::clicked, this, &ApkManagerWidget::clearAppData);

    m_btnExportApk = new QPushButton("📤 Скачать APK", rightContainer);
    connect(m_btnExportApk, &QPushButton::clicked, this, &ApkManagerWidget::exportSelectedApk);

    m_btnUninstall = new QPushButton("🗑 Удалить", rightContainer);
    m_btnUninstall->setStyleSheet("QPushButton { background-color: rgba(239, 68, 68, 0.15); color: #F87171; border: 1px solid rgba(239, 68, 68, 0.4); } QPushButton:hover { background-color: rgba(239, 68, 68, 0.3); color: #FFA3A3; border-color: #EF4444; }");
    connect(m_btnUninstall, &QPushButton::clicked, this, &ApkManagerWidget::uninstallApp);

    actionsLayout->addWidget(m_btnLaunch);
    actionsLayout->addWidget(m_btnStop);
    actionsLayout->addWidget(m_btnToggleFreeze);
    actionsLayout->addWidget(m_btnClearData);
    actionsLayout->addWidget(m_btnExportApk);
    actionsLayout->addWidget(m_btnUninstall);
    rightLayout->addLayout(actionsLayout);

    // Tabbed Details Inspector
    m_tabs = new QTabWidget(rightContainer);

    // Tab 1: Overview
    QWidget *tabOverview = new QWidget();
    QVBoxLayout *overLayout = new QVBoxLayout(tabOverview);
    overLayout->setContentsMargins(8, 8, 8, 8);
    m_overviewTable = new QTableWidget(tabOverview);
    m_overviewTable->setColumnCount(2);
    m_overviewTable->setHorizontalHeaderLabels(QStringList() << "Параметр" << "Значение");
    m_overviewTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_overviewTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_overviewTable->verticalHeader()->setVisible(false);
    m_overviewTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_overviewTable->setShowGrid(true);
    overLayout->addWidget(m_overviewTable);
    m_tabs->addTab(tabOverview, "📊 Обзор");

    // Tab 2: Permissions
    QWidget *tabPerms = new QWidget();
    QVBoxLayout *permLayout = new QVBoxLayout(tabPerms);
    permLayout->setContentsMargins(8, 8, 8, 8);
    permLayout->setSpacing(6);

    QHBoxLayout *permTop = new QHBoxLayout();
    m_permSearchEdit = new QLineEdit(tabPerms);
    m_permSearchEdit->setPlaceholderText("🔍 Поиск разрешений...");
    connect(m_permSearchEdit, &QLineEdit::textChanged, this, &ApkManagerWidget::filterPermissions);

    m_permStatLabel = new QLabel("Всего: 0", tabPerms);
    permTop->addWidget(m_permSearchEdit, 1);
    permTop->addWidget(m_permStatLabel);
    permLayout->addLayout(permTop);

    m_permTable = new QTableWidget(tabPerms);
    m_permTable->setColumnCount(3);
    m_permTable->setHorizontalHeaderLabels(QStringList() << "Категория" << "Разрешение" << "Статус");
    m_permTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_permTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_permTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_permTable->verticalHeader()->setVisible(false);
    m_permTable->setShowGrid(false);
    m_permTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    permLayout->addWidget(m_permTable);
    m_tabs->addTab(tabPerms, "🛡 Разрешения");

    // Tab 3: Components
    QWidget *tabComps = new QWidget();
    QVBoxLayout *compLayout = new QVBoxLayout(tabComps);
    compLayout->setContentsMargins(8, 8, 8, 8);
    m_compTable = new QTableWidget(tabComps);
    m_compTable->setColumnCount(2);
    m_compTable->setHorizontalHeaderLabels(QStringList() << "Тип компонента" << "Имя класса / точки входа");
    m_compTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_compTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_compTable->verticalHeader()->setVisible(false);
    m_compTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    compLayout->addWidget(m_compTable);
    m_tabs->addTab(tabComps, "🧩 Компоненты");

    // Tab 4: Raw Dumpsys
    QWidget *tabDump = new QWidget();
    QVBoxLayout *dumpLayout = new QVBoxLayout(tabDump);
    dumpLayout->setContentsMargins(8, 8, 8, 8);
    dumpLayout->setSpacing(6);

    QHBoxLayout *dumpTop = new QHBoxLayout();
    QLabel *dumpTitle = new QLabel("Сырые данные команды dumpsys package:", tabDump);
    QPushButton *btnCopyDump = new QPushButton("📋 Копировать весь дамп", tabDump);
    connect(btnCopyDump, &QPushButton::clicked, this, &ApkManagerWidget::copyRawDumpsys);
    dumpTop->addWidget(dumpTitle);
    dumpTop->addStretch(1);
    dumpTop->addWidget(btnCopyDump);
    dumpLayout->addLayout(dumpTop);

    m_dumpsysEdit = new QTextEdit(tabDump);
    m_dumpsysEdit->setReadOnly(true);
    dumpLayout->addWidget(m_dumpsysEdit);
    m_tabs->addTab(tabDump, "📜 Dumpsys");

    rightLayout->addWidget(m_tabs, 1);
    splitter->addWidget(rightContainer);

    splitter->setStretchFactor(0, 5);
    splitter->setStretchFactor(1, 6);
    mainLayout->addWidget(splitter, 1);

    // Bottom Status Bar
    QHBoxLayout *bottomBar = new QHBoxLayout();
    bottomBar->setContentsMargins(2, 0, 2, 0);

    m_statCountLabel = new QLabel("Всего: 0", this);
    m_statCountLabel->setStyleSheet("color: #94A3B8; font-size: 11px; font-weight: 500;");

    m_statusLabel = new QLabel("Готово", this);
    m_statusLabel->setStyleSheet("color: #38BDF8; font-size: 11px; font-weight: 500;");

    bottomBar->addWidget(m_statCountLabel);
    bottomBar->addStretch(1);
    bottomBar->addWidget(m_statusLabel);
    mainLayout->addLayout(bottomBar);

    updateButtonsEnabled(false);
}

void ApkManagerWidget::updateButtonsEnabled(bool enabled)
{
    m_btnLaunch->setEnabled(enabled);
    m_btnStop->setEnabled(enabled);
    m_btnToggleFreeze->setEnabled(enabled);
    m_btnClearData->setEnabled(enabled);
    m_btnExportApk->setEnabled(enabled);
    m_btnUninstall->setEnabled(enabled);
}

void ApkManagerWidget::loadPackages()
{
    if(m_device.devId.isEmpty() && MainWindow::current && !MainWindow::current->currentAdbDevice().isEmpty())
        setDevice(MainWindow::current->currentAdbDevice());

    if(!m_shell.isConnect() && !m_device.devId.isEmpty())
        m_shell.connect(m_device.devId);

    if(!m_shell.isConnect())
    {
        m_statusLabel->setText("⚠️ Нет подключения к устройству");
        return;
    }

    m_statusLabel->setText("Загрузка списка пакетов устройства...");
    qApp->processEvents();

    auto replyPkgs = m_shell.commandQueueWait(QStringList() << "pm" << "list" << "packages" << "-f" << "-u");
    auto replyUser = m_shell.commandQueueWait(QStringList() << "pm" << "list" << "packages" << "-3");
    auto replyDis = m_shell.commandQueueWait(QStringList() << "pm" << "list" << "packages" << "-d");

    if(!replyPkgs.first)
    {
        m_statusLabel->setText("[X] Ошибка при получении списка пакетов");
        return;
    }

    QSet<QString> userPkgs;
    if(replyUser.first)
    {
        QStringList uLines = replyUser.second.split('\n', Qt::SkipEmptyParts);
        for(QString &ul : uLines)
        {
            ul = ul.trimmed();
            if(ul.startsWith("package:"))
                ul.remove(0, 8);
            userPkgs.insert(ul.trimmed());
        }
    }

    QSet<QString> disabledPkgs;
    if(replyDis.first)
    {
        QStringList dLines = replyDis.second.split('\n', Qt::SkipEmptyParts);
        for(QString &dl : dLines)
        {
            dl = dl.trimmed();
            if(dl.startsWith("package:"))
                dl.remove(0, 8);
            disabledPkgs.insert(dl.trimmed());
        }
    }

    m_allPackages.clear();
    QStringList lines = replyPkgs.second.split('\n', Qt::SkipEmptyParts);

    for(const QString &line : lines)
    {
        QString clean = line.trimmed();
        if(clean.startsWith("package:"))
            clean.remove(0, 8);

        int eqIdx = clean.lastIndexOf('=');
        if(eqIdx == -1)
            continue;

        QString apkPath = clean.left(eqIdx).trimmed();
        QString pkgName = clean.mid(eqIdx + 1).trimmed();
        if(pkgName.isEmpty())
            continue;

        AppPackageInfo info;
        info.packageName = pkgName;
        info.apkPath = apkPath;
        info.isSystem = !userPkgs.contains(pkgName);
        info.isDisabled = disabledPkgs.contains(pkgName);
        info.appName = friendlyAppName(pkgName);

        info.icon = generateFallbackIcon(info.appName, info.packageName, info.isSystem);

        m_allPackages.append(info);
    }

    std::sort(
        m_allPackages.begin(),
        m_allPackages.end(),
        [](const AppPackageInfo &a, const AppPackageInfo &b)
        {
            if(a.isSystem != b.isSystem)
                return !a.isSystem;
            return a.appName.localeAwareCompare(b.appName) < 0;
        });

    populateTable();

    int userCount = 0, sysCount = 0, disCount = 0;
    for(const auto &p : m_allPackages)
    {
        if(p.isSystem)
            sysCount++;
        else
            userCount++;
        if(p.isDisabled)
            disCount++;
    }

    m_statCountLabel->setText(QString("Всего: %1 | 👤 Пользовательских: %2 | ⚙ Системных: %3 | ❄ Отключенных: %4").arg(m_allPackages.size()).arg(userCount).arg(sysCount).arg(disCount));
    m_statusLabel->setText("Готово");
}

void ApkManagerWidget::populateTable()
{
    m_table->setRowCount(0);

    QString filterText = m_searchEdit->text().trimmed().toLower();
    QString filterType = m_filterCombo->currentData().toString();

    int row = 0;
    for(int i = 0; i < m_allPackages.size(); ++i)
    {
        const AppPackageInfo &info = m_allPackages[i];

        if(filterType == "user" && info.isSystem)
            continue;
        if(filterType == "system" && !info.isSystem)
            continue;
        if(filterType == "disabled" && !info.isDisabled)
            continue;

        if(!filterText.isEmpty())
        {
            if(!info.packageName.toLower().contains(filterText) && !info.appName.toLower().contains(filterText))
                continue;
        }

        m_table->insertRow(row);

        QTableWidgetItem *iconItem = new QTableWidgetItem();
        iconItem->setIcon(info.icon);
        iconItem->setData(Qt::UserRole, i);

        QTableWidgetItem *nameItem = new QTableWidgetItem(info.appName);
        nameItem->setData(Qt::UserRole, i);

        QTableWidgetItem *pkgItem = new QTableWidgetItem(info.packageName);
        pkgItem->setData(Qt::UserRole, i);

        QTableWidgetItem *typeItem = new QTableWidgetItem(info.isSystem ? "⚙ Системное" : "👤 Пользовательское");
        typeItem->setForeground(info.isSystem ? QColor("#94A3B8") : QColor("#34D399"));

        QTableWidgetItem *statusItem = new QTableWidgetItem(info.isDisabled ? "❄ Отключено" : "● Активно");
        statusItem->setForeground(info.isDisabled ? QColor("#EF4444") : QColor("#10B981"));

        m_table->setItem(row, 0, iconItem);
        m_table->setItem(row, 1, nameItem);
        m_table->setItem(row, 2, pkgItem);
        m_table->setItem(row, 3, typeItem);
        m_table->setItem(row, 4, statusItem);

        row++;
    }

    if(m_table->rowCount() > 0)
    {
        m_table->selectRow(0);
    }
    else
    {
        clearInspector();
    }
}

void ApkManagerWidget::onSearchOrFilterChanged()
{
    populateTable();
}

void ApkManagerWidget::clearInspector()
{
    m_appIconLabel->clear();
    m_appNameLabel->setText("Приложение не выбрано");
    m_packageIdLabel->setText("—");
    m_typeBadge->setText("—");
    m_statusBadge->setText("—");
    m_installerBadge->setText("—");
    m_overviewTable->setRowCount(0);
    m_permTable->setRowCount(0);
    m_compTable->setRowCount(0);
    m_dumpsysEdit->clear();
    updateButtonsEnabled(false);
}

void ApkManagerWidget::onPackageSelected()
{
    int curRow = m_table->currentRow();
    if(curRow < 0)
    {
        clearInspector();
        return;
    }

    QTableWidgetItem *item = m_table->item(curRow, 0);
    if(!item)
        return;

    int pkgIndex = item->data(Qt::UserRole).toInt();
    if(pkgIndex < 0 || pkgIndex >= m_allPackages.size())
        return;

    AppPackageInfo &pkgInfo = m_allPackages[pkgIndex];
    inspectPackage(pkgInfo, curRow);
}

void ApkManagerWidget::inspectPackage(AppPackageInfo &pkgInfo, int tableRow)
{
    m_statusLabel->setText("Получение подробных данных для " + pkgInfo.packageName + "...");
    qApp->processEvents();

    QIcon extracted = extractAppIcon(m_shell, pkgInfo.apkPath, pkgInfo.packageName, pkgInfo.appName, pkgInfo.isSystem, m_iconCache);
    pkgInfo.icon = extracted;

    QTableWidgetItem *iconItem = m_table->item(tableRow, 0);
    if(iconItem)
        iconItem->setIcon(extracted);

    m_appIconLabel->setPixmap(extracted.pixmap(64, 64));
    m_appNameLabel->setText(pkgInfo.appName);
    m_packageIdLabel->setText(pkgInfo.packageName);

    if(pkgInfo.isSystem)
    {
        m_typeBadge->setText("⚙ Системное");
        m_typeBadge->setStyleSheet("background-color: #1E293B; color: #94A3B8; border: 1px solid #334155; border-radius: 4px; padding: 2px 8px; font-size: 10.5px; font-weight: 600;");
    }
    else
    {
        m_typeBadge->setText("👤 Пользовательское");
        m_typeBadge->setStyleSheet("background-color: #064E3B; color: #34D399; border: 1px solid #059669; border-radius: 4px; padding: 2px 8px; font-size: 10.5px; font-weight: 600;");
    }

    if(pkgInfo.isDisabled)
    {
        m_statusBadge->setText("❄ Отключено");
        m_statusBadge->setStyleSheet("background-color: #450A0A; color: #F87171; border: 1px solid #DC2626; border-radius: 4px; padding: 2px 8px; font-size: 10.5px; font-weight: 600;");
        m_btnToggleFreeze->setText("🟢 Включить");
        m_btnToggleFreeze->setStyleSheet("QPushButton { background-color: #065F46; color: #34D399; border: 1px solid #059669; } QPushButton:hover { background-color: #059669; color: #FFFFFF; }");
    }
    else
    {
        m_statusBadge->setText("● Активно");
        m_statusBadge->setStyleSheet("background-color: #064E3B; color: #34D399; border: 1px solid #059669; border-radius: 4px; padding: 2px 8px; font-size: 10.5px; font-weight: 600;");
        m_btnToggleFreeze->setText("❄ Отключить");
        m_btnToggleFreeze->setStyleSheet("QPushButton { background-color: #0F172A; color: #E2E8F0; border: 1.5px solid #1E293B; border-radius: 6px; } QPushButton:hover { background-color: #131E35; border-color: #38BDF8; color: #38BDF8; }");
    }

    auto dumpsysReply = m_shell.commandQueueWait(QStringList() << "dumpsys" << "package" << pkgInfo.packageName);
    QString dumpsys = dumpsysReply.first ? dumpsysReply.second : "";
    m_currentDetails = parseDumpsys(dumpsys, pkgInfo.packageName);
    m_currentDetails.appName = pkgInfo.appName;
    m_currentDetails.icon = extracted;
    m_currentDetails.isSystem = pkgInfo.isSystem;
    m_currentDetails.isDisabled = pkgInfo.isDisabled;

    if(m_currentDetails.codePath.isEmpty() || m_currentDetails.codePath == "—")
        m_currentDetails.codePath = pkgInfo.apkPath;

    auto sizeReply = m_shell.commandQueueWait(QStringList() << "stat" << "-c" << "%s" << "\"" + pkgInfo.apkPath + "\"" << "2>/dev/null");
    if(sizeReply.first && !sizeReply.second.trimmed().isEmpty())
    {
        qint64 sz = sizeReply.second.trimmed().toLongLong();
        if(sz > 0)
            m_currentDetails.apkSize = sz;
    }

    QString instText = m_currentDetails.installer;
    if(instText == "com.android.vending")
        instText = "🏪 Google Play Store";
    else if(instText.contains("packageinstaller"))
        instText = "📦 Пакетный установщик";
    else if(instText.isEmpty() || instText == "—")
        instText = "📥 Прямая установка / ADB";

    m_installerBadge->setText(instText);

    fillOverviewTab(m_currentDetails);
    fillPermissionsTab(m_currentDetails);
    fillComponentsTab(m_currentDetails);
    m_dumpsysEdit->setPlainText(dumpsys);

    updateButtonsEnabled(true);
    m_statusLabel->setText("Готово");
}

void ApkManagerWidget::fillOverviewTab(const AppDetails &d)
{
    m_overviewTable->setRowCount(0);

    auto addRow = [this](const QString &param, const QString &val)
    {
        int r = m_overviewTable->rowCount();
        m_overviewTable->insertRow(r);

        QTableWidgetItem *pItem = new QTableWidgetItem(param);
        pItem->setForeground(QColor("#94A3B8"));
        pItem->setFont(QFont("Segoe UI", 9, QFont::Bold));

        QTableWidgetItem *vItem = new QTableWidgetItem(val);
        vItem->setForeground(QColor("#FFFFFF"));

        m_overviewTable->setItem(r, 0, pItem);
        m_overviewTable->setItem(r, 1, vItem);
    };

    addRow("Название приложения", d.appName);
    addRow("Идентификатор пакета (Package ID)", d.packageName);
    addRow("Версия (versionName)", d.versionName);
    addRow("Номер сборки (versionCode)", d.versionCode);
    addRow("Целевая версия ОС (targetSdk)", d.targetSdk + " (" + sdkToAndroidVersion(d.targetSdk.toInt()) + ")");
    addRow("Минимальная версия ОС (minSdk)", d.minSdk + " (" + sdkToAndroidVersion(d.minSdk.toInt()) + ")");
    addRow("Размер файла APK", d.apkSize > 0 ? formatBytes(d.apkSize) : "—");
    addRow("Путь к APK на устройстве", d.codePath);
    addRow("Каталог данных (dataDir)", d.dataDir);
    addRow("Архитектура CPU (primaryCpuAbi)", d.primaryCpuAbi);
    addRow("Источник / Установщик", d.installer);
    addRow("Дата первой установки", d.firstInstallTime);
    addRow("Дата последнего обновления", d.lastUpdateTime);
    addRow("Главная активность (Main Activity)", d.mainActivity);
    addRow("Цифровая подпись / Сертификат", d.signatures.left(120) + (d.signatures.size() > 120 ? "..." : ""));
}

void ApkManagerWidget::fillPermissionsTab(const AppDetails &d)
{
    m_permTable->setRowCount(0);
    int grantedCount = 0;

    for(const QString &perm : d.requestedPermissions)
    {
        int r = m_permTable->rowCount();
        m_permTable->insertRow(r);

        QString cat = "🔧 Системное";
        if(perm.contains("CAMERA"))
            cat = "📷 Камера";
        else if(perm.contains("LOCATION"))
            cat = "📍 Геолокация";
        else if(perm.contains("AUDIO") || perm.contains("RECORD"))
            cat = "🎙 Микрофон";
        else if(perm.contains("STORAGE") || perm.contains("MEDIA"))
            cat = "📁 Память и файлы";
        else if(perm.contains("CONTACTS"))
            cat = "👥 Контакты";
        else if(perm.contains("SMS") || perm.contains("PHONE") || perm.contains("CALL"))
            cat = "💬 Связь и SMS";
        else if(perm.contains("INTERNET") || perm.contains("NETWORK"))
            cat = "🌐 Интернет";
        else if(perm.contains("BLUETOOTH"))
            cat = "📶 Bluetooth";
        else if(perm.contains("NOTIFICATION"))
            cat = "🔔 Уведомления";

        bool granted = d.grantedPermissions.contains(perm);
        if(granted)
            grantedCount++;

        QTableWidgetItem *catItem = new QTableWidgetItem(cat);
        QTableWidgetItem *permItem = new QTableWidgetItem(perm);
        QTableWidgetItem *statusItem = new QTableWidgetItem(granted ? "🟢 Разрешено" : "🔴 Запрещено");
        statusItem->setForeground(granted ? QColor("#34D399") : QColor("#F87171"));

        m_permTable->setItem(r, 0, catItem);
        m_permTable->setItem(r, 1, permItem);
        m_permTable->setItem(r, 2, statusItem);
    }

    m_permStatLabel->setText(QString("Всего: %1 | 🟢 Разрешено: %2 | 🔴 Запрещено: %3").arg(d.requestedPermissions.size()).arg(grantedCount).arg(d.requestedPermissions.size() - grantedCount));
}

void ApkManagerWidget::filterPermissions(const QString &text)
{
    QString search = text.trimmed().toLower();
    for(int r = 0; r < m_permTable->rowCount(); ++r)
    {
        bool match = false;
        if(search.isEmpty())
        {
            match = true;
        }
        else
        {
            for(int c = 0; c < m_permTable->columnCount(); ++c)
            {
                QTableWidgetItem *it = m_permTable->item(r, c);
                if(it && it->text().toLower().contains(search))
                {
                    match = true;
                    break;
                }
            }
        }
        m_permTable->setRowHidden(r, !match);
    }
}

void ApkManagerWidget::fillComponentsTab(const AppDetails &d)
{
    m_compTable->setRowCount(0);

    auto addComp = [this](const QString &type, const QString &name)
    {
        int r = m_compTable->rowCount();
        m_compTable->insertRow(r);
        QTableWidgetItem *tItem = new QTableWidgetItem(type);
        tItem->setForeground(QColor("#38BDF8"));
        QTableWidgetItem *nItem = new QTableWidgetItem(name);
        nItem->setForeground(QColor("#FFFFFF"));
        m_compTable->setItem(r, 0, tItem);
        m_compTable->setItem(r, 1, nItem);
    };

    for(const QString &act : d.activities)
        addComp("🎬 Активность (Activity)", act);
    for(const QString &srv : d.services)
        addComp("⚙ Служба (Service)", srv);
    for(const QString &rec : d.receivers)
        addComp("📡 Приемник (Receiver)", rec);
    for(const QString &prv : d.providers)
        addComp("🗄 Провайдер (Provider)", prv);
}

void ApkManagerWidget::launchApp()
{
    if(m_currentDetails.packageName.isEmpty())
        return;

    m_statusLabel->setText("Запуск " + m_currentDetails.packageName + "...");
    qApp->processEvents();

    auto reply = m_shell.commandQueueWait(QStringList() << "monkey" << "-p" << m_currentDetails.packageName << "-c" << "android.intent.category.LAUNCHER" << "1");

    if(reply.first)
        m_statusLabel->setText("[+] Приложение запущено: " + m_currentDetails.packageName);
    else
        m_statusLabel->setText("⚠️ Не удалось запустить приложение");
}

void ApkManagerWidget::forceStopApp()
{
    if(m_currentDetails.packageName.isEmpty())
        return;

    m_statusLabel->setText("Остановка " + m_currentDetails.packageName + "...");
    qApp->processEvents();

    auto reply = m_shell.commandQueueWait(QStringList() << "am" << "force-stop" << m_currentDetails.packageName);
    if(reply.first)
        m_statusLabel->setText("⏹ Приложение остановлено: " + m_currentDetails.packageName);
    else
        m_statusLabel->setText("⚠️ Ошибка при остановке приложения");
}

void ApkManagerWidget::toggleFreezeApp()
{
    if(m_currentDetails.packageName.isEmpty())
        return;

    bool wasDisabled = m_currentDetails.isDisabled;
    QString cmd = wasDisabled ? "enable" : "disable-user";

    m_statusLabel->setText((wasDisabled ? "Включение " : "Отключение ") + m_currentDetails.packageName + "...");
    qApp->processEvents();

    auto reply = m_shell.commandQueueWait(QStringList() << "pm" << cmd << "--user" << "0" << m_currentDetails.packageName);

    if(reply.first)
    {
        m_currentDetails.isDisabled = !wasDisabled;

        int curRow = m_table->currentRow();
        if(curRow >= 0)
        {
            QTableWidgetItem *item = m_table->item(curRow, 0);
            if(item)
            {
                int idx = item->data(Qt::UserRole).toInt();
                if(idx >= 0 && idx < m_allPackages.size())
                    m_allPackages[idx].isDisabled = m_currentDetails.isDisabled;
            }

            QTableWidgetItem *statusItem = m_table->item(curRow, 4);
            if(statusItem)
            {
                statusItem->setText(m_currentDetails.isDisabled ? "❄ Отключено" : "● Активно");
                statusItem->setForeground(m_currentDetails.isDisabled ? QColor("#EF4444") : QColor("#10B981"));
            }
        }

        if(m_currentDetails.isDisabled)
        {
            m_statusBadge->setText("❄ Отключено");
            m_statusBadge->setStyleSheet("background-color: #450A0A; color: #F87171; border: 1px solid #DC2626; border-radius: 4px; padding: 2px 8px; font-size: 10.5px; font-weight: 600;");
            m_btnToggleFreeze->setText("🟢 Включить");
            m_btnToggleFreeze->setStyleSheet("QPushButton { background-color: #065F46; color: #34D399; border: 1px solid #059669; } QPushButton:hover { background-color: #059669; color: #FFFFFF; }");
        }
        else
        {
            m_statusBadge->setText("● Активно");
            m_statusBadge->setStyleSheet("background-color: #064E3B; color: #34D399; border: 1px solid #059669; border-radius: 4px; padding: 2px 8px; font-size: 10.5px; font-weight: 600;");
            m_btnToggleFreeze->setText("❄ Отключить");
            m_btnToggleFreeze->setStyleSheet("QPushButton { background-color: #0F172A; color: #E2E8F0; border: 1.5px solid #1E293B; border-radius: 6px; } QPushButton:hover { background-color: #131E35; border-color: #38BDF8; color: #38BDF8; }");
        }

        m_statusLabel->setText(wasDisabled ? "[+] Приложение включено" : "❄ Приложение заморожено / отключено");
    }
    else
    {
        m_statusLabel->setText("[x] Ошибка смены состояния приложения");
    }
}

void ApkManagerWidget::clearAppData()
{
    if(m_currentDetails.packageName.isEmpty())
        return;

    auto ans = QMessageBox::question(this, "Очистка данных", "Вы действительно хотите сбросить все данные и кэш приложения " + m_currentDetails.appName + " (" + m_currentDetails.packageName + ")?", QMessageBox::Yes | QMessageBox::No);

    if(ans != QMessageBox::Yes)
        return;

    m_statusLabel->setText("Очистка данных " + m_currentDetails.packageName + "...");
    qApp->processEvents();

    auto reply = m_shell.commandQueueWait(QStringList() << "pm" << "clear" << m_currentDetails.packageName);
    if(reply.first)
        m_statusLabel->setText("[+] Данные приложения успешно сброшены");
    else
        m_statusLabel->setText("[x] Не удалось очистить данные");
}

void ApkManagerWidget::exportSelectedApk()
{
    if(m_currentDetails.packageName.isEmpty() || m_currentDetails.codePath.isEmpty())
        return;

    QString defName = QDir::homePath() + "/" + m_currentDetails.packageName + ".apk";
    QString savePath = QFileDialog::getSaveFileName(this, "Сохранить APK файл", defName, "Android Package (*.apk)");
    if(savePath.isEmpty())
        return;

    m_statusLabel->setText("Выгрузка APK файла с устройства...");
    qApp->processEvents();

    int exitCode = 0;
    auto reply = adb_send_cmd(exitCode, QStringList() << "-s" << m_device.devId << "pull" << m_currentDetails.codePath << savePath);

    if(reply.first && exitCode == 0)
    {
        m_statusLabel->setText("[+] APK успешно сохранен: " + savePath);
        QMessageBox::information(this, "Успешно", "APK файл успешно выгружен на компьютер:\n" + savePath);
    }
    else
    {
        m_statusLabel->setText("[x] Ошибка при скачивании APK");
        QMessageBox::warning(this, "Ошибка", "Не удалось скачать APK файл с устройства.");
    }
}

void ApkManagerWidget::installApk()
{
    QString apkFile = QFileDialog::getOpenFileName(this, "Выберите APK файл для установки", QDir::homePath(), "Android Package (*.apk)");
    if(apkFile.isEmpty())
        return;

    m_statusLabel->setText("Установка APK на устройство...");
    qApp->processEvents();

    int exitCode = 0;
    auto reply = adb_send_cmd(exitCode, QStringList() << "-s" << m_device.devId << "install" << "-r" << apkFile);

    if(reply.first && exitCode == 0 && (reply.second.contains("Success") || !reply.second.contains("Failure")))
    {
        m_statusLabel->setText("[+] Приложение успешно установлено!");
        QMessageBox::information(this, "Установка завершена", "Приложение успешно установлено на устройство.");
        loadPackages();
    }
    else
    {
        m_statusLabel->setText("[x] Ошибка установки APK");
        QMessageBox::warning(this, "Ошибка установки", "Не удалось установить APK:\n" + reply.second);
    }
}

void ApkManagerWidget::uninstallApp()
{
    if(m_currentDetails.packageName.isEmpty())
        return;

    auto ans = QMessageBox::question(this, "Удаление приложения", "Вы действительно хотите удалить " + m_currentDetails.appName + "\n(" + m_currentDetails.packageName + ") с устройства?", QMessageBox::Yes | QMessageBox::No);

    if(ans != QMessageBox::Yes)
        return;

    m_statusLabel->setText("Удаление " + m_currentDetails.packageName + "...");
    qApp->processEvents();

    auto reply = m_shell.commandQueueWait(QStringList() << "pm" << "uninstall" << "--user" << "0" << m_currentDetails.packageName);

    if(reply.first && reply.second.contains("Success"))
    {
        m_statusLabel->setText("[+] Приложение удалено: " + m_currentDetails.packageName);
        QMessageBox::information(this, "Удалено", "Приложение успешно удалено.");
        loadPackages();
    }
    else
    {
        m_statusLabel->setText("[x] Не удалось удалить приложение");
        QMessageBox::warning(this, "Ошибка", "Не удалось удалить приложение:\n" + reply.second);
    }
}

void ApkManagerWidget::copyPackageId()
{
    if(!m_currentDetails.packageName.isEmpty())
    {
        QApplication::clipboard()->setText(m_currentDetails.packageName);
        m_statusLabel->setText("📋 Package ID скопирован в буфер обмена");
    }
}

void ApkManagerWidget::copyRawDumpsys()
{
    if(!m_dumpsysEdit->toPlainText().isEmpty())
    {
        QApplication::clipboard()->setText(m_dumpsysEdit->toPlainText());
        m_statusLabel->setText("📋 Дамп скопирован в буфер обмена");
    }
}

ApkManagerService::ApkManagerService(QObject *parent) : Service(DeviceConnectType::ADB, parent)
{
    title = "APK Менеджер";
}

ApkManagerService::~ApkManagerService()
{
    stop();
}

QString ApkManagerService::uuid() const
{
    return IDServiceAPKManagerString;
}

PageIndex ApkManagerService::targetPage()
{
    return ApkManagerPage;
}

QString ApkManagerService::widgetIconName()
{
    return "white-apk-manager";
}

bool ApkManagerService::canStart()
{
    return Service::canStart();
}

bool ApkManagerService::isStarted()
{
    return m_started;
}

bool ApkManagerService::isFinish()
{
    return m_finished;
}

bool ApkManagerService::start()
{
    if(!canStart())
        return false;

    m_started = true;
    m_finished = false;

    if(MainWindow::current)
    {
        auto *widget = static_cast<ApkManagerWidget *>(MainWindow::current->pageWidget(ApkManagerPage));
        if(widget)
        {
            widget->setDevice(mAdbDevice);
            widget->loadPackages();
        }
    }

    return true;
}

void ApkManagerService::stop()
{
    m_started = false;
    m_finished = true;
}
