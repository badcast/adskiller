#include "AppleIpswWidget.h"
#include "Services.h"
#include "mainwindow.h"

#include <QApplication>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QDesktopServices>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QScrollBar>
#include <QScrollArea>
#include <QRegularExpression>
#include <QClipboard>

// Helper to map Apple ProductType to human-friendly name
static QString getAppleMarketingName(const QString &prodType)
{
    return Apple::marketingNameForModel(prodType);
}

static QString formatFileSize(qint64 bytes)
{
    if(bytes <= 0)
        return QString("0 Б");
    double gb = bytes / (1024.0 * 1024.0 * 1024.0);
    if(gb >= 1.0)
    {
        return QString("%1 ГБ").arg(gb, 0, 'f', 2);
    }
    double mb = bytes / (1024.0 * 1024.0);
    return QString("%1 МБ").arg(mb, 0, 'f', 1);
}

static QScrollArea *createMetroScrollArea(QWidget *contentWidget, QWidget *parent = nullptr)
{
    QScrollArea *scroll = new QScrollArea(parent);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { background: transparent; border: none; } QScrollArea > QWidget > QWidget { background: transparent; }");
    scroll->setWidget(contentWidget);
    return scroll;
}

// -------------------------------------------------------------
// IpswChecksumWorker Implementation (Asynchronous Hashing)
// -------------------------------------------------------------
IpswChecksumWorker::IpswChecksumWorker(QObject *parent) : QThread(parent)
{
}

IpswChecksumWorker::~IpswChecksumWorker()
{
    cancel();
    wait(2000);
}

void IpswChecksumWorker::startVerification(const QString &filePath, const QString &expectedSha1, const QString &expectedMd5)
{
    cancel();
    wait();

    m_filePath = filePath;
    m_expectedSha1 = expectedSha1.trimmed().toLower();
    m_expectedMd5 = expectedMd5.trimmed().toLower();
    m_cancelRequested.store(false);

    start();
}

void IpswChecksumWorker::cancel()
{
    m_cancelRequested.store(true);
}

void IpswChecksumWorker::run()
{
    QFile file(m_filePath);
    if(!file.open(QIODevice::ReadOnly))
    {
        emit finished(false, QString(), m_expectedSha1, QString(), QString::fromUtf8("Не удалось открыть файл прошивки для чтения: %1").arg(file.errorString()));
        return;
    }

    qint64 totalSize = file.size();
    if(totalSize <= 0)
    {
        emit finished(false, QString(), m_expectedSha1, QString(), QString::fromUtf8("Файл прошивки пуст (размер 0 байт)."));
        return;
    }

    QCryptographicHash sha1Hash(QCryptographicHash::Sha1);
    QCryptographicHash md5Hash(QCryptographicHash::Md5);

    const qint64 bufferSize = 4 * 1024 * 1024; // 4 MB chunks for high disk throughput
    QByteArray buffer;
    buffer.resize(bufferSize);

    qint64 processed = 0;
    qint64 lastTime = QDateTime::currentMSecsSinceEpoch();
    qint64 lastBytes = 0;

    while(!file.atEnd() && !m_cancelRequested.load())
    {
        qint64 bytesRead = file.read(buffer.data(), bufferSize);
        if(bytesRead <= 0)
            break;

        sha1Hash.addData(buffer.constData(), bytesRead);
        md5Hash.addData(buffer.constData(), bytesRead);
        processed += bytesRead;

        qint64 now = QDateTime::currentMSecsSinceEpoch();
        if(now - lastTime >= 180 || processed == totalSize)
        {
            int pct = static_cast<int>((processed * 100) / totalSize);
            double speedMBs = 0.0;
            if(now > lastTime)
            {
                speedMBs = ((processed - lastBytes) / (1024.0 * 1024.0)) / ((now - lastTime) / 1000.0);
            }
            lastTime = now;
            lastBytes = processed;
            emit progress(pct, speedMBs, processed, totalSize);
        }
    }

    if(m_cancelRequested.load())
    {
        emit finished(false, QString(), m_expectedSha1, QString(), QString::fromUtf8("Проверка контрольной суммы отменена пользователем."));
        return;
    }

    QString calculatedSha1 = QString::fromLatin1(sha1Hash.result().toHex()).toLower();
    QString calculatedMd5 = QString::fromLatin1(md5Hash.result().toHex()).toLower();

    bool matched = false;
    if(!m_expectedSha1.isEmpty())
    {
        matched = (calculatedSha1 == m_expectedSha1);
    }
    else if(!m_expectedMd5.isEmpty())
    {
        matched = (calculatedMd5 == m_expectedMd5);
    }
    else
    {
        matched = true; // No reference hash to compare against, but calculated successfully
    }

    emit finished(matched, calculatedSha1, m_expectedSha1, calculatedMd5, QString());
}

// -------------------------------------------------------------
// IpswDownloadWorker Implementation
// -------------------------------------------------------------
IpswDownloadWorker::IpswDownloadWorker(QObject *parent) : QObject(parent)
{
    m_nam = new QNetworkAccessManager(this);
}

IpswDownloadWorker::~IpswDownloadWorker()
{
    cancelDownload();
}

void IpswDownloadWorker::startDownload(const QString &urlStr, const QString &destPath)
{
    cancelDownload();

    m_destPath = destPath;
    m_outputFile = new QFile(m_destPath, this);

    qint64 existingSize = 0;
    if(m_outputFile->exists())
    {
        existingSize = m_outputFile->size();
    }

    if(!m_outputFile->open(QIODevice::WriteOnly | QIODevice::Append))
    {
        emit downloadFinished(false, destPath, QString::fromUtf8("Не удалось открыть файл для записи: %1").arg(m_outputFile->errorString()));
        delete m_outputFile;
        m_outputFile = nullptr;
        return;
    }

    QUrl url(urlStr);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "AdsKiller-AppleFlasher/2.0");

    if(existingSize > 0)
    {
        QByteArray rangeHeader = "bytes=" + QByteArray::number(existingSize) + "-";
        request.setRawHeader("Range", rangeHeader);
    }

    m_lastBytes = existingSize;
    m_lastTime = QDateTime::currentMSecsSinceEpoch();

    m_reply = m_nam->get(request);
    connect(m_reply, &QNetworkReply::readyRead, this, &IpswDownloadWorker::onReadyRead);
    connect(m_reply, &QNetworkReply::downloadProgress, this, &IpswDownloadWorker::onDownloadProgress);
    connect(m_reply, &QNetworkReply::finished, this, &IpswDownloadWorker::onFinished);
}

void IpswDownloadWorker::cancelDownload()
{
    if(m_reply)
    {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    if(m_outputFile)
    {
        m_outputFile->flush();
        m_outputFile->close();
        delete m_outputFile;
        m_outputFile = nullptr;
    }
}

void IpswDownloadWorker::onReadyRead()
{
    if(m_reply && m_outputFile && m_outputFile->isOpen())
    {
        m_outputFile->write(m_reply->readAll());
    }
}

void IpswDownloadWorker::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 dt = now - m_lastTime;
    double speedMBs = 0.0;
    if(dt >= 400)
    {
        qint64 bytesDiff = bytesReceived - m_lastBytes;
        speedMBs = (bytesDiff / (1024.0 * 1024.0)) / (dt / 1000.0);
        m_lastBytes = bytesReceived;
        m_lastTime = now;
    }

    emit downloadProgress(bytesReceived, bytesTotal, speedMBs);
}

void IpswDownloadWorker::onFinished()
{
    if(!m_reply)
        return;

    bool ok = (m_reply->error() == QNetworkReply::NoError);
    QString err = m_reply->errorString();

    if(m_outputFile)
    {
        m_outputFile->flush();
        m_outputFile->close();
        delete m_outputFile;
        m_outputFile = nullptr;
    }

    m_reply->deleteLater();
    m_reply = nullptr;

    emit downloadFinished(ok, m_destPath, ok ? QString() : err);
}

// -------------------------------------------------------------
// AppleRestoreWorker Implementation
// -------------------------------------------------------------
AppleRestoreWorker::AppleRestoreWorker(QObject *parent) : QObject(parent)
{
}

AppleRestoreWorker::~AppleRestoreWorker()
{
    abortRestore();
}

void AppleRestoreWorker::startRestore(const QString &ipswPath, bool retainUserData, const QString &udid)
{
    abortRestore();
    m_aborted = false;

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_process, &QProcess::readyReadStandardOutput, this, &AppleRestoreWorker::onReadyReadStandardOutput);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &AppleRestoreWorker::onProcessFinished);

    QStringList args;
    if(retainUserData)
    {
        args << "-u"; // update mode: retains data
    }
    else
    {
        args << "-e"; // erase mode: clean flash
    }

    if(!udid.isEmpty())
    {
        args << "-u" << udid;
    }

    args << ipswPath;

    QString prog = AppleExecutableFilename("idevicerestore");

    emit progressChanged(2, QString::fromUtf8("Инициализация прошивки (%1)...").arg(retainUserData ? "Update (-u)" : "Clean Erase (-e)"));
    emit logReceived(QString("[COMMAND] %1 %2").arg(prog, args.join(" ")));

    m_process->start(prog, args);
}

void AppleRestoreWorker::abortRestore()
{
    if(m_process && m_process->state() != QProcess::NotRunning)
    {
        m_aborted = true;
        m_process->terminate();
        if(!m_process->waitForFinished(2000))
        {
            m_process->kill();
        }
        delete m_process;
        m_process = nullptr;
    }
}

void AppleRestoreWorker::onReadyReadStandardOutput()
{
    if(!m_process)
        return;
    QByteArray data = m_process->readAllStandardOutput();
    QString text = QString::fromUtf8(data);
    QStringList lines = text.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);

    for(const QString &line : lines)
    {
        parseLogLine(line.trimmed());
    }
}

void AppleRestoreWorker::onReadyReadStandardError()
{
    onReadyReadStandardOutput();
}

void AppleRestoreWorker::parseLogLine(const QString &line)
{
    if(line.isEmpty())
        return;

    emit logReceived(line);

    // Parse idevicerestore stages
    if(line.contains("Extracting", Qt::CaseInsensitive))
    {
        emit progressChanged(8, QString::fromUtf8("Распаковка IPSW архива..."));
    }
    else if(line.contains("TSS", Qt::CaseInsensitive) || line.contains("ticket", Qt::CaseInsensitive))
    {
        emit progressChanged(18, QString::fromUtf8("Запрос цифровой подписи (TSS) у серверов Apple..."));
    }
    else if(line.contains("iBSS", Qt::CaseInsensitive))
    {
        emit progressChanged(28, QString::fromUtf8("Отправка загрузчика iBSS..."));
    }
    else if(line.contains("iBEC", Qt::CaseInsensitive))
    {
        emit progressChanged(38, QString::fromUtf8("Отправка загрузчика iBEC..."));
    }
    else if(line.contains("Ramdisk", Qt::CaseInsensitive))
    {
        emit progressChanged(48, QString::fromUtf8("Загрузка Restore Ramdisk в оперативную память..."));
    }
    else if(line.contains("DeviceTree", Qt::CaseInsensitive) || line.contains("KernelCache", Qt::CaseInsensitive))
    {
        emit progressChanged(55, QString::fromUtf8("Отправка ядра KernelCache..."));
    }
    else if(line.contains("Restoring image", Qt::CaseInsensitive) || line.contains("Writing", Qt::CaseInsensitive))
    {
        static const QRegularExpression rx("(\\d+)%");
        auto match = rx.match(line);
        if(match.hasMatch())
        {
            int subPct = match.captured(1).toInt();
            int mapped = 60 + static_cast<int>(subPct * 0.32); // 60% -> 92%
            emit progressChanged(mapped, QString::fromUtf8("Запись файловой системы iOS (%1%)...").arg(subPct));
        }
        else
        {
            emit progressChanged(65, QString::fromUtf8("Запись файловой системы iOS..."));
        }
    }
    else if(line.contains("Baseband", Qt::CaseInsensitive) || line.contains("Modem", Qt::CaseInsensitive))
    {
        emit progressChanged(94, QString::fromUtf8("Прошивка модема устройства (Baseband)..."));
    }
    else if(line.contains("Restore complete", Qt::CaseInsensitive) || line.contains("SUCCESS", Qt::CaseInsensitive))
    {
        emit progressChanged(100, QString::fromUtf8("Прошивка успешно завершена! Перезагрузка устройства."));
    }
}

void AppleRestoreWorker::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    (void) exitStatus;
    bool ok = (exitCode == 0) && !m_aborted;
    QString msg;
    if(m_aborted)
    {
        msg = QString::fromUtf8("Операция была отменена пользователем.");
    }
    else if(!ok)
    {
        msg = QString::fromUtf8("Процесс прошивки завершился с кодом ошибки %1. Подробности в консоли терминала.").arg(exitCode);
    }
    emit restoreFinished(ok, msg);
}

// -------------------------------------------------------------
// AppleIpswWidget Implementation
// -------------------------------------------------------------
AppleIpswWidget::AppleIpswWidget(QWidget *parent) : QWidget(parent)
{
    m_netManager = new QNetworkAccessManager(this);
    m_downloader = new IpswDownloadWorker(this);
    m_restoreWorker = new AppleRestoreWorker(this);
    m_checksumWorker = new IpswChecksumWorker(this);

    connect(m_downloader, &IpswDownloadWorker::downloadProgress, this, &AppleIpswWidget::onDownloadProgress);
    connect(m_downloader, &IpswDownloadWorker::downloadFinished, this, &AppleIpswWidget::onDownloadFinished);
    connect(m_restoreWorker, &AppleRestoreWorker::progressChanged, this, &AppleIpswWidget::onRestoreProgress);
    connect(m_restoreWorker, &AppleRestoreWorker::logReceived, this, &AppleIpswWidget::onRestoreLog);
    connect(m_restoreWorker, &AppleRestoreWorker::restoreFinished, this, &AppleIpswWidget::onRestoreFinished);

    connect(m_checksumWorker, &IpswChecksumWorker::progress, this, &AppleIpswWidget::onChecksumProgress);
    connect(m_checksumWorker, &IpswChecksumWorker::finished, this, &AppleIpswWidget::onChecksumFinished);

    setupUi();
    loadDeviceList();
    scanLocalFirmwares();
    updateDiskSpaceInfo();

    m_detectTimer = new QTimer(this);
    connect(m_detectTimer, &QTimer::timeout, this, &AppleIpswWidget::scanDevices);
    m_detectTimer->start(4000); // Poll USB every 4 sec

    // Initial scan
    QTimer::singleShot(500, this, &AppleIpswWidget::scanDevices);
}

AppleIpswWidget::~AppleIpswWidget()
{
    if(m_detectTimer)
    {
        m_detectTimer->stop();
    }
    if(m_checksumWorker)
    {
        m_checksumWorker->cancel();
        m_checksumWorker->wait(1000);
    }
    if(m_downloader)
    {
        m_downloader->cancelDownload();
    }
    if(m_restoreWorker)
    {
        m_restoreWorker->abortRestore();
    }
}

QString AppleIpswWidget::downloadDirectory() const
{
    if(!m_customDownloadDir.isEmpty() && QDir(m_customDownloadDir).exists())
        return m_customDownloadDir;
    QString def = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + "/AdsKiller/IPSW";
    QDir().mkpath(def);
    return def;
}

