#include "AppleIpswWidget.h"
#include "Services.h"
#include "MainWindow.h"

#include <QApplication>
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

// Helper to map Apple ProductType to human-friendly name
static QString getAppleMarketingName(const QString &prodType)
{
    return Apple::marketingNameForModel(prodType);
}

static QString formatFileSize(qint64 bytes)
{
    if(bytes <= 0) return QString("0 Б");
    double gb = bytes / (1024.0 * 1024.0 * 1024.0);
    if(gb >= 1.0)
    {
        return QString("%1 ГБ").arg(gb, 0, 'f', 2);
    }
    double mb = bytes / (1024.0 * 1024.0);
    return QString("%1 МБ").arg(mb, 0, 'f', 1);
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
        emit downloadFinished(false, destPath, QString("Не удалось открыть файл для записи: %1").arg(m_outputFile->errorString()));
        delete m_outputFile;
        m_outputFile = nullptr;
        return;
    }

    QUrl url(urlStr);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "AdsKiller-AppleFlasher/1.0");

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
    if(dt >= 500)
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
    if(!m_reply) return;

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

    // Check binaries across appDir/apple, appDir/bin/apple, and system path
    QString prog = AppleExecutableFilename("idevicerestore");

    emit progressChanged(2, QString::fromUtf8("Запуск процесса прошивки (%1)...").arg(retainUserData ? "Update" : "Clean Erase"));
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
    if(!m_process) return;
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
    if(line.isEmpty()) return;

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
        emit progressChanged(48, QString::fromUtf8("Загрузка Restore Ramdisk в память устройства..."));
    }
    else if(line.contains("DeviceTree", Qt::CaseInsensitive) || line.contains("KernelCache", Qt::CaseInsensitive))
    {
        emit progressChanged(55, QString::fromUtf8("Отправка ядра KernelCache..."));
    }
    else if(line.contains("Restoring image", Qt::CaseInsensitive) || line.contains("Writing", Qt::CaseInsensitive))
    {
        // Check for percentage e.g. [ 45% ]
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
    (void)exitStatus;
    bool ok = (exitCode == 0) && !m_aborted;
    QString msg;
    if(m_aborted)
    {
        msg = QString::fromUtf8("Операция была отменена пользователем.");
    }
    else if(!ok)
    {
        msg = QString::fromUtf8("Процесс прошивки завершился с кодом ошибки %1. Проверьте лог.").arg(exitCode);
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

    connect(m_downloader, &IpswDownloadWorker::downloadProgress, this, &AppleIpswWidget::onDownloadProgress);
    connect(m_downloader, &IpswDownloadWorker::downloadFinished, this, &AppleIpswWidget::onDownloadFinished);
    connect(m_restoreWorker, &AppleRestoreWorker::progressChanged, this, &AppleIpswWidget::onRestoreProgress);
    connect(m_restoreWorker, &AppleRestoreWorker::logReceived, this, &AppleIpswWidget::onRestoreLog);
    connect(m_restoreWorker, &AppleRestoreWorker::restoreFinished, this, &AppleIpswWidget::onRestoreFinished);

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
    // Main dark obsidian styling
    setStyleSheet(
        "QWidget { background-color: #121214; color: #FFFFFF; font-family: 'Segoe UI', sans-serif; font-size: 13px; }"
        "QSplitter::handle { background-color: #27272A; width: 2px; }"
        "QTabWidget::pane { border: 1px solid #27272A; background-color: #18181B; border-radius: 0px; }"
        "QTabBar::tab { background-color: #141416; color: #A1A1AA; padding: 8px 16px; border: 1px solid #27272A; border-bottom: none; font-weight: 600; font-size: 12px; }"
        "QTabBar::tab:selected { background-color: #18181B; color: #00E5FF; border-top: 2px solid #00E5FF; }"
        "QTabBar::tab:hover:!selected { background-color: #202024; color: #FFFFFF; }"
        "QComboBox { background-color: #202024; border: 1px solid #27272A; color: #FFFFFF; padding: 6px 10px; font-size: 12px; font-weight: 600; border-radius: 0px; min-height: 20px; }"
        "QComboBox:hover { border-color: #00E5FF; }"
        "QComboBox::drop-down { border: none; width: 24px; background: #27272A; }"
        "QComboBox QAbstractItemView { background-color: #18181B; border: 1px solid #27272A; color: #FFFFFF; selection-background-color: #0284C7; selection-color: #FFFFFF; outline: none; }"
        "QTableWidget { background-color: #18181B; border: 1px solid #27272A; gridline-color: #27272A; selection-background-color: #0284C7; selection-color: #FFFFFF; border-radius: 0px; }"
        "QHeaderView::section { background-color: #202024; color: #9E9E9E; padding: 6px; border: 1px solid #27272A; font-weight: bold; border-radius: 0px; }"
        "QPushButton { background-color: #27272A; border: 1px solid #3F3F46; color: #FFFFFF; padding: 6px 14px; border-radius: 0px; font-weight: 600; }"
        "QPushButton:hover { background-color: #3F3F46; border-color: #00E5FF; }"
        "QPushButton:pressed { background-color: #18181B; }"
        "QPushButton:disabled { background-color: #1A1A1D; color: #52525B; border-color: #27272A; }"
        "QRadioButton { spacing: 8px; color: #FFFFFF; font-weight: 500; }"
        "QRadioButton::indicator { width: 14px; height: 14px; border: 2px solid #52525B; background: transparent; border-radius: 7px; }"
        "QRadioButton::indicator:checked { background-color: #00E5FF; border-color: #00E5FF; }"
        "QProgressBar { background-color: #18181B; border: 1px solid #27272A; text-align: center; color: #FFFFFF; font-weight: bold; border-radius: 0px; height: 20px; }"
        "QProgressBar::chunk { background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0078D4, stop:1 #00E5FF); }"
        "QTextEdit { background-color: #09090B; border: 1px solid #27272A; color: #A1A1AA; font-family: 'Consolas', 'Courier New', monospace; font-size: 11px; border-radius: 0px; }"
        "QScrollArea { background: transparent; border: none; }"
        "QScrollArea > QWidget > QWidget { background: transparent; }"
        "QScrollBar:vertical { background-color: #141416; width: 8px; margin: 0px; border: none; }"
        "QScrollBar::handle:vertical { background-color: #3F3F46; min-height: 24px; border-radius: 0px; }"
        "QScrollBar::handle:vertical:hover { background-color: #00E5FF; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; background: none; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }"
    );

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 12, 16, 16);
    mainLayout->setSpacing(12);

    // Top Header Banner with Wizard Mode Switcher
    QWidget *headerWidget = new QWidget(this);
    headerWidget->setStyleSheet("background-color: #18181B; border: 1px solid #27272A; padding: 4px;");
    QHBoxLayout *headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(12, 8, 12, 8);

    QLabel *headerTitle = new QLabel(QString::fromUtf8("<span style='font-size: 15px; font-weight: bold; color: #FFFFFF;'>⚡ Мастер прошивки и восстановления Apple iOS</span>"), this);
    QLabel *headerSubtitle = new QLabel(QString::fromUtf8("<span style='font-size: 11.5px; color: #A1A1AA;'>Поэтапный мастер с подсказками • Автонастройки • Защита от сбоев</span>"), this);

    QVBoxLayout *headerTextLayout = new QVBoxLayout();
    headerTextLayout->addWidget(headerTitle);
    headerTextLayout->addWidget(headerSubtitle);
    headerLayout->addLayout(headerTextLayout);
    headerLayout->addSpacing(16);

    // Segmented Mode Switcher Buttons
    QWidget *modeBtnBox = new QWidget(headerWidget);
    modeBtnBox->setStyleSheet("background: #141416; border: 1px solid #27272A; border-radius: 0px; padding: 2px;");
    QHBoxLayout *modeBtnLayout = new QHBoxLayout(modeBtnBox);
    modeBtnLayout->setContentsMargins(2, 2, 2, 2);
    modeBtnLayout->setSpacing(4);

    m_btnModeSimple = new QPushButton(QString::fromUtf8("⚡ ОБЫЧНЫЙ РЕЖИМ (Мастер)"), modeBtnBox);
    m_btnModeSimple->setObjectName(QStringLiteral("ipswBtnModeSimple"));
    m_btnModeSimple->setCursor(Qt::PointingHandCursor);
    m_btnModeSimple->setAttribute(Qt::WA_Hover, true);
    m_btnModeSimple->setStyleSheet("background-color: #0284C7; color: #FFFFFF; font-weight: bold; border: 1px solid #00E5FF; padding: 6px 14px; font-size: 12px;");
    connect(m_btnModeSimple, &QPushButton::clicked, this, [this]() { onSwitchMode(0); });
    modeBtnLayout->addWidget(m_btnModeSimple);

    m_btnModeAdvanced = new QPushButton(QString::fromUtf8("🛠 РАСШИРЕННЫЙ (Эксперт)"), modeBtnBox);
    m_btnModeAdvanced->setObjectName(QStringLiteral("ipswBtnModeAdvanced"));
    m_btnModeAdvanced->setCursor(Qt::PointingHandCursor);
    m_btnModeAdvanced->setAttribute(Qt::WA_Hover, true);
    m_btnModeAdvanced->setStyleSheet("background-color: #27272A; color: #A1A1AA; font-weight: 600; border: 1px solid #3F3F46; padding: 6px 14px; font-size: 12px;");
    connect(m_btnModeAdvanced, &QPushButton::clicked, this, [this]() { onSwitchMode(1); });
    modeBtnLayout->addWidget(m_btnModeAdvanced);

    headerLayout->addWidget(modeBtnBox);
    headerLayout->addStretch();

    m_btnRefreshDevice = new QPushButton(QString::fromUtf8("🔄 Статус USB"), this);
    m_btnRefreshDevice->setObjectName(QStringLiteral("ipswBtnRefreshDevice"));
    m_btnRefreshDevice->setAttribute(Qt::WA_Hover, true);
    m_btnRefreshDevice->setCursor(Qt::PointingHandCursor);
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
    if(!m_modeStack) return;
    m_modeStack->setCurrentIndex(mode);

    if(mode == 0)
    {
        m_btnModeSimple->setStyleSheet("background-color: #0284C7; color: #FFFFFF; font-weight: bold; border: 1px solid #00E5FF; padding: 6px 14px; font-size: 12px;");
        m_btnModeAdvanced->setStyleSheet("background-color: #27272A; color: #A1A1AA; font-weight: 600; border: 1px solid #3F3F46; padding: 6px 14px; font-size: 12px;");
        updateSimpleWizardStep1();
    }
    else
    {
        m_btnModeAdvanced->setStyleSheet("background-color: #0284C7; color: #FFFFFF; font-weight: bold; border: 1px solid #00E5FF; padding: 6px 14px; font-size: 12px;");
        m_btnModeSimple->setStyleSheet("background-color: #27272A; color: #A1A1AA; font-weight: 600; border: 1px solid #3F3F46; padding: 6px 14px; font-size: 12px;");
        updateDeviceCard();
    }
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

void AppleIpswWidget::setupSimpleWizardPage()
{
    QWidget *simplePage = new QWidget(m_modeStack);
    QVBoxLayout *layout = new QVBoxLayout(simplePage);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    m_lblSimpleWizardStepHeader = new QLabel(simplePage);
    m_lblSimpleWizardStepHeader->setStyleSheet("background-color: #18181B; border: 1px solid #27272A; color: #00E5FF; font-size: 13px; font-weight: bold; padding: 8px 12px;");
    m_lblSimpleWizardStepHeader->setText(QString::fromUtf8("ШАГ 1 ИЗ 3: ПОДКЛЮЧЕНИЕ IPHONE ПО USB"));
    layout->addWidget(m_lblSimpleWizardStepHeader);

    m_simpleStepStack = new QStackedWidget(simplePage);

    // STEP 1: Connect USB Device (Scrollable with Anti-Stretch)
    QWidget *step1 = new QWidget();
    QVBoxLayout *s1Layout = new QVBoxLayout(step1);
    s1Layout->setContentsMargins(12, 8, 12, 16);
    s1Layout->setSpacing(14);

    QWidget *s1Card = new QWidget(step1);
    s1Card->setStyleSheet("background-color: #18181B; border: 1px solid #27272A; padding: 16px;");
    QVBoxLayout *s1CardLayout = new QVBoxLayout(s1Card);
    s1CardLayout->setSpacing(14);

    QLabel *s1Title = new QLabel(QString::fromUtf8("<span style='font-size: 16px; font-weight: bold; color: #FFFFFF;'>📱 Шаг 1: Подключите iPhone к компьютеру</span>"), s1Card);
    QLabel *s1Desc = new QLabel(QString::fromUtf8("Для автоматического подбора официальной прошивки и параметров восстановления подключите устройство через оригинальный кабель USB к компьютеру."), s1Card);
    s1Desc->setStyleSheet("color: #A1A1AA; font-size: 12.5px;");
    s1Desc->setWordWrap(true);

    s1CardLayout->addWidget(s1Title);
    s1CardLayout->addWidget(s1Desc);

    m_lblSimpleUsbStatus = new QLabel(s1Card);
    m_lblSimpleUsbStatus->setWordWrap(true);
    s1CardLayout->addWidget(m_lblSimpleUsbStatus);

    m_lblSimpleDetectedModel = new QLabel(s1Card);
    m_lblSimpleDetectedModel->setWordWrap(true);
    s1CardLayout->addWidget(m_lblSimpleDetectedModel);

    QWidget *hintBox = new QWidget(s1Card);
    hintBox->setStyleSheet("background-color: #141416; border-left: 3px solid #00E5FF; padding: 12px;");
    QVBoxLayout *hintLayout = new QVBoxLayout(hintBox);
    hintLayout->setSpacing(6);

    QLabel *hintTitle = new QLabel(QString::fromUtf8("<b>💡 Подсказки и советы для успешного подключения:</b>"), hintBox);
    hintTitle->setStyleSheet("color: #00E5FF; font-size: 12px;");
    hintLayout->addWidget(hintTitle);

    auto addHint = [hintLayout, hintBox](const QString &text) {
        QLabel *l = new QLabel(text, hintBox);
        l->setStyleSheet("color: #D4D4D8; font-size: 11.5px;");
        l->setWordWrap(true);
        hintLayout->addWidget(l);
    };

    addHint(QString::fromUtf8("• <b>Прямой порт USB:</b> Подключайте кабель напрямую к портам ПК (избегайте USB-хабов)."));
    addHint(QString::fromUtf8("• <b>Доверие компьютеру:</b> Если экран iPhone активен, разблокируйте его и нажмите «Доверять этому компьютеру»."));
    addHint(QString::fromUtf8("• <b>Если телефон завис на логотипе Apple:</b> Переведите его в Recovery Mode (зажмите кнопки громкости и питания до значка кабеля и компьютера)."));
    addHint(QString::fromUtf8("• <b>Оригинальный кабель:</b> Поврежденные или неоригинальные кабели могут обрывать связь при записи NAND."));

    s1CardLayout->addWidget(hintBox);
    s1Layout->addWidget(s1Card);

    QHBoxLayout *s1Bar = new QHBoxLayout();
    s1Bar->addStretch();

    QPushButton *btnManualRefresh = new QPushButton(QString::fromUtf8("🔄 Проверить подключение USB"), step1);
    btnManualRefresh->setObjectName(QStringLiteral("ipswBtnManualRefreshUSB"));
    btnManualRefresh->setAttribute(Qt::WA_Hover, true);
    btnManualRefresh->setCursor(Qt::PointingHandCursor);
    btnManualRefresh->setStyleSheet("background-color: #27272A; border: 1px solid #3F3F46; color: #FFFFFF; padding: 8px 16px; font-weight: 600;");
    connect(btnManualRefresh, &QPushButton::clicked, this, &AppleIpswWidget::scanDevices);
    s1Bar->addWidget(btnManualRefresh);

    m_btnSimpleStep1Next = new QPushButton(QString::fromUtf8("Продолжить (Автонастройки) ➔"), step1);
    m_btnSimpleStep1Next->setObjectName(QStringLiteral("ipswBtnStep1Next"));
    m_btnSimpleStep1Next->setAttribute(Qt::WA_Hover, true);
    m_btnSimpleStep1Next->setCursor(Qt::PointingHandCursor);
    m_btnSimpleStep1Next->setStyleSheet("QPushButton { background-color: #0284C7; border: 1px solid #00E5FF; color: #FFFFFF; padding: 8px 20px; font-weight: bold; font-size: 13px; } QPushButton:disabled { background-color: #1A1A1D; color: #52525B; border-color: #27272A; }");
    m_btnSimpleStep1Next->setEnabled(false);
    connect(m_btnSimpleStep1Next, &QPushButton::clicked, this, &AppleIpswWidget::onSimpleStep1Next);
    s1Bar->addWidget(m_btnSimpleStep1Next);

    s1Layout->addLayout(s1Bar);
    s1Layout->addStretch(1);

    QScrollArea *s1Scroll = createMetroScrollArea(step1, m_simpleStepStack);
    m_simpleStepStack->addWidget(s1Scroll);

    // STEP 2: Auto Settings & Reset Mode Selection (Scrollable with Anti-Stretch)
    QWidget *step2 = new QWidget();
    QVBoxLayout *s2Layout = new QVBoxLayout(step2);
    s2Layout->setContentsMargins(12, 8, 12, 16);
    s2Layout->setSpacing(14);

    QWidget *s2Card = new QWidget(step2);
    s2Card->setStyleSheet("background-color: #18181B; border: 1px solid #27272A; padding: 16px;");
    QVBoxLayout *s2CardLayout = new QVBoxLayout(s2Card);
    s2CardLayout->setSpacing(14);

    QLabel *s2Title = new QLabel(QString::fromUtf8("<span style='font-size: 16px; font-weight: bold; color: #FFFFFF;'>⚙ Шаг 2: Автонастройки и выбор режима сброса</span>"), s2Card);
    s2CardLayout->addWidget(s2Title);

    m_lblSimpleDeviceSummary = new QLabel(s2Card);
    m_lblSimpleDeviceSummary->setStyleSheet("background: #141416; border: 1px solid #27272A; padding: 10px; font-size: 13px; font-weight: bold; color: #FFFFFF;");
    s2CardLayout->addWidget(m_lblSimpleDeviceSummary);

    m_lblSimpleAutoIpswInfo = new QLabel(s2Card);
    m_lblSimpleAutoIpswInfo->setWordWrap(true);
    s2CardLayout->addWidget(m_lblSimpleAutoIpswInfo);

    QLabel *s2ChoiceTitle = new QLabel(QString::fromUtf8("<b>Выберите тип восстановления:</b>"), s2Card);
    s2ChoiceTitle->setStyleSheet("color: #FFFFFF; font-size: 13px;");
    s2CardLayout->addWidget(s2ChoiceTitle);

    QWidget *modeContainer = new QWidget(s2Card);
    modeContainer->setStyleSheet("background-color: #141416; border: 1px solid #27272A; padding: 10px;");
    QVBoxLayout *mContLayout = new QVBoxLayout(modeContainer);
    mContLayout->setSpacing(12);

    m_simpleResetGroup = new QButtonGroup(this);

    QVBoxLayout *opt1Layout = new QVBoxLayout();
    m_rbSimpleRetain = new QRadioButton(QString::fromUtf8("<b>1. Обновление с сохранением данных (Рекомендуется)</b>"), modeContainer);
    m_rbSimpleRetain->setChecked(true);
    m_simpleResetGroup->addButton(m_rbSimpleRetain, 0);
    QLabel *opt1Desc = new QLabel(QString::fromUtf8("Все ваши фото, приложения, контакты и сообщения сохранятся на iPhone. Будет обновлена только операционная система iOS."), modeContainer);
    opt1Desc->setStyleSheet("color: #A1A1AA; font-size: 11.5px; margin-left: 24px;");
    opt1Desc->setWordWrap(true);
    opt1Layout->addWidget(m_rbSimpleRetain);
    opt1Layout->addWidget(opt1Desc);
    mContLayout->addLayout(opt1Layout);

    QVBoxLayout *opt2Layout = new QVBoxLayout();
    m_rbSimpleErase = new QRadioButton(QString::fromUtf8("<b>2. Полная очистка и восстановление (Сброс до заводских настроек)</b>"), modeContainer);
    m_simpleResetGroup->addButton(m_rbSimpleErase, 1);
    QLabel *opt2Desc = new QLabel(QString::fromUtf8("ВНИМАНИЕ! Все данные пользователя на iPhone будут полностью стерты. Рекомендуется, если забыт код-пароль или телефон завис на логотипе Apple."), modeContainer);
    opt2Desc->setStyleSheet("color: #F87171; font-size: 11.5px; margin-left: 24px;");
    opt2Desc->setWordWrap(true);
    opt2Layout->addWidget(m_rbSimpleErase);
    opt2Layout->addWidget(opt2Desc);
    mContLayout->addLayout(opt2Layout);

    s2CardLayout->addWidget(modeContainer);

    QLabel *safetyLbl = new QLabel(QString::fromUtf8("⚠️ <b>Безопасность:</b> Убедитесь, что заряд аккумулятора iPhone не менее 50%, и не отключайте кабель во время прошивки."), s2Card);
    safetyLbl->setStyleSheet("background: rgba(234,179,8,0.1); border: 1px solid #854D0E; color: #FBBF24; padding: 8px; font-size: 11.5px;");
    safetyLbl->setWordWrap(true);
    s2CardLayout->addWidget(safetyLbl);

    s2Layout->addWidget(s2Card);

    QHBoxLayout *s2Bar = new QHBoxLayout();
    m_btnSimpleStep2Back = new QPushButton(QString::fromUtf8("⬅ Назад (к устройству)"), step2);
    m_btnSimpleStep2Back->setObjectName(QStringLiteral("ipswBtnStep2Back"));
    m_btnSimpleStep2Back->setAttribute(Qt::WA_Hover, true);
    m_btnSimpleStep2Back->setCursor(Qt::PointingHandCursor);
    m_btnSimpleStep2Back->setStyleSheet("background-color: #27272A; border: 1px solid #3F3F46; color: #FFFFFF; padding: 8px 16px; font-weight: 600;");
    connect(m_btnSimpleStep2Back, &QPushButton::clicked, this, &AppleIpswWidget::onSimpleStep2Back);
    s2Bar->addWidget(m_btnSimpleStep2Back);

    s2Bar->addStretch();

    m_btnSimpleStep2Start = new QPushButton(QString::fromUtf8("⚡ Начать восстановление ➔"), step2);
    m_btnSimpleStep2Start->setObjectName(QStringLiteral("ipswBtnStep2Start"));
    m_btnSimpleStep2Start->setAttribute(Qt::WA_Hover, true);
    m_btnSimpleStep2Start->setCursor(Qt::PointingHandCursor);
    m_btnSimpleStep2Start->setStyleSheet("QPushButton { background-color: #0284C7; border: 1px solid #00E5FF; color: #FFFFFF; padding: 8px 24px; font-weight: bold; font-size: 13px; } QPushButton:hover { background-color: #0369A1; }");
    connect(m_btnSimpleStep2Start, &QPushButton::clicked, this, &AppleIpswWidget::onSimpleStep2Start);
    s2Bar->addWidget(m_btnSimpleStep2Start);

    s2Layout->addLayout(s2Bar);
    s2Layout->addStretch(1);

    QScrollArea *s2Scroll = createMetroScrollArea(step2, m_simpleStepStack);
    m_simpleStepStack->addWidget(s2Scroll);

    // STEP 3: Progress & Retry (Scrollable with Anti-Stretch)
    QWidget *step3 = new QWidget();
    QVBoxLayout *s3Layout = new QVBoxLayout(step3);
    s3Layout->setContentsMargins(12, 8, 12, 16);
    s3Layout->setSpacing(14);

    QWidget *s3Card = new QWidget(step3);
    s3Card->setStyleSheet("background-color: #18181B; border: 1px solid #27272A; padding: 20px;");
    QVBoxLayout *s3CardLayout = new QVBoxLayout(s3Card);
    s3CardLayout->setSpacing(14);

    QLabel *s3Title = new QLabel(QString::fromUtf8("<span style='font-size: 16px; font-weight: bold; color: #FFFFFF;'>⚡ Шаг 3: Процесс восстановления iPhone</span>"), s3Card);
    s3CardLayout->addWidget(s3Title);

    m_lblSimpleProgressStatus = new QLabel(QString::fromUtf8("Инициализация процесса..."), s3Card);
    m_lblSimpleProgressStatus->setObjectName(QStringLiteral("ipswLblSimpleProgressStatus"));
    m_lblSimpleProgressStatus->setStyleSheet("color: #00E5FF; font-size: 13px; font-weight: bold;");
    s3CardLayout->addWidget(m_lblSimpleProgressStatus);

    m_simpleProgressBar = new QProgressBar(s3Card);
    m_simpleProgressBar->setObjectName(QStringLiteral("ipswSimpleProgressBar"));
    m_simpleProgressBar->setMinimumHeight(24);
    m_simpleProgressBar->setValue(0);
    s3CardLayout->addWidget(m_simpleProgressBar);

    QWidget *stageBox = new QWidget(s3Card);
    stageBox->setStyleSheet("background: #141416; border: 1px solid #27272A; padding: 12px;");
    QVBoxLayout *stageLayout = new QVBoxLayout(stageBox);
    stageLayout->setSpacing(8);

    QLabel *stLbl = new QLabel(QString::fromUtf8("<b>Этапы выполнения:</b>"), stageBox);
    stLbl->setStyleSheet("color: #FFFFFF; font-size: 12px;");
    stageLayout->addWidget(stLbl);

    QLabel *stDesc = new QLabel(QString::fromUtf8(
        "1. Загрузка / проверка официального пакета IPSW<br>"
        "2. Получение билета подписи TSS от серверов Apple<br>"
        "3. Перевод iPhone в Recovery и отправка компонентов загрузчика<br>"
        "4. Запись файловой системы на встроенную память NAND<br>"
        "5. Финализация и безопасная перезагрузка iPhone"
    ), stageBox);
    stDesc->setStyleSheet("color: #71717A; font-size: 11.5px; line-height: 1.4;");
    stageLayout->addWidget(stDesc);
    s3CardLayout->addWidget(stageBox);

    s3Layout->addWidget(s3Card);

    QHBoxLayout *s3Bar = new QHBoxLayout();

    m_btnSimpleCancel = new QPushButton(QString::fromUtf8("⏹ Отмена операции"), step3);
    m_btnSimpleCancel->setObjectName(QStringLiteral("ipswBtnSimpleCancel"));
    m_btnSimpleCancel->setAttribute(Qt::WA_Hover, true);
    m_btnSimpleCancel->setCursor(Qt::PointingHandCursor);
    m_btnSimpleCancel->setStyleSheet("background-color: #991B1B; border: 1px solid #EF4444; color: #FFFFFF; padding: 8px 16px; font-weight: bold;");
    connect(m_btnSimpleCancel, &QPushButton::clicked, this, &AppleIpswWidget::onCancelFlashClicked);
    s3Bar->addWidget(m_btnSimpleCancel);

    s3Bar->addStretch();

    m_btnSimpleRetry = new QPushButton(QString::fromUtf8("🔄 Повторить попытку"), step3);
    m_btnSimpleRetry->setObjectName(QStringLiteral("ipswBtnSimpleRetry"));
    m_btnSimpleRetry->setAttribute(Qt::WA_Hover, true);
    m_btnSimpleRetry->setCursor(Qt::PointingHandCursor);
    m_btnSimpleRetry->setStyleSheet("background-color: #047857; border: 1px solid #10B981; color: #FFFFFF; padding: 8px 24px; font-weight: bold; font-size: 13px;");
    m_btnSimpleRetry->setVisible(false);
    connect(m_btnSimpleRetry, &QPushButton::clicked, this, &AppleIpswWidget::onSimpleRetry);
    s3Bar->addWidget(m_btnSimpleRetry);

    s3Layout->addLayout(s3Bar);
    s3Layout->addStretch(1);

    QScrollArea *s3Scroll = createMetroScrollArea(step3, m_simpleStepStack);
    m_simpleStepStack->addWidget(s3Scroll);

    layout->addWidget(m_simpleStepStack, 1);
    m_modeStack->addWidget(simplePage);

    updateSimpleWizardStep1();
}

void AppleIpswWidget::setupAdvancedWizardPage()
{
    QWidget *advPage = new QWidget(m_modeStack);
    QVBoxLayout *layout = new QVBoxLayout(advPage);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    m_lblAdvStepHeader = new QLabel(advPage);
    m_lblAdvStepHeader->setStyleSheet("background-color: #18181B; border: 1px solid #27272A; color: #00E5FF; font-size: 13px; font-weight: bold; padding: 8px 12px;");
    m_lblAdvStepHeader->setText(QString::fromUtf8("ЭТАП 1 ИЗ 3: КАТАЛОГ ПРОШИВОК И МЕНЕДЖЕР ЗАГРУЗОК"));
    layout->addWidget(m_lblAdvStepHeader);

    m_advancedStepStack = new QStackedWidget(advPage);

    // ADVANCED STEP 1: Catalog & Pre-download Storage Manager (Scrollable with Anti-Stretch)
    QWidget *advStep1 = new QWidget();
    QVBoxLayout *a1Layout = new QVBoxLayout(advStep1);
    a1Layout->setContentsMargins(12, 8, 12, 16);
    a1Layout->setSpacing(10);

    m_tabWidget = new QTabWidget(advStep1);
    m_tabWidget->setMinimumHeight(460);

    // Tab 1: Catalog
    QWidget *tabCatalog = new QWidget(m_tabWidget);
    QVBoxLayout *tcLayout = new QVBoxLayout(tabCatalog);
    tcLayout->setContentsMargins(10, 10, 10, 10);
    tcLayout->setSpacing(8);

    QHBoxLayout *modelRow = new QHBoxLayout();
    QLabel *lblModel = new QLabel(QString::fromUtf8("Модель устройства:"), tabCatalog);
    lblModel->setStyleSheet("font-weight: 600; color: #FFFFFF; font-size: 12px;");
    modelRow->addWidget(lblModel);

    m_comboDeviceModel = new QComboBox(tabCatalog);
    m_comboDeviceModel->setObjectName(QStringLiteral("ipswComboDeviceModel"));
    m_comboDeviceModel->setMinimumWidth(300);
    connect(m_comboDeviceModel, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AppleIpswWidget::onModelSelected);
    modelRow->addWidget(m_comboDeviceModel);

    m_btnRefreshCatalog = new QPushButton(QString::fromUtf8("🔄 Обновить список"), tabCatalog);
    m_btnRefreshCatalog->setObjectName(QStringLiteral("ipswBtnRefreshCatalog"));
    m_btnRefreshCatalog->setAttribute(Qt::WA_Hover, true);
    m_btnRefreshCatalog->setCursor(Qt::PointingHandCursor);
    connect(m_btnRefreshCatalog, &QPushButton::clicked, this, &AppleIpswWidget::onRefreshCatalogClicked);
    modelRow->addWidget(m_btnRefreshCatalog);

    modelRow->addStretch();

    m_btnSelectLocal = new QPushButton(QString::fromUtf8("📂 Выбрать локальный .ipsw..."), tabCatalog);
    m_btnSelectLocal->setObjectName(QStringLiteral("ipswBtnSelectLocal"));
    m_btnSelectLocal->setAttribute(Qt::WA_Hover, true);
    m_btnSelectLocal->setCursor(Qt::PointingHandCursor);
    m_btnSelectLocal->setStyleSheet("QPushButton { background-color: #27272A; border: 1px solid #00E5FF; color: #00E5FF; padding: 6px 14px; font-weight: 600; } QPushButton:hover { background-color: #00E5FF; color: #000000; }");
    connect(m_btnSelectLocal, &QPushButton::clicked, this, &AppleIpswWidget::onSelectLocalIpswClicked);
    modelRow->addWidget(m_btnSelectLocal);

    tcLayout->addLayout(modelRow);

    m_lblSelectedIpsw = new QLabel(QString::fromUtf8("Файл прошивки не выбран. Выберите версию из таблицы ниже или укажите локальный .ipsw"), tabCatalog);
    m_lblSelectedIpsw->setObjectName(QStringLiteral("ipswLblSelectedIpsw"));
    m_lblSelectedIpsw->setStyleSheet("color: #00E5FF; font-size: 11.5px; padding: 4px; background: rgba(0,229,255,0.05); border: 1px dashed #0284C7;");
    tcLayout->addWidget(m_lblSelectedIpsw);

    m_firmwareTable = new QTableWidget(tabCatalog);
    m_firmwareTable->setObjectName(QStringLiteral("ipswFirmwareCatalogTable"));
    m_firmwareTable->setColumnCount(5);
    m_firmwareTable->setHorizontalHeaderLabels({
        QString::fromUtf8("Версия iOS"),
        QString::fromUtf8("Номер сборки"),
        QString::fromUtf8("Статус Apple (TSS)"),
        QString::fromUtf8("Размер"),
        QString::fromUtf8("Действие")
    });
    m_firmwareTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_firmwareTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_firmwareTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_firmwareTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_firmwareTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_firmwareTable->verticalHeader()->setVisible(false);
    m_firmwareTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_firmwareTable->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_firmwareTable, &QTableWidget::cellDoubleClicked, this, &AppleIpswWidget::onFirmwareSelected);
    tcLayout->addWidget(m_firmwareTable);

    m_tabWidget->addTab(tabCatalog, QString::fromUtf8("🌐 Каталог прошивок онлайн"));

    // Tab 2: Storage Manager
    QWidget *tabStorage = new QWidget(m_tabWidget);
    QVBoxLayout *tsLayout = new QVBoxLayout(tabStorage);
    tsLayout->setContentsMargins(10, 10, 10, 10);
    tsLayout->setSpacing(8);

    QWidget *storageBar = new QWidget(tabStorage);
    storageBar->setStyleSheet("background-color: #141416; border: 1px solid #27272A; padding: 6px;");
    QHBoxLayout *sbLayout = new QHBoxLayout(storageBar);
    sbLayout->setContentsMargins(8, 4, 8, 4);

    m_lblStoragePath = new QLabel(storageBar);
    m_lblStoragePath->setObjectName(QStringLiteral("ipswLblStoragePath"));
    m_lblDiskSpace = new QLabel(storageBar);
    m_lblDiskSpace->setObjectName(QStringLiteral("ipswLblDiskSpace"));
    m_lblDiskSpace->setStyleSheet("color: #00E5FF; font-weight: bold;");

    sbLayout->addWidget(m_lblStoragePath);
    sbLayout->addStretch();
    sbLayout->addWidget(m_lblDiskSpace);
    sbLayout->addSpacing(12);

    m_btnOpenFolder = new QPushButton(QString::fromUtf8("📂 Открыть папку"), storageBar);
    m_btnOpenFolder->setObjectName(QStringLiteral("ipswBtnOpenFolder"));
    m_btnOpenFolder->setAttribute(Qt::WA_Hover, true);
    m_btnOpenFolder->setCursor(Qt::PointingHandCursor);
    connect(m_btnOpenFolder, &QPushButton::clicked, this, &AppleIpswWidget::onOpenFolderClicked);
    sbLayout->addWidget(m_btnOpenFolder);

    m_btnChangeFolder = new QPushButton(QString::fromUtf8("Изменить папку..."), storageBar);
    m_btnChangeFolder->setObjectName(QStringLiteral("ipswBtnChangeFolder"));
    m_btnChangeFolder->setAttribute(Qt::WA_Hover, true);
    m_btnChangeFolder->setCursor(Qt::PointingHandCursor);
    connect(m_btnChangeFolder, &QPushButton::clicked, this, &AppleIpswWidget::onChangeFolderClicked);
    sbLayout->addWidget(m_btnChangeFolder);

    m_btnRescanLocal = new QPushButton(QString::fromUtf8("🔄 Обновить"), storageBar);
    m_btnRescanLocal->setObjectName(QStringLiteral("ipswBtnRescanLocal"));
    m_btnRescanLocal->setAttribute(Qt::WA_Hover, true);
    m_btnRescanLocal->setCursor(Qt::PointingHandCursor);
    connect(m_btnRescanLocal, &QPushButton::clicked, this, &AppleIpswWidget::scanLocalFirmwares);
    sbLayout->addWidget(m_btnRescanLocal);

    tsLayout->addWidget(storageBar);

    m_localFirmwareTable = new QTableWidget(tabStorage);
    m_localFirmwareTable->setObjectName(QStringLiteral("ipswLocalFirmwareTable"));
    m_localFirmwareTable->setColumnCount(5);
    m_localFirmwareTable->setHorizontalHeaderLabels({
        QString::fromUtf8("Имя файла"),
        QString::fromUtf8("Модель устройства"),
        QString::fromUtf8("Размер"),
        QString::fromUtf8("Дата скачивания"),
        QString::fromUtf8("Действие")
    });
    m_localFirmwareTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_localFirmwareTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_localFirmwareTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_localFirmwareTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_localFirmwareTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_localFirmwareTable->verticalHeader()->setVisible(false);
    m_localFirmwareTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_localFirmwareTable->setSelectionMode(QAbstractItemView::SingleSelection);
    tsLayout->addWidget(m_localFirmwareTable);

    m_tabWidget->addTab(tabStorage, QString::fromUtf8("💾 Менеджер хранилища и Предзагруженные прошивки"));

    a1Layout->addWidget(m_tabWidget);

    QHBoxLayout *a1Bar = new QHBoxLayout();
    a1Bar->addStretch();
    m_btnAdvStep1Next = new QPushButton(QString::fromUtf8("Далее: Режим сброса и настройки ➔"), advStep1);
    m_btnAdvStep1Next->setObjectName(QStringLiteral("ipswBtnAdvStep1Next"));
    m_btnAdvStep1Next->setAttribute(Qt::WA_Hover, true);
    m_btnAdvStep1Next->setCursor(Qt::PointingHandCursor);
    m_btnAdvStep1Next->setStyleSheet("QPushButton { background-color: #0284C7; border: 1px solid #00E5FF; color: #FFFFFF; padding: 8px 20px; font-weight: bold; font-size: 13px; } QPushButton:hover { background-color: #0369A1; }");
    connect(m_btnAdvStep1Next, &QPushButton::clicked, this, &AppleIpswWidget::onAdvStep1Next);
    a1Bar->addWidget(m_btnAdvStep1Next);

    a1Layout->addLayout(a1Bar);
    a1Layout->addStretch(1);

    QScrollArea *a1Scroll = createMetroScrollArea(advStep1, m_advancedStepStack);
    m_advancedStepStack->addWidget(a1Scroll);

    // ADVANCED STEP 2: Fine-tuning reset mode & parameters (Scrollable with Anti-Stretch)
    QWidget *advStep2 = new QWidget();
    QVBoxLayout *a2Layout = new QVBoxLayout(advStep2);
    a2Layout->setContentsMargins(12, 8, 12, 16);
    a2Layout->setSpacing(14);

    QWidget *a2Card = new QWidget(advStep2);
    a2Card->setStyleSheet("background-color: #18181B; border: 1px solid #27272A; padding: 16px;");
    QVBoxLayout *a2CardLayout = new QVBoxLayout(a2Card);
    a2CardLayout->setSpacing(14);

    QLabel *a2Title = new QLabel(QString::fromUtf8("<span style='font-size: 16px; font-weight: bold; color: #FFFFFF;'>🛠 Этап 2: Тонкая настройка режима сброса</span>"), a2Card);
    a2CardLayout->addWidget(a2Title);

    m_flashModeGroup = new QButtonGroup(this);

    QWidget *advModeBox = new QWidget(a2Card);
    advModeBox->setStyleSheet("background-color: #141416; border: 1px solid #27272A; padding: 12px;");
    QVBoxLayout *advModeBoxLayout = new QVBoxLayout(advModeBox);
    advModeBoxLayout->setSpacing(12);

    m_rbRetainData = new QRadioButton(QString::fromUtf8("<b>Обновление (сохранить пользовательские данные) [-u Update]</b>"), advModeBox);
    m_rbRetainData->setObjectName(QStringLiteral("ipswRbRetainData"));
    m_rbRetainData->setChecked(true);
    m_flashModeGroup->addButton(m_rbRetainData, 0);
    QLabel *retDesc = new QLabel(QString::fromUtf8("Флаг <code>-u</code>: Сохраняет разделы с пользовательскими данными (/private/var). Обновляет ядро, RootFS и Baseband."), advModeBox);
    retDesc->setStyleSheet("color: #A1A1AA; font-size: 11.5px; margin-left: 24px;");
    advModeBoxLayout->addWidget(m_rbRetainData);
    advModeBoxLayout->addWidget(retDesc);

    m_rbCleanRestore = new QRadioButton(QString::fromUtf8("<b>Чистая прошивка со сбросом (стереть все данные) [-e Erase]</b>"), advModeBox);
    m_rbCleanRestore->setObjectName(QStringLiteral("ipswRbCleanRestore"));
    m_flashModeGroup->addButton(m_rbCleanRestore, 1);
    QLabel *eraseDesc = new QLabel(QString::fromUtf8("Флаг <code>-e</code>: Полная переразметка NAND накопителя, уничтожение файловой системы. Рекомендуется при повреждении системы."), advModeBox);
    eraseDesc->setStyleSheet("color: #F87171; font-size: 11.5px; margin-left: 24px;");
    advModeBoxLayout->addWidget(m_rbCleanRestore);
    advModeBoxLayout->addWidget(eraseDesc);

    a2CardLayout->addWidget(advModeBox);

    QWidget *advNoteBox = new QWidget(a2Card);
    advNoteBox->setStyleSheet("background-color: #141416; border-left: 3px solid #00E5FF; padding: 10px;");
    QVBoxLayout *anbLayout = new QVBoxLayout(advNoteBox);
    anbLayout->setSpacing(6);

    QLabel *anbTitle = new QLabel(QString::fromUtf8("<b>Технические примечания idevicerestore:</b>"), advNoteBox);
    anbTitle->setStyleSheet("color: #00E5FF; font-size: 12px;");
    anbLayout->addWidget(anbTitle);

    QLabel *anbContent = new QLabel(QString::fromUtf8(
        "• При прошивке отправляется запрос билета подписи (TSS) на серверы Apple (gs.apple.com).<br>"
        "• Неподписанные прошивки (красный статус) будут отклонены TSS, если не предоставлены SHSH blobs.<br>"
        "• Процесс восстановления автоматически определит Recovery/DFU режим и перезагрузит устройство."
    ), advNoteBox);
    anbContent->setStyleSheet("color: #D4D4D8; font-size: 11.5px; line-height: 1.4;");
    anbLayout->addWidget(anbContent);
    a2CardLayout->addWidget(advNoteBox);

    a2Layout->addWidget(a2Card);

    QHBoxLayout *a2Bar = new QHBoxLayout();
    m_btnAdvStep2Back = new QPushButton(QString::fromUtf8("⬅ Назад к каталогу"), advStep2);
    m_btnAdvStep2Back->setObjectName(QStringLiteral("ipswBtnAdvStep2Back"));
    m_btnAdvStep2Back->setAttribute(Qt::WA_Hover, true);
    m_btnAdvStep2Back->setCursor(Qt::PointingHandCursor);
    m_btnAdvStep2Back->setStyleSheet("background-color: #27272A; border: 1px solid #3F3F46; color: #FFFFFF; padding: 8px 16px; font-weight: 600;");
    connect(m_btnAdvStep2Back, &QPushButton::clicked, this, &AppleIpswWidget::onAdvStep2Back);
    a2Bar->addWidget(m_btnAdvStep2Back);

    a2Bar->addStretch();

    m_btnAdvStep2Next = new QPushButton(QString::fromUtf8("Далее: Подключение USB и запуск ➔"), advStep2);
    m_btnAdvStep2Next->setObjectName(QStringLiteral("ipswBtnAdvStep2Next"));
    m_btnAdvStep2Next->setAttribute(Qt::WA_Hover, true);
    m_btnAdvStep2Next->setCursor(Qt::PointingHandCursor);
    m_btnAdvStep2Next->setStyleSheet("QPushButton { background-color: #0284C7; border: 1px solid #00E5FF; color: #FFFFFF; padding: 8px 20px; font-weight: bold; font-size: 13px; } QPushButton:hover { background-color: #0369A1; }");
    connect(m_btnAdvStep2Next, &QPushButton::clicked, this, &AppleIpswWidget::onAdvStep2Next);
    a2Bar->addWidget(m_btnAdvStep2Next);


    a2Layout->addLayout(a2Bar);
    a2Layout->addStretch(1);

    QScrollArea *a2Scroll = createMetroScrollArea(advStep2, m_advancedStepStack);
    m_advancedStepStack->addWidget(a2Scroll);

    // ADVANCED STEP 3: Device check, Flashing & Terminal logs (Scrollable with Anti-Stretch)
    QWidget *advStep3 = new QWidget();
    QVBoxLayout *a3Layout = new QVBoxLayout(advStep3);
    a3Layout->setContentsMargins(12, 8, 12, 16);
    a3Layout->setSpacing(10);

    QSplitter *splitter = new QSplitter(Qt::Horizontal, advStep3);
    splitter->setHandleWidth(4);
    splitter->setMinimumHeight(480);

    m_deviceCardWidget = new QWidget(splitter);
    m_deviceCardWidget->setStyleSheet("background-color: #18181B; border: 1px solid #27272A; border-radius: 0px;");
    QVBoxLayout *cardLayout = new QVBoxLayout(m_deviceCardWidget);
    cardLayout->setContentsMargins(14, 14, 14, 14);
    cardLayout->setSpacing(10);

    m_lblDeviceIcon = new QLabel(m_deviceCardWidget);
    m_lblDeviceIcon->setAlignment(Qt::AlignCenter);
    QIcon appleIcon(":/svg/services/apple");
    if(appleIcon.isNull()) appleIcon = QIcon(":/svg/apple");
    m_lblDeviceIcon->setPixmap(appleIcon.pixmap(48, 48));
    cardLayout->addWidget(m_lblDeviceIcon);

    m_lblDeviceName = new QLabel(QString::fromUtf8("Устройство не обнаружено"), m_deviceCardWidget);
    m_lblDeviceName->setAlignment(Qt::AlignCenter);
    m_lblDeviceName->setStyleSheet("font-size: 14px; font-weight: bold; color: #FFFFFF;");
    cardLayout->addWidget(m_lblDeviceName);

    m_lblDeviceMode = new QLabel(QString::fromUtf8("● Ожидание подключения USB"), m_deviceCardWidget);
    m_lblDeviceMode->setAlignment(Qt::AlignCenter);
    m_lblDeviceMode->setStyleSheet("font-size: 11.5px; color: #EAB308; font-weight: 600; padding: 3px 8px; background: rgba(234, 179, 8, 0.1); border: 1px solid #854D0E;");
    cardLayout->addWidget(m_lblDeviceMode);

    cardLayout->addSpacing(6);

    auto addAttrRow = [this, cardLayout](const QString &title, QLabel *&valLbl) {
        QHBoxLayout *row = new QHBoxLayout();
        QLabel *tLbl = new QLabel(title, m_deviceCardWidget);
        tLbl->setStyleSheet("color: #71717A; font-size: 11.5px;");
        valLbl = new QLabel("—", m_deviceCardWidget);
        valLbl->setStyleSheet("color: #E4E4E7; font-weight: 600; font-size: 11.5px;");
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

    QLabel *actionsTitle = new QLabel(QString::fromUtf8("Быстрые действия:"), m_deviceCardWidget);
    actionsTitle->setStyleSheet("color: #A1A1AA; font-weight: bold; font-size: 11px;");
    cardLayout->addWidget(actionsTitle);

    m_btnExitRecovery = new QPushButton(QString::fromUtf8("Выйти из Recovery"), m_deviceCardWidget);
    m_btnExitRecovery->setObjectName(QStringLiteral("ipswBtnExitRecovery"));
    m_btnExitRecovery->setAttribute(Qt::WA_Hover, true);
    m_btnExitRecovery->setCursor(Qt::PointingHandCursor);
    connect(m_btnExitRecovery, &QPushButton::clicked, this, &AppleIpswWidget::onExitRecoveryClicked);
    cardLayout->addWidget(m_btnExitRecovery);

    m_btnEnterRecovery = new QPushButton(QString::fromUtf8("Войти в Recovery"), m_deviceCardWidget);
    m_btnEnterRecovery->setObjectName(QStringLiteral("ipswBtnEnterRecovery"));
    m_btnEnterRecovery->setAttribute(Qt::WA_Hover, true);
    m_btnEnterRecovery->setCursor(Qt::PointingHandCursor);
    connect(m_btnEnterRecovery, &QPushButton::clicked, this, &AppleIpswWidget::onEnterRecoveryClicked);
    cardLayout->addWidget(m_btnEnterRecovery);

    m_btnReboot = new QPushButton(QString::fromUtf8("Перезагрузить устройство"), m_deviceCardWidget);
    m_btnReboot->setObjectName(QStringLiteral("ipswBtnReboot"));
    m_btnReboot->setAttribute(Qt::WA_Hover, true);
    m_btnReboot->setCursor(Qt::PointingHandCursor);
    connect(m_btnReboot, &QPushButton::clicked, this, &AppleIpswWidget::onRebootClicked);
    cardLayout->addWidget(m_btnReboot);

    cardLayout->addStretch();
    splitter->addWidget(m_deviceCardWidget);

    QWidget *rightWidget = new QWidget(splitter);
    QVBoxLayout *rLayout = new QVBoxLayout(rightWidget);
    rLayout->setContentsMargins(0, 0, 0, 0);
    rLayout->setSpacing(8);

    QWidget *execBox = new QWidget(rightWidget);
    execBox->setStyleSheet("background-color: #18181B; border: 1px solid #27272A; padding: 10px;");
    QVBoxLayout *ebLayout = new QVBoxLayout(execBox);
    ebLayout->setSpacing(8);

    m_lblProgressStatus = new QLabel(QString::fromUtf8("Готов к работе"), execBox);
    m_lblProgressStatus->setObjectName(QStringLiteral("ipswLblProgressStatus"));
    m_lblProgressStatus->setStyleSheet("color: #A1A1AA; font-size: 12px;");
    ebLayout->addWidget(m_lblProgressStatus);

    m_progressBar = new QProgressBar(execBox);
    m_progressBar->setObjectName(QStringLiteral("ipswAdvProgressBar"));
    m_progressBar->setValue(0);
    ebLayout->addWidget(m_progressBar);

    QHBoxLayout *ctrlBtns = new QHBoxLayout();

    m_btnAdvStep3Back = new QPushButton(QString::fromUtf8("⬅ Назад"), execBox);
    m_btnAdvStep3Back->setObjectName(QStringLiteral("ipswBtnAdvStep3Back"));
    m_btnAdvStep3Back->setAttribute(Qt::WA_Hover, true);
    m_btnAdvStep3Back->setCursor(Qt::PointingHandCursor);
    m_btnAdvStep3Back->setStyleSheet("background-color: #27272A; border: 1px solid #3F3F46; color: #FFFFFF; padding: 8px 16px; font-weight: 600;");
    connect(m_btnAdvStep3Back, &QPushButton::clicked, this, &AppleIpswWidget::onAdvStep2Back);
    ctrlBtns->addWidget(m_btnAdvStep3Back);

    m_btnStartFlash = new QPushButton(QString::fromUtf8("⚡ НАЧАТЬ ПРОШИВКУ"), execBox);
    m_btnStartFlash->setObjectName(QStringLiteral("ipswBtnStartFlash"));
    m_btnStartFlash->setAttribute(Qt::WA_Hover, true);
    m_btnStartFlash->setCursor(Qt::PointingHandCursor);
    m_btnStartFlash->setStyleSheet("QPushButton { background-color: #0284C7; border: 1px solid #00E5FF; color: #FFFFFF; padding: 8px 24px; font-weight: bold; font-size: 13px; } QPushButton:hover { background-color: #0369A1; } QPushButton:disabled { background-color: #27272A; color: #52525B; border-color: #3F3F46; }");
    connect(m_btnStartFlash, &QPushButton::clicked, this, &AppleIpswWidget::onStartFlashClicked);
    ctrlBtns->addWidget(m_btnStartFlash, 2);

    m_btnCancelFlash = new QPushButton(QString::fromUtf8("⏹ Отмена"), execBox);
    m_btnCancelFlash->setObjectName(QStringLiteral("ipswBtnCancelFlash"));
    m_btnCancelFlash->setAttribute(Qt::WA_Hover, true);
    m_btnCancelFlash->setCursor(Qt::PointingHandCursor);
    m_btnCancelFlash->setStyleSheet("QPushButton { background-color: #991B1B; border: 1px solid #EF4444; color: #FFFFFF; padding: 8px 16px; font-weight: bold; } QPushButton:disabled { background-color: #27272A; color: #52525B; border-color: #3F3F46; }");
    m_btnCancelFlash->setEnabled(false);
    connect(m_btnCancelFlash, &QPushButton::clicked, this, &AppleIpswWidget::onCancelFlashClicked);
    ctrlBtns->addWidget(m_btnCancelFlash);

    m_btnAdvRetry = new QPushButton(QString::fromUtf8("🔄 Повторить"), execBox);
    m_btnAdvRetry->setObjectName(QStringLiteral("ipswBtnAdvRetry"));
    m_btnAdvRetry->setAttribute(Qt::WA_Hover, true);
    m_btnAdvRetry->setCursor(Qt::PointingHandCursor);
    m_btnAdvRetry->setStyleSheet("QPushButton { background-color: #047857; border: 1px solid #10B981; color: #FFFFFF; padding: 8px 16px; font-weight: bold; } QPushButton:hover { background-color: #059669; }");
    m_btnAdvRetry->setVisible(false);
    connect(m_btnAdvRetry, &QPushButton::clicked, this, &AppleIpswWidget::onAdvRetry);
    ctrlBtns->addWidget(m_btnAdvRetry);

    ebLayout->addLayout(ctrlBtns);
    rLayout->addWidget(execBox);

    m_logTerminal = new QTextEdit(rightWidget);
    m_logTerminal->setObjectName(QStringLiteral("ipswLogTerminal"));
    m_logTerminal->setReadOnly(true);
    rLayout->addWidget(m_logTerminal, 1);


    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);

    a3Layout->addWidget(splitter);
    a3Layout->addStretch(1);

    QScrollArea *a3Scroll = createMetroScrollArea(advStep3, m_advancedStepStack);
    m_advancedStepStack->addWidget(a3Scroll);

    layout->addWidget(m_advancedStepStack, 1);
    m_modeStack->addWidget(advPage);

    updateDeviceCard();
}