void AppleIpswWidget::setupUi()
{
    // Modern Midnight Obsidian Styling (Flat, Sharp, Cyber/Metro Zero-Radius Palette)
    setStyleSheet(
        "AppleIpswWidget, QWidget#ipswMainContainer { background-color: #0A0E1A; color: #F8FAFC; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; font-size: 12px; }"
        "QSplitter::handle { background-color: #1E293B; width: 3px; }"
        "QTabWidget::pane { border: 1px solid #1E293B; background-color: #0F172A; border-radius: 0px; }"
        "QTabBar::tab { background-color: #0B101D; color: #94A3B8; padding: 8px 18px; border: 1px solid #1E293B; border-bottom: none; font-weight: 600; font-size: 12px; }"
        "QTabBar::tab:selected { background-color: #0F172A; color: #38BDF8; border-top: 2px solid #38BDF8; }"
        "QTabBar::tab:hover:!selected { background-color: #141C2E; color: #FFFFFF; }"
        "QComboBox { background-color: #070A12; border: 1px solid #1E293B; color: #F8FAFC; padding: 6px 28px 6px 12px; font-size: 12px; font-weight: 600; border-radius: 0px; min-height: 22px; }"
        "QComboBox:hover { border-color: #38BDF8; background-color: #0B101D; }"
        "QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: top right; border: none; width: 26px; background: #141C2E; }"
        "QComboBox::down-arrow { image: url(:/svg/arrow-down); width: 12px; height: 12px; }"
        "QComboBox::down-arrow:hover { image: url(:/svg/arrow-down-hover); }"
        "QComboBox QAbstractItemView { background-color: #0F172A; border: 1px solid #1E293B; color: #F8FAFC; selection-background-color: #0284C7; selection-color: #FFFFFF; outline: none; }"
        "QLineEdit { background-color: #070A12; border: 1px solid #1E293B; color: #F8FAFC; padding: 6px 10px; font-size: 12px; border-radius: 0px; }"
        "QLineEdit:hover { border-color: #334155; }"
        "QLineEdit:focus { border: 1px solid #38BDF8; background-color: #0B101D; }"
        "QTableWidget { background-color: #0B101D; border: 1px solid #1E293B; gridline-color: #1E293B; selection-background-color: #0284C7; selection-color: #FFFFFF; border-radius: 0px; }"
        "QHeaderView::section { background-color: #0F172A; color: #94A3B8; padding: 7px 10px; border: 1px solid #1E293B; font-weight: bold; border-radius: 0px; }"
        "QPushButton { background-color: #0F172A; border: 1px solid #1E293B; color: #F8FAFC; padding: 7px 16px; border-radius: 0px; font-weight: 600; }"
        "QPushButton:hover { background-color: #1E293B; border-color: #38BDF8; color: #FFFFFF; }"
        "QPushButton:pressed { background-color: #0284C7; }"
        "QPushButton:disabled { background-color: #070A12; color: #475569; border-color: #1E293B; }"
        "QRadioButton { spacing: 8px; color: #F8FAFC; font-weight: 500; font-size: 12px; }"
        "QRadioButton::indicator { width: 15px; height: 15px; border: 2px solid #475569; background: transparent; border-radius: 7px; }"
        "QRadioButton::indicator:checked { background-color: #38BDF8; border-color: #38BDF8; }"
        "QCheckBox { spacing: 8px; color: #F8FAFC; font-weight: 500; }"
        "QCheckBox::indicator { width: 15px; height: 15px; border: 1px solid #334155; background: #070A12; border-radius: 0px; }"
        "QCheckBox::indicator:checked { background-color: #0284C7; border-color: #38BDF8; image: url(:/resources/checkbox-checked); }"
        "QProgressBar { background-color: #070A12; border: 1px solid #1E293B; text-align: center; color: #F8FAFC; font-weight: bold; border-radius: 0px; height: 22px; }"
        "QProgressBar::chunk { background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0284C7, stop:1 #38BDF8); }"
        "QTextEdit { background-color: #070A12; border: 1px solid #1E293B; color: #E2E8F0; font-family: 'Consolas', 'Courier New', monospace; font-size: 11.5px; border-radius: 0px; }"
        "QScrollBar:vertical { background-color: #070A12; width: 8px; margin: 0px; border: none; }"
        "QScrollBar::handle:vertical { background-color: #1E293B; min-height: 24px; border-radius: 0px; }"
        "QScrollBar::handle:vertical:hover { background-color: #38BDF8; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; background: none; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(14, 10, 14, 14);
    mainLayout->setSpacing(10);

    // =========================================================
    // Top Obsidian Header Bar with Studio Branding & Mode Switcher
    // =========================================================
    QWidget *headerWidget = new QWidget(this);
    headerWidget->setStyleSheet("background-color: #0F172A; border: 1px solid #1E293B; border-radius: 0px;");
    QHBoxLayout *headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(14, 10, 14, 10);
    headerLayout->setSpacing(14);

    QLabel *lblAppleIcon = new QLabel(headerWidget);
    QIcon iconApple(":/svg/apple");
    if(iconApple.isNull())
        iconApple = QIcon(":/svg/services/apple");
    lblAppleIcon->setPixmap(iconApple.pixmap(30, 30));
    lblAppleIcon->setFixedSize(30, 30);
    headerLayout->addWidget(lblAppleIcon);

    QVBoxLayout *headerTitleLayout = new QVBoxLayout();
    headerTitleLayout->setSpacing(2);
    QLabel *headerTitle = new QLabel(QString::fromUtf8("<span style='font-size: 14.5px; font-weight: 700; color: #FFFFFF;'>Apple iOS Firmware & Recovery Studio</span>"), headerWidget);
    QLabel *headerSubtitle = new QLabel(QString::fromUtf8("<span style='font-size: 11px; color: #94A3B8;'>Пошаговый мастер восстановления • Контроль целостности SHA-1 • TSS Подписи Apple</span>"), headerWidget);
    headerTitleLayout->addWidget(headerTitle);
    headerTitleLayout->addWidget(headerSubtitle);
    headerLayout->addLayout(headerTitleLayout);

    headerLayout->addSpacing(16);

    // Segmented Mode Switcher (Pill Style)
    QWidget *modeBtnBox = new QWidget(headerWidget);
    modeBtnBox->setStyleSheet("background: #070A12; border: 1px solid #1E293B; border-radius: 0px; padding: 2px;");
    QHBoxLayout *modeBtnLayout = new QHBoxLayout(modeBtnBox);
    modeBtnLayout->setContentsMargins(2, 2, 2, 2);
    modeBtnLayout->setSpacing(4);

    m_btnModeSimple = new QPushButton(QString::fromUtf8("⚡ МАСТЕР (Рекомендуется)"), modeBtnBox);
    m_btnModeSimple->setObjectName(QStringLiteral("ipswBtnModeSimple"));
    m_btnModeSimple->setCursor(Qt::PointingHandCursor);
    m_btnModeSimple->setStyleSheet("background-color: #0284C7; color: #FFFFFF; font-weight: bold; border: 1px solid #38BDF8; padding: 6px 14px; font-size: 11.5px;");
    connect(m_btnModeSimple, &QPushButton::clicked, this, [this]() { onSwitchMode(0); });
    modeBtnLayout->addWidget(m_btnModeSimple);

    m_btnModeAdvanced = new QPushButton(QString::fromUtf8("🛠 ЭКСПЕРТНЫЙ РЕЖИМ"), modeBtnBox);
    m_btnModeAdvanced->setObjectName(QStringLiteral("ipswBtnModeAdvanced"));
    m_btnModeAdvanced->setCursor(Qt::PointingHandCursor);
    m_btnModeAdvanced->setStyleSheet("background-color: transparent; color: #94A3B8; font-weight: 600; border: none; padding: 6px 14px; font-size: 11.5px;");
    connect(m_btnModeAdvanced, &QPushButton::clicked, this, [this]() { onSwitchMode(1); });
    modeBtnLayout->addWidget(m_btnModeAdvanced);

    headerLayout->addWidget(modeBtnBox);
    headerLayout->addStretch();

    // Top Header Device Pill
    m_lblHeaderDevicePill = new QLabel(headerWidget);
    m_lblHeaderDevicePill->setStyleSheet("background: rgba(148, 163, 184, 0.08); color: #94A3B8; border: 1px solid #1E293B; padding: 5px 12px; font-weight: 600; font-size: 11.5px;");
    m_lblHeaderDevicePill->setText(QString::fromUtf8("○ Устройство не подключено"));
    headerLayout->addWidget(m_lblHeaderDevicePill);

    m_btnRefreshDevice = new QPushButton(QString::fromUtf8("🔄 Опросить USB"), headerWidget);
    m_btnRefreshDevice->setObjectName(QStringLiteral("ipswBtnRefreshDevice"));
    m_btnRefreshDevice->setCursor(Qt::PointingHandCursor);
    m_btnRefreshDevice->setIcon(QIcon(":/svg/refresh-cw"));
    m_btnRefreshDevice->setIconSize(QSize(13, 13));
    m_btnRefreshDevice->setStyleSheet("background-color: #141C2E; border: 1px solid #1E293B; color: #F8FAFC; padding: 6px 12px; font-weight: 600; font-size: 11.5px;");
    connect(m_btnRefreshDevice, &QPushButton::clicked, this, &AppleIpswWidget::scanDevices);
    headerLayout->addWidget(m_btnRefreshDevice);

    mainLayout->addWidget(headerWidget);

    // Main Mode Stack: Page 0 = Simple Wizard, Page 1 = Advanced Mode
    m_modeStack = new QStackedWidget(this);
    setupSimpleWizardPage();
    setupAdvancedWizardPage();
    mainLayout->addWidget(m_modeStack, 1);

    m_modeStack->setCurrentIndex(0);
}

void AppleIpswWidget::onSwitchMode(int mode)
{
    if(!m_modeStack)
        return;
    m_modeStack->setCurrentIndex(mode);

    if(mode == 0)
    {
        m_btnModeSimple->setStyleSheet("background-color: #0284C7; color: #FFFFFF; font-weight: bold; border: 1px solid #38BDF8; padding: 6px 14px; font-size: 11.5px;");
        m_btnModeAdvanced->setStyleSheet("background-color: transparent; color: #94A3B8; font-weight: 600; border: none; padding: 6px 14px; font-size: 11.5px;");
        updateSimpleWizardStep1();
    }
    else
    {
        m_btnModeAdvanced->setStyleSheet("background-color: #0284C7; color: #FFFFFF; font-weight: bold; border: 1px solid #38BDF8; padding: 6px 14px; font-size: 11.5px;");
        m_btnModeSimple->setStyleSheet("background-color: transparent; color: #94A3B8; font-weight: 600; border: none; padding: 6px 14px; font-size: 11.5px;");
        updateDeviceCard();
        updateDiskSpaceInfo();
    }
}

// -------------------------------------------------------------
// 1. SIMPLE MODE (Быстрый пошаговый мастер)
// -------------------------------------------------------------
void AppleIpswWidget::updateSimpleStepPills(int step)
{
    if(!m_lblSimplePill1 || !m_lblSimplePill2 || !m_lblSimplePill3)
        return;

    auto styleActive = "background-color: #0284C7; color: #FFFFFF; font-weight: bold; border: 1px solid #38BDF8; padding: 7px 12px; font-size: 11.5px;";
    auto styleCompleted = "background-color: #064E3B; color: #34D399; font-weight: bold; border: 1px solid #059669; padding: 7px 12px; font-size: 11.5px;";
    auto stylePending = "background-color: #0F172A; color: #64748B; font-weight: 600; border: 1px solid #1E293B; padding: 7px 12px; font-size: 11.5px;";

    if(step == 0)
    {
        m_lblSimplePill1->setStyleSheet(styleActive);
        m_lblSimplePill1->setText(QString::fromUtf8("1. ПОДКЛЮЧЕНИЕ IPHONE"));
        m_lblSimplePill2->setStyleSheet(stylePending);
        m_lblSimplePill2->setText(QString::fromUtf8("2. РЕЖИМ & ПРОШИВКА"));
        m_lblSimplePill3->setStyleSheet(stylePending);
        m_lblSimplePill3->setText(QString::fromUtf8("3. ВОССТАНОВЛЕНИЕ"));
    }
    else if(step == 1)
    {
        m_lblSimplePill1->setStyleSheet(styleCompleted);
        m_lblSimplePill1->setText(QString::fromUtf8("✔ 1. ПОДКЛЮЧЕНИЕ IPHONE"));
        m_lblSimplePill2->setStyleSheet(styleActive);
        m_lblSimplePill2->setText(QString::fromUtf8("2. РЕЖИМ & ПРОШИВКА"));
        m_lblSimplePill3->setStyleSheet(stylePending);
        m_lblSimplePill3->setText(QString::fromUtf8("3. ВОССТАНОВЛЕНИЕ"));
    }
    else
    {
        m_lblSimplePill1->setStyleSheet(styleCompleted);
        m_lblSimplePill1->setText(QString::fromUtf8("✔ 1. ПОДКЛЮЧЕНИЕ IPHONE"));
        m_lblSimplePill2->setStyleSheet(styleCompleted);
        m_lblSimplePill2->setText(QString::fromUtf8("✔ 2. РЕЖИМ & ПРОШИВКА"));
        m_lblSimplePill3->setStyleSheet(styleActive);
        m_lblSimplePill3->setText(QString::fromUtf8("3. ВОССТАНОВЛЕНИЕ"));
    }
}

void AppleIpswWidget::setupSimpleWizardPage()
{
    QWidget *simplePage = new QWidget(m_modeStack);
    QVBoxLayout *layout = new QVBoxLayout(simplePage);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    // Step Pills Navigation Bar
    QWidget *stepPillBar = new QWidget(simplePage);
    QHBoxLayout *spLayout = new QHBoxLayout(stepPillBar);
    spLayout->setContentsMargins(0, 0, 0, 0);
    spLayout->setSpacing(8);

    m_lblSimplePill1 = new QLabel(QString::fromUtf8("1. ПОДКЛЮЧЕНИЕ IPHONE"), stepPillBar);
    m_lblSimplePill2 = new QLabel(QString::fromUtf8("2. РЕЖИМ & ПРОШИВКА"), stepPillBar);
    m_lblSimplePill3 = new QLabel(QString::fromUtf8("3. ВОССТАНОВЛЕНИЕ"), stepPillBar);

    m_lblSimplePill1->setAlignment(Qt::AlignCenter);
    m_lblSimplePill2->setAlignment(Qt::AlignCenter);
    m_lblSimplePill3->setAlignment(Qt::AlignCenter);

    spLayout->addWidget(m_lblSimplePill1, 1);
    spLayout->addWidget(m_lblSimplePill2, 1);
    spLayout->addWidget(m_lblSimplePill3, 1);
    layout->addWidget(stepPillBar);

    m_simpleStepStack = new QStackedWidget(simplePage);

    // -------------------------------------------------------------
    // STEP 1: Connect iPhone
    // -------------------------------------------------------------
    QWidget *step1 = new QWidget();
    QVBoxLayout *s1Layout = new QVBoxLayout(step1);
    s1Layout->setContentsMargins(4, 4, 4, 12);
    s1Layout->setSpacing(12);

    QWidget *s1Card = new QWidget(step1);
    s1Card->setStyleSheet("background-color: #0F172A; border: 1px solid #1E293B; padding: 18px; border-radius: 0px;");
    QVBoxLayout *s1CardLayout = new QVBoxLayout(s1Card);
    s1CardLayout->setSpacing(14);

    QLabel *s1Title = new QLabel(QString::fromUtf8("<span style='font-size: 15px; font-weight: bold; color: #FFFFFF;'>📱 Шаг 1: Подключение устройства Apple по USB</span>"), s1Card);
    QLabel *s1Desc = new QLabel(QString::fromUtf8("Подключите iPhone, iPad или iPod оригинальным кабелем напрямую к порту компьютера. AdsKiller автоматически определит модель и подберет актуальную цифровую прошивку Apple."), s1Card);
    s1Desc->setStyleSheet("color: #94A3B8; font-size: 12px;");
    s1Desc->setWordWrap(true);

    s1CardLayout->addWidget(s1Title);
    s1CardLayout->addWidget(s1Desc);

    m_lblSimpleUsbStatus = new QLabel(s1Card);
    m_lblSimpleUsbStatus->setWordWrap(true);
    s1CardLayout->addWidget(m_lblSimpleUsbStatus);

    m_lblSimpleDetectedModel = new QLabel(s1Card);
    m_lblSimpleDetectedModel->setWordWrap(true);
    s1CardLayout->addWidget(m_lblSimpleDetectedModel);

    // Connected Device Card (visible when device is plugged in)
    m_simpleConnectedDeviceCard = new QWidget(s1Card);
    m_simpleConnectedDeviceCard->setStyleSheet("background-color: #070A12; border: 1px solid #0284C7; padding: 14px;");
    QVBoxLayout *scdLayout = new QVBoxLayout(m_simpleConnectedDeviceCard);
    scdLayout->setSpacing(8);

    QHBoxLayout *scdHead = new QHBoxLayout();
    m_lblSimpleCardDeviceTitle = new QLabel(m_simpleConnectedDeviceCard);
    m_lblSimpleCardDeviceTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #38BDF8;");
    m_lblSimpleCardDeviceMode = new QLabel(m_simpleConnectedDeviceCard);
    m_lblSimpleCardDeviceMode->setStyleSheet("background: rgba(16, 185, 129, 0.15); color: #34D399; font-weight: bold; padding: 3px 8px; border: 1px solid #059669; font-size: 11px;");
    scdHead->addWidget(m_lblSimpleCardDeviceTitle);
    scdHead->addStretch();
    scdHead->addWidget(m_lblSimpleCardDeviceMode);
    scdLayout->addLayout(scdHead);

    QGridLayout *scdGrid = new QGridLayout();
    scdGrid->setHorizontalSpacing(16);
    scdGrid->setVerticalSpacing(6);

    auto addScdProp = [this, scdGrid](int r, int c, const QString &label, QLabel *&valLbl)
    {
        QLabel *l = new QLabel(label, m_simpleConnectedDeviceCard);
        l->setStyleSheet("color: #64748B; font-weight: 600; font-size: 11.5px;");
        valLbl = new QLabel("—", m_simpleConnectedDeviceCard);
        valLbl->setStyleSheet("color: #F8FAFC; font-weight: 600; font-size: 11.5px;");
        valLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
        scdGrid->addWidget(l, r, c * 2);
        scdGrid->addWidget(valLbl, r, c * 2 + 1);
    };

    addScdProp(0, 0, QString::fromUtf8("Модель (ProductType):"), m_lblSimpleCardModel);
    addScdProp(0, 1, QString::fromUtf8("Установленная iOS:"), m_lblSimpleCardIos);
    addScdProp(1, 0, QString::fromUtf8("Серийный номер:"), m_lblSimpleCardSerial);
    addScdProp(1, 1, QString::fromUtf8("ECID чипа:"), m_lblSimpleCardEcid);

    scdLayout->addLayout(scdGrid);
    s1CardLayout->addWidget(m_simpleConnectedDeviceCard);
    m_simpleConnectedDeviceCard->setVisible(false);

    // Tips Box
    QWidget *hintBox = new QWidget(s1Card);
    hintBox->setStyleSheet("background-color: #070A12; border-left: 3px solid #38BDF8; padding: 12px;");
    QVBoxLayout *hintLayout = new QVBoxLayout(hintBox);
    hintLayout->setSpacing(6);

    QLabel *hintTitle = new QLabel(QString::fromUtf8("<b>💡 Подсказки для успешного подключения:</b>"), hintBox);
    hintTitle->setStyleSheet("color: #38BDF8; font-size: 12px;");
    hintLayout->addWidget(hintTitle);

    auto addHint = [hintLayout, hintBox](const QString &text)
    {
        QLabel *l = new QLabel(text, hintBox);
        l->setStyleSheet("color: #CBD5E1; font-size: 11.5px;");
        l->setWordWrap(true);
        hintLayout->addWidget(l);
    };

    addHint(QString::fromUtf8("• <b>Прямой порт USB:</b> Подключайте кабель напрямую к портам материнской платы ПК (избегайте USB-хабов)."));
    addHint(QString::fromUtf8("• <b>Доверие компьютеру:</b> Если экран iPhone активен, разблокируйте его и нажмите «Доверять этому компьютеру»."));
    addHint(QString::fromUtf8("• <b>Зависание на логотипе Apple / Bootloop:</b> Переведите устройство в Recovery Mode (зажмите кнопки громкости и питания до значка кабеля и ПК)."));
    addHint(QString::fromUtf8("• <b>Контроль кабеля:</b> Поврежденные неоригинальные кабели могут обрывать связь при записи разделов RootFS."));

    s1CardLayout->addWidget(hintBox);
    s1Layout->addWidget(s1Card);

    QHBoxLayout *s1Bar = new QHBoxLayout();
    s1Bar->addStretch();

    QPushButton *btnManualRefresh = new QPushButton(QString::fromUtf8("🔄 Проверить подключение USB"), step1);
    btnManualRefresh->setObjectName(QStringLiteral("ipswBtnManualRefreshUSB"));
    btnManualRefresh->setCursor(Qt::PointingHandCursor);
    btnManualRefresh->setIcon(QIcon(":/svg/refresh-cw"));
    btnManualRefresh->setStyleSheet("background-color: #141C2E; border: 1px solid #1E293B; color: #FFFFFF; padding: 8px 16px; font-weight: 600; font-size: 12px;");
    connect(btnManualRefresh, &QPushButton::clicked, this, &AppleIpswWidget::scanDevices);
    s1Bar->addWidget(btnManualRefresh);

    m_btnSimpleStep1Next = new QPushButton(QString::fromUtf8("Продолжить: Выбор режима и прошивка ➔"), step1);
    m_btnSimpleStep1Next->setObjectName(QStringLiteral("ipswBtnStep1Next"));
    m_btnSimpleStep1Next->setCursor(Qt::PointingHandCursor);
    m_btnSimpleStep1Next->setStyleSheet("QPushButton { background-color: #0284C7; border: 1px solid #38BDF8; color: #FFFFFF; padding: 8px 22px; font-weight: bold; font-size: 12.5px; } QPushButton:disabled { background-color: #0B101D; color: #475569; border-color: #1E293B; }");
    m_btnSimpleStep1Next->setEnabled(false);
    connect(m_btnSimpleStep1Next, &QPushButton::clicked, this, &AppleIpswWidget::onSimpleStep1Next);
    s1Bar->addWidget(m_btnSimpleStep1Next);

    s1Layout->addLayout(s1Bar);
    s1Layout->addStretch(1);

    QScrollArea *s1Scroll = createMetroScrollArea(step1, m_simpleStepStack);
    m_simpleStepStack->addWidget(s1Scroll);

    // -------------------------------------------------------------
    // STEP 2: Auto settings, Reset mode & Checksum Verification
    // -------------------------------------------------------------
    QWidget *step2 = new QWidget();
    QVBoxLayout *s2Layout = new QVBoxLayout(step2);
    s2Layout->setContentsMargins(4, 4, 4, 12);
    s2Layout->setSpacing(12);

    QWidget *s2Card = new QWidget(step2);
    s2Card->setStyleSheet("background-color: #0F172A; border: 1px solid #1E293B; padding: 18px; border-radius: 0px;");
    QVBoxLayout *s2CardLayout = new QVBoxLayout(s2Card);
    s2CardLayout->setSpacing(14);

    QLabel *s2Title = new QLabel(QString::fromUtf8("<span style='font-size: 15px; font-weight: bold; color: #FFFFFF;'>⚙ Шаг 2: Автонастройки, режим восстановления и проверка IPSW</span>"), s2Card);
    s2CardLayout->addWidget(s2Title);

    m_lblSimpleDeviceSummary = new QLabel(s2Card);
    m_lblSimpleDeviceSummary->setStyleSheet("background: #070A12; border: 1px solid #1E293B; padding: 10px 14px; font-size: 12.5px; font-weight: bold; color: #FFFFFF;");
    s2CardLayout->addWidget(m_lblSimpleDeviceSummary);

    // Mode Selection Interactive Cards
    QLabel *s2ChoiceTitle = new QLabel(QString::fromUtf8("<b>Выберите тип восстановления:</b>"), s2Card);
    s2ChoiceTitle->setStyleSheet("color: #FFFFFF; font-size: 12.5px;");
    s2CardLayout->addWidget(s2ChoiceTitle);

    QWidget *modeContainer = new QWidget(s2Card);
    modeContainer->setStyleSheet("background-color: #070A12; border: 1px solid #1E293B; padding: 12px;");
    QVBoxLayout *mContLayout = new QVBoxLayout(modeContainer);
    mContLayout->setSpacing(12);

    m_simpleResetGroup = new QButtonGroup(this);

    // Option 1: Safe Retain Data
    QWidget *opt1Card = new QWidget(modeContainer);
    opt1Card->setStyleSheet("background: #0F172A; border: 1px solid #1E293B; padding: 10px;");
    QVBoxLayout *opt1Layout = new QVBoxLayout(opt1Card);
    opt1Layout->setSpacing(4);

    QHBoxLayout *opt1H = new QHBoxLayout();
    m_rbSimpleRetain = new QRadioButton(QString::fromUtf8("<b>1. Обновление с сохранением данных</b>"), opt1Card);
    m_rbSimpleRetain->setChecked(true);
    m_simpleResetGroup->addButton(m_rbSimpleRetain, 0);
    QLabel *badgeRetain = new QLabel(QString::fromUtf8("РЕКОМЕНДУЕТСЯ"), opt1Card);
    badgeRetain->setStyleSheet("background: rgba(16,185,129,0.15); color: #34D399; font-weight: bold; padding: 2px 8px; border: 1px solid #059669; font-size: 10.5px;");
    opt1H->addWidget(m_rbSimpleRetain);
    opt1H->addStretch();
    opt1H->addWidget(badgeRetain);
    opt1Layout->addLayout(opt1H);

    QLabel *opt1Desc = new QLabel(QString::fromUtf8("Все ваши фото, приложения, контакты и сообщения сохранятся на устройстве. Будет обновлена только операционная система iOS."), opt1Card);
    opt1Desc->setStyleSheet("color: #94A3B8; font-size: 11.5px; margin-left: 24px;");
    opt1Desc->setWordWrap(true);
    opt1Layout->addWidget(opt1Desc);
    mContLayout->addWidget(opt1Card);

    // Option 2: Clean Erase
    QWidget *opt2Card = new QWidget(modeContainer);
    opt2Card->setStyleSheet("background: #0F172A; border: 1px solid #1E293B; padding: 10px;");
    QVBoxLayout *opt2Layout = new QVBoxLayout(opt2Card);
    opt2Layout->setSpacing(4);

    QHBoxLayout *opt2H = new QHBoxLayout();
    m_rbSimpleErase = new QRadioButton(QString::fromUtf8("<b>2. Полная очистка и восстановление (Сброс до завода)</b>"), opt2Card);
    m_simpleResetGroup->addButton(m_rbSimpleErase, 1);
    QLabel *badgeErase = new QLabel(QString::fromUtf8("УДАЛЕНИЕ ВСЕХ ДАННЫХ"), opt2Card);
    badgeErase->setStyleSheet("background: rgba(239,68,68,0.15); color: #F87171; font-weight: bold; padding: 2px 8px; border: 1px solid #DC2626; font-size: 10.5px;");
    opt2H->addWidget(m_rbSimpleErase);
    opt2H->addStretch();
    opt2H->addWidget(badgeErase);
    opt2Layout->addLayout(opt2H);

    QLabel *opt2Desc = new QLabel(QString::fromUtf8("ВНИМАНИЕ! Полная переразметка накопителя NAND. Все данные пользователя будут безвозвратно удалены. Рекомендуется при сбоях загрузки или забытом пароле экрана."), opt2Card);
    opt2Desc->setStyleSheet("color: #F87171; font-size: 11.5px; margin-left: 24px;");
    opt2Desc->setWordWrap(true);
    opt2Layout->addWidget(opt2Desc);
    mContLayout->addWidget(opt2Card);

    s2CardLayout->addWidget(modeContainer);

    // Firmware Info & Checksum Card
    QWidget *fwCard = new QWidget(s2Card);
    fwCard->setStyleSheet("background: #070A12; border: 1px solid #1E293B; padding: 12px;");
    QVBoxLayout *fwLayout = new QVBoxLayout(fwCard);
    fwLayout->setSpacing(8);

    QLabel *fwHead = new QLabel(QString::fromUtf8("<b>📦 Официальная прошивка Apple IPSW:</b>"), fwCard);
    fwHead->setStyleSheet("color: #38BDF8; font-size: 12px;");
    fwLayout->addWidget(fwHead);

    m_lblSimpleAutoIpswInfo = new QLabel(fwCard);
    m_lblSimpleAutoIpswInfo->setWordWrap(true);
    fwLayout->addWidget(m_lblSimpleAutoIpswInfo);

    // Checksum verification section
    m_simpleChecksumCard = new QWidget(fwCard);
    m_simpleChecksumCard->setStyleSheet("background: #0B101D; border: 1px solid #1E293B; padding: 8px;");
    QVBoxLayout *scLayout = new QVBoxLayout(m_simpleChecksumCard);
    scLayout->setSpacing(6);

    QHBoxLayout *scHead = new QHBoxLayout();
    QLabel *scTitle = new QLabel(QString::fromUtf8("<b>🔒 Контроль целостности (SHA-1 Checksum):</b>"), m_simpleChecksumCard);
    scTitle->setStyleSheet("color: #94A3B8; font-size: 11.5px;");
    scHead->addWidget(scTitle);
    scHead->addStretch();

    m_btnSimpleVerifyChecksum = new QPushButton(QString::fromUtf8("🔍 Сверить SHA-1"), m_simpleChecksumCard);
    m_btnSimpleVerifyChecksum->setCursor(Qt::PointingHandCursor);
    m_btnSimpleVerifyChecksum->setStyleSheet("background-color: #141C2E; border: 1px solid #38BDF8; color: #38BDF8; padding: 4px 10px; font-weight: 600; font-size: 11px;");
    connect(m_btnSimpleVerifyChecksum, &QPushButton::clicked, this, &AppleIpswWidget::onStartChecksumVerification);
    scHead->addWidget(m_btnSimpleVerifyChecksum);

    m_btnSimpleCancelChecksum = new QPushButton(QString::fromUtf8("⏹ Отмена"), m_simpleChecksumCard);
    m_btnSimpleCancelChecksum->setCursor(Qt::PointingHandCursor);
    m_btnSimpleCancelChecksum->setStyleSheet("background-color: #7F1D1D; border: 1px solid #EF4444; color: #FFFFFF; padding: 4px 10px; font-weight: 600; font-size: 11px;");
    m_btnSimpleCancelChecksum->setVisible(false);
    connect(m_btnSimpleCancelChecksum, &QPushButton::clicked, this, &AppleIpswWidget::onCancelChecksumVerification);
    scHead->addWidget(m_btnSimpleCancelChecksum);

    scLayout->addLayout(scHead);

    m_lblSimpleChecksumStatus = new QLabel(QString::fromUtf8("Контрольная сумма еще не проверялась."), m_simpleChecksumCard);
    m_lblSimpleChecksumStatus->setStyleSheet("color: #64748B; font-size: 11.5px;");
    m_lblSimpleChecksumStatus->setWordWrap(true);
    scLayout->addWidget(m_lblSimpleChecksumStatus);

    m_simpleChecksumProgress = new QProgressBar(m_simpleChecksumCard);
    m_simpleChecksumProgress->setFixedHeight(16);
    m_simpleChecksumProgress->setValue(0);
    m_simpleChecksumProgress->setVisible(false);
    scLayout->addWidget(m_simpleChecksumProgress);

    fwLayout->addWidget(m_simpleChecksumCard);
    s2CardLayout->addWidget(fwCard);

    QLabel *safetyLbl = new QLabel(QString::fromUtf8("⚠️ <b>Безопасность:</b> Убедитесь, что заряд аккумулятора iPhone не менее 50%, и не отключайте кабель во время прошивки."), s2Card);
    safetyLbl->setStyleSheet("background: rgba(245,158,11,0.1); border: 1px solid #D97706; color: #FBBF24; padding: 8px 12px; font-size: 11.5px;");
    safetyLbl->setWordWrap(true);
    s2CardLayout->addWidget(safetyLbl);

    s2Layout->addWidget(s2Card);

    QHBoxLayout *s2Bar = new QHBoxLayout();
    m_btnSimpleStep2Back = new QPushButton(QString::fromUtf8("⬅ Назад к устройству"), step2);
    m_btnSimpleStep2Back->setObjectName(QStringLiteral("ipswBtnStep2Back"));
    m_btnSimpleStep2Back->setCursor(Qt::PointingHandCursor);
    m_btnSimpleStep2Back->setStyleSheet("background-color: #141C2E; border: 1px solid #1E293B; color: #FFFFFF; padding: 8px 16px; font-weight: 600;");
    connect(m_btnSimpleStep2Back, &QPushButton::clicked, this, &AppleIpswWidget::onSimpleStep2Back);
    s2Bar->addWidget(m_btnSimpleStep2Back);

    s2Bar->addStretch();

    m_btnSimpleStep2Start = new QPushButton(QString::fromUtf8("⚡ Начать восстановление ➔"), step2);
    m_btnSimpleStep2Start->setObjectName(QStringLiteral("ipswBtnStep2Start"));
    m_btnSimpleStep2Start->setCursor(Qt::PointingHandCursor);
    m_btnSimpleStep2Start->setStyleSheet("QPushButton { background-color: #0284C7; border: 1px solid #38BDF8; color: #FFFFFF; padding: 8px 24px; font-weight: bold; font-size: 13px; } QPushButton:hover { background-color: #0369A1; }");
    connect(m_btnSimpleStep2Start, &QPushButton::clicked, this, &AppleIpswWidget::onSimpleStep2Start);
    s2Bar->addWidget(m_btnSimpleStep2Start);

    s2Layout->addLayout(s2Bar);
    s2Layout->addStretch(1);

    QScrollArea *s2Scroll = createMetroScrollArea(step2, m_simpleStepStack);
    m_simpleStepStack->addWidget(s2Scroll);

    // -------------------------------------------------------------
    // STEP 3: Progress & Execution
    // -------------------------------------------------------------
    QWidget *step3 = new QWidget();
    QVBoxLayout *s3Layout = new QVBoxLayout(step3);
    s3Layout->setContentsMargins(4, 4, 4, 12);
    s3Layout->setSpacing(12);

    QWidget *s3Card = new QWidget(step3);
    s3Card->setStyleSheet("background-color: #0F172A; border: 1px solid #1E293B; padding: 22px; border-radius: 0px;");
    QVBoxLayout *s3CardLayout = new QVBoxLayout(s3Card);
    s3CardLayout->setSpacing(16);

    QLabel *s3Title = new QLabel(QString::fromUtf8("<span style='font-size: 16px; font-weight: bold; color: #FFFFFF;'>⚡ Шаг 3: Процесс восстановления iPhone</span>"), s3Card);
    s3CardLayout->addWidget(s3Title);

    m_lblSimpleProgressStatus = new QLabel(QString::fromUtf8("Инициализация процесса восстановления..."), s3Card);
    m_lblSimpleProgressStatus->setObjectName(QStringLiteral("ipswLblSimpleProgressStatus"));
    m_lblSimpleProgressStatus->setStyleSheet("color: #38BDF8; font-size: 13px; font-weight: bold;");
    s3CardLayout->addWidget(m_lblSimpleProgressStatus);

    m_simpleProgressBar = new QProgressBar(s3Card);
    m_simpleProgressBar->setObjectName(QStringLiteral("ipswSimpleProgressBar"));
    m_simpleProgressBar->setMinimumHeight(26);
    m_simpleProgressBar->setValue(0);
    s3CardLayout->addWidget(m_simpleProgressBar);

    QWidget *stageBox = new QWidget(s3Card);
    stageBox->setStyleSheet("background: #070A12; border: 1px solid #1E293B; padding: 14px;");
    QVBoxLayout *stageLayout = new QVBoxLayout(stageBox);
    stageLayout->setSpacing(8);

    QLabel *stLbl = new QLabel(QString::fromUtf8("<b>Этапы выполнения операции:</b>"), stageBox);
    stLbl->setStyleSheet("color: #FFFFFF; font-size: 12px;");
    stageLayout->addWidget(stLbl);

    QLabel *stDesc = new QLabel(
        QString::fromUtf8(
            "1. Загрузка / проверка контрольной суммы SHA-1 официального IPSW<br>"
            "2. Запрос цифрового билета подписи TSS от серверов Apple (gs.apple.com)<br>"
            "3. Перевод iPhone в Recovery и отправка компонентов загрузчика (iBSS / iBEC)<br>"
            "4. Запись файловой системы RootFS на встроенный накопитель NAND<br>"
            "5. Прошивка модема Baseband и безопасная перезагрузка устройства"),
        stageBox);
    stDesc->setStyleSheet("color: #94A3B8; font-size: 11.5px; line-height: 1.5;");
    stageLayout->addWidget(stDesc);
    s3CardLayout->addWidget(stageBox);

    s3Layout->addWidget(s3Card);

    QHBoxLayout *s3Bar = new QHBoxLayout();

    m_btnSimpleCancel = new QPushButton(QString::fromUtf8("⏹ Отмена операции"), step3);
    m_btnSimpleCancel->setObjectName(QStringLiteral("ipswBtnSimpleCancel"));
    m_btnSimpleCancel->setCursor(Qt::PointingHandCursor);
    m_btnSimpleCancel->setStyleSheet("background-color: #7F1D1D; border: 1px solid #EF4444; color: #FFFFFF; padding: 8px 16px; font-weight: bold;");
    connect(m_btnSimpleCancel, &QPushButton::clicked, this, &AppleIpswWidget::onCancelFlashClicked);
    s3Bar->addWidget(m_btnSimpleCancel);

    s3Bar->addStretch();

    m_btnSimpleRetry = new QPushButton(QString::fromUtf8("🔄 Повторить попытку"), step3);
    m_btnSimpleRetry->setObjectName(QStringLiteral("ipswBtnSimpleRetry"));
    m_btnSimpleRetry->setCursor(Qt::PointingHandCursor);
    m_btnSimpleRetry->setStyleSheet("background-color: #064E3B; border: 1px solid #059669; color: #34D399; padding: 8px 24px; font-weight: bold; font-size: 12.5px;");
    m_btnSimpleRetry->setVisible(false);
    connect(m_btnSimpleRetry, &QPushButton::clicked, this, &AppleIpswWidget::onSimpleRetry);
    s3Bar->addWidget(m_btnSimpleRetry);

    s3Layout->addLayout(s3Bar);
    s3Layout->addStretch(1);

    QScrollArea *s3Scroll = createMetroScrollArea(step3, m_simpleStepStack);
    m_simpleStepStack->addWidget(s3Scroll);

    layout->addWidget(m_simpleStepStack, 1);
    m_modeStack->addWidget(simplePage);

    updateSimpleStepPills(0);
    updateSimpleWizardStep1();
}

// -------------------------------------------------------------
// 2. ADVANCED MODE (Расширенный пошаговый экспертный режим)
// -------------------------------------------------------------
void AppleIpswWidget::updateAdvStepPills(int step)
{
    if(!m_lblAdvPill1 || !m_lblAdvPill2 || !m_lblAdvPill3)
        return;

    auto styleActive = "background-color: #0284C7; color: #FFFFFF; font-weight: bold; border: 1px solid #38BDF8; padding: 7px 12px; font-size: 11.5px;";
    auto styleCompleted = "background-color: #064E3B; color: #34D399; font-weight: bold; border: 1px solid #059669; padding: 7px 12px; font-size: 11.5px;";
    auto stylePending = "background-color: #0F172A; color: #64748B; font-weight: 600; border: 1px solid #1E293B; padding: 7px 12px; font-size: 11.5px;";

    if(step == 0)
    {
        m_lblAdvPill1->setStyleSheet(styleActive);
        m_lblAdvPill1->setText(QString::fromUtf8("1. КАТАЛОГ & ХРАНИЛИЩЕ"));
        m_lblAdvPill2->setStyleSheet(stylePending);
        m_lblAdvPill2->setText(QString::fromUtf8("2. ТОНКАЯ НАСТРОЙКА"));
        m_lblAdvPill3->setStyleSheet(stylePending);
        m_lblAdvPill3->setText(QString::fromUtf8("3. ДИАГНОСТИКА & ТЕРМИНАЛ"));
    }
    else if(step == 1)
    {
        m_lblAdvPill1->setStyleSheet(styleCompleted);
        m_lblAdvPill1->setText(QString::fromUtf8("✔ 1. КАТАЛОГ & ХРАНИЛИЩЕ"));
        m_lblAdvPill2->setStyleSheet(styleActive);
        m_lblAdvPill2->setText(QString::fromUtf8("2. ТОНКАЯ НАСТРОЙКА"));
        m_lblAdvPill3->setStyleSheet(stylePending);
        m_lblAdvPill3->setText(QString::fromUtf8("3. ДИАГНОСТИКА & ТЕРМИНАЛ"));
    }
    else
    {
        m_lblAdvPill1->setStyleSheet(styleCompleted);
        m_lblAdvPill1->setText(QString::fromUtf8("✔ 1. КАТАЛОГ & ХРАНИЛИЩЕ"));
        m_lblAdvPill2->setStyleSheet(styleCompleted);
        m_lblAdvPill2->setText(QString::fromUtf8("✔ 2. ТОНКАЯ НАСТРОЙКА"));
        m_lblAdvPill3->setStyleSheet(styleActive);
        m_lblAdvPill3->setText(QString::fromUtf8("3. ДИАГНОСТИКА & ТЕРМИНАЛ"));
    }
}

void AppleIpswWidget::setupAdvancedWizardPage()
{
    QWidget *advPage = new QWidget(m_modeStack);
    QVBoxLayout *layout = new QVBoxLayout(advPage);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    // Advanced Step Pills Navigation Bar
    QWidget *stepPillBar = new QWidget(advPage);
    QHBoxLayout *spLayout = new QHBoxLayout(stepPillBar);
    spLayout->setContentsMargins(0, 0, 0, 0);
    spLayout->setSpacing(8);

    m_lblAdvPill1 = new QLabel(QString::fromUtf8("1. КАТАЛОГ & ХРАНИЛИЩЕ"), stepPillBar);
    m_lblAdvPill2 = new QLabel(QString::fromUtf8("2. ТОНКАЯ НАСТРОЙКА"), stepPillBar);
    m_lblAdvPill3 = new QLabel(QString::fromUtf8("3. ДИАГНОСТИКА & ТЕРМИНАЛ"), stepPillBar);

    m_lblAdvPill1->setAlignment(Qt::AlignCenter);
    m_lblAdvPill2->setAlignment(Qt::AlignCenter);
    m_lblAdvPill3->setAlignment(Qt::AlignCenter);

    spLayout->addWidget(m_lblAdvPill1, 1);
    spLayout->addWidget(m_lblAdvPill2, 1);
    spLayout->addWidget(m_lblAdvPill3, 1);
    layout->addWidget(stepPillBar);

    m_advancedStepStack = new QStackedWidget(advPage);

    // -------------------------------------------------------------
    // ADVANCED STEP 1: Catalog, Download Manager & Storage
    // -------------------------------------------------------------
    QWidget *advStep1 = new QWidget();
    QVBoxLayout *a1Layout = new QVBoxLayout(advStep1);
    a1Layout->setContentsMargins(4, 4, 4, 12);
    a1Layout->setSpacing(10);

    m_tabWidget = new QTabWidget(advStep1);
    m_tabWidget->setMinimumHeight(480);

    // Tab 1: Catalog
    QWidget *tabCatalog = new QWidget(m_tabWidget);
    QVBoxLayout *tcLayout = new QVBoxLayout(tabCatalog);
    tcLayout->setContentsMargins(12, 12, 12, 12);
    tcLayout->setSpacing(10);

    // Filters row
    QHBoxLayout *filterRow = new QHBoxLayout();
    filterRow->setSpacing(10);

    QLabel *lblModel = new QLabel(QString::fromUtf8("Устройство:"), tabCatalog);
    lblModel->setStyleSheet("font-weight: 700; color: #FFFFFF; font-size: 12px;");
    filterRow->addWidget(lblModel);

    m_comboDeviceModel = new QComboBox(tabCatalog);
    m_comboDeviceModel->setObjectName(QStringLiteral("ipswComboDeviceModel"));
    m_comboDeviceModel->setMinimumWidth(260);
    connect(m_comboDeviceModel, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AppleIpswWidget::onModelSelected);
    filterRow->addWidget(m_comboDeviceModel);

    m_editCatalogFilter = new QLineEdit(tabCatalog);
    m_editCatalogFilter->setPlaceholderText(QString::fromUtf8("🔍 Фильтр версии iOS или сборок..."));
    m_editCatalogFilter->setClearButtonEnabled(true);
    connect(m_editCatalogFilter, &QLineEdit::textChanged, this, &AppleIpswWidget::onFilterCatalogChanged);
    filterRow->addWidget(m_editCatalogFilter);

    m_chkOnlySigned = new QCheckBox(QString::fromUtf8("Только подписанные (TSS)"), tabCatalog);
    m_chkOnlySigned->setChecked(false);
    connect(m_chkOnlySigned, &QCheckBox::toggled, this, &AppleIpswWidget::onFilterCatalogChanged);
    filterRow->addWidget(m_chkOnlySigned);

    m_btnRefreshCatalog = new QPushButton(QString::fromUtf8("🔄 Обновить"), tabCatalog);
    m_btnRefreshCatalog->setObjectName(QStringLiteral("ipswBtnRefreshCatalog"));
    m_btnRefreshCatalog->setCursor(Qt::PointingHandCursor);
    connect(m_btnRefreshCatalog, &QPushButton::clicked, this, &AppleIpswWidget::onRefreshCatalogClicked);
    filterRow->addWidget(m_btnRefreshCatalog);

    filterRow->addStretch();

    m_btnSelectLocal = new QPushButton(QString::fromUtf8("📂 Импорт .ipsw..."), tabCatalog);
    m_btnSelectLocal->setObjectName(QStringLiteral("ipswBtnSelectLocal"));
    m_btnSelectLocal->setCursor(Qt::PointingHandCursor);
    m_btnSelectLocal->setIcon(QIcon(":/svg/folder"));
    m_btnSelectLocal->setStyleSheet("QPushButton { background-color: #141C2E; border: 1px solid #38BDF8; color: #38BDF8; padding: 6px 14px; font-weight: 600; } QPushButton:hover { background-color: #38BDF8; color: #000000; }");
    connect(m_btnSelectLocal, &QPushButton::clicked, this, &AppleIpswWidget::onSelectLocalIpswClicked);
    filterRow->addWidget(m_btnSelectLocal);

    tcLayout->addLayout(filterRow);

    // Selected firmware summary card with Checksum info
    QWidget *selFwBox = new QWidget(tabCatalog);
    selFwBox->setStyleSheet("background: #070A12; border: 1px solid #1E293B; padding: 10px;");
    QVBoxLayout *sfbLayout = new QVBoxLayout(selFwBox);
    sfbLayout->setSpacing(6);

    m_lblSelectedIpsw = new QLabel(QString::fromUtf8("Файл прошивки не выбран. Выберите версию из таблицы или укажите локальный файл."), selFwBox);
    m_lblSelectedIpsw->setObjectName(QStringLiteral("ipswLblSelectedIpsw"));
    m_lblSelectedIpsw->setStyleSheet("color: #38BDF8; font-size: 11.5px;");
    sfbLayout->addWidget(m_lblSelectedIpsw);

    // Advanced Checksum Card
    m_advChecksumCard = new QWidget(selFwBox);
    m_advChecksumCard->setStyleSheet("background: #0F172A; border: 1px solid #1E293B; padding: 6px 10px;");
    QHBoxLayout *accLayout = new QHBoxLayout(m_advChecksumCard);
    accLayout->setContentsMargins(4, 4, 4, 4);
    accLayout->setSpacing(10);

    m_lblAdvChecksumStatus = new QLabel(QString::fromUtf8("Контрольная сумма SHA-1: не проверялась."), m_advChecksumCard);
    m_lblAdvChecksumStatus->setStyleSheet("color: #94A3B8; font-size: 11px;");
    accLayout->addWidget(m_lblAdvChecksumStatus, 1);

    m_advChecksumProgress = new QProgressBar(m_advChecksumCard);
    m_advChecksumProgress->setFixedSize(140, 16);
    m_advChecksumProgress->setValue(0);
    m_advChecksumProgress->setVisible(false);
    accLayout->addWidget(m_advChecksumProgress);

    m_btnAdvVerifyChecksum = new QPushButton(QString::fromUtf8("🔍 Сверить SHA-1"), m_advChecksumCard);
    m_btnAdvVerifyChecksum->setCursor(Qt::PointingHandCursor);
    m_btnAdvVerifyChecksum->setStyleSheet("background-color: #141C2E; border: 1px solid #38BDF8; color: #38BDF8; padding: 4px 10px; font-weight: 600; font-size: 11px;");
    connect(m_btnAdvVerifyChecksum, &QPushButton::clicked, this, &AppleIpswWidget::onStartChecksumVerification);
    accLayout->addWidget(m_btnAdvVerifyChecksum);

    m_btnAdvCancelChecksum = new QPushButton(QString::fromUtf8("⏹ Отмена"), m_advChecksumCard);
    m_btnAdvCancelChecksum->setCursor(Qt::PointingHandCursor);
    m_btnAdvCancelChecksum->setStyleSheet("background-color: #7F1D1D; border: 1px solid #EF4444; color: #FFFFFF; padding: 4px 10px; font-weight: 600; font-size: 11px;");
    m_btnAdvCancelChecksum->setVisible(false);
    connect(m_btnAdvCancelChecksum, &QPushButton::clicked, this, &AppleIpswWidget::onCancelChecksumVerification);
    accLayout->addWidget(m_btnAdvCancelChecksum);

    sfbLayout->addWidget(m_advChecksumCard);
    tcLayout->addWidget(selFwBox);

    // Firmware Table
    m_firmwareTable = new QTableWidget(tabCatalog);
    m_firmwareTable->setObjectName(QStringLiteral("ipswFirmwareCatalogTable"));
    m_firmwareTable->setColumnCount(6);
    m_firmwareTable->setHorizontalHeaderLabels({QString::fromUtf8("Версия iOS"), QString::fromUtf8("Номер сборки"), QString::fromUtf8("Дата релиза"), QString::fromUtf8("Статус Apple (TSS)"), QString::fromUtf8("Размер"), QString::fromUtf8("Действие")});
    m_firmwareTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_firmwareTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_firmwareTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_firmwareTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_firmwareTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_firmwareTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_firmwareTable->verticalHeader()->setVisible(false);
    m_firmwareTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_firmwareTable->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_firmwareTable, &QTableWidget::cellDoubleClicked, this, &AppleIpswWidget::onFirmwareSelected);
    tcLayout->addWidget(m_firmwareTable);

    m_tabWidget->addTab(tabCatalog, QString::fromUtf8("🌐 Каталог прошивок онлайн"));

    // Tab 2: Storage Manager
    QWidget *tabStorage = new QWidget(m_tabWidget);
    QVBoxLayout *tsLayout = new QVBoxLayout(tabStorage);
    tsLayout->setContentsMargins(12, 12, 12, 12);
    tsLayout->setSpacing(10);

    QWidget *storageBar = new QWidget(tabStorage);
    storageBar->setStyleSheet("background-color: #070A12; border: 1px solid #1E293B; padding: 8px 12px;");
    QHBoxLayout *sbLayout = new QHBoxLayout(storageBar);
    sbLayout->setContentsMargins(0, 0, 0, 0);
    sbLayout->setSpacing(12);

    m_lblStoragePath = new QLabel(storageBar);
    m_lblStoragePath->setObjectName(QStringLiteral("ipswLblStoragePath"));
    m_lblDiskSpace = new QLabel(storageBar);
    m_lblDiskSpace->setObjectName(QStringLiteral("ipswLblDiskSpace"));
    m_lblDiskSpace->setStyleSheet("color: #38BDF8; font-weight: bold;");

    sbLayout->addWidget(m_lblStoragePath);
    sbLayout->addStretch();
    sbLayout->addWidget(m_lblDiskSpace);
    sbLayout->addSpacing(8);

    m_btnOpenFolder = new QPushButton(QString::fromUtf8("📂 Открыть папку"), storageBar);
    m_btnOpenFolder->setObjectName(QStringLiteral("ipswBtnOpenFolder"));
    m_btnOpenFolder->setCursor(Qt::PointingHandCursor);
    connect(m_btnOpenFolder, &QPushButton::clicked, this, &AppleIpswWidget::onOpenFolderClicked);
    sbLayout->addWidget(m_btnOpenFolder);

    m_btnChangeFolder = new QPushButton(QString::fromUtf8("Изменить..."), storageBar);
    m_btnChangeFolder->setObjectName(QStringLiteral("ipswBtnChangeFolder"));
    m_btnChangeFolder->setCursor(Qt::PointingHandCursor);
    connect(m_btnChangeFolder, &QPushButton::clicked, this, &AppleIpswWidget::onChangeFolderClicked);
    sbLayout->addWidget(m_btnChangeFolder);

    m_btnRescanLocal = new QPushButton(QString::fromUtf8("🔄 Обновить"), storageBar);
    m_btnRescanLocal->setObjectName(QStringLiteral("ipswBtnRescanLocal"));
    m_btnRescanLocal->setCursor(Qt::PointingHandCursor);
    connect(m_btnRescanLocal, &QPushButton::clicked, this, &AppleIpswWidget::scanLocalFirmwares);
    sbLayout->addWidget(m_btnRescanLocal);

    tsLayout->addWidget(storageBar);

    m_localFirmwareTable = new QTableWidget(tabStorage);
    m_localFirmwareTable->setObjectName(QStringLiteral("ipswLocalFirmwareTable"));
    m_localFirmwareTable->setColumnCount(6);
    m_localFirmwareTable->setHorizontalHeaderLabels({QString::fromUtf8("Имя файла"), QString::fromUtf8("Модель устройства"), QString::fromUtf8("Размер"), QString::fromUtf8("Дата изменения"), QString::fromUtf8("Контрольная сумма"), QString::fromUtf8("Действия")});
    m_localFirmwareTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_localFirmwareTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_localFirmwareTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_localFirmwareTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_localFirmwareTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_localFirmwareTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_localFirmwareTable->verticalHeader()->setVisible(false);
    m_localFirmwareTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_localFirmwareTable->setSelectionMode(QAbstractItemView::SingleSelection);
    tsLayout->addWidget(m_localFirmwareTable);

    m_tabWidget->addTab(tabStorage, QString::fromUtf8("💾 Менеджер локального хранилища IPSW"));

    a1Layout->addWidget(m_tabWidget);

    QHBoxLayout *a1Bar = new QHBoxLayout();
    a1Bar->addStretch();
    m_btnAdvStep1Next = new QPushButton(QString::fromUtf8("Далее: Режим сброса и настройки ➔"), advStep1);
    m_btnAdvStep1Next->setObjectName(QStringLiteral("ipswBtnAdvStep1Next"));
    m_btnAdvStep1Next->setCursor(Qt::PointingHandCursor);
    m_btnAdvStep1Next->setStyleSheet("QPushButton { background-color: #0284C7; border: 1px solid #38BDF8; color: #FFFFFF; padding: 8px 22px; font-weight: bold; font-size: 12.5px; } QPushButton:hover { background-color: #0369A1; }");
    connect(m_btnAdvStep1Next, &QPushButton::clicked, this, &AppleIpswWidget::onAdvStep1Next);
    a1Bar->addWidget(m_btnAdvStep1Next);

    a1Layout->addLayout(a1Bar);
    a1Layout->addStretch(1);

    QScrollArea *a1Scroll = createMetroScrollArea(advStep1, m_advancedStepStack);
    m_advancedStepStack->addWidget(a1Scroll);

    // -------------------------------------------------------------
    // ADVANCED STEP 2: Fine-Tuning Reset Mode & Parameters
    // -------------------------------------------------------------
    QWidget *advStep2 = new QWidget();
    QVBoxLayout *a2Layout = new QVBoxLayout(advStep2);
    a2Layout->setContentsMargins(4, 4, 4, 12);
    a2Layout->setSpacing(12);

    QWidget *a2Card = new QWidget(advStep2);
    a2Card->setStyleSheet("background-color: #0F172A; border: 1px solid #1E293B; padding: 18px; border-radius: 0px;");
    QVBoxLayout *a2CardLayout = new QVBoxLayout(a2Card);
    a2CardLayout->setSpacing(14);

    QLabel *a2Title = new QLabel(QString::fromUtf8("<span style='font-size: 15px; font-weight: bold; color: #FFFFFF;'>🛠 Этап 2: Тонкая настройка режима прошивки</span>"), a2Card);
    a2CardLayout->addWidget(a2Title);

    m_lblAdvStep2FirmwareSummary = new QLabel(a2Card);
    m_lblAdvStep2FirmwareSummary->setStyleSheet("background: #070A12; border: 1px solid #1E293B; padding: 10px 14px; color: #FFFFFF; font-size: 12px; font-weight: 600;");
    a2CardLayout->addWidget(m_lblAdvStep2FirmwareSummary);

    m_flashModeGroup = new QButtonGroup(this);

    QWidget *advModeBox = new QWidget(a2Card);
    advModeBox->setStyleSheet("background-color: #070A12; border: 1px solid #1E293B; padding: 14px;");
    QVBoxLayout *advModeBoxLayout = new QVBoxLayout(advModeBox);
    advModeBoxLayout->setSpacing(12);

    m_rbRetainData = new QRadioButton(QString::fromUtf8("<b>Обновление (сохранить пользовательские данные) [-u Update]</b>"), advModeBox);
    m_rbRetainData->setObjectName(QStringLiteral("ipswRbRetainData"));
    m_rbRetainData->setChecked(true);
    m_flashModeGroup->addButton(m_rbRetainData, 0);
    QLabel *retDesc = new QLabel(QString::fromUtf8("Флаг <code>-u</code>: Сохраняет разделы с личными данными (/private/var). Обновляет операционную систему, ядро KernelCache и модем Baseband."), advModeBox);
    retDesc->setStyleSheet("color: #94A3B8; font-size: 11.5px; margin-left: 24px;");
    advModeBoxLayout->addWidget(m_rbRetainData);
    advModeBoxLayout->addWidget(retDesc);

    m_rbCleanRestore = new QRadioButton(QString::fromUtf8("<b>Чистая прошивка со сбросом (стереть все данные) [-e Erase]</b>"), advModeBox);
    m_rbCleanRestore->setObjectName(QStringLiteral("ipswRbCleanRestore"));
    m_flashModeGroup->addButton(m_rbCleanRestore, 1);
    QLabel *eraseDesc = new QLabel(QString::fromUtf8("Флаг <code>-e</code>: Полная переразметка разделов флеш-памяти NAND. Рекомендуется при повреждении системной таблицы или забытом пароле экрана."), advModeBox);
    eraseDesc->setStyleSheet("color: #F87171; font-size: 11.5px; margin-left: 24px;");
    advModeBoxLayout->addWidget(m_rbCleanRestore);
    advModeBoxLayout->addWidget(eraseDesc);

    a2CardLayout->addWidget(advModeBox);

    QWidget *advNoteBox = new QWidget(a2Card);
    advNoteBox->setStyleSheet("background-color: #070A12; border-left: 3px solid #38BDF8; padding: 12px;");
    QVBoxLayout *anbLayout = new QVBoxLayout(advNoteBox);
    anbLayout->setSpacing(6);

    QLabel *anbTitle = new QLabel(QString::fromUtf8("<b>Технические примечания idevicerestore:</b>"), advNoteBox);
    anbTitle->setStyleSheet("color: #38BDF8; font-size: 12px;");
    anbLayout->addWidget(anbTitle);

    QLabel *anbContent = new QLabel(
        QString::fromUtf8(
            "• При прошивке отправляется запрос цифровой подписи (TSS ticket) на серверы Apple (gs.apple.com).<br>"
            "• Неподписанные версии iOS будут отклонены TSS, если не предоставлены сохраненные SHSH blobs.<br>"
            "• Утилита автоматически переведет устройство в нужный режим (Recovery / Restore Ramdisk)."),
        advNoteBox);
    anbContent->setStyleSheet("color: #CBD5E1; font-size: 11.5px; line-height: 1.5;");
    anbLayout->addWidget(anbContent);
    a2CardLayout->addWidget(advNoteBox);

    a2Layout->addWidget(a2Card);

    QHBoxLayout *a2Bar = new QHBoxLayout();
    m_btnAdvStep2Back = new QPushButton(QString::fromUtf8("⬅ Назад к каталогу"), advStep2);
    m_btnAdvStep2Back->setObjectName(QStringLiteral("ipswBtnAdvStep2Back"));
    m_btnAdvStep2Back->setCursor(Qt::PointingHandCursor);
    m_btnAdvStep2Back->setStyleSheet("background-color: #141C2E; border: 1px solid #1E293B; color: #FFFFFF; padding: 8px 16px; font-weight: 600;");
    connect(m_btnAdvStep2Back, &QPushButton::clicked, this, &AppleIpswWidget::onAdvStep2Back);
    a2Bar->addWidget(m_btnAdvStep2Back);

    a2Bar->addStretch();

    m_btnAdvStep2Next = new QPushButton(QString::fromUtf8("Далее: Управление устройством и запуск ➔"), advStep2);
    m_btnAdvStep2Next->setObjectName(QStringLiteral("ipswBtnAdvStep2Next"));
    m_btnAdvStep2Next->setCursor(Qt::PointingHandCursor);
    m_btnAdvStep2Next->setStyleSheet("QPushButton { background-color: #0284C7; border: 1px solid #38BDF8; color: #FFFFFF; padding: 8px 22px; font-weight: bold; font-size: 12.5px; } QPushButton:hover { background-color: #0369A1; }");
    connect(m_btnAdvStep2Next, &QPushButton::clicked, this, &AppleIpswWidget::onAdvStep2Next);
    a2Bar->addWidget(m_btnAdvStep2Next);

    a2Layout->addLayout(a2Bar);
    a2Layout->addStretch(1);

    QScrollArea *a2Scroll = createMetroScrollArea(advStep2, m_advancedStepStack);
    m_advancedStepStack->addWidget(a2Scroll);

    // -------------------------------------------------------------
    // ADVANCED STEP 3: Device check, Flashing & Cyber Terminal
    // -------------------------------------------------------------
    QWidget *advStep3 = new QWidget();
    QVBoxLayout *a3Layout = new QVBoxLayout(advStep3);
    a3Layout->setContentsMargins(4, 4, 4, 12);
    a3Layout->setSpacing(10);

    QSplitter *splitter = new QSplitter(Qt::Horizontal, advStep3);
    splitter->setHandleWidth(4);
    splitter->setMinimumHeight(520);

    // Left Panel: Hardware Control & Specs
    m_deviceCardWidget = new QWidget(splitter);
    m_deviceCardWidget->setStyleSheet("background-color: #0F172A; border: 1px solid #1E293B; border-radius: 0px;");
    QVBoxLayout *cardLayout = new QVBoxLayout(m_deviceCardWidget);
    cardLayout->setContentsMargins(16, 16, 16, 16);
    cardLayout->setSpacing(12);

    m_lblDeviceIcon = new QLabel(m_deviceCardWidget);
    m_lblDeviceIcon->setAlignment(Qt::AlignCenter);
    QIcon devIcon(":/svg/apple");
    if(devIcon.isNull())
        devIcon = QIcon(":/svg/services/apple");
    m_lblDeviceIcon->setPixmap(devIcon.pixmap(48, 48));
    cardLayout->addWidget(m_lblDeviceIcon);

    m_lblDeviceName = new QLabel(QString::fromUtf8("Устройство не обнаружено"), m_deviceCardWidget);
    m_lblDeviceName->setAlignment(Qt::AlignCenter);
    m_lblDeviceName->setStyleSheet("font-size: 14px; font-weight: bold; color: #FFFFFF;");
    cardLayout->addWidget(m_lblDeviceName);

    m_lblDeviceMode = new QLabel(QString::fromUtf8("● Ожидание подключения USB"), m_deviceCardWidget);
    m_lblDeviceMode->setAlignment(Qt::AlignCenter);
    m_lblDeviceMode->setStyleSheet("font-size: 11.5px; color: #EAB308; font-weight: 600; padding: 4px 10px; background: rgba(234, 179, 8, 0.1); border: 1px solid #854D0E;");
    cardLayout->addWidget(m_lblDeviceMode);

    cardLayout->addSpacing(4);

    auto addAttrRow = [this, cardLayout](const QString &title, QLabel *&valLbl)
    {
        QHBoxLayout *row = new QHBoxLayout();
        QLabel *tLbl = new QLabel(title, m_deviceCardWidget);
        tLbl->setStyleSheet("color: #64748B; font-size: 11.5px; font-weight: 600;");
        valLbl = new QLabel("—", m_deviceCardWidget);
        valLbl->setStyleSheet("color: #F8FAFC; font-weight: 600; font-size: 11.5px;");
        valLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
        row->addWidget(tLbl);
        row->addStretch();
        row->addWidget(valLbl);
        cardLayout->addLayout(row);
    };

    addAttrRow("iOS версия:", m_lblIosVersion);
    addAttrRow("Серийный номер:", m_lblSerial);
    addAttrRow("ECID чипа:", m_lblEcid);
    addAttrRow("UDID:", m_lblUdid);

    cardLayout->addSpacing(8);

    QLabel *actionsTitle = new QLabel(QString::fromUtf8("Управление устройством:"), m_deviceCardWidget);
    actionsTitle->setStyleSheet("color: #94A3B8; font-weight: bold; font-size: 11px;");
    cardLayout->addWidget(actionsTitle);

    m_btnExitRecovery = new QPushButton(QString::fromUtf8("🚪 Выйти из Recovery"), m_deviceCardWidget);
    m_btnExitRecovery->setObjectName(QStringLiteral("ipswBtnExitRecovery"));
    m_btnExitRecovery->setCursor(Qt::PointingHandCursor);
    connect(m_btnExitRecovery, &QPushButton::clicked, this, &AppleIpswWidget::onExitRecoveryClicked);
    cardLayout->addWidget(m_btnExitRecovery);

    m_btnEnterRecovery = new QPushButton(QString::fromUtf8("⚡ Войти в Recovery"), m_deviceCardWidget);
    m_btnEnterRecovery->setObjectName(QStringLiteral("ipswBtnEnterRecovery"));
    m_btnEnterRecovery->setCursor(Qt::PointingHandCursor);
    connect(m_btnEnterRecovery, &QPushButton::clicked, this, &AppleIpswWidget::onEnterRecoveryClicked);
    cardLayout->addWidget(m_btnEnterRecovery);

    m_btnReboot = new QPushButton(QString::fromUtf8("🔄 Перезагрузить устройство"), m_deviceCardWidget);
    m_btnReboot->setObjectName(QStringLiteral("ipswBtnReboot"));
    m_btnReboot->setCursor(Qt::PointingHandCursor);
    connect(m_btnReboot, &QPushButton::clicked, this, &AppleIpswWidget::onRebootClicked);
    cardLayout->addWidget(m_btnReboot);

    cardLayout->addStretch();
    splitter->addWidget(m_deviceCardWidget);

    // Right Panel: Flashing Controls & Live Cyber Terminal
    QWidget *rightWidget = new QWidget(splitter);
    QVBoxLayout *rLayout = new QVBoxLayout(rightWidget);
    rLayout->setContentsMargins(0, 0, 0, 0);
    rLayout->setSpacing(8);

    QWidget *execBox = new QWidget(rightWidget);
    execBox->setStyleSheet("background-color: #0F172A; border: 1px solid #1E293B; padding: 12px;");
    QVBoxLayout *ebLayout = new QVBoxLayout(execBox);
    ebLayout->setSpacing(8);

    QHBoxLayout *statusRow = new QHBoxLayout();
    m_lblProgressStatus = new QLabel(QString::fromUtf8("Готов к работе"), execBox);
    m_lblProgressStatus->setObjectName(QStringLiteral("ipswLblProgressStatus"));
    m_lblProgressStatus->setStyleSheet("color: #38BDF8; font-size: 12px; font-weight: 600;");
    statusRow->addWidget(m_lblProgressStatus);
    statusRow->addStretch();

    m_lblAdvStep3ChecksumBadge = new QLabel(execBox);
    m_lblAdvStep3ChecksumBadge->setStyleSheet("background: rgba(148, 163, 184, 0.08); color: #94A3B8; border: 1px solid #1E293B; padding: 2px 8px; font-size: 11px; font-weight: 600;");
    m_lblAdvStep3ChecksumBadge->setText(QString::fromUtf8("○ Хеш не проверен"));
    statusRow->addWidget(m_lblAdvStep3ChecksumBadge);

    ebLayout->addLayout(statusRow);

    m_progressBar = new QProgressBar(execBox);
    m_progressBar->setObjectName(QStringLiteral("ipswAdvProgressBar"));
    m_progressBar->setFixedHeight(22);
    m_progressBar->setValue(0);
    ebLayout->addWidget(m_progressBar);

    QHBoxLayout *ctrlBtns = new QHBoxLayout();

    m_btnAdvStep3Back = new QPushButton(QString::fromUtf8("⬅ Назад"), execBox);
    m_btnAdvStep3Back->setObjectName(QStringLiteral("ipswBtnAdvStep3Back"));
    m_btnAdvStep3Back->setCursor(Qt::PointingHandCursor);
    m_btnAdvStep3Back->setStyleSheet("background-color: #141C2E; border: 1px solid #1E293B; color: #FFFFFF; padding: 8px 16px; font-weight: 600;");
    connect(m_btnAdvStep3Back, &QPushButton::clicked, this, &AppleIpswWidget::onAdvStep2Back);
    ctrlBtns->addWidget(m_btnAdvStep3Back);

    m_btnStartFlash = new QPushButton(QString::fromUtf8("⚡ НАЧАТЬ ПРОШИВКУ"), execBox);
    m_btnStartFlash->setObjectName(QStringLiteral("ipswBtnStartFlash"));
    m_btnStartFlash->setCursor(Qt::PointingHandCursor);
    m_btnStartFlash->setStyleSheet(
        "QPushButton { background-color: #0284C7; border: 1px solid #38BDF8; color: #FFFFFF; padding: 8px 24px; font-weight: bold; font-size: 13px; } QPushButton:hover { background-color: #0369A1; } QPushButton:disabled { background-color: #0B101D; color: #475569; border-color: #1E293B; }");
    connect(m_btnStartFlash, &QPushButton::clicked, this, &AppleIpswWidget::onStartFlashClicked);
    ctrlBtns->addWidget(m_btnStartFlash, 2);

    m_btnCancelFlash = new QPushButton(QString::fromUtf8("⏹ Отмена"), execBox);
    m_btnCancelFlash->setObjectName(QStringLiteral("ipswBtnCancelFlash"));
    m_btnCancelFlash->setCursor(Qt::PointingHandCursor);
    m_btnCancelFlash->setStyleSheet("QPushButton { background-color: #7F1D1D; border: 1px solid #EF4444; color: #FFFFFF; padding: 8px 16px; font-weight: bold; } QPushButton:disabled { background-color: #0B101D; color: #475569; border-color: #1E293B; }");
    m_btnCancelFlash->setEnabled(false);
    connect(m_btnCancelFlash, &QPushButton::clicked, this, &AppleIpswWidget::onCancelFlashClicked);
    ctrlBtns->addWidget(m_btnCancelFlash);

    m_btnAdvRetry = new QPushButton(QString::fromUtf8("🔄 Повторить"), execBox);
    m_btnAdvRetry->setObjectName(QStringLiteral("ipswBtnAdvRetry"));
    m_btnAdvRetry->setCursor(Qt::PointingHandCursor);
    m_btnAdvRetry->setStyleSheet("QPushButton { background-color: #064E3B; border: 1px solid #059669; color: #34D399; padding: 8px 16px; font-weight: bold; } QPushButton:hover { background-color: #047857; }");
    m_btnAdvRetry->setVisible(false);
    connect(m_btnAdvRetry, &QPushButton::clicked, this, &AppleIpswWidget::onAdvRetry);
    ctrlBtns->addWidget(m_btnAdvRetry);

    ebLayout->addLayout(ctrlBtns);
    rLayout->addWidget(execBox);

    // Terminal Box with Header Bar
    QWidget *termBox = new QWidget(rightWidget);
    termBox->setStyleSheet("background-color: #070A12; border: 1px solid #1E293B;");
    QVBoxLayout *tbLayout = new QVBoxLayout(termBox);
    tbLayout->setContentsMargins(0, 0, 0, 0);
    tbLayout->setSpacing(0);

    QWidget *termHead = new QWidget(termBox);
    termHead->setStyleSheet("background-color: #0F172A; border-bottom: 1px solid #1E293B; padding: 4px 8px;");
    QHBoxLayout *thLayout = new QHBoxLayout(termHead);
    thLayout->setContentsMargins(6, 2, 6, 2);
    thLayout->setSpacing(8);

    QLabel *termTitle = new QLabel(QString::fromUtf8("<span style='color: #38BDF8; font-family: monospace; font-size: 11.5px;'>&gt; idevicerestore log terminal</span>"), termHead);
    thLayout->addWidget(termTitle);
    thLayout->addStretch();

    QPushButton *btnCopyLog = new QPushButton(QString::fromUtf8("📋 Копировать"), termHead);
    btnCopyLog->setCursor(Qt::PointingHandCursor);
    btnCopyLog->setStyleSheet("background: transparent; border: 1px solid #334155; color: #94A3B8; padding: 2px 8px; font-size: 11px;");
    connect(btnCopyLog, &QPushButton::clicked, this, &AppleIpswWidget::onCopyLogClicked);
    thLayout->addWidget(btnCopyLog);

    QPushButton *btnClearLog = new QPushButton(QString::fromUtf8("🗑 Очистить"), termHead);
    btnClearLog->setCursor(Qt::PointingHandCursor);
    btnClearLog->setStyleSheet("background: transparent; border: 1px solid #334155; color: #94A3B8; padding: 2px 8px; font-size: 11px;");
    connect(btnClearLog, &QPushButton::clicked, this, &AppleIpswWidget::onClearLogClicked);
    thLayout->addWidget(btnClearLog);

    tbLayout->addWidget(termHead);

    m_logTerminal = new QTextEdit(termBox);
    m_logTerminal->setObjectName(QStringLiteral("ipswLogTerminal"));
    m_logTerminal->setReadOnly(true);
    m_logTerminal->setFrameShape(QFrame::NoFrame);
    tbLayout->addWidget(m_logTerminal, 1);

    rLayout->addWidget(termBox, 1);

    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);

    a3Layout->addWidget(splitter);
    a3Layout->addStretch(1);

    QScrollArea *a3Scroll = createMetroScrollArea(advStep3, m_advancedStepStack);
    m_advancedStepStack->addWidget(a3Scroll);

    layout->addWidget(m_advancedStepStack, 1);
    m_modeStack->addWidget(advPage);

    updateAdvStepPills(0);
    updateDeviceCard();
}