void AppleIpswWidget::updateSimpleWizardStep1()
{
    if(!m_lblSimpleUsbStatus || !m_lblSimpleDetectedModel || !m_btnSimpleStep1Next) return;

    if(m_device.isEmpty())
    {
        m_lblSimpleUsbStatus->setText(QString::fromUtf8("⏳ Ожидание подключения iPhone по USB..."));
        m_lblSimpleUsbStatus->setStyleSheet("background: rgba(234, 179, 8, 0.12); color: #FBBF24; font-size: 13px; font-weight: bold; padding: 12px; border: 1px solid #D97706;");
        m_lblSimpleDetectedModel->setText(QString::fromUtf8("Устройство пока не обнаружено. Подключите кабель USB и разблокируйте экран."));
        m_lblSimpleDetectedModel->setStyleSheet("color: #71717A; font-size: 12px;");
        m_btnSimpleStep1Next->setEnabled(false);
    }
    else
    {
        QString name = !m_device.displayName.isEmpty() ? m_device.displayName : (!m_device.marketingName.isEmpty() ? m_device.marketingName : m_device.model);
        m_lblSimpleUsbStatus->setText(QString::fromUtf8("✔ iPhone успешно обнаружен!"));
        m_lblSimpleUsbStatus->setStyleSheet("background: rgba(16, 185, 129, 0.12); color: #34D399; font-size: 13px; font-weight: bold; padding: 12px; border: 1px solid #059669;");
        m_lblSimpleDetectedModel->setText(QString::fromUtf8("📱 <b>%1</b> • Режим: <b>%2</b> • Установленная iOS: <b>%3</b> • ECID: %4")
            .arg(name, m_device.modeString(), !m_device.productVersion.isEmpty() ? m_device.productVersion : QString::fromUtf8("В Recovery/DFU"), !m_device.ecid.isEmpty() ? m_device.ecid : "—"));
        m_lblSimpleDetectedModel->setStyleSheet("color: #FFFFFF; font-size: 12.5px;");
        m_btnSimpleStep1Next->setEnabled(true);
    }
}

void AppleIpswWidget::updateSimpleWizardStep2()
{
    if(!m_lblSimpleDeviceSummary || !m_lblSimpleAutoIpswInfo) return;

    QString name = !m_device.displayName.isEmpty() ? m_device.displayName : (!m_device.marketingName.isEmpty() ? m_device.marketingName : m_device.model);
    m_lblSimpleDeviceSummary->setText(QString::fromUtf8("📱 Выбранное устройство: <b>%1</b> (Режим: %2)")
        .arg(name, m_device.modeString()));

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
        QString expectedFileName = QString("%1_%2_%3_Restore.ipsw")
                                       .arg(bestSigned.identifier, bestSigned.version, bestSigned.buildid);
        m_selectedIpswPath = downloadDirectory() + "/" + expectedFileName;

        if(QFile::exists(m_selectedIpswPath))
        {
            m_lblSimpleAutoIpswInfo->setText(QString::fromUtf8("✔ Официальная прошивка iOS %1 (%2) уже загружена в хранилище [%3] и готова к установке.")
                .arg(bestSigned.version, bestSigned.buildid, formatFileSize(QFileInfo(m_selectedIpswPath).size())));
            m_lblSimpleAutoIpswInfo->setStyleSheet("color: #34D399; font-size: 12px; padding: 8px; background: rgba(52,211,153,0.08); border: 1px solid #059669;");
        }
        else
        {
            m_lblSimpleAutoIpswInfo->setText(QString::fromUtf8("⬇ Подобрана актуальная официальная прошивка Apple: iOS %1 (Сборка %2) [Размер: %3]. Будет скачана автоматически.")
                .arg(bestSigned.version, bestSigned.buildid, formatFileSize(bestSigned.size)));
            m_lblSimpleAutoIpswInfo->setStyleSheet("color: #00E5FF; font-size: 12px; padding: 8px; background: rgba(0,229,255,0.08); border: 1px solid #0284C7;");
        }
    }
    else
    {
        m_lblSimpleAutoIpswInfo->setText(QString::fromUtf8("⏳ Запрос актуальной подписанной прошивки у серверов Apple..."));
        m_lblSimpleAutoIpswInfo->setStyleSheet("color: #FBBF24; font-size: 12px; padding: 8px; background: rgba(251,191,36,0.08); border: 1px solid #D97706;");
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
        QMessageBox::information(this, QString::fromUtf8("Подключение USB"),
            QString::fromUtf8("Сначала подключите iPhone через USB к компьютеру."));
        return;
    }
    m_simpleStepStack->setCurrentIndex(1);
    m_lblSimpleWizardStepHeader->setText(QString::fromUtf8("ШАГ 2 ИЗ 3: АВТОНАСТРОЙКИ И ВЫБОР РЕЖИМА СБРОСА"));
    updateSimpleWizardStep2();
}