// -------------------------------------------------------------
// Checksum Verification Methods
// -------------------------------------------------------------
void AppleIpswWidget::startChecksumForFile(const QString &filePath, const QString &expectedSha1, const QString &expectedMd5)
{
    if(filePath.isEmpty() || !QFile::exists(filePath))
    {
        m_checksumStatus = ChecksumStatus::NotChecked;
        updateChecksumUi();
        return;
    }

    m_checksumStatus = ChecksumStatus::Calculating;
    m_currentCalculatedSha1.clear();
    m_currentCalculatedMd5.clear();
    m_currentExpectedSha1 = expectedSha1;
    m_checksumErrorStr.clear();

    updateChecksumUi();

    appendLog(QString::fromUtf8("[ХЕШ] Запуск асинхронной сверки SHA-1 для: %1...").arg(QFileInfo(filePath).fileName()), "#38BDF8");
    m_checksumWorker->startVerification(filePath, expectedSha1, expectedMd5);
}

void AppleIpswWidget::onStartChecksumVerification()
{
    if(m_selectedIpswPath.isEmpty() || !QFile::exists(m_selectedIpswPath))
    {
        QMessageBox::warning(this, QString::fromUtf8("Файл не найден"), QString::fromUtf8("Файл прошивки не найден на диске.\nСначала скачайте его или выберите локальный .ipsw."));
        return;
    }

    startChecksumForFile(m_selectedIpswPath, m_selectedFirmware.sha1sum, m_selectedFirmware.md5sum);
}

void AppleIpswWidget::onCancelChecksumVerification()
{
    if(m_checksumWorker && m_checksumWorker->isRunning())
    {
        m_checksumWorker->cancel();
        m_checksumStatus = ChecksumStatus::NotChecked;
        updateChecksumUi();
        appendLog(QString::fromUtf8("[ХЕШ] Проверка контрольной суммы отменена."), "#F59E0B");
    }
}

void AppleIpswWidget::onChecksumProgress(int percent, double speedMBs, qint64 processedBytes, qint64 totalBytes)
{
    if(m_simpleChecksumProgress)
    {
        m_simpleChecksumProgress->setValue(percent);
    }
    if(m_advChecksumProgress)
    {
        m_advChecksumProgress->setValue(percent);
    }

    QString status = QString::fromUtf8("Сверка SHA-1: %1% (%2 из %3 • %4 МБ/с)...").arg(percent).arg(formatFileSize(processedBytes), formatFileSize(totalBytes)).arg(speedMBs, 0, 'f', 1);

    if(m_lblSimpleChecksumStatus)
        m_lblSimpleChecksumStatus->setText(status);
    if(m_lblAdvChecksumStatus)
        m_lblAdvChecksumStatus->setText(status);
}

void AppleIpswWidget::onChecksumFinished(bool matched, const QString &calcSha1, const QString &expSha1, const QString &calcMd5, const QString &errorStr)
{
    if(!errorStr.isEmpty())
    {
        m_checksumStatus = ChecksumStatus::Error;
        m_checksumErrorStr = errorStr;
        appendLog(QString::fromUtf8("[ХЕШ ОШИБКА] %1").arg(errorStr), "#EF4444");
    }
    else
    {
        m_currentCalculatedSha1 = calcSha1;
        m_currentCalculatedMd5 = calcMd5;
        m_currentExpectedSha1 = expSha1;

        if(!expSha1.isEmpty())
        {
            if(matched)
            {
                m_checksumStatus = ChecksumStatus::Matched;
                appendLog(QString::fromUtf8("[ХЕШ ПОДТВЕРЖДЕН] SHA-1 %1 совпадает с официальным реестром Apple TSS (100% целостность)!").arg(calcSha1), "#10B981");
            }
            else
            {
                m_checksumStatus = ChecksumStatus::Mismatch;
                appendLog(QString::fromUtf8("[ХЕШ НЕСОВПАДЕНИЕ!] ВНИМАНИЕ: Контрольная сумма не совпадает!\nВычислено: %1\nОжидалось:  %2").arg(calcSha1, expSha1), "#EF4444");
            }
        }
        else
        {
            m_checksumStatus = ChecksumStatus::VerifiedWithoutExpected;
            appendLog(QString::fromUtf8("[ХЕШ ВЫЧИСЛЕН] SHA-1: %1 | MD5: %2").arg(calcSha1, calcMd5), "#38BDF8");
        }
    }

    updateChecksumUi();
}