void AppleIpswWidget::onSimpleStep2Back()
{
    m_simpleStepStack->setCurrentIndex(0);
    m_lblSimpleWizardStepHeader->setText(QString::fromUtf8("ШАГ 1 ИЗ 3: ПОДКЛЮЧЕНИЕ IPHONE ПО USB"));
    updateSimpleWizardStep1();
}

void AppleIpswWidget::onSimpleStep2Start()
{
    if(m_device.isEmpty())
    {
        QMessageBox::warning(this, QString::fromUtf8("Устройство отключено"),
            QString::fromUtf8("Устройство Apple не найдено. Подключите кабель USB."));
        onSimpleStep2Back();
        return;
    }

    bool retain = m_rbSimpleRetain->isChecked();
    QString confirmText = retain
        ? QString::fromUtf8("Начать ОБНОВЛЕНИЕ устройства %1 с сохранением данных?\n\nФайл прошивки: iOS %2 (%3)")
            .arg(!m_device.displayName.isEmpty() ? m_device.displayName : m_device.marketingName, m_selectedFirmware.version, m_selectedFirmware.buildid)
        : QString::fromUtf8("ВНИМАНИЕ! Выбран режим ПОЛНОЙ ОЧИСТКИ.\n\n"
                            "Все данные пользователя на iPhone будут ПОЛНОСТЬЮ СТЕРТЫ!\n\n"
                            "Вы уверены, что хотите продолжить?");

    auto reply = QMessageBox::question(this, QString::fromUtf8("Подтверждение восстановления"),
        confirmText, QMessageBox::Yes | QMessageBox::No);

    if(reply != QMessageBox::Yes) return;

    m_simpleStepStack->setCurrentIndex(2);
    m_lblSimpleWizardStepHeader->setText(QString::fromUtf8("ШАГ 3 ИЗ 3: ВЫПОЛНЕНИЕ ВОССТАНОВЛЕНИЯ"));
    m_btnSimpleRetry->setVisible(false);
    m_btnSimpleCancel->setEnabled(true);
    m_simpleProgressBar->setValue(0);

    if(m_selectedIpswPath.isEmpty() || !QFile::exists(m_selectedIpswPath))
    {
        if(m_selectedFirmware.url.isEmpty())
        {
            QMessageBox::critical(this, QString::fromUtf8("Ошибка"),
                QString::fromUtf8("Не удалось определить ссылку на прошивку. Попробуйте обновить каталог или переключитесь в расширенный режим."));
            onSimpleStep2Back();
            return;
        }

        m_lblSimpleProgressStatus->setText(QString::fromUtf8("Загрузка официальной прошивки iOS %1...").arg(m_selectedFirmware.version));
        m_downloader->startDownload(m_selectedFirmware.url, m_selectedIpswPath);
        return;
    }

    m_lblSimpleProgressStatus->setText(QString::fromUtf8("Запуск прошивки %1...").arg(retain ? "с сохранением данных" : "со сбросом"));
    m_restoreWorker->startRestore(m_selectedIpswPath, retain, m_device.devId);
}