void AppleIpswWidget::updateChecksumUi()
{
    // Simple Mode Checksum Card
    if(m_lblSimpleChecksumStatus && m_simpleChecksumProgress && m_btnSimpleVerifyChecksum && m_btnSimpleCancelChecksum)
    {
        switch(m_checksumStatus)
        {
            case ChecksumStatus::NotChecked:
                m_simpleChecksumProgress->setVisible(false);
                m_btnSimpleCancelChecksum->setVisible(false);
                m_btnSimpleVerifyChecksum->setVisible(true);
                m_btnSimpleVerifyChecksum->setText(QString::fromUtf8("🔍 Сверить SHA-1"));
                if(!m_selectedFirmware.sha1sum.isEmpty())
                {
                    m_lblSimpleChecksumStatus->setText(QString::fromUtf8("Официальный хеш Apple: <span style='font-family: monospace; color: #38BDF8;'>%1</span> (Не проверен)").arg(m_selectedFirmware.sha1sum));
                }
                else
                {
                    m_lblSimpleChecksumStatus->setText(QString::fromUtf8("Контрольная сумма файла еще не проверялась."));
                }
                m_lblSimpleChecksumStatus->setStyleSheet("color: #64748B; font-size: 11.5px;");
                break;

            case ChecksumStatus::Calculating:
                m_simpleChecksumProgress->setVisible(true);
                m_btnSimpleCancelChecksum->setVisible(true);
                m_btnSimpleVerifyChecksum->setVisible(false);
                m_lblSimpleChecksumStatus->setStyleSheet("color: #38BDF8; font-size: 11.5px; font-weight: 600;");
                break;

            case ChecksumStatus::Matched:
                m_simpleChecksumProgress->setVisible(false);
                m_btnSimpleCancelChecksum->setVisible(false);
                m_btnSimpleVerifyChecksum->setVisible(true);
                m_btnSimpleVerifyChecksum->setText(QString::fromUtf8("🔄 Перепроверить"));
                m_lblSimpleChecksumStatus->setText(QString::fromUtf8("✔ <b>Хеш SHA-1 подтвержден!</b> Файл на 100% целостен и совпадает с официальным реестром Apple TSS.<br><span style='font-family: monospace; color: #34D399;'>%1</span>").arg(m_currentCalculatedSha1));
                m_lblSimpleChecksumStatus->setStyleSheet("background: rgba(16, 185, 129, 0.12); color: #34D399; border: 1px solid #059669; padding: 6px 10px; font-size: 11.5px;");
                break;

            case ChecksumStatus::Mismatch:
                m_simpleChecksumProgress->setVisible(false);
                m_btnSimpleCancelChecksum->setVisible(false);
                m_btnSimpleVerifyChecksum->setVisible(true);
                m_btnSimpleVerifyChecksum->setText(QString::fromUtf8("🔄 Повторить сверку"));
                m_lblSimpleChecksumStatus->setText(
                    QString::fromUtf8("⚠ <b>ОШИБКА ЦЕЛОСТНОСТИ!</b> Контрольная сумма файла НЕ совпадает с официальной!<br>Вычислен: <span style='font-family: monospace;'>%1</span><br>Ожидался:  <span style='font-family: monospace;'>%2</span>").arg(m_currentCalculatedSha1, m_currentExpectedSha1));
                m_lblSimpleChecksumStatus->setStyleSheet("background: rgba(239, 68, 68, 0.15); color: #F87171; border: 1px solid #DC2626; padding: 6px 10px; font-size: 11.5px;");
                break;

            case ChecksumStatus::VerifiedWithoutExpected:
                m_simpleChecksumProgress->setVisible(false);
                m_btnSimpleCancelChecksum->setVisible(false);
                m_btnSimpleVerifyChecksum->setVisible(true);
                m_btnSimpleVerifyChecksum->setText(QString::fromUtf8("🔄 Перепроверить"));
                m_lblSimpleChecksumStatus->setText(QString::fromUtf8("✔ Вычислен SHA-1: <span style='font-family: monospace; color: #38BDF8;'>%1</span> (MD5: %2)").arg(m_currentCalculatedSha1, m_currentCalculatedMd5));
                m_lblSimpleChecksumStatus->setStyleSheet("background: rgba(56, 189, 248, 0.1); color: #38BDF8; border: 1px solid #0284C7; padding: 6px 10px; font-size: 11.5px;");
                break;

            case ChecksumStatus::Error:
                m_simpleChecksumProgress->setVisible(false);
                m_btnSimpleCancelChecksum->setVisible(false);
                m_btnSimpleVerifyChecksum->setVisible(true);
                m_lblSimpleChecksumStatus->setText(QString::fromUtf8("Ошибка при чтении файла: %1").arg(m_checksumErrorStr));
                m_lblSimpleChecksumStatus->setStyleSheet("background: rgba(239, 68, 68, 0.15); color: #F87171; border: 1px solid #DC2626; padding: 6px 10px; font-size: 11.5px;");
                break;
        }
    }

    // Advanced Mode Checksum Card (Step 1)
    if(m_lblAdvChecksumStatus && m_advChecksumProgress && m_btnAdvVerifyChecksum && m_btnAdvCancelChecksum)
    {
        switch(m_checksumStatus)
        {
            case ChecksumStatus::NotChecked:
                m_advChecksumProgress->setVisible(false);
                m_btnAdvCancelChecksum->setVisible(false);
                m_btnAdvVerifyChecksum->setVisible(true);
                m_btnAdvVerifyChecksum->setText(QString::fromUtf8("🔍 Сверить SHA-1"));
                if(!m_selectedFirmware.sha1sum.isEmpty())
                {
                    m_lblAdvChecksumStatus->setText(QString::fromUtf8("Хеш Apple TSS: <span style='font-family: monospace; color: #38BDF8;'>%1</span> (Не проверен)").arg(m_selectedFirmware.sha1sum));
                }
                else
                {
                    m_lblAdvChecksumStatus->setText(QString::fromUtf8("Хеш SHA-1: не проверялся."));
                }
                m_lblAdvChecksumStatus->setStyleSheet("color: #94A3B8; font-size: 11px;");
                break;

            case ChecksumStatus::Calculating:
                m_advChecksumProgress->setVisible(true);
                m_btnAdvCancelChecksum->setVisible(true);
                m_btnAdvVerifyChecksum->setVisible(false);
                m_lblAdvChecksumStatus->setStyleSheet("color: #38BDF8; font-size: 11px; font-weight: 600;");
                break;

            case ChecksumStatus::Matched:
                m_advChecksumProgress->setVisible(false);
                m_btnAdvCancelChecksum->setVisible(false);
                m_btnAdvVerifyChecksum->setVisible(true);
                m_btnAdvVerifyChecksum->setText(QString::fromUtf8("🔄 Перепроверить"));
                m_lblAdvChecksumStatus->setText(QString::fromUtf8("✔ <b>SHA-1 Валиден (100% совпадение):</b> <span style='font-family: monospace; color: #34D399;'>%1</span>").arg(m_currentCalculatedSha1));
                m_lblAdvChecksumStatus->setStyleSheet("color: #34D399; font-size: 11px;");
                break;

            case ChecksumStatus::Mismatch:
                m_advChecksumProgress->setVisible(false);
                m_btnAdvCancelChecksum->setVisible(false);
                m_btnAdvVerifyChecksum->setVisible(true);
                m_btnAdvVerifyChecksum->setText(QString::fromUtf8("🔄 Повторить сверку"));
                m_lblAdvChecksumStatus->setText(QString::fromUtf8("⚠ <b>НЕСОВПАДЕНИЕ ХЕША:</b> <span style='font-family: monospace;'>%1</span> (Ожидался: %2)").arg(m_currentCalculatedSha1, m_currentExpectedSha1));
                m_lblAdvChecksumStatus->setStyleSheet("color: #F87171; font-size: 11px;");
                break;

            case ChecksumStatus::VerifiedWithoutExpected:
                m_advChecksumProgress->setVisible(false);
                m_btnAdvCancelChecksum->setVisible(false);
                m_btnAdvVerifyChecksum->setVisible(true);
                m_btnAdvVerifyChecksum->setText(QString::fromUtf8("🔄 Перепроверить"));
                m_lblAdvChecksumStatus->setText(QString::fromUtf8("✔ SHA-1: <span style='font-family: monospace; color: #38BDF8;'>%1</span>").arg(m_currentCalculatedSha1));
                m_lblAdvChecksumStatus->setStyleSheet("color: #38BDF8; font-size: 11px;");
                break;

            case ChecksumStatus::Error:
                m_advChecksumProgress->setVisible(false);
                m_btnAdvCancelChecksum->setVisible(false);
                m_btnAdvVerifyChecksum->setVisible(true);
                m_lblAdvChecksumStatus->setText(QString::fromUtf8("Ошибка: %1").arg(m_checksumErrorStr));
                m_lblAdvChecksumStatus->setStyleSheet("color: #F87171; font-size: 11px;");
                break;
        }
    }

    // Step 3 Checksum Badge
    if(m_lblAdvStep3ChecksumBadge)
    {
        switch(m_checksumStatus)
        {
            case ChecksumStatus::NotChecked:
                m_lblAdvStep3ChecksumBadge->setText(QString::fromUtf8("○ Хеш не проверен"));
                m_lblAdvStep3ChecksumBadge->setStyleSheet("background: rgba(148, 163, 184, 0.08); color: #94A3B8; border: 1px solid #1E293B; padding: 2px 8px; font-size: 11px; font-weight: 600;");
                break;
            case ChecksumStatus::Calculating:
                m_lblAdvStep3ChecksumBadge->setText(QString::fromUtf8("⏳ Идет сверка SHA-1..."));
                m_lblAdvStep3ChecksumBadge->setStyleSheet("background: rgba(56, 189, 248, 0.15); color: #38BDF8; border: 1px solid #0284C7; padding: 2px 8px; font-size: 11px; font-weight: 600;");
                break;
            case ChecksumStatus::Matched:
                m_lblAdvStep3ChecksumBadge->setText(QString::fromUtf8("✔ Целостность SHA-1 подтверждена"));
                m_lblAdvStep3ChecksumBadge->setStyleSheet("background: rgba(16, 185, 129, 0.15); color: #34D399; border: 1px solid #059669; padding: 2px 8px; font-size: 11px; font-weight: 600;");
                break;
            case ChecksumStatus::Mismatch:
                m_lblAdvStep3ChecksumBadge->setText(QString::fromUtf8("⚠ ХЕШ ПОВРЕЖДЕН!"));
                m_lblAdvStep3ChecksumBadge->setStyleSheet("background: rgba(239, 68, 68, 0.15); color: #F87171; border: 1px solid #DC2626; padding: 2px 8px; font-size: 11px; font-weight: bold;");
                break;
            case ChecksumStatus::VerifiedWithoutExpected:
                m_lblAdvStep3ChecksumBadge->setText(QString::fromUtf8("✔ SHA-1 Вычислен"));
                m_lblAdvStep3ChecksumBadge->setStyleSheet("background: rgba(56, 189, 248, 0.15); color: #38BDF8; border: 1px solid #0284C7; padding: 2px 8px; font-size: 11px; font-weight: 600;");
                break;
            case ChecksumStatus::Error:
                m_lblAdvStep3ChecksumBadge->setText(QString::fromUtf8("Ошибка проверки"));
                m_lblAdvStep3ChecksumBadge->setStyleSheet("background: rgba(239, 68, 68, 0.15); color: #F87171; border: 1px solid #DC2626; padding: 2px 8px; font-size: 11px; font-weight: 600;");
                break;
        }
    }
}