void AppleIpswWidget::onSimpleRetry()
{
    m_btnSimpleRetry->setVisible(false);
    onSimpleStep2Start();
}

void AppleIpswWidget::onAdvStep1Next()
{
    if(m_selectedIpswPath.isEmpty() || !QFile::exists(m_selectedIpswPath))
    {
        if(!m_selectedFirmware.url.isEmpty())
        {
            auto res = QMessageBox::question(this, QString::fromUtf8("Прошивка не загружена"),
                QString::fromUtf8("Выбранная прошивка iOS %1 еще не загружена на диск.\nПредзагрузить её сейчас?").arg(m_selectedFirmware.version),
                QMessageBox::Yes | QMessageBox::No);
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
            QMessageBox::warning(this, QString::fromUtf8("Прошивка не выбрана"),
                QString::fromUtf8("Пожалуйста, выберите версию прошивки из таблицы или укажите локальный файл .ipsw."));
            return;
        }
    }

    m_advancedStepStack->setCurrentIndex(1);
    m_lblAdvStepHeader->setText(QString::fromUtf8("ЭТАП 2 ИЗ 3: ТОНКАЯ НАСТРОЙКА РЕЖИМА СБРОСА"));
}

void AppleIpswWidget::onAdvStep2Back()
{
    m_advancedStepStack->setCurrentIndex(0);
    m_lblAdvStepHeader->setText(QString::fromUtf8("ЭТАП 1 ИЗ 3: КАТАЛОГ ПРОШИВОК И МЕНЕДЖЕР ЗАГРУЗОК"));
}

void AppleIpswWidget::onAdvStep2Next()
{
    m_advancedStepStack->setCurrentIndex(2);
    m_lblAdvStepHeader->setText(QString::fromUtf8("ЭТАП 3 ИЗ 3: ПОДКЛЮЧЕНИЕ USB, ПРОШИВКА И ТЕРМИНАЛ"));
    updateDeviceCard();
}

void AppleIpswWidget::onAdvRetry()
{
    m_btnAdvRetry->setVisible(false);
    onStartFlashClicked();
}

void AppleIpswWidget::appendLog(const QString &line, const QString &color)
{
    if(!m_logTerminal) return;
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString formatted = QString("<span style='color: #52525B;'>[%1]</span> <span style='color: %2;'>%3</span>")
                            .arg(timestamp, color, line.toHtmlEscaped());
    m_logTerminal->append(formatted);
    m_logTerminal->verticalScrollBar()->setValue(m_logTerminal->verticalScrollBar()->maximum());
}

void AppleIpswWidget::loadDeviceList()
{
    if(!m_comboDeviceModel) return;

    m_comboDeviceModel->blockSignals(true);
    m_comboDeviceModel->clear();

    // Comprehensive list of popular Apple device models
    static const struct {
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
        {"iPad14,1", "iPad mini (6th gen)"}
    };

    for(const auto &item : defaultModels)
    {
        m_comboDeviceModel->addItem(QString("%1 (%2)").arg(item.name, item.id), item.id);
    }

    m_comboDeviceModel->blockSignals(false);

    // Dynamic fetch from api.ipsw.me to discover newly released devices
    QUrl url("https://api.ipsw.me/v4/devices");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "AdsKiller-AppleFlasher/1.0");
    QNetworkReply *reply = m_netManager->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if(reply->error() != QNetworkReply::NoError) return;
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if(!doc.isArray()) return;

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

    // Default select first item
    if(m_comboDeviceModel->count() > 0)
    {
        m_comboDeviceModel->setCurrentIndex(0);
        m_currentSelectedModelId = m_comboDeviceModel->currentData().toString();
        fetchFirmwareCatalog(m_currentSelectedModelId);
    }
}