// -------------------------------------------------------------
// Simple Wizard Handlers
// -------------------------------------------------------------
void AppleIpswWidget::updateSimpleWizardStep1()
{
    if(!m_lblSimpleUsbStatus || !m_lblSimpleDetectedModel || !m_btnSimpleStep1Next)
        return;

    if(m_device.isEmpty())
    {
        m_lblSimpleUsbStatus->setText(QString::fromUtf8("⏳ Ожидание подключения iPhone / iPad по USB..."));
        m_lblSimpleUsbStatus->setStyleSheet("background: rgba(245, 158, 11, 0.1); color: #FBBF24; font-size: 12.5px; font-weight: bold; padding: 12px; border: 1px solid #D97706;");
        m_lblSimpleDetectedModel->setText(QString::fromUtf8("Устройство пока не обнаружено. Подключите кабель USB к компьютеру и разблокируйте экран."));
        m_lblSimpleDetectedModel->setStyleSheet("color: #94A3B8; font-size: 12px;");

        if(m_simpleConnectedDeviceCard)
            m_simpleConnectedDeviceCard->setVisible(false);
        m_btnSimpleStep1Next->setEnabled(false);
    }
    else
    {
        QString name = !m_device.displayName.isEmpty() ? m_device.displayName : (!m_device.marketingName.isEmpty() ? m_device.marketingName : m_device.model);
        m_lblSimpleUsbStatus->setText(QString::fromUtf8("✔ Устройство Apple успешно обнаружено!"));
        m_lblSimpleUsbStatus->setStyleSheet("background: rgba(16, 185, 129, 0.12); color: #34D399; font-size: 12.5px; font-weight: bold; padding: 12px; border: 1px solid #059669;");
        m_lblSimpleDetectedModel->setText(QString::fromUtf8("Готово к автонастройке и прошивке. Нажмите «Продолжить» для перехода к параметрам."));
        m_lblSimpleDetectedModel->setStyleSheet("color: #F8FAFC; font-size: 12px;");

        if(m_simpleConnectedDeviceCard)
        {
            m_simpleConnectedDeviceCard->setVisible(true);
            if(m_lblSimpleCardDeviceTitle)
                m_lblSimpleCardDeviceTitle->setText(name);
            if(m_lblSimpleCardDeviceMode)
            {
                m_lblSimpleCardDeviceMode->setText(QString("● %1").arg(m_device.modeString()));
                if(m_device.mode == AppleDeviceMode::Normal)
                {
                    m_lblSimpleCardDeviceMode->setStyleSheet("background: rgba(16, 185, 129, 0.15); color: #34D399; font-weight: bold; padding: 3px 8px; border: 1px solid #059669; font-size: 11px;");
                }
                else
                {
                    m_lblSimpleCardDeviceMode->setStyleSheet("background: rgba(56, 189, 248, 0.15); color: #38BDF8; font-weight: bold; padding: 3px 8px; border: 1px solid #0284C7; font-size: 11px;");
                }
            }
            if(m_lblSimpleCardModel)
                m_lblSimpleCardModel->setText(!m_device.model.isEmpty() ? m_device.model : "—");
            if(m_lblSimpleCardIos)
                m_lblSimpleCardIos->setText(!m_device.productVersion.isEmpty() ? m_device.productVersion : QString::fromUtf8("В Recovery/DFU"));
            if(m_lblSimpleCardSerial)
                m_lblSimpleCardSerial->setText(!m_device.serialNumber.isEmpty() ? m_device.serialNumber : "—");
            if(m_lblSimpleCardEcid)
                m_lblSimpleCardEcid->setText(!m_device.ecid.isEmpty() ? m_device.ecid : "—");
        }

        m_btnSimpleStep1Next->setEnabled(true);
    }
}

void AppleIpswWidget::updateSimpleWizardStep2()
{
    if(!m_lblSimpleDeviceSummary || !m_lblSimpleAutoIpswInfo)
        return;

    QString name = !m_device.displayName.isEmpty() ? m_device.displayName : (!m_device.marketingName.isEmpty() ? m_device.marketingName : m_device.model);
    m_lblSimpleDeviceSummary->setText(QString::fromUtf8("📱 Выбранное устройство: <b>%1</b> (Режим: %2 • ECID: %3)").arg(name, m_device.modeString(), !m_device.ecid.isEmpty() ? m_device.ecid : "—"));

    IpswFirmwareInfo bestSigned;
    for(const auto &fw : m_firmwares)
    {
        if(fw.isSigned)
        {
            bestSigned = fw;
            break;
        }
    }

    if(!bestSigned.version.isEmpty())
    {
        m_selectedFirmware = bestSigned;
        m_isLocalIpsw = false;
        QString expectedFileName = QString("%1_%2_%3_Restore.ipsw").arg(bestSigned.identifier, bestSigned.version, bestSigned.buildid);
        m_selectedIpswPath = downloadDirectory() + "/" + expectedFileName;

        if(QFile::exists(m_selectedIpswPath))
        {
            m_lblSimpleAutoIpswInfo->setText(QString::fromUtf8("✔ Официальная прошивка <b>iOS %1 (Сборка %2)</b> уже загружена в хранилище [%3] и готова к установке.").arg(bestSigned.version, bestSigned.buildid, formatFileSize(QFileInfo(m_selectedIpswPath).size())));
            m_lblSimpleAutoIpswInfo->setStyleSheet("color: #34D399; font-size: 12px; padding: 6px;");

            // Check if checksum was already verified for this file
            if(m_checksumStatus == ChecksumStatus::NotChecked)
            {
                startChecksumForFile(m_selectedIpswPath, bestSigned.sha1sum, bestSigned.md5sum);
            }
        }
        else
        {
            m_lblSimpleAutoIpswInfo->setText(QString::fromUtf8("⬇ Актуальная официальная прошивка Apple: <b>iOS %1 (Сборка %2)</b> [Размер: %3]. Будет загружена автоматически с серверов Apple перед установкой.").arg(bestSigned.version, bestSigned.buildid, formatFileSize(bestSigned.size)));
            m_lblSimpleAutoIpswInfo->setStyleSheet("color: #38BDF8; font-size: 12px; padding: 6px;");
            m_checksumStatus = ChecksumStatus::NotChecked;
            updateChecksumUi();
        }
    }
    else
    {
        m_lblSimpleAutoIpswInfo->setText(QString::fromUtf8("⏳ Запрос актуальной подписанной прошивки у серверов Apple..."));
        m_lblSimpleAutoIpswInfo->setStyleSheet("color: #FBBF24; font-size: 12px; padding: 6px;");
        m_checksumStatus = ChecksumStatus::NotChecked;
        updateChecksumUi();
        if(!m_device.model.isEmpty())
        {
            fetchFirmwareCatalog(m_device.model);
        }
    }
}

void AppleIpswWidget::onSimpleStep1Next()
{
    if(m_device.isEmpty())
    {
        QMessageBox::information(this, QString::fromUtf8("Подключение USB"), QString::fromUtf8("Сначала подключите устройство Apple через кабель USB к компьютеру."));
        return;
    }
    m_simpleStepStack->setCurrentIndex(1);
    updateSimpleStepPills(1);
    updateSimpleWizardStep2();
}

void AppleIpswWidget::onSimpleStep2Back()
{
    m_simpleStepStack->setCurrentIndex(0);
    updateSimpleStepPills(0);
    updateSimpleWizardStep1();
}

void AppleIpswWidget::onSimpleStep2Start()
{
    if(m_device.isEmpty())
    {
        QMessageBox::warning(this, QString::fromUtf8("Устройство отключено"), QString::fromUtf8("Устройство Apple не найдено. Подключите кабель USB."));
        onSimpleStep2Back();
        return;
    }

    if(m_checksumStatus == ChecksumStatus::Calculating)
    {
        QMessageBox::information(this, QString::fromUtf8("Проверка контрольной суммы"), QString::fromUtf8("В данный момент выполняется проверка целостности файла (SHA-1).\nПожалуйста, дождитесь завершения или отмените проверку."));
        return;
    }

    if(m_checksumStatus == ChecksumStatus::Mismatch)
    {
        auto res = QMessageBox::critical(
            this,
            QString::fromUtf8("ВНИМАНИЕ: Файл поврежден!"),
            QString::fromUtf8(
                "ВНИМАНИЕ! Контрольная сумма SHA-1 файла прошивки НЕ совпадает с официальным образом Apple!\n\n"
                "Файл поврежден при загрузке или модифицирован.\n"
                "Прошивка поврежденным образом может привести к критическому сбою и окирпичиванию устройства!\n\n"
                "Вы действительно хотите проигнорировать предупреждение и продолжить?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if(res != QMessageBox::Yes)
            return;
    }

    bool retain = m_rbSimpleRetain->isChecked();
    QString confirmText = retain ? QString::fromUtf8("Начать ОБНОВЛЕНИЕ устройства %1 с сохранением данных?\n\nФайл прошивки: iOS %2 (%3)").arg(!m_device.displayName.isEmpty() ? m_device.displayName : m_device.marketingName, m_selectedFirmware.version, m_selectedFirmware.buildid)
                                 : QString::fromUtf8(
                                       "ВНИМАНИЕ! Выбран режим ПОЛНОЙ ОЧИСТКИ.\n\n"
                                       "Все данные пользователя на iPhone будут ПОЛНОСТЬЮ СТЕРТЫ!\n\n"
                                       "Вы уверены, что хотите продолжить?");

    auto reply = QMessageBox::question(this, QString::fromUtf8("Подтверждение восстановления"), confirmText, QMessageBox::Yes | QMessageBox::No);

    if(reply != QMessageBox::Yes)
        return;

    m_simpleStepStack->setCurrentIndex(2);
    updateSimpleStepPills(2);
    m_btnSimpleRetry->setVisible(false);
    m_btnSimpleCancel->setEnabled(true);
    m_simpleProgressBar->setValue(0);

    if(m_selectedIpswPath.isEmpty() || !QFile::exists(m_selectedIpswPath))
    {
        if(m_selectedFirmware.url.isEmpty())
        {
            QMessageBox::critical(this, QString::fromUtf8("Ошибка"), QString::fromUtf8("Не удалось определить ссылку на прошивку. Попробуйте обновить каталог или переключитесь в расширенный режим."));
            onSimpleStep2Back();
            return;
        }

        m_lblSimpleProgressStatus->setText(QString::fromUtf8("Загрузка официальной прошивки iOS %1 с серверов Apple...").arg(m_selectedFirmware.version));
        m_downloader->startDownload(m_selectedFirmware.url, m_selectedIpswPath);
        return;
    }

    m_lblSimpleProgressStatus->setText(QString::fromUtf8("Запуск процесса прошивки (%1)...").arg(retain ? "с сохранением данных" : "со сбросом"));
    m_restoreWorker->startRestore(m_selectedIpswPath, retain, m_device.devId);
}

void AppleIpswWidget::onSimpleRetry()
{
    m_btnSimpleRetry->setVisible(false);
    onSimpleStep2Start();
}

// -------------------------------------------------------------
// Advanced Wizard Handlers
// -------------------------------------------------------------
void AppleIpswWidget::onAdvStep1Next()
{
    if(m_selectedIpswPath.isEmpty() || !QFile::exists(m_selectedIpswPath))
    {
        if(!m_selectedFirmware.url.isEmpty())
        {
            auto res = QMessageBox::question(this, QString::fromUtf8("Прошивка не загружена"), QString::fromUtf8("Выбранная прошивка iOS %1 еще не загружена на диск.\nПредзагрузить её сейчас?").arg(m_selectedFirmware.version), QMessageBox::Yes | QMessageBox::No);
            if(res == QMessageBox::Yes)
            {
                if(m_firmwareTable && m_firmwareTable->currentRow() >= 0)
                    onDownloadIpswClicked(m_firmwareTable->currentRow());
                else
                    onDownloadIpswClicked(0);
                return;
            }
        }
        else
        {
            QMessageBox::warning(this, QString::fromUtf8("Прошивка не выбрана"), QString::fromUtf8("Пожалуйста, выберите версию прошивки из таблицы или укажите локальный файл .ipsw."));
            return;
        }
    }

    m_advancedStepStack->setCurrentIndex(1);
    updateAdvStepPills(1);

    if(m_lblAdvStep2FirmwareSummary)
    {
        m_lblAdvStep2FirmwareSummary->setText(QString::fromUtf8("📦 Выбранный файл прошивки: <b>%1</b> (%2)").arg(QFileInfo(m_selectedIpswPath).fileName(), formatFileSize(QFileInfo(m_selectedIpswPath).size())));
    }
}

void AppleIpswWidget::onAdvStep2Back()
{
    m_advancedStepStack->setCurrentIndex(0);
    updateAdvStepPills(0);
}

void AppleIpswWidget::onAdvStep2Next()
{
    m_advancedStepStack->setCurrentIndex(2);
    updateAdvStepPills(2);
    updateDeviceCard();
    updateChecksumUi();
}

void AppleIpswWidget::onAdvRetry()
{
    m_btnAdvRetry->setVisible(false);
    onStartFlashClicked();
}

void AppleIpswWidget::appendLog(const QString &line, const QString &color)
{
    if(!m_logTerminal)
        return;
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString formatted = QString("<span style='color: #475569;'>[%1]</span> <span style='color: %2;'>%3</span>").arg(timestamp, color, line.toHtmlEscaped());
    m_logTerminal->append(formatted);
    m_logTerminal->verticalScrollBar()->setValue(m_logTerminal->verticalScrollBar()->maximum());
}

void AppleIpswWidget::onClearLogClicked()
{
    if(m_logTerminal)
        m_logTerminal->clear();
}

void AppleIpswWidget::onCopyLogClicked()
{
    if(m_logTerminal)
    {
        QApplication::clipboard()->setText(m_logTerminal->toPlainText());
        appendLog(QString::fromUtf8("[СИСТЕМА] Консольный лог скопирован в буфер обмена."), "#38BDF8");
    }
}

void AppleIpswWidget::loadDeviceList()
{
    if(!m_comboDeviceModel)
        return;

    m_comboDeviceModel->blockSignals(true);
    m_comboDeviceModel->clear();

    static const struct
    {
        const char *id;
        const char *name;
    } defaultModels[] = {
        {"iPhone17,2", "iPhone 16 Pro Max"},
        {"iPhone17,1", "iPhone 16 Pro"},
        {"iPhone17,4", "iPhone 16 Plus"},
        {"iPhone17,3", "iPhone 16"},
        {"iPhone16,2", "iPhone 15 Pro Max"},
        {"iPhone16,1", "iPhone 15 Pro"},
        {"iPhone15,5", "iPhone 15 Plus"},
        {"iPhone15,4", "iPhone 15"},
        {"iPhone15,3", "iPhone 14 Pro Max"},
        {"iPhone15,2", "iPhone 14 Pro"},
        {"iPhone14,8", "iPhone 14 Plus"},
        {"iPhone14,7", "iPhone 14"},
        {"iPhone14,3", "iPhone 13 Pro Max"},
        {"iPhone14,2", "iPhone 13 Pro"},
        {"iPhone14,5", "iPhone 13"},
        {"iPhone14,4", "iPhone 13 mini"},
        {"iPhone13,4", "iPhone 12 Pro Max"},
        {"iPhone13,3", "iPhone 12 Pro"},
        {"iPhone13,2", "iPhone 12"},
        {"iPhone13,1", "iPhone 12 mini"},
        {"iPhone12,5", "iPhone 11 Pro Max"},
        {"iPhone12,3", "iPhone 11 Pro"},
        {"iPhone12,1", "iPhone 11"},
        {"iPhone11,6", "iPhone XS Max"},
        {"iPhone11,2", "iPhone XS"},
        {"iPhone11,8", "iPhone XR"},
        {"iPhone10,3", "iPhone X"},
        {"iPhone10,2", "iPhone 8 Plus"},
        {"iPhone10,1", "iPhone 8"},
        {"iPhone14,6", "iPhone SE (3rd gen)"},
        {"iPhone12,8", "iPhone SE (2nd gen)"},
        {"iPad14,5", "iPad Pro 12.9-inch (6th gen)"},
        {"iPad14,3", "iPad Pro 11-inch (4th gen)"},
        {"iPad13,16", "iPad Air (5th gen)"},
        {"iPad13,18", "iPad (10th gen)"},
        {"iPad14,1", "iPad mini (6th gen)"}};

    for(const auto &item : defaultModels)
    {
        m_comboDeviceModel->addItem(QString("%1 (%2)").arg(item.name, item.id), item.id);
    }

    m_comboDeviceModel->blockSignals(false);

    // Fetch live device catalog from api.ipsw.me
    QUrl url("https://api.ipsw.me/v4/devices");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "AdsKiller-AppleFlasher/2.0");
    QNetworkReply *reply = m_netManager->get(req);
    connect(
        reply,
        &QNetworkReply::finished,
        this,
        [this, reply]()
        {
            reply->deleteLater();
            if(reply->error() != QNetworkReply::NoError)
                return;
            QByteArray data = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if(!doc.isArray())
                return;

            QJsonArray arr = doc.array();
            for(const QJsonValue &v : arr)
            {
                QJsonObject obj = v.toObject();
                QString id = obj["identifier"].toString();
                QString name = obj["name"].toString();
                if(!id.isEmpty() && m_comboDeviceModel)
                {
                    if(m_comboDeviceModel->findData(id) == -1)
                    {
                        m_comboDeviceModel->addItem(QString("%1 (%2)").arg(name, id), id);
                    }
                }
            }
        });

    if(m_comboDeviceModel->count() > 0)
    {
        m_comboDeviceModel->setCurrentIndex(0);
        m_currentSelectedModelId = m_comboDeviceModel->currentData().toString();
        fetchFirmwareCatalog(m_currentSelectedModelId);
    }
}

void AppleIpswWidget::onModelSelected(int index)
{
    if(index < 0 || !m_comboDeviceModel)
        return;
    QString modelId = m_comboDeviceModel->itemData(index).toString();
    if(!modelId.isEmpty() && modelId != m_currentSelectedModelId)
    {
        m_currentSelectedModelId = modelId;
        fetchFirmwareCatalog(modelId);
    }
}

void AppleIpswWidget::onRefreshCatalogClicked()
{
    if(!m_currentSelectedModelId.isEmpty())
    {
        fetchFirmwareCatalog(m_currentSelectedModelId);
    }
    else if(m_comboDeviceModel && m_comboDeviceModel->currentIndex() >= 0)
    {
        onModelSelected(m_comboDeviceModel->currentIndex());
    }
}

void AppleIpswWidget::onFilterCatalogChanged()
{
    if(!m_firmwareTable)
        return;
    QString filterText = m_editCatalogFilter ? m_editCatalogFilter->text().trimmed().toLower() : QString();
    bool onlySigned = m_chkOnlySigned ? m_chkOnlySigned->isChecked() : false;

    for(int row = 0; row < m_firmwareTable->rowCount(); ++row)
    {
        bool matchesText = true;
        if(!filterText.isEmpty())
        {
            QString ver = m_firmwareTable->item(row, 0) ? m_firmwareTable->item(row, 0)->text().toLower() : QString();
            QString build = m_firmwareTable->item(row, 1) ? m_firmwareTable->item(row, 1)->text().toLower() : QString();
            matchesText = ver.contains(filterText) || build.contains(filterText);
        }

        bool matchesSigned = true;
        if(onlySigned)
        {
            QString signStatus = m_firmwareTable->item(row, 3) ? m_firmwareTable->item(row, 3)->text() : QString();
            matchesSigned = signStatus.contains(QString::fromUtf8("Подписывается"));
        }

        m_firmwareTable->setRowHidden(row, !(matchesText && matchesSigned));
    }
}

void AppleIpswWidget::updateDiskSpaceInfo()
{
    QString dir = downloadDirectory();
    if(m_lblStoragePath)
    {
        m_lblStoragePath->setText(QString::fromUtf8("📁 Каталог IPSW: <b>%1</b>").arg(dir));
        m_lblStoragePath->setToolTip(dir);
    }

    if(m_lblDiskSpace)
    {
        QStorageInfo storage(dir);
        if(storage.isValid())
        {
            qint64 freeBytes = storage.bytesAvailable();
            m_lblDiskSpace->setText(QString::fromUtf8("Свободно на диске: %1").arg(formatFileSize(freeBytes)));
        }
    }
}

void AppleIpswWidget::onOpenFolderClicked()
{
    QString dir = downloadDirectory();
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void AppleIpswWidget::onChangeFolderClicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, QString::fromUtf8("Выберите папку для хранения прошивок"), downloadDirectory());
    if(!dir.isEmpty())
    {
        m_customDownloadDir = dir;
        updateDiskSpaceInfo();
        scanLocalFirmwares();
        populateFirmwareTable();
        appendLog(QString::fromUtf8("[ХРАНИЛИЩЕ] Папка загрузки изменена на: %1").arg(dir), "#38BDF8");
    }
}