void AppleIpswWidget::onModelSelected(int index)
{
    if(index < 0 || !m_comboDeviceModel) return;
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

void AppleIpswWidget::updateDiskSpaceInfo()
{
    QString dir = downloadDirectory();
    if(m_lblStoragePath)
    {
        m_lblStoragePath->setText(QString::fromUtf8("📁 Папка загрузки: <b>%1</b>").arg(dir));
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
        appendLog(QString::fromUtf8("[ХРАНИЛИЩЕ] Папка загрузки изменена на: %1").arg(dir), "#00E5FF");
    }
}

void AppleIpswWidget::scanLocalFirmwares()
{
    updateDiskSpaceInfo();
    populateLocalFirmwareTable();
}

void AppleIpswWidget::populateLocalFirmwareTable()
{
    if(!m_localFirmwareTable) return;
    m_localFirmwareTable->setRowCount(0);

    QDir dir(downloadDirectory());
    QFileInfoList files = dir.entryInfoList(QStringList() << "*.ipsw", QDir::Files, QDir::Time);

    for(const QFileInfo &fi : files)
    {
        int row = m_localFirmwareTable->rowCount();
        m_localFirmwareTable->insertRow(row);

        // File name
        QTableWidgetItem *nameItem = new QTableWidgetItem(fi.fileName());
        nameItem->setFont(QFont("Segoe UI", 9, QFont::Bold));
        nameItem->setToolTip(fi.absoluteFilePath());
        m_localFirmwareTable->setItem(row, 0, nameItem);

        // Model / Info (parsed from file name)
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
        modelItem->setForeground(QColor("#A1A1AA"));
        m_localFirmwareTable->setItem(row, 1, modelItem);

        // Size
        QTableWidgetItem *sizeItem = new QTableWidgetItem(formatFileSize(fi.size()));
        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_localFirmwareTable->setItem(row, 2, sizeItem);

        // Date
        QTableWidgetItem *dateItem = new QTableWidgetItem(fi.lastModified().toString("dd.MM.yyyy hh:mm"));
        dateItem->setForeground(QColor("#71717A"));
        dateItem->setTextAlignment(Qt::AlignCenter);
        m_localFirmwareTable->setItem(row, 3, dateItem);

        // Actions: Select for Flash & Delete
        QWidget *actionWidget = new QWidget(m_localFirmwareTable);
        QHBoxLayout *aLayout = new QHBoxLayout(actionWidget);
        aLayout->setContentsMargins(4, 2, 4, 2);
        aLayout->setSpacing(6);

        QPushButton *btnSelect = new QPushButton(QString::fromUtf8("⚡ Выбрать"), actionWidget);
        btnSelect->setStyleSheet("background-color: #0284C7; color: #FFFFFF; font-weight: bold; padding: 4px 8px; font-size: 11px;");
        QString filePath = fi.absoluteFilePath();
        connect(btnSelect, &QPushButton::clicked, this, [this, filePath, detectedModel, baseName]() {
            m_selectedIpswPath = filePath;
            m_isLocalIpsw = true;
            m_lblSelectedIpsw->setText(QString::fromUtf8("✔ Выбрана предзагруженная прошивка: %1 [%2]")
                .arg(baseName, formatFileSize(QFileInfo(filePath).size())));
            m_lblSelectedIpsw->setStyleSheet("color: #34D399; font-size: 11.5px; padding: 4px; background: rgba(52,211,153,0.08); border: 1px solid #059669;");
            if(m_tabWidget) m_tabWidget->setCurrentIndex(0);
            appendLog(QString::fromUtf8("[ВЫБОР ПРОШИВКИ] Выбран предзагруженный файл: %1").arg(filePath), "#10B981");
        });
        aLayout->addWidget(btnSelect);

        QPushButton *btnDel = new QPushButton(QString::fromUtf8("🗑"), actionWidget);
        btnDel->setStyleSheet("background-color: #27272A; color: #EF4444; font-weight: bold; padding: 4px 8px; font-size: 11px;");
        btnDel->setToolTip(QString::fromUtf8("Удалить этот файл с диска"));
        connect(btnDel, &QPushButton::clicked, this, [this, filePath]() {
            auto res = QMessageBox::question(this, QString::fromUtf8("Удаление файла"),
                QString::fromUtf8("Вы уверены, что хотите удалить файл прошивки?\n\n%1").arg(filePath),
                QMessageBox::Yes | QMessageBox::No);
            if(res == QMessageBox::Yes)
            {
                QFile::remove(filePath);
                scanLocalFirmwares();
                populateFirmwareTable();
                appendLog(QString::fromUtf8("[УДАЛЕНИЕ] Файл прошивки удален: %1").arg(filePath), "#EF4444");
            }
        });
        aLayout->addWidget(btnDel);

        m_localFirmwareTable->setCellWidget(row, 4, actionWidget);
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
    if(!m_lblDeviceName || !m_lblDeviceMode) return;

    if(m_device.isEmpty())
    {
        m_lblDeviceName->setText(QString::fromUtf8("Устройство не обнаружено"));
        m_lblDeviceMode->setText(QString::fromUtf8("● Ожидание подключения USB"));
        m_lblDeviceMode->setStyleSheet("font-size: 12px; color: #EAB308; font-weight: 600; padding: 3px 8px; background: rgba(234, 179, 8, 0.1); border: 1px solid #854D0E;");
        if(m_lblIosVersion) m_lblIosVersion->setText("—");
        if(m_lblSerial) m_lblSerial->setText("—");
        if(m_lblEcid) m_lblEcid->setText("—");
        if(m_lblUdid) m_lblUdid->setText("—");

        if(m_btnExitRecovery) m_btnExitRecovery->setEnabled(false);
        if(m_btnEnterRecovery) m_btnEnterRecovery->setEnabled(false);
        if(m_btnReboot) m_btnReboot->setEnabled(false);
        return;
    }

    m_lblDeviceName->setText(!m_device.displayName.isEmpty() ? m_device.displayName : (!m_device.marketingName.isEmpty() ? m_device.marketingName : m_device.model));
    m_lblDeviceMode->setText(QString("● %1").arg(m_device.modeString()));

    if(m_device.mode == AppleDeviceMode::Normal)
    {
        m_lblDeviceMode->setStyleSheet("font-size: 12px; color: #10B981; font-weight: 600; padding: 3px 8px; background: rgba(16, 185, 129, 0.1); border: 1px solid #059669;");
        if(m_btnExitRecovery) m_btnExitRecovery->setEnabled(false);
        if(m_btnEnterRecovery) m_btnEnterRecovery->setEnabled(true);
        if(m_btnReboot) m_btnReboot->setEnabled(true);
    }
    else
    {
        m_lblDeviceMode->setStyleSheet("font-size: 12px; color: #38BDF8; font-weight: 600; padding: 3px 8px; background: rgba(56, 189, 248, 0.1); border: 1px solid #0284C7;");
        if(m_btnExitRecovery) m_btnExitRecovery->setEnabled(true);
        if(m_btnEnterRecovery) m_btnEnterRecovery->setEnabled(false);
        if(m_btnReboot) m_btnReboot->setEnabled(true);
    }

    if(m_lblIosVersion) m_lblIosVersion->setText(!m_device.productVersion.isEmpty() ? m_device.productVersion : QString::fromUtf8("В Recovery/DFU"));
    if(m_lblSerial) m_lblSerial->setText(!m_device.serialNumber.isEmpty() ? m_device.serialNumber : "—");
    if(m_lblEcid) m_lblEcid->setText(!m_device.ecid.isEmpty() ? m_device.ecid : "—");
    if(m_lblUdid) m_lblUdid->setText(!m_device.devId.isEmpty() ? (m_device.devId.left(14) + "...") : "—");
}

void AppleIpswWidget::fetchFirmwareCatalog(const QString &modelId)
{
    if(modelId.isEmpty()) return;

    appendLog(QString::fromUtf8("[API] Запрос каталога прошивок для %1...").arg(modelId), "#38BDF8");

    QUrl url(QString("https://api.ipsw.me/v4/device/%1?type=ipsw").arg(modelId));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "AdsKiller-AppleFlasher/1.0");

    QNetworkReply *reply = m_netManager->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, modelId]() {
        reply->deleteLater();
        if(reply->error() != QNetworkReply::NoError)
        {
            appendLog(QString::fromUtf8("[API ОШИБКА] Не удалось загрузить каталог: %1").arg(reply->errorString()), "#EF4444");
            return;
        }

        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if(!doc.isObject()) return;

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
            info.sha1sum = fObj["sha1sum"].toString();
            info.md5sum = fObj["md5sum"].toString();
            info.size = fObj["size"].toVariant().toLongLong();
            info.url = fObj["url"].toString();
            info.isSigned = fObj["signed"].toBool();
            info.releaseDate = fObj["releasedate"].toString().left(10);
            m_firmwares.append(info);
        }

        populateFirmwareTable();
        updateSimpleWizardStep2();
        appendLog(QString::fromUtf8("[API] Получено %1 прошивок для %2").arg(m_firmwares.size()).arg(modelId), "#10B981");
    });
}

void AppleIpswWidget::populateFirmwareTable()
{
    if(!m_firmwareTable) return;
    m_firmwareTable->setRowCount(0);

    for(int i = 0; i < m_firmwares.size(); ++i)
    {
        const auto &fw = m_firmwares[i];
        int row = m_firmwareTable->rowCount();
        m_firmwareTable->insertRow(row);

        // Version
        QTableWidgetItem *verItem = new QTableWidgetItem(QString("iOS %1").arg(fw.version));
        verItem->setFont(QFont("Segoe UI", 10, QFont::Bold));
        m_firmwareTable->setItem(row, 0, verItem);

        // Build
        QTableWidgetItem *buildItem = new QTableWidgetItem(fw.buildid);
        buildItem->setForeground(QColor("#A1A1AA"));
        m_firmwareTable->setItem(row, 1, buildItem);

        // Signed Status Badge
        QTableWidgetItem *signItem = new QTableWidgetItem(
            fw.isSigned ? QString::fromUtf8("🟢 Подписывается Apple (Готова к установке)")
                        : QString::fromUtf8("🔴 Не подписывается Apple (Отказ TSS)")
        );
        signItem->setFont(QFont("Segoe UI", 9, QFont::Bold));
        signItem->setForeground(fw.isSigned ? QColor("#34D399") : QColor("#F87171"));
        m_firmwareTable->setItem(row, 2, signItem);

        // Size
        QTableWidgetItem *sizeItem = new QTableWidgetItem(formatFileSize(fw.size));
        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_firmwareTable->setItem(row, 3, sizeItem);

        // Action Button: Download / Pre-download or Select
        QString expectedFileName = QString("%1_%2_%3_Restore.ipsw")
                                       .arg(fw.identifier, fw.version, fw.buildid);
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
            btnUse->setStyleSheet("background-color: #0284C7; color: #FFFFFF; font-weight: bold; padding: 4px 8px; font-size: 11px;");
            connect(btnUse, &QPushButton::clicked, this, [this, i]() {
                onFirmwareSelected(i, 0);
            });
            actLayout->addWidget(btnUse);
        }
        else
        {
            QPushButton *btnAction = new QPushButton(fw.isSigned ? QString::fromUtf8("⬇ Предзагрузить") : QString::fromUtf8("⬇ Скачать"), actionWidget);
            btnAction->setStyleSheet(fw.isSigned
                ? "background-color: #047857; color: #FFFFFF; font-weight: bold; padding: 4px 8px; font-size: 11px;"
                : "background-color: #27272A; color: #A1A1AA; padding: 4px 8px; font-size: 11px;");
            connect(btnAction, &QPushButton::clicked, this, [this, i]() {
                onDownloadIpswClicked(i);
            });
            actLayout->addWidget(btnAction);
        }

        m_firmwareTable->setCellWidget(row, 4, actionWidget);
    }
}

void AppleIpswWidget::onFirmwareSelected(int row, int column)
{
    (void)column;
    if(row < 0 || row >= m_firmwares.size()) return;

    m_selectedFirmware = m_firmwares[row];
    m_isLocalIpsw = false;

    QString expectedFileName = QString("%1_%2_%3_Restore.ipsw")
                                   .arg(m_selectedFirmware.identifier, m_selectedFirmware.version, m_selectedFirmware.buildid);
    m_selectedIpswPath = downloadDirectory() + "/" + expectedFileName;

    if(QFile::exists(m_selectedIpswPath))
    {
        m_lblSelectedIpsw->setText(QString::fromUtf8("✔ Выбрана готовая прошивка: iOS %1 (%2) [%3]")
            .arg(m_selectedFirmware.version, m_selectedFirmware.buildid, m_selectedIpswPath));
        m_lblSelectedIpsw->setStyleSheet("color: #34D399; font-size: 11.5px; padding: 4px; background: rgba(52,211,153,0.08); border: 1px solid #059669;");
    }
    else
    {
        m_lblSelectedIpsw->setText(QString::fromUtf8("Выбрана прошивка: iOS %1 (%2). Нажмите «Предзагрузить» для сохранения файла.")
            .arg(m_selectedFirmware.version, m_selectedFirmware.buildid));
        m_lblSelectedIpsw->setStyleSheet("color: #FBBF24; font-size: 11.5px; padding: 4px; background: rgba(251,191,36,0.08); border: 1px solid #D97706;");
    }
}

void AppleIpswWidget::onDownloadIpswClicked(int row)
{
    if(row < 0 || row >= m_firmwares.size()) return;
    const auto &fw = m_firmwares[row];
    m_selectedFirmware = fw;

    QString expectedFileName = QString("%1_%2_%3_Restore.ipsw")
                                   .arg(fw.identifier, fw.version, fw.buildid);
    m_selectedIpswPath = downloadDirectory() + "/" + expectedFileName;

    if(!fw.isSigned)
    {
        auto res = QMessageBox::warning(this, QString::fromUtf8("Внимание: неподписанная прошивка"),
            QString::fromUtf8("Выбранная версия iOS %1 больше не подписывается серверами Apple (TSS).\n"
                              "Установка на устройство завершится ошибкой TSS, если у вас нет сохраненных SHSH blobs.\n\n"
                              "Всё равно начать скачивание?").arg(fw.version),
            QMessageBox::Yes | QMessageBox::No);
        if(res != QMessageBox::Yes) return;
    }

    appendLog(QString::fromUtf8("[СКАЧИВАНИЕ] Предварительная загрузка IPSW: %1").arg(fw.url), "#00E5FF");
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
        if(m_progressBar) m_progressBar->setValue(pct);
        if(m_simpleProgressBar) m_simpleProgressBar->setValue(pct);
        QString status = QString::fromUtf8("Скачивание: %1 из %2 (%3%) • Скорость: %4 МБ/с")
            .arg(formatFileSize(received), formatFileSize(total))
            .arg(pct)
            .arg(speedMBs, 0, 'f', 1);
        if(m_lblProgressStatus) m_lblProgressStatus->setText(status);
        if(m_lblSimpleProgressStatus) m_lblSimpleProgressStatus->setText(status);
    }
}

void AppleIpswWidget::onDownloadFinished(bool success, const QString &path, const QString &errorStr)
{
    if(m_btnCancelFlash) m_btnCancelFlash->setEnabled(false);
    if(m_btnSimpleCancel) m_btnSimpleCancel->setEnabled(false);

    if(success)
    {
        if(m_progressBar) m_progressBar->setValue(100);
        if(m_simpleProgressBar) m_simpleProgressBar->setValue(100);
        if(m_lblProgressStatus) m_lblProgressStatus->setText(QString::fromUtf8("Скачивание завершено! Прошивка сохранена на диск."));
        if(m_lblSimpleProgressStatus) m_lblSimpleProgressStatus->setText(QString::fromUtf8("Загрузка прошивки завершена. Подготовка к установке..."));

        if(m_lblSelectedIpsw)
        {
            m_lblSelectedIpsw->setText(QString::fromUtf8("✔ Файл готов: %1").arg(path));
            m_lblSelectedIpsw->setStyleSheet("color: #34D399; font-size: 11.5px; padding: 4px; background: rgba(52,211,153,0.08); border: 1px solid #059669;");
        }
        appendLog(QString::fromUtf8("[СКАЧИВАНИЕ УСПЕШНО] Файл сохранен в хранилище: %1").arg(path), "#10B981");
        scanLocalFirmwares();
        populateFirmwareTable();

        if(m_modeStack && m_modeStack->currentIndex() == 0 && m_simpleStepStack && m_simpleStepStack->currentIndex() == 2)
        {
            if(!m_device.isEmpty())
            {
                bool retain = m_rbSimpleRetain ? m_rbSimpleRetain->isChecked() : true;
                if(m_lblSimpleProgressStatus) m_lblSimpleProgressStatus->setText(QString::fromUtf8("Запуск прошивки устройства..."));
                if(m_btnSimpleCancel) m_btnSimpleCancel->setEnabled(true);
                m_restoreWorker->startRestore(path, retain, m_device.devId);
            }
            else
            {
                if(m_lblSimpleProgressStatus) m_lblSimpleProgressStatus->setText(QString::fromUtf8("Прошивка загружена. Ожидание подключения iPhone по USB..."));
                if(m_btnSimpleRetry) m_btnSimpleRetry->setVisible(true);
            }
        }
    }
    else
    {
        if(m_lblProgressStatus) m_lblProgressStatus->setText(QString::fromUtf8("Ошибка скачивания: %1").arg(errorStr));
        if(m_lblSimpleProgressStatus) m_lblSimpleProgressStatus->setText(QString::fromUtf8("Ошибка скачивания: %1").arg(errorStr));
        if(m_btnSimpleRetry) m_btnSimpleRetry->setVisible(true);
        if(m_btnAdvRetry) m_btnAdvRetry->setVisible(true);
        appendLog(QString::fromUtf8("[СКАЧИВАНИЕ ОШИБКА] %1").arg(errorStr), "#EF4444");
    }
}

void AppleIpswWidget::onSelectLocalIpswClicked()
{
    QString path = QFileDialog::getOpenFileName(this, QString::fromUtf8("Выберите файл прошивки Apple IPSW"),
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
        QString::fromUtf8("Apple Firmware (*.ipsw)"));

    if(!path.isEmpty())
    {
        m_selectedIpswPath = path;
        m_isLocalIpsw = true;

        QFileInfo fi(path);
        if(m_lblSelectedIpsw)
        {
            m_lblSelectedIpsw->setText(QString::fromUtf8("✔ Локальный IPSW выбран: %1 (%2)")
                .arg(fi.fileName(), formatFileSize(fi.size())));
            m_lblSelectedIpsw->setStyleSheet("color: #34D399; font-size: 11.5px; padding: 4px; background: rgba(52,211,153,0.08); border: 1px solid #059669;");
        }

        appendLog(QString::fromUtf8("[ЛОКАЛЬНЫЙ IPSW] Выбран файл: %1").arg(path), "#00E5FF");
    }
}

void AppleIpswWidget::onStartFlashClicked()
{
    if(m_device.isEmpty())
    {
        QMessageBox::warning(this, QString::fromUtf8("Устройство не подключено"),
            QString::fromUtf8("Пожалуйста, подключите устройство Apple через USB в обычном режиме, Recovery или DFU."));
        return;
    }

    if(m_selectedIpswPath.isEmpty() || !QFile::exists(m_selectedIpswPath))
    {
        QMessageBox::warning(this, QString::fromUtf8("Файл прошивки не найден"),
            QString::fromUtf8("Пожалуйста, скачайте прошивку из таблицы либо укажите локальный файл .ipsw."));
        return;
    }

    bool retain = m_rbRetainData ? m_rbRetainData->isChecked() : true;
    QString confirmText = retain
        ? QString::fromUtf8("Начать ОБНОВЛЕНИЕ устройства %1 с сохранением данных?\n\nФайл: %2")
            .arg(!m_device.displayName.isEmpty() ? m_device.displayName : m_device.marketingName, QFileInfo(m_selectedIpswPath).fileName())
        : QString::fromUtf8("ВНИМАНИЕ! Выбран режим ЧИСТОЙ ПРОШИВКИ.\n\n"
                            "Все данные пользователя на устройстве %1 будут ПОЛНОСТЬЮ СТЕРТЫ!\n\n"
                            "Продолжить?").arg(!m_device.displayName.isEmpty() ? m_device.displayName : m_device.marketingName);

    auto reply = QMessageBox::question(this, QString::fromUtf8("Подтверждение прошивки iOS"),
        confirmText, QMessageBox::Yes | QMessageBox::No);

    if(reply != QMessageBox::Yes) return;

    if(m_btnStartFlash) m_btnStartFlash->setEnabled(false);
    if(m_btnCancelFlash) m_btnCancelFlash->setEnabled(true);
    if(m_btnAdvRetry) m_btnAdvRetry->setVisible(false);
    if(m_progressBar) m_progressBar->setValue(0);

    appendLog(QString::fromUtf8("=================================================="), "#71717A");
    appendLog(QString::fromUtf8("[СТАРТ] Запуск процесса прошивки %1...").arg(!m_device.displayName.isEmpty() ? m_device.displayName : m_device.marketingName), "#38BDF8");
    appendLog(QString::fromUtf8("[РЕЖИМ] %1").arg(retain ? "Обновление с сохранением данных" : "Полная чистая прошивка со сбросом"), "#38BDF8");
    appendLog(QString::fromUtf8("[ФАЙЛ] %1").arg(m_selectedIpswPath), "#38BDF8");

    m_restoreWorker->startRestore(m_selectedIpswPath, retain, m_device.devId);
}

void AppleIpswWidget::onCancelFlashClicked()
{
    auto reply = QMessageBox::question(this, QString::fromUtf8("Отмена операции"),
        QString::fromUtf8("Вы уверены, что хотите прервать текущую операцию?"),
        QMessageBox::Yes | QMessageBox::No);

    if(reply == QMessageBox::Yes)
    {
        m_downloader->cancelDownload();
        m_restoreWorker->abortRestore();
        if(m_btnCancelFlash) m_btnCancelFlash->setEnabled(false);
        if(m_btnStartFlash) m_btnStartFlash->setEnabled(true);
        if(m_btnSimpleCancel) m_btnSimpleCancel->setEnabled(false);
        if(m_btnSimpleRetry) m_btnSimpleRetry->setVisible(true);
        if(m_btnAdvRetry) m_btnAdvRetry->setVisible(true);
        if(m_lblProgressStatus) m_lblProgressStatus->setText(QString::fromUtf8("Операция прервана."));
        if(m_lblSimpleProgressStatus) m_lblSimpleProgressStatus->setText(QString::fromUtf8("Операция прервана."));
        appendLog(QString::fromUtf8("[ПРЕРВАНО] Операция отменена пользователем."), "#EF4444");
    }
}

void AppleIpswWidget::onRestoreProgress(int percent, const QString &stageText)
{
    if(m_progressBar) m_progressBar->setValue(percent);
    if(m_simpleProgressBar) m_simpleProgressBar->setValue(percent);
    QString text = QString("%1% • %2").arg(percent).arg(stageText);
    if(m_lblProgressStatus) m_lblProgressStatus->setText(text);
    if(m_lblSimpleProgressStatus) m_lblSimpleProgressStatus->setText(text);
}

void AppleIpswWidget::onRestoreLog(const QString &line)
{
    QString color = "#E0E0E0";
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
    if(m_btnStartFlash) m_btnStartFlash->setEnabled(true);
    if(m_btnCancelFlash) m_btnCancelFlash->setEnabled(false);
    if(m_btnSimpleCancel) m_btnSimpleCancel->setEnabled(false);
    if(m_btnSimpleRetry) m_btnSimpleRetry->setVisible(true);
    if(m_btnAdvRetry) m_btnAdvRetry->setVisible(true);

    if(success)
    {
        if(m_progressBar) m_progressBar->setValue(100);
        if(m_simpleProgressBar) m_simpleProgressBar->setValue(100);
        if(m_lblProgressStatus) m_lblProgressStatus->setText(QString::fromUtf8("Прошивка успешно завершена!"));
        if(m_lblSimpleProgressStatus) m_lblSimpleProgressStatus->setText(QString::fromUtf8("✔ Прошивка успешно завершена!"));
        appendLog(QString::fromUtf8("[УСПЕХ] Устройство успешно прошито и перезагружается!"), "#10B981");
        QMessageBox::information(this, QString::fromUtf8("Успех"),
            QString::fromUtf8("Устройство Apple успешно прошито!\nДождитесь появления логотипа Apple и полосы прогресса на экране телефона."));
    }
    else
    {
        if(m_lblProgressStatus) m_lblProgressStatus->setText(QString::fromUtf8("Ошибка прошивки: %1").arg(errorStr));
        if(m_lblSimpleProgressStatus) m_lblSimpleProgressStatus->setText(QString::fromUtf8("Ошибка прошивки: %1").arg(errorStr));
        appendLog(QString::fromUtf8("[ОШИБКА] %1").arg(errorStr), "#EF4444");
        QMessageBox::critical(this, QString::fromUtf8("Ошибка прошивки"),
            QString::fromUtf8("Процесс прошивки завершился ошибкой:\n%1\n\nВы можете повторить попытку нажав «Повторить».").arg(errorStr));
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
    if(m_device.devId.isEmpty()) return;
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
}