void AppleIpswWidget::scanLocalFirmwares()
{
    updateDiskSpaceInfo();
    populateLocalFirmwareTable();
}

void AppleIpswWidget::populateLocalFirmwareTable()
{
    if(!m_localFirmwareTable)
        return;
    m_localFirmwareTable->setRowCount(0);

    QDir dir(downloadDirectory());
    QFileInfoList files = dir.entryInfoList(QStringList() << "*.ipsw", QDir::Files, QDir::Time);

    for(const QFileInfo &fi : files)
    {
        int row = m_localFirmwareTable->rowCount();
        m_localFirmwareTable->insertRow(row);

        // 0: File name
        QTableWidgetItem *nameItem = new QTableWidgetItem(fi.fileName());
        nameItem->setFont(QFont("Segoe UI", 9, QFont::Bold));
        nameItem->setToolTip(fi.absoluteFilePath());
        m_localFirmwareTable->setItem(row, 0, nameItem);

        // 1: Model
        QString baseName = fi.baseName();
        QString detectedModel = "Apple Device";
        static const QRegularExpression rx("([a-zA-Z]+[0-9]+,[0-9]+)");
        auto match = rx.match(baseName);
        if(match.hasMatch())
        {
            QString mId = match.captured(1);
            detectedModel = QString("%1 (%2)").arg(Apple::marketingNameForModel(mId), mId);
        }
        QTableWidgetItem *modelItem = new QTableWidgetItem(detectedModel);
        modelItem->setForeground(QColor("#94A3B8"));
        m_localFirmwareTable->setItem(row, 1, modelItem);

        // 2: Size
        QTableWidgetItem *sizeItem = new QTableWidgetItem(formatFileSize(fi.size()));
        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_localFirmwareTable->setItem(row, 2, sizeItem);

        // 3: Date
        QTableWidgetItem *dateItem = new QTableWidgetItem(fi.lastModified().toString("dd.MM.yyyy hh:mm"));
        dateItem->setForeground(QColor("#64748B"));
        dateItem->setTextAlignment(Qt::AlignCenter);
        m_localFirmwareTable->setItem(row, 3, dateItem);

        // 4: Checksum Cell with Quick Verification Button
        QString filePath = fi.absoluteFilePath();
        QWidget *checkWidget = new QWidget(m_localFirmwareTable);
        QHBoxLayout *cwLayout = new QHBoxLayout(checkWidget);
        cwLayout->setContentsMargins(4, 2, 4, 2);
        cwLayout->setSpacing(4);

        QPushButton *btnCheckHash = new QPushButton(QString::fromUtf8("🔍 Сверить SHA-1"), checkWidget);
        btnCheckHash->setStyleSheet("background-color: #141C2E; color: #38BDF8; font-weight: 600; padding: 4px 8px; font-size: 11px; border: 1px solid #1E293B;");
        connect(
            btnCheckHash,
            &QPushButton::clicked,
            this,
            [this, filePath]()
            {
                m_selectedIpswPath = filePath;
                m_isLocalIpsw = true;
                startChecksumForFile(filePath);
            });
        cwLayout->addWidget(btnCheckHash);
        m_localFirmwareTable->setCellWidget(row, 4, checkWidget);

        // 5: Actions: Select for Flash & Delete
        QWidget *actionWidget = new QWidget(m_localFirmwareTable);
        QHBoxLayout *aLayout = new QHBoxLayout(actionWidget);
        aLayout->setContentsMargins(4, 2, 4, 2);
        aLayout->setSpacing(6);

        QPushButton *btnSelect = new QPushButton(QString::fromUtf8("⚡ Выбрать"), actionWidget);
        btnSelect->setStyleSheet("background-color: #0284C7; color: #FFFFFF; font-weight: bold; padding: 4px 8px; font-size: 11px; border: 1px solid #38BDF8;");
        connect(
            btnSelect,
            &QPushButton::clicked,
            this,
            [this, filePath, detectedModel, baseName]()
            {
                m_selectedIpswPath = filePath;
                m_isLocalIpsw = true;
                m_lblSelectedIpsw->setText(QString::fromUtf8("✔ Выбрана прошивка: <b>%1</b> [%2]").arg(baseName, formatFileSize(QFileInfo(filePath).size())));
                m_lblSelectedIpsw->setStyleSheet("color: #34D399; font-size: 11.5px; padding: 4px;");
                if(m_tabWidget)
                    m_tabWidget->setCurrentIndex(0);
                startChecksumForFile(filePath);
                appendLog(QString::fromUtf8("[ВЫБОР ПРОШИВКИ] Выбран локальный файл: %1").arg(filePath), "#10B981");
            });
        aLayout->addWidget(btnSelect);

        QPushButton *btnDel = new QPushButton(QString::fromUtf8("🗑"), actionWidget);
        btnDel->setStyleSheet("background-color: #141C2E; color: #EF4444; font-weight: bold; padding: 4px 8px; font-size: 11px; border: 1px solid #1E293B;");
        btnDel->setToolTip(QString::fromUtf8("Удалить файл с диска"));
        connect(
            btnDel,
            &QPushButton::clicked,
            this,
            [this, filePath]()
            {
                auto res = QMessageBox::question(this, QString::fromUtf8("Удаление файла"), QString::fromUtf8("Вы уверены, что хотите удалить файл прошивки?\n\n%1").arg(filePath), QMessageBox::Yes | QMessageBox::No);
                if(res == QMessageBox::Yes)
                {
                    QFile::remove(filePath);
                    scanLocalFirmwares();
                    populateFirmwareTable();
                    appendLog(QString::fromUtf8("[УДАЛЕНИЕ] Файл прошивки удален: %1").arg(filePath), "#EF4444");
                }
            });
        aLayout->addWidget(btnDel);

        m_localFirmwareTable->setCellWidget(row, 5, actionWidget);
    }
}

void AppleIpswWidget::setDevice(const AppleDevice &device)
{
    bool changed = (m_device.devId != device.devId || m_device.model != device.model || m_device.mode != device.mode);
    m_device = device;
    updateDeviceCard();
    updateSimpleWizardStep1();

    if(m_simpleStepStack && m_simpleStepStack->currentIndex() == 1)
    {
        updateSimpleWizardStep2();
    }

    if(changed && !m_device.model.isEmpty() && !m_device.model.startsWith("Apple"))
    {
        if(m_comboDeviceModel)
        {
            int idx = m_comboDeviceModel->findData(m_device.model);
            if(idx != -1)
            {
                m_comboDeviceModel->blockSignals(true);
                m_comboDeviceModel->setCurrentIndex(idx);
                m_comboDeviceModel->blockSignals(false);
            }
        }
        m_currentSelectedModelId = m_device.model;
        fetchFirmwareCatalog(m_device.model);
    }
}

void AppleIpswWidget::refreshDevice()
{
    if(MainWindow::current && !MainWindow::current->currentAppleDevice().isEmpty())
    {
        setDevice(MainWindow::current->currentAppleDevice());
    }
    else
    {
        scanDevices();
    }
}

void AppleIpswWidget::scanDevices()
{
    QList<AppleDevice> devs = Apple::getDevices();
    if(!devs.isEmpty())
    {
        setDevice(devs.first());
    }
    else
    {
        if(!m_device.isEmpty())
        {
            m_device = AppleDevice();
            updateDeviceCard();
            updateSimpleWizardStep1();
            appendLog(QString::fromUtf8("[USB] Устройство Apple отключено."), "#EF4444");
        }
    }
}

void AppleIpswWidget::updateDeviceCard()
{
    if(!m_lblDeviceName || !m_lblDeviceMode)
        return;

    if(m_device.isEmpty())
    {
        if(m_lblHeaderDevicePill)
        {
            m_lblHeaderDevicePill->setText(QString::fromUtf8("○ Устройство не подключено"));
            m_lblHeaderDevicePill->setStyleSheet("background: rgba(148, 163, 184, 0.08); color: #94A3B8; border: 1px solid #1E293B; padding: 5px 12px; font-weight: 600; font-size: 11.5px;");
        }

        m_lblDeviceName->setText(QString::fromUtf8("Устройство не обнаружено"));
        m_lblDeviceMode->setText(QString::fromUtf8("● Ожидание подключения USB"));
        m_lblDeviceMode->setStyleSheet("font-size: 11.5px; color: #EAB308; font-weight: 600; padding: 4px 10px; background: rgba(234, 179, 8, 0.1); border: 1px solid #854D0E;");
        if(m_lblIosVersion)
            m_lblIosVersion->setText("—");
        if(m_lblSerial)
            m_lblSerial->setText("—");
        if(m_lblEcid)
            m_lblEcid->setText("—");
        if(m_lblUdid)
            m_lblUdid->setText("—");

        if(m_btnExitRecovery)
            m_btnExitRecovery->setEnabled(false);
        if(m_btnEnterRecovery)
            m_btnEnterRecovery->setEnabled(false);
        if(m_btnReboot)
            m_btnReboot->setEnabled(false);
        return;
    }

    QString name = !m_device.displayName.isEmpty() ? m_device.displayName : (!m_device.marketingName.isEmpty() ? m_device.marketingName : m_device.model);
    m_lblDeviceName->setText(name);
    m_lblDeviceMode->setText(QString("● %1").arg(m_device.modeString()));

    if(m_lblHeaderDevicePill)
    {
        m_lblHeaderDevicePill->setText(QString("● %1 (%2)").arg(name, m_device.modeString()));
        m_lblHeaderDevicePill->setStyleSheet("background: rgba(16, 185, 129, 0.15); color: #34D399; border: 1px solid #059669; padding: 5px 12px; font-weight: 600; font-size: 11.5px;");
    }

    if(m_device.mode == AppleDeviceMode::Normal)
    {
        m_lblDeviceMode->setStyleSheet("font-size: 11.5px; color: #10B981; font-weight: 600; padding: 4px 10px; background: rgba(16, 185, 129, 0.1); border: 1px solid #059669;");
        if(m_btnExitRecovery)
            m_btnExitRecovery->setEnabled(false);
        if(m_btnEnterRecovery)
            m_btnEnterRecovery->setEnabled(true);
        if(m_btnReboot)
            m_btnReboot->setEnabled(true);
    }
    else
    {
        m_lblDeviceMode->setStyleSheet("font-size: 11.5px; color: #38BDF8; font-weight: 600; padding: 4px 10px; background: rgba(56, 189, 248, 0.1); border: 1px solid #0284C7;");
        if(m_btnExitRecovery)
            m_btnExitRecovery->setEnabled(true);
        if(m_btnEnterRecovery)
            m_btnEnterRecovery->setEnabled(false);
        if(m_btnReboot)
            m_btnReboot->setEnabled(true);
    }

    if(m_lblIosVersion)
        m_lblIosVersion->setText(!m_device.productVersion.isEmpty() ? m_device.productVersion : QString::fromUtf8("В Recovery/DFU"));
    if(m_lblSerial)
        m_lblSerial->setText(!m_device.serialNumber.isEmpty() ? m_device.serialNumber : "—");
    if(m_lblEcid)
        m_lblEcid->setText(!m_device.ecid.isEmpty() ? m_device.ecid : "—");
    if(m_lblUdid)
        m_lblUdid->setText(!m_device.devId.isEmpty() ? (m_device.devId.left(14) + "...") : "—");
}

void AppleIpswWidget::fetchFirmwareCatalog(const QString &modelId)
{
    if(modelId.isEmpty())
        return;

    appendLog(QString::fromUtf8("[API] Запрос каталога прошивок Apple для %1...").arg(modelId), "#38BDF8");

    QUrl url(QString("https://api.ipsw.me/v4/device/%1?type=ipsw").arg(modelId));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "AdsKiller-AppleFlasher/2.0");

    QNetworkReply *reply = m_netManager->get(req);
    connect(
        reply,
        &QNetworkReply::finished,
        this,
        [this, reply, modelId]()
        {
            reply->deleteLater();
            if(reply->error() != QNetworkReply::NoError)
            {
                appendLog(QString::fromUtf8("[API ОШИБКА] Не удалось загрузить каталог: %1").arg(reply->errorString()), "#EF4444");
                return;
            }

            QByteArray data = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if(!doc.isObject())
                return;

            QJsonObject root = doc.object();
            QJsonArray firmwares = root["firmwares"].toArray();

            m_firmwares.clear();
            for(const QJsonValue &val : firmwares)
            {
                QJsonObject fObj = val.toObject();
                IpswFirmwareInfo info;
                info.identifier = fObj["identifier"].toString();
                info.version = fObj["version"].toString();
                info.buildid = fObj["buildid"].toString();
                info.sha1sum = fObj["sha1sum"].toString().trimmed().toLower();
                info.md5sum = fObj["md5sum"].toString().trimmed().toLower();
                info.size = fObj["size"].toVariant().toLongLong();
                info.url = fObj["url"].toString();
                info.isSigned = fObj["signed"].toBool();
                info.releaseDate = fObj["releasedate"].toString().left(10);
                m_firmwares.append(info);
            }

            populateFirmwareTable();
            updateSimpleWizardStep2();
            appendLog(QString::fromUtf8("[API] Каталог Apple обновлен: %1 версий iOS для %2").arg(m_firmwares.size()).arg(modelId), "#10B981");
        });
}

void AppleIpswWidget::populateFirmwareTable()
{
    if(!m_firmwareTable)
        return;
    m_firmwareTable->setRowCount(0);

    for(int i = 0; i < m_firmwares.size(); ++i)
    {
        const auto &fw = m_firmwares[i];
        int row = m_firmwareTable->rowCount();
        m_firmwareTable->insertRow(row);

        // 0: Version
        QTableWidgetItem *verItem = new QTableWidgetItem(QString("iOS %1").arg(fw.version));
        verItem->setFont(QFont("Segoe UI", 9, QFont::Bold));
        m_firmwareTable->setItem(row, 0, verItem);

        // 1: Build
        QTableWidgetItem *buildItem = new QTableWidgetItem(fw.buildid);
        buildItem->setForeground(QColor("#94A3B8"));
        m_firmwareTable->setItem(row, 1, buildItem);

        // 2: Release Date
        QTableWidgetItem *dateItem = new QTableWidgetItem(!fw.releaseDate.isEmpty() ? fw.releaseDate : "—");
        dateItem->setForeground(QColor("#64748B"));
        dateItem->setTextAlignment(Qt::AlignCenter);
        m_firmwareTable->setItem(row, 2, dateItem);

        // 3: Signed Status
        QTableWidgetItem *signItem = new QTableWidgetItem(fw.isSigned ? QString::fromUtf8("🟢 Подписывается Apple (Готова к установке)") : QString::fromUtf8("🔴 Не подписывается (Отказ TSS)"));
        signItem->setFont(QFont("Segoe UI", 9, QFont::Bold));
        signItem->setForeground(fw.isSigned ? QColor("#34D399") : QColor("#F87171"));
        m_firmwareTable->setItem(row, 3, signItem);

        // 4: Size
        QTableWidgetItem *sizeItem = new QTableWidgetItem(formatFileSize(fw.size));
        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_firmwareTable->setItem(row, 4, sizeItem);

        // 5: Action Button
        QString expectedFileName = QString("%1_%2_%3_Restore.ipsw").arg(fw.identifier, fw.version, fw.buildid);
        QString expectedPath = downloadDirectory() + "/" + expectedFileName;
        bool alreadyDownloaded = QFile::exists(expectedPath);

        QWidget *actionWidget = new QWidget(m_firmwareTable);
        QHBoxLayout *actLayout = new QHBoxLayout(actionWidget);
        actLayout->setContentsMargins(4, 2, 4, 2);
        actLayout->setSpacing(6);

        if(alreadyDownloaded)
        {
            QLabel *lblReady = new QLabel(QString::fromUtf8("✔ На диске"), actionWidget);
            lblReady->setStyleSheet("color: #34D399; font-weight: bold; font-size: 11px;");
            actLayout->addWidget(lblReady);

            QPushButton *btnUse = new QPushButton(QString::fromUtf8("⚡ Выбрать"), actionWidget);
            btnUse->setStyleSheet("background-color: #0284C7; color: #FFFFFF; font-weight: bold; padding: 4px 8px; font-size: 11px; border: 1px solid #38BDF8;");
            connect(btnUse, &QPushButton::clicked, this, [this, i]() { onFirmwareSelected(i, 0); });
            actLayout->addWidget(btnUse);
        }
        else
        {
            QPushButton *btnAction = new QPushButton(fw.isSigned ? QString::fromUtf8("⬇ Предзагрузить") : QString::fromUtf8("⬇ Скачать"), actionWidget);
            btnAction->setStyleSheet(fw.isSigned ? "background-color: #064E3B; color: #34D399; font-weight: bold; padding: 4px 8px; font-size: 11px; border: 1px solid #059669;" : "background-color: #141C2E; color: #94A3B8; padding: 4px 8px; font-size: 11px; border: 1px solid #1E293B;");
            connect(btnAction, &QPushButton::clicked, this, [this, i]() { onDownloadIpswClicked(i); });
            actLayout->addWidget(btnAction);
        }

        m_firmwareTable->setCellWidget(row, 5, actionWidget);
    }

    onFilterCatalogChanged();
}

void AppleIpswWidget::onFirmwareSelected(int row, int column)
{
    (void) column;
    if(row < 0 || row >= m_firmwares.size())
        return;

    m_selectedFirmware = m_firmwares[row];
    m_isLocalIpsw = false;

    QString expectedFileName = QString("%1_%2_%3_Restore.ipsw").arg(m_selectedFirmware.identifier, m_selectedFirmware.version, m_selectedFirmware.buildid);
    m_selectedIpswPath = downloadDirectory() + "/" + expectedFileName;

    if(QFile::exists(m_selectedIpswPath))
    {
        m_lblSelectedIpsw->setText(QString::fromUtf8("✔ Выбрана готовая прошивка: <b>iOS %1 (%2)</b> [%3]").arg(m_selectedFirmware.version, m_selectedFirmware.buildid, m_selectedIpswPath));
        m_lblSelectedIpsw->setStyleSheet("color: #34D399; font-size: 11.5px; padding: 4px;");
        startChecksumForFile(m_selectedIpswPath, m_selectedFirmware.sha1sum, m_selectedFirmware.md5sum);
    }
    else
    {
        m_lblSelectedIpsw->setText(QString::fromUtf8("Выбрана прошивка: <b>iOS %1 (%2)</b>. Нажмите «Предзагрузить» для скачивания файла.").arg(m_selectedFirmware.version, m_selectedFirmware.buildid));
        m_lblSelectedIpsw->setStyleSheet("color: #FBBF24; font-size: 11.5px; padding: 4px;");
        m_checksumStatus = ChecksumStatus::NotChecked;
        updateChecksumUi();
    }
}

void AppleIpswWidget::onDownloadIpswClicked(int row)
{
    if(row < 0 || row >= m_firmwares.size())
        return;
    const auto &fw = m_firmwares[row];
    m_selectedFirmware = fw;

    QString expectedFileName = QString("%1_%2_%3_Restore.ipsw").arg(fw.identifier, fw.version, fw.buildid);
    m_selectedIpswPath = downloadDirectory() + "/" + expectedFileName;

    if(!fw.isSigned)
    {
        auto res = QMessageBox::warning(
            this,
            QString::fromUtf8("Внимание: неподписанная прошивка"),
            QString::fromUtf8(
                "Выбранная версия iOS %1 больше не подписывается серверами Apple (TSS).\n"
                "Установка на устройство завершится ошибкой TSS, если у вас нет сохраненных SHSH blobs.\n\n"
                "Всё равно начать скачивание?")
                .arg(fw.version),
            QMessageBox::Yes | QMessageBox::No);
        if(res != QMessageBox::Yes)
            return;
    }

    appendLog(QString::fromUtf8("[СКАЧИВАНИЕ] Предварительная загрузка IPSW: %1").arg(fw.url), "#38BDF8");
    m_lblProgressStatus->setText(QString::fromUtf8("Скачивание iOS %1...").arg(fw.version));
    m_progressBar->setValue(0);
    m_btnCancelFlash->setEnabled(true);

    m_downloader->startDownload(fw.url, m_selectedIpswPath);
}

void AppleIpswWidget::onDownloadProgress(qint64 received, qint64 total, double speedMBs)
{
    if(total > 0)
    {
        int pct = static_cast<int>((received * 100) / total);
        if(m_progressBar)
            m_progressBar->setValue(pct);
        if(m_simpleProgressBar)
            m_simpleProgressBar->setValue(pct);
        QString status = QString::fromUtf8("Скачивание: %1 из %2 (%3%) • Скорость: %4 МБ/с").arg(formatFileSize(received), formatFileSize(total)).arg(pct).arg(speedMBs, 0, 'f', 1);
        if(m_lblProgressStatus)
            m_lblProgressStatus->setText(status);
        if(m_lblSimpleProgressStatus)
            m_lblSimpleProgressStatus->setText(status);
    }
}

void AppleIpswWidget::onDownloadFinished(bool success, const QString &path, const QString &errorStr)
{
    if(m_btnCancelFlash)
        m_btnCancelFlash->setEnabled(false);
    if(m_btnSimpleCancel)
        m_btnSimpleCancel->setEnabled(false);

    if(success)
    {
        if(m_progressBar)
            m_progressBar->setValue(100);
        if(m_simpleProgressBar)
            m_simpleProgressBar->setValue(100);
        if(m_lblProgressStatus)
            m_lblProgressStatus->setText(QString::fromUtf8("Скачивание завершено! Выполняется проверка целостности SHA-1..."));
        if(m_lblSimpleProgressStatus)
            m_lblSimpleProgressStatus->setText(QString::fromUtf8("Загрузка прошивки завершена. Выполняется проверка контрольной суммы..."));

        if(m_lblSelectedIpsw)
        {
            m_lblSelectedIpsw->setText(QString::fromUtf8("✔ Файл сохранен: %1").arg(path));
            m_lblSelectedIpsw->setStyleSheet("color: #34D399; font-size: 11.5px; padding: 4px;");
        }
        appendLog(QString::fromUtf8("[СКАЧИВАНИЕ УСПЕШНО] Файл сохранен в хранилище: %1").arg(path), "#10B981");
        scanLocalFirmwares();
        populateFirmwareTable();

        // Automatically launch checksum verification on newly downloaded file
        startChecksumForFile(path, m_selectedFirmware.sha1sum, m_selectedFirmware.md5sum);

        if(m_modeStack && m_modeStack->currentIndex() == 0 && m_simpleStepStack && m_simpleStepStack->currentIndex() == 2)
        {
            if(!m_device.isEmpty())
            {
                bool retain = m_rbSimpleRetain ? m_rbSimpleRetain->isChecked() : true;
                if(m_lblSimpleProgressStatus)
                    m_lblSimpleProgressStatus->setText(QString::fromUtf8("Запуск процесса прошивки устройства..."));
                if(m_btnSimpleCancel)
                    m_btnSimpleCancel->setEnabled(true);
                m_restoreWorker->startRestore(path, retain, m_device.devId);
            }
            else
            {
                if(m_lblSimpleProgressStatus)
                    m_lblSimpleProgressStatus->setText(QString::fromUtf8("Прошивка загружена. Ожидание подключения iPhone по USB..."));
                if(m_btnSimpleRetry)
                    m_btnSimpleRetry->setVisible(true);
            }
        }
    }
    else
    {
        if(m_lblProgressStatus)
            m_lblProgressStatus->setText(QString::fromUtf8("Ошибка скачивания: %1").arg(errorStr));
        if(m_lblSimpleProgressStatus)
            m_lblSimpleProgressStatus->setText(QString::fromUtf8("Ошибка скачивания: %1").arg(errorStr));
        if(m_btnSimpleRetry)
            m_btnSimpleRetry->setVisible(true);
        if(m_btnAdvRetry)
            m_btnAdvRetry->setVisible(true);
        appendLog(QString::fromUtf8("[СКАЧИВАНИЕ ОШИБКА] %1").arg(errorStr), "#EF4444");
    }
}

void AppleIpswWidget::onSelectLocalIpswClicked()
{
    QString path = QFileDialog::getOpenFileName(this, QString::fromUtf8("Выберите файл прошивки Apple IPSW"), QStandardPaths::writableLocation(QStandardPaths::DownloadLocation), QString::fromUtf8("Apple Firmware (*.ipsw)"));

    if(!path.isEmpty())
    {
        m_selectedIpswPath = path;
        m_isLocalIpsw = true;

        QFileInfo fi(path);
        if(m_lblSelectedIpsw)
        {
            m_lblSelectedIpsw->setText(QString::fromUtf8("✔ Локальный IPSW выбран: <b>%1</b> (%2)").arg(fi.fileName(), formatFileSize(fi.size())));
            m_lblSelectedIpsw->setStyleSheet("color: #34D399; font-size: 11.5px; padding: 4px;");
        }

        appendLog(QString::fromUtf8("[ЛОКАЛЬНЫЙ IPSW] Выбран файл: %1").arg(path), "#38BDF8");

        // Automatically start hash computation on imported local IPSW
        startChecksumForFile(path, m_selectedFirmware.sha1sum, m_selectedFirmware.md5sum);
    }
}

void AppleIpswWidget::onStartFlashClicked()
{
    if(m_device.isEmpty())
    {
        QMessageBox::warning(this, QString::fromUtf8("Устройство не подключено"), QString::fromUtf8("Пожалуйста, подключите устройство Apple через USB в обычном режиме, Recovery или DFU."));
        return;
    }

    if(m_selectedIpswPath.isEmpty() || !QFile::exists(m_selectedIpswPath))
    {
        QMessageBox::warning(this, QString::fromUtf8("Файл прошивки не найден"), QString::fromUtf8("Пожалуйста, скачайте прошивку из таблицы либо укажите локальный файл .ipsw."));
        return;
    }

    if(m_checksumStatus == ChecksumStatus::Calculating)
    {
        QMessageBox::information(this, QString::fromUtf8("Проверка контрольной суммы"), QString::fromUtf8("В данный момент выполняется проверка целостности файла (SHA-1).\nПожалуйста, дождитесь завершения сверки."));
        return;
    }

    if(m_checksumStatus == ChecksumStatus::Mismatch)
    {
        auto res = QMessageBox::critical(
            this,
            QString::fromUtf8("ВНИМАНИЕ: Несовпадение контрольной суммы!"),
            QString::fromUtf8(
                "ВНИМАНИЕ! Контрольная сумма SHA-1 выбранного файла не совпадает с официальным образом Apple!\n\n"
                "Файл поврежден или изменен. Прошивка битым образом может окирпичить устройство!\n\n"
                "Вы действительно хотите проигнорировать предупреждение и продолжить?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if(res != QMessageBox::Yes)
            return;
    }

    bool retain = m_rbRetainData ? m_rbRetainData->isChecked() : true;
    QString confirmText = retain ? QString::fromUtf8("Начать ОБНОВЛЕНИЕ устройства %1 с сохранением данных?\n\nФайл: %2").arg(!m_device.displayName.isEmpty() ? m_device.displayName : m_device.marketingName, QFileInfo(m_selectedIpswPath).fileName())
                                 : QString::fromUtf8(
                                       "ВНИМАНИЕ! Выбран режим ЧИСТОЙ ПРОШИВКИ СО СБРОСОМ.\n\n"
                                       "Все данные пользователя на устройстве %1 будут ПОЛНОСТЬЮ СТЕРТЫ!\n\n"
                                       "Продолжить?")
                                       .arg(!m_device.displayName.isEmpty() ? m_device.displayName : m_device.marketingName);

    auto reply = QMessageBox::question(this, QString::fromUtf8("Подтверждение прошивки iOS"), confirmText, QMessageBox::Yes | QMessageBox::No);

    if(reply != QMessageBox::Yes)
        return;

    if(m_btnStartFlash)
        m_btnStartFlash->setEnabled(false);
    if(m_btnCancelFlash)
        m_btnCancelFlash->setEnabled(true);
    if(m_btnAdvRetry)
        m_btnAdvRetry->setVisible(false);
    if(m_progressBar)
        m_progressBar->setValue(0);

    appendLog(QString::fromUtf8("=================================================="), "#475569");
    appendLog(QString::fromUtf8("[СТАРТ] Запуск процесса прошивки %1...").arg(!m_device.displayName.isEmpty() ? m_device.displayName : m_device.marketingName), "#38BDF8");
    appendLog(QString::fromUtf8("[РЕЖИМ] %1").arg(retain ? "Обновление с сохранением данных (-u)" : "Полная чистая прошивка со сбросом (-e)"), "#38BDF8");
    appendLog(QString::fromUtf8("[ФАЙЛ] %1").arg(m_selectedIpswPath), "#38BDF8");

    m_restoreWorker->startRestore(m_selectedIpswPath, retain, m_device.devId);
}

void AppleIpswWidget::onCancelFlashClicked()
{
    auto reply = QMessageBox::question(this, QString::fromUtf8("Отмена операции"), QString::fromUtf8("Вы уверены, что хотите прервать текущую операцию?"), QMessageBox::Yes | QMessageBox::No);

    if(reply == QMessageBox::Yes)
    {
        m_downloader->cancelDownload();
        m_restoreWorker->abortRestore();
        if(m_checksumWorker && m_checksumWorker->isRunning())
        {
            m_checksumWorker->cancel();
        }

        if(m_btnCancelFlash)
            m_btnCancelFlash->setEnabled(false);
        if(m_btnStartFlash)
            m_btnStartFlash->setEnabled(true);
        if(m_btnSimpleCancel)
            m_btnSimpleCancel->setEnabled(false);
        if(m_btnSimpleRetry)
            m_btnSimpleRetry->setVisible(true);
        if(m_btnAdvRetry)
            m_btnAdvRetry->setVisible(true);
        if(m_lblProgressStatus)
            m_lblProgressStatus->setText(QString::fromUtf8("Операция прервана."));
        if(m_lblSimpleProgressStatus)
            m_lblSimpleProgressStatus->setText(QString::fromUtf8("Операция прервана."));
        appendLog(QString::fromUtf8("[ПРЕРВАНО] Операция отменена пользователем."), "#EF4444");
    }
}

void AppleIpswWidget::onRestoreProgress(int percent, const QString &stageText)
{
    if(m_progressBar)
        m_progressBar->setValue(percent);
    if(m_simpleProgressBar)
        m_simpleProgressBar->setValue(percent);
    QString text = QString("%1% • %2").arg(percent).arg(stageText);
    if(m_lblProgressStatus)
        m_lblProgressStatus->setText(text);
    if(m_lblSimpleProgressStatus)
        m_lblSimpleProgressStatus->setText(text);
}

void AppleIpswWidget::onRestoreLog(const QString &line)
{
    QString color = "#E2E8F0";
    if(line.contains("ERROR", Qt::CaseInsensitive) || line.contains("failed", Qt::CaseInsensitive))
        color = "#EF4444";
    else if(line.contains("SUCCESS", Qt::CaseInsensitive) || line.contains("complete", Qt::CaseInsensitive))
        color = "#10B981";
    else if(line.contains("Extracting", Qt::CaseInsensitive) || line.contains("TSS", Qt::CaseInsensitive) || line.contains("Sending", Qt::CaseInsensitive))
        color = "#38BDF8";

    appendLog(line, color);
}

void AppleIpswWidget::onRestoreFinished(bool success, const QString &errorStr)
{
    if(m_btnStartFlash)
        m_btnStartFlash->setEnabled(true);
    if(m_btnCancelFlash)
        m_btnCancelFlash->setEnabled(false);
    if(m_btnSimpleCancel)
        m_btnSimpleCancel->setEnabled(false);
    if(m_btnSimpleRetry)
        m_btnSimpleRetry->setVisible(true);
    if(m_btnAdvRetry)
        m_btnAdvRetry->setVisible(true);

    if(success)
    {
        if(m_progressBar)
            m_progressBar->setValue(100);
        if(m_simpleProgressBar)
            m_simpleProgressBar->setValue(100);
        if(m_lblProgressStatus)
            m_lblProgressStatus->setText(QString::fromUtf8("Прошивка успешно завершена!"));
        if(m_lblSimpleProgressStatus)
            m_lblSimpleProgressStatus->setText(QString::fromUtf8("✔ Прошивка успешно завершена!"));
        appendLog(QString::fromUtf8("[УСПЕХ] Устройство успешно прошито и перезагружается!"), "#10B981");
        QMessageBox::information(this, QString::fromUtf8("Успех"), QString::fromUtf8("Устройство Apple успешно прошито!\nДождитесь появления логотипа Apple и полосы прогресса на экране телефона."));
    }
    else
    {
        if(m_lblProgressStatus)
            m_lblProgressStatus->setText(QString::fromUtf8("Ошибка прошивки: %1").arg(errorStr));
        if(m_lblSimpleProgressStatus)
            m_lblSimpleProgressStatus->setText(QString::fromUtf8("Ошибка прошивки: %1").arg(errorStr));
        appendLog(QString::fromUtf8("[ОШИБКА] %1").arg(errorStr), "#EF4444");
        QMessageBox::critical(this, QString::fromUtf8("Ошибка прошивки"), QString::fromUtf8("Процесс прошивки завершился ошибкой:\n%1\n\nВы можете повторить попытку нажав «Повторить».").arg(errorStr));
    }
}

void AppleIpswWidget::onExitRecoveryClicked()
{
    appendLog(QString::fromUtf8("[КОМАНДА] Выход из Recovery Mode (AppleShell)..."), "#38BDF8");
    AppleShell shell(m_device.devId);
    if(shell.exitRecovery())
    {
        appendLog(QString::fromUtf8("[КОМАНДА] Команда отправлена. Устройство перезагружается в обычный режим."), "#10B981");
    }
    else
    {
        appendLog(QString::fromUtf8("[ОШИБКА] Не удалось выполнить выход из Recovery"), "#EF4444");
    }
    QTimer::singleShot(2000, this, &AppleIpswWidget::scanDevices);
}

void AppleIpswWidget::onEnterRecoveryClicked()
{
    if(m_device.devId.isEmpty())
        return;
    appendLog(QString::fromUtf8("[КОМАНДА] Ввод устройства в Recovery Mode (AppleShell)..."), "#38BDF8");
    AppleShell shell(m_device.devId);
    if(shell.enterRecovery())
    {
        appendLog(QString::fromUtf8("[КОМАНДА] Устройство переводится в Recovery Mode."), "#10B981");
    }
    else
    {
        appendLog(QString::fromUtf8("[ОШИБКА] Не удалось выполнить команду ideviceenterrecovery"), "#EF4444");
    }
    QTimer::singleShot(3000, this, &AppleIpswWidget::scanDevices);
}

void AppleIpswWidget::onRebootClicked()
{
    appendLog(QString::fromUtf8("[КОМАНДА] Перезагрузка устройства (AppleShell)..."), "#38BDF8");
    AppleShell shell(m_device.devId);
    if(shell.restartDevice())
    {
        appendLog(QString::fromUtf8("[КОМАНДА] Команда перезагрузки отправлена."), "#10B981");
    }
    else
    {
        appendLog(QString::fromUtf8("[ОШИБКА] Не удалось перезагрузить устройство"), "#EF4444");
    }
    QTimer::singleShot(3000, this, &AppleIpswWidget::scanDevices);
}

void AppleIpswWidget::resetSession()
{
    if(m_downloader)
        m_downloader->cancelDownload();
    if(m_restoreWorker)
        m_restoreWorker->abortRestore();
    if(m_checksumWorker && m_checksumWorker->isRunning())
        m_checksumWorker->cancel();

    if(m_btnCancelFlash)
        m_btnCancelFlash->setEnabled(false);
    if(m_btnStartFlash)
        m_btnStartFlash->setEnabled(true);
    if(m_btnSimpleCancel)
        m_btnSimpleCancel->setEnabled(false);

    if(m_progressBar)
        m_progressBar->setValue(0);
    if(m_simpleProgressBar)
        m_simpleProgressBar->setValue(0);
    if(m_lblProgressStatus)
        m_lblProgressStatus->setText(QString::fromUtf8("Готов к прошивке"));
    if(m_lblSimpleProgressStatus)
        m_lblSimpleProgressStatus->setText(QString::fromUtf8("Готов к прошивке"));
}

void AppleIpswWidget::showBetaDisclaimer()
{
    QDialog dlg(this);
    dlg.setWindowTitle(QString::fromUtf8("Предупреждение: Модуль Apple (BETA)"));
    dlg.setModal(true);
    dlg.setMinimumWidth(560);
    dlg.setStyleSheet(QStringLiteral(
        "QDialog {"
        "    background-color: #0B1120;"
        "    border: 1.5px solid #F59E0B;"
        "    color: #F8FAFC;"
        "    font-family: 'Segoe UI', -apple-system, sans-serif;"
        "}"
        "QLabel {"
        "    background: transparent;"
        "    color: #CBD5E1;"
        "}"
        "QPushButton#btnAckDisclaimer {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #D97706, stop:1 #B45309);"
        "    color: #FFFFFF;"
        "    border: 1px solid #F59E0B;"
        "    border-radius: 0px;"
        "    font-size: 12px;"
        "    font-weight: bold;"
        "    padding: 8px 24px;"
        "    min-height: 28px;"
        "}"
        "QPushButton#btnAckDisclaimer:hover {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #F59E0B, stop:1 #D97706);"
        "    border-color: #FDE047;"
        "}"
        "QPushButton#btnAckDisclaimer:pressed {"
        "    background-color: #92400E;"
        "}"));

    QVBoxLayout *layout = new QVBoxLayout(&dlg);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(16);

    // Header with warning icon and title
    QHBoxLayout *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(12);

    QLabel *iconLbl = new QLabel(&dlg);
    iconLbl->setPixmap(QIcon(":/svg/alert-triangle").pixmap(36, 36));
    iconLbl->setFixedSize(36, 36);
    headerLayout->addWidget(iconLbl);

    QVBoxLayout *headerTextLayout = new QVBoxLayout();
    headerTextLayout->setSpacing(2);

    QLabel *titleLbl = new QLabel(QString::fromUtf8("Внимание: Экспериментальная функция (BETA)"), &dlg);
    titleLbl->setStyleSheet("font-size: 15px; font-weight: 800; color: #FBBF24;");
    headerTextLayout->addWidget(titleLbl);

    QLabel *subTitleLbl = new QLabel(QString::fromUtf8("Модуль восстановления и прошивки Apple iOS устройств"), &dlg);
    subTitleLbl->setStyleSheet("font-size: 11px; color: #94A3B8; font-weight: 500;");
    headerTextLayout->addWidget(subTitleLbl);

    headerLayout->addLayout(headerTextLayout);
    headerLayout->addStretch(1);
    layout->addLayout(headerLayout);

    // Divider
    QFrame *div = new QFrame(&dlg);
    div->setFrameShape(QFrame::HLine);
    div->setStyleSheet("background-color: rgba(245, 158, 11, 0.3); max-height: 1px; border: none;");
    layout->addWidget(div);

    // Content text with rich styling
    QLabel *bodyLbl = new QLabel(&dlg);
    bodyLbl->setWordWrap(true);
    bodyLbl->setStyleSheet("font-size: 12px; line-height: 1.5; color: #E2E8F0;");
    bodyLbl->setText(
        QString::fromUtf8(
            "<div style='line-height: 150%;'>"
            "<p style='margin-top: 0px;'>Данный сервис находится в статусе <b>активного бета-тестирования</b> (экспериментальный модуль).</p>"
            "<p>⚠️ <b>Все действия вы принимаете исключительно на свой страх и риск!</b></p>"
            "<p>Программа полноценно поддерживает <b>скачивание официальных прошивок Apple IPSW</b> напрямую с серверов Apple и <b>проверку контрольных сумм (хешей SHA1 / MD5)</b>. "
            "Однако для непосредственной прошивки устройства разработчик настоятельно рекомендует использовать проверенное ПО — <b>3uTools</b> или официальный <b>Apple iTunes / Finder</b>.</p>"
            "<p style='color: #CBD5E1; background: rgba(15, 23, 42, 0.7); border-left: 3px solid #38BDF8; padding: 8px 10px; margin: 10px 0;'>"
            "ℹ️ <i>В ближайшее время модуль будет полностью финализирован. В настоящий момент у разработчика нет под рукой физического устройства iPhone/iPad для аппаратного тестирования каждого сценария. "
            "Вы можете протестировать функционал и при желании сообщить разработчику о результатах.</i></p>"
            "<p style='margin-bottom: 0px;'>Спасибо за понимание и содействие развитию проекта!</p>"
            "</div>"));
    layout->addWidget(bodyLbl);

    // Bottom action button
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch(1);

    QPushButton *btnOk = new QPushButton(QString::fromUtf8("Я понимаю риски, продолжить"), &dlg);
    btnOk->setObjectName("btnAckDisclaimer");
    btnOk->setCursor(Qt::PointingHandCursor);
    connect(btnOk, &QPushButton::clicked, &dlg, &QDialog::accept);
    btnLayout->addWidget(btnOk);

    layout->addLayout(btnLayout);

    dlg.exec();
}

// -------------------------------------------------------------
// AppleIpswService Implementation
// -------------------------------------------------------------
AppleIpswService::AppleIpswService(QObject *parent) : Service(DeviceConnectType::Apple, parent)
{
}

AppleIpswService::~AppleIpswService()
{
    stop();
}

QString AppleIpswService::uuid() const
{
    return IDServiceAppleIpswString;
}

PageIndex AppleIpswService::targetPage()
{
    return AppleIpswPage;
}

QString AppleIpswService::widgetIconName()
{
    return "apple";
}

bool AppleIpswService::canStart()
{
    return true;
}

bool AppleIpswService::isStarted()
{
    return m_started;
}

bool AppleIpswService::isFinish()
{
    return m_finished;
}

bool AppleIpswService::start()
{
    sendCheckPull();
    m_started = true;
    m_finished = false;

    if(MainWindow::current)
    {
        auto *widget = static_cast<AppleIpswWidget *>(MainWindow::current->pageWidget(AppleIpswPage));
        if(widget)
        {
            widget->resetSession();
            if(!mAppleDevice.isEmpty())
                widget->setDevice(mAppleDevice);
            widget->refreshDevice();
        }
    }

    return true;
}

void AppleIpswService::stop()
{
    m_started = false;
    m_finished = true;

    if(MainWindow::current)
    {
        auto *widget = static_cast<AppleIpswWidget *>(MainWindow::current->pageWidget(AppleIpswPage));
        if(widget)
            widget->resetSession();
    }
}
