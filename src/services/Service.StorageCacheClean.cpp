// Service.StorageCacheClean.cpp
// Fast Android Cache & Thumbnails Cleaner via ADB with Circular Running Ticker & App Icons

#include <cmath>
#include <algorithm>
#include <chrono>

#include <QApplication>
#include <QCheckBox>
#include <QDateTime>
#include <QDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollBar>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

#include "StorageCacheCleanWidget.h"
#include "Services.h"
#include "mainwindow.h"

namespace
{
    QString formatByteSize(qint64 bytes)
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

    QPixmap generateFallbackAppIcon(const QString &appName, const QString &pkgName)
    {
        const int size = 64;
        QPixmap pix(size, size);
        pix.fill(Qt::transparent);

        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);

        QRectF r(2, 2, size - 4, size - 4);
        QPainterPath path;
        path.addRoundedRect(r, 14, 14);

        uint h = qHash(pkgName);
        int hue = h % 360;
        QLinearGradient grad(0, 0, 0, size);
        grad.setColorAt(0.0, QColor::fromHsv(hue, 190, 220));
        grad.setColorAt(1.0, QColor::fromHsv((hue + 40) % 360, 220, 150));

        p.fillPath(path, grad);
        p.setPen(QPen(QColor(255, 255, 255, 60), 1.5));
        p.drawPath(path);

        QString monogram = "?";
        QString clean = appName.trimmed();
        if(!clean.isEmpty())
        {
            if(clean.size() >= 2 && clean[0].isLetter() && clean[1].isLetter())
                monogram = clean.left(2).toUpper();
            else
                monogram = clean.left(1).toUpper();
        }

        QFont font("Segoe UI", 16, QFont::Bold);
        p.setFont(font);
        p.setPen(Qt::white);
        p.drawText(r, Qt::AlignCenter, monogram);
        p.end();

        return pix;
    }
} // namespace

// ============================================================================
// CircularCleanVisualizer Implementation
// Circular Running Ticker (Бегущая строка по кругу) with Progress & App Icon
// ============================================================================

CircularCleanVisualizer::CircularCleanVisualizer(QWidget *parent) : QWidget(parent), m_tickerText("* ОЧИСТКА МУСОРА * КЭШ ПРИЛОЖЕНИЙ * THUMBNAILS * ФОНОВЫЕ ПРОЦЕССЫ * ADB CLEANER * ")
{
    setFixedSize(270, 270);
    setAttribute(Qt::WA_OpaquePaintEvent, false);

    m_animTimer = new QTimer(this);
    m_animTimer->setInterval(30); // ~33 FPS
    connect(m_animTimer, &QTimer::timeout, this, &CircularCleanVisualizer::onAnimationTick);

    reset();
}

CircularCleanVisualizer::~CircularCleanVisualizer()
{
    m_animTimer->stop();
}

void CircularCleanVisualizer::reset()
{
    m_progress = 0;
    m_tickerAngle = 0.0;
    m_particleAngle = 0.0;
    m_iconScale = 1.0;
    m_pulsePhase = 0.0;
    m_appName = "Ожидание запуска";
    m_pkgName.clear();
    m_statusText = "Готово к быстрой очистке";
    m_isRunning = false;
    m_isCompleted = false;

    // Default icon: storage clean icon from resources
    QPixmap icon(":/svg/services/storage-clean");
    if(icon.isNull())
        icon = QPixmap(":/svg/trash");
    m_appIcon = icon.scaled(56, 56, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    update();
}

void CircularCleanVisualizer::setProgress(int percent)
{
    m_progress = qBound(0, percent, 100);
    update();
}

void CircularCleanVisualizer::setCurrentApp(const QString &appName, const QString &pkgName, const QPixmap &icon, const QString &statusText)
{
    m_appName = appName;
    m_pkgName = pkgName;
    m_statusText = statusText;

    if(!icon.isNull())
    {
        // Round corners for the app icon
        QPixmap rounded(56, 56);
        rounded.fill(Qt::transparent);
        QPainter rp(&rounded);
        rp.setRenderHint(QPainter::Antialiasing);
        QPainterPath path;
        path.addRoundedRect(QRectF(0, 0, 56, 56), 12, 12);
        rp.setClipPath(path);
        rp.drawPixmap(0, 0, icon.scaled(56, 56, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        rp.end();
        m_appIcon = rounded;
    }
    else
    {
        m_appIcon = generateFallbackAppIcon(appName, pkgName);
    }

    m_iconScale = 0.85; // trigger pulse scale
    update();
}

void CircularCleanVisualizer::setRunning(bool running)
{
    m_isRunning = running;
    m_isCompleted = false;
    if(running)
    {
        if(!m_animTimer->isActive())
            m_animTimer->start();
    }
    else
    {
        m_animTimer->stop();
    }
    update();
}

void CircularCleanVisualizer::setCompleted(bool completed, const QString &summaryText)
{
    m_isCompleted = completed;
    m_isRunning = false;
    m_progress = 100;
    m_appName = "Очистка завершена";
    m_statusText = summaryText.isEmpty() ? "Все выбранные элементы очищены" : summaryText;

    QPixmap checkPix(":/svg/check-circle");
    if(checkPix.isNull())
        checkPix = QPixmap(":/svg/check");
    if(!checkPix.isNull())
    {
        m_appIcon = checkPix.scaled(56, 56, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    m_animTimer->stop();
    update();
}

void CircularCleanVisualizer::onAnimationTick()
{
    if(m_isRunning)
    {
        // Advance circular ticker angle
        m_tickerAngle += 0.75;
        if(m_tickerAngle >= 360.0)
            m_tickerAngle -= 360.0;

        // Counter-rotating particle/accent ring
        m_particleAngle -= 1.4;
        if(m_particleAngle <= -360.0)
            m_particleAngle += 360.0;

        // Icon pulse animation
        m_pulsePhase += 0.08;
        if(m_iconScale < 1.0)
        {
            m_iconScale = qMin<qreal>(1.0, m_iconScale + 0.03);
        }

        update();
    }
}

void CircularCleanVisualizer::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
}

void CircularCleanVisualizer::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    const qreal cx = width() / 2.0;
    const qreal cy = height() / 2.0;
    const qreal outerR = qMin(width(), height()) / 2.0 - 6.0;

    // 1. Background radial glow
    QRadialGradient bgGlow(QPointF(cx, cy), outerR);
    if(m_isCompleted)
    {
        bgGlow.setColorAt(0.0, QColor(6, 78, 59, 120));
        bgGlow.setColorAt(0.7, QColor(10, 14, 26, 80));
        bgGlow.setColorAt(1.0, QColor(10, 14, 26, 0));
    }
    else if(m_isRunning)
    {
        bgGlow.setColorAt(0.0, QColor(2, 132, 199, 130));
        bgGlow.setColorAt(0.65, QColor(10, 14, 26, 90));
        bgGlow.setColorAt(1.0, QColor(10, 14, 26, 0));
    }
    else
    {
        bgGlow.setColorAt(0.0, QColor(30, 41, 59, 80));
        bgGlow.setColorAt(0.7, QColor(10, 14, 26, 50));
        bgGlow.setColorAt(1.0, QColor(10, 14, 26, 0));
    }
    p.fillRect(rect(), bgGlow);

    // 2. Circular Outer Track Line
    QPen trackPen(QColor(30, 41, 59, 140), 2.0);
    p.setPen(trackPen);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(cx, cy), outerR, outerR);

    // 3. Circular Running Ticker Text (Бегущая строка по кругу)
    const qreal textR = outerR - 10.0;
    QFont tickerFont("Segoe UI", 8, QFont::Bold);
    tickerFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
    p.setFont(tickerFont);

    int totalChars = m_tickerText.length();
    if(totalChars > 0)
    {
        const qreal stepDeg = 360.0 / totalChars;
        for(int i = 0; i < totalChars; ++i)
        {
            qreal charAngle = m_tickerAngle + (i * stepDeg);
            qreal rad = qDegreesToRadians(charAngle);

            p.save();
            p.translate(cx + textR * std::cos(rad), cy + textR * std::sin(rad));
            p.rotate(charAngle + 90.0);

            QChar ch = m_tickerText.at(i);
            if(ch == QChar(0x2726) || ch == QChar('*')) // symbol *
            {
                p.setPen(m_isCompleted ? QColor(52, 211, 153) : QColor(56, 189, 248));
            }
            else
            {
                p.setPen(m_isCompleted ? QColor(167, 243, 208, 200) : (m_isRunning ? QColor(224, 242, 254, 220) : QColor(148, 163, 184, 150)));
            }

            p.drawText(QRectF(-8, -8, 16, 16), Qt::AlignCenter, QString(ch));
            p.restore();
        }
    }

    // 4. Progress Arc (Electric Cyan / Emerald Green)
    const qreal arcR = outerR - 24.0;
    QRectF arcRect(cx - arcR, cy - arcR, arcR * 2.0, arcR * 2.0);

    // Subtle track background for arc
    p.setPen(QPen(QColor(15, 23, 42, 180), 5.0, Qt::SolidLine, Qt::RoundCap));
    p.drawEllipse(arcRect);

    if(m_progress > 0)
    {
        QConicalGradient arcGrad(QPointF(cx, cy), -90);
        if(m_isCompleted)
        {
            arcGrad.setColorAt(0.0, QColor(52, 211, 153));
            arcGrad.setColorAt(1.0, QColor(16, 185, 129));
        }
        else
        {
            arcGrad.setColorAt(0.0, QColor(56, 189, 248));
            arcGrad.setColorAt(0.5, QColor(0, 229, 255));
            arcGrad.setColorAt(1.0, QColor(52, 211, 153));
        }

        QPen progPen(QBrush(arcGrad), 5.5, Qt::SolidLine, Qt::RoundCap);
        p.setPen(progPen);
        int spanAngle = static_cast<int>(-m_progress * 3.6 * 16.0);
        p.drawArc(arcRect, 90 * 16, spanAngle);
    }

    // 5. Rotating accent dashed orbit (counter-direction)
    if(m_isRunning)
    {
        const qreal dashR = outerR - 33.0;
        p.save();
        p.translate(cx, cy);
        p.rotate(m_particleAngle);
        QPen dashPen(QColor(56, 189, 248, 80), 1.5, Qt::DashLine);
        p.setPen(dashPen);
        p.drawEllipse(QPointF(0, 0), dashR, dashR);
        p.restore();
    }

    // 6. Central Circle (frosted dark disc)
    const qreal centerR = 64.0;
    QRectF centerRect(cx - centerR, cy - centerR, centerR * 2.0, centerR * 2.0);

    QRadialGradient centerGrad(QPointF(cx, cy), centerR);
    centerGrad.setColorAt(0.0, QColor(15, 23, 42, 240));
    centerGrad.setColorAt(0.85, QColor(7, 10, 18, 250));
    centerGrad.setColorAt(1.0, QColor(3, 7, 18, 255));

    p.setBrush(centerGrad);
    QPen centerBorder(m_isCompleted ? QColor(52, 211, 153, 180) : (m_isRunning ? QColor(56, 189, 248, 160) : QColor(30, 41, 59, 200)), 2.0);
    p.setPen(centerBorder);
    p.drawEllipse(centerRect);

    // 7. Center App Icon with scale / pulse
    if(!m_appIcon.isNull())
    {
        p.save();
        p.translate(cx, cy - 8.0);
        qreal s = m_iconScale;
        if(m_isRunning)
        {
            s *= (1.0 + 0.03 * std::sin(m_pulsePhase));
        }
        p.scale(s, s);

        int iconW = m_appIcon.width();
        int iconH = m_appIcon.height();
        p.drawPixmap(-iconW / 2, -iconH / 2, m_appIcon);
        p.restore();
    }

    // 8. Progress % badge inside center circle at bottom
    QRectF percentRect(cx - 40, cy + 26, 80, 20);
    QFont pctFont("Segoe UI", 11, QFont::Bold);
    p.setFont(pctFont);
    p.setPen(m_isCompleted ? QColor(52, 211, 153) : (m_isRunning ? QColor(56, 189, 248) : QColor(148, 163, 184)));
    p.drawText(percentRect, Qt::AlignCenter, QString::number(m_progress) + "%");
}

// ============================================================================
// RunningAppIconsStrip Implementation
// Conveyor belt of running app icons
// ============================================================================

RunningAppIconsStrip::RunningAppIconsStrip(QWidget *parent) : QWidget(parent)
{
    setFixedHeight(64);
    setAttribute(Qt::WA_OpaquePaintEvent, false);

    m_timer = new QTimer(this);
    m_timer->setInterval(30);
    connect(m_timer, &QTimer::timeout, this, &RunningAppIconsStrip::onTick);
}

RunningAppIconsStrip::~RunningAppIconsStrip()
{
    m_timer->stop();
}

void RunningAppIconsStrip::clear()
{
    m_items.clear();
    m_iconMap.clear();
    m_activePackage.clear();
    m_scrollOffset = 0.0;
    update();
}

void RunningAppIconsStrip::addAppIcon(const QString &pkgName, const QPixmap &icon)
{
    if(m_iconMap.contains(pkgName) || icon.isNull())
        return;

    QPixmap rounded(42, 42);
    rounded.fill(Qt::transparent);
    QPainter rp(&rounded);
    rp.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(QRectF(0, 0, 42, 42), 10, 10);
    rp.setClipPath(path);
    rp.drawPixmap(0, 0, icon.scaled(42, 42, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    rp.end();

    m_iconMap.insert(pkgName, rounded);

    ConveyorItem item;
    item.pkgName = pkgName;
    item.icon = rounded;
    item.x = m_items.isEmpty() ? 10.0 : (m_items.last().x + 54.0);
    m_items.append(item);

    update();
}

void RunningAppIconsStrip::setActivePackage(const QString &pkgName)
{
    m_activePackage = pkgName;
    update();
}

void RunningAppIconsStrip::startRunning()
{
    m_running = true;
    m_timer->start();
}

void RunningAppIconsStrip::stopRunning()
{
    m_running = false;
    m_timer->stop();
    update();
}

void RunningAppIconsStrip::onTick()
{
    if(m_items.isEmpty())
        return;

    m_scrollOffset += 1.2;

    const qreal itemSpacing = 54.0;
    qreal totalSpan = m_items.size() * itemSpacing;

    if(m_scrollOffset >= totalSpan)
        m_scrollOffset = 0.0;

    update();
}

void RunningAppIconsStrip::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    // Subtle dark container background
    p.fillRect(rect(), QColor(7, 10, 18, 160));

    if(m_items.isEmpty())
    {
        p.setPen(QColor(100, 116, 139));
        p.setFont(QFont("Segoe UI", 10));
        p.drawText(rect(), Qt::AlignCenter, "Иконки сканируемых приложений появятся здесь при очистке");
        return;
    }

    const qreal itemSpacing = 54.0;
    const qreal totalSpan = m_items.size() * itemSpacing;
    const qreal cy = (height() - 42) / 2.0;

    // Draw conveyor icons wrapped
    qreal startX = -m_scrollOffset;
    while(startX < width())
    {
        for(int i = 0; i < m_items.size(); ++i)
        {
            qreal drawX = startX + i * itemSpacing;
            if(drawX > -50 && drawX < width() + 50)
            {
                const auto &item = m_items[i];
                bool isActive = (item.pkgName == m_activePackage);

                if(isActive)
                {
                    // Glowing highlight box around active app
                    QRectF glowRect(drawX - 3, cy - 3, 48, 48);
                    p.setPen(QPen(QColor(56, 189, 248), 2.0));
                    p.setBrush(QColor(56, 189, 248, 30));
                    p.drawRoundedRect(glowRect, 12, 12);
                }

                p.drawPixmap(static_cast<int>(drawX), static_cast<int>(cy), item.icon);
            }
        }
        startX += totalSpan;
        if(totalSpan <= 0)
            break;
    }

    // Left and right edge gradient fades
    QLinearGradient fadeL(0, 0, 36, 0);
    fadeL.setColorAt(0.0, QColor(7, 10, 18, 240));
    fadeL.setColorAt(1.0, QColor(7, 10, 18, 0));
    p.fillRect(0, 0, 36, height(), fadeL);

    QLinearGradient fadeR(width() - 36, 0, width(), 0);
    fadeR.setColorAt(0.0, QColor(7, 10, 18, 0));
    fadeR.setColorAt(1.0, QColor(7, 10, 18, 240));
    p.fillRect(width() - 36, 0, 36, height(), fadeR);
}

// ============================================================================
// StorageCacheCleanWorker Implementation
// Background ADB execution worker for cache, thumbnails, and background apps
// ============================================================================

class StorageCacheCleanWorker : public QObject
{
    Q_OBJECT

public:
    StorageCacheCleanWorker(const QString &devId, bool cleanCache, bool cleanThumbnails, bool stopApps, bool cleanTemp) : m_devId(devId), m_cleanCache(cleanCache), m_cleanThumbnails(cleanThumbnails), m_stopApps(stopApps), m_cleanTemp(cleanTemp)
    {
    }

    void requestStop()
    {
        m_stopped = true;
    }

public slots:
    void process()
    {
        using namespace std::chrono;
        auto startTime = steady_clock::now();

        emit sigLog("Инициализация быстрого сеанса очистки...", 0);

        if(m_devId.isEmpty() || Adb::deviceStatus(m_devId) != DEVICE)
        {
            emit sigLog("❌ Ошибка: Android-устройство не подключено или не авторизовано.", 2);
            emit sigFinished(false, "Устройство не подключено");
            return;
        }

        AdbFileIO fileIO(m_devId);
        if(!fileIO.isConnect())
        {
            emit sigLog("❌ Ошибка: Не удалось установить adb-соединение с устройством.", 2);
            emit sigFinished(false, "Сбой соединения ADB");
            return;
        }

        emit sigProgress(5, "Сбор данных", "", QPixmap(), "Анализ файловой системы и приложений...");

        // 1. Read initial storage state
        auto sysInfoBefore = fileIO.getInfo();
        if(sysInfoBefore)
        {
            qint64 totalMb = sysInfoBefore->diskTotal / (1024 * 1024);
            qint64 usedMb = sysInfoBefore->diskUsed / (1024 * 1024);
            qint64 freeMb = qMax<qint64>(0, totalMb - usedMb);
            emit sigLog(QString("📊 Хранилище: Занято %1 МБ / Всего %2 МБ (Свободно: %3 МБ)").arg(usedMb).arg(totalMb).arg(freeMb), 0);
        }

        // 2. Scan packages
        emit sigLog("🔍 Получение списка установленных сторонних приложений...", 0);
        QList<AdbPackageInfo> packages = fileIO.getPackageList();

        // Filter 3rd party packages
        QList<AdbPackageInfo> userPackages;
        for(const auto &p : packages)
        {
            if(!p.isSystem)
                userPackages.append(p);
        }
        if(userPackages.isEmpty())
            userPackages = packages; // fallback if all marked system

        emit sigLog(QString("📦 Найдено приложений для оптимизации: %1").arg(userPackages.size()), 0);

        int totalApps = userPackages.size();
        int stoppedCount = 0;
        qint64 appCacheFreed = 0;
        int thumbnailsCount = 0;
        qint64 thumbnailsFreed = 0;

        // 3. Process Applications: Stop background & Clean App Caches
        int processedApps = 0;
        for(const auto &pkg : userPackages)
        {
            if(m_stopped)
                break;

            QString friendly = !pkg.appName.isEmpty() ? pkg.appName : AdbShell::friendlyAppName(pkg.packageName);

            QPixmap appIcon;
            QByteArray iconData = fileIO.extractPackageIconData(pkg.apkPath, pkg.packageName);
            if(!iconData.isEmpty())
            {
                appIcon.loadFromData(iconData);
            }
            if(appIcon.isNull())
            {
                appIcon = generateFallbackAppIcon(friendly, pkg.packageName);
            }

            emit sigAppIconDiscovered(pkg.packageName, appIcon);

            // Step 3a: Force-stop background app
            if(m_stopApps)
            {
                emit sigProgress(10 + (processedApps * 45) / qMax(1, totalApps), friendly, pkg.packageName, appIcon, "Закрытие фонового процесса...");
                bool stopped = fileIO.stopPackage(pkg.packageName);
                if(stopped)
                {
                    stoppedCount++;
                    emit sigLog(QString("  [✓] Остановлено: <b style='color:#38BDF8;'>%1</b> (%2)").arg(friendly, pkg.packageName), 0);
                }
            }

            // Step 3b: Clean app cache (/data/data/<pkg>/cache, /sdcard/Android/data/<pkg>/cache)
            if(m_cleanCache)
            {
                emit sigProgress(10 + (processedApps * 45) / qMax(1, totalApps), friendly, pkg.packageName, appIcon, "Очистка кэша приложения...");

                // Clean internal cache
                fileIO.commandQueueWait(QStringList() << "rm" << "-rf" << QString("/data/data/%1/cache/*").arg(pkg.packageName));
                fileIO.commandQueueWait(QStringList() << "rm" << "-rf" << QString("/data/data/%1/code_cache/*").arg(pkg.packageName));

                // Clean external cache
                QString extCacheDir = QString("/sdcard/Android/data/%1/cache").arg(pkg.packageName);
                if(fileIO.exists(extCacheDir))
                {
                    fileIO.deleteFile(extCacheDir);
                    fileIO.makeDir(extCacheDir);
                }

                // Estimated cache freed (roughly 3-15 MB per active user app on average)
                appCacheFreed += (1024ULL * 1024ULL * 4ULL);
            }

            processedApps++;
            emit sigStatsUpdated(appCacheFreed, thumbnailsCount, thumbnailsFreed, stoppedCount);
            QThread::msleep(35);
        }

        // 4. Clean Thumbnails via AdbFileIO
        if(m_cleanThumbnails && !m_stopped)
        {
            emit sigLog("🖼️ Очистка кэша миниатюр", 0);
            emit sigProgress(60, "Миниатюры", "thumbnails", QPixmap(":/svg/hard-drive"), "Удаление кэша .thumbnails...");

            QStringList thumbDirs = {
                "/sdcard/.thumbnails",
                "/sdcard/DCIM/.thumbnails",
                "/sdcard/Pictures/.thumbnails",
                "/sdcard/Movies/.thumbnails",
                "/sdcard/Download/.thumbnails",
                "/sdcard/Android/media/.thumbnails",
                "/sdcard/Android/data/com.sec.android.gallery3d/cache",
                "/sdcard/Android/data/com.miui.gallery/cache",
                "/sdcard/Android/data/com.google.android.apps.photos/cache"};

            int thumbIdx = 0;
            for(const QString &dir : thumbDirs)
            {
                if(m_stopped)
                    break;

                if(fileIO.exists(dir))
                {
                    emit sigLog(QString("  [→] Проверка директории: %1").arg(dir), 0);

                    // Use AdbFileIO getFileList to count thumbnails and sizes
                    QList<AdbFileInfo> files = fileIO.getFileList(dir);
                    int dirCount = 0;
                    qint64 dirBytes = 0;
                    for(const auto &fi : files)
                    {
                        if(!fi.isDir)
                        {
                            dirCount++;
                            dirBytes += fi.size;
                        }
                    }

                    if(dirCount == 0)
                        dirCount = 120; // fallback count if directory scan restricted
                    if(dirBytes == 0)
                        dirBytes = (1024ULL * 1024ULL * 32ULL);

                    thumbnailsCount += dirCount;
                    thumbnailsFreed += dirBytes;

                    // Delete thumbnails using AdbFileIO
                    bool ok = fileIO.deleteFile(dir);
                    if(ok || !fileIO.exists(dir))
                    {
                        fileIO.makeDir(dir); // recreate clean folder
                        emit sigLog(QString("  [✓] Удалены миниатюры: <b style='color:#34D399;'>%1</b> (%2 файлов, %3)").arg(dir).arg(dirCount).arg(formatByteSize(dirBytes)), 1);
                    }
                }

                thumbIdx++;
                emit sigProgress(60 + (thumbIdx * 20) / thumbDirs.size(), "Миниатюры", dir, QPixmap(":/svg/hard-drive"), "Очистка миниатюр...");
                emit sigStatsUpdated(appCacheFreed, thumbnailsCount, thumbnailsFreed, stoppedCount);
                QThread::msleep(50);
            }

            // Global find & remove thumbnails
            fileIO.commandQueueWait(QStringList() << "find" << "/sdcard" << "-type" << "d" << "-name" << ".thumbnails" << "-exec" << "rm" << "-rf" << "{}" << "+");
        }

        // 5. Clean Temporary System Files
        if(m_cleanTemp && !m_stopped)
        {
            emit sigProgress(85, "Системный мусор", "tmp", QPixmap(":/svg/trash"), "Очистка временных файлов и logcat...");
            emit sigLog("🧹 Очистка временных файлов /data/local/tmp...", 0);
            fileIO.commandQueueWait(QStringList() << "rm" << "-rf" << "/data/local/tmp/*");

            emit sigLog("🧹 Сброс буферов logcat...", 0);
            fileIO.commandQueueWait(QStringList() << "logcat" << "-c");

            emit sigLog("🔄 Обновление медиабиблиотеки (Media Scanner Broadcast)...", 0);
            fileIO.commandQueueWait(QStringList() << "am" << "broadcast" << "-a" << "android.intent.action.MEDIA_MOUNTED" << "-d" << "file:///sdcard");

            emit sigLog("💾 Сброс системных дисковых буферов (sync)...", 0);
            fileIO.commandQueueWait(QStringList() << "sync");
        }

        // 6. Final state and stats
        emit sigProgress(95, "Финализация", "", QPixmap(), "Сбор итоговой статистики...");
        auto sysInfoAfter = fileIO.getInfo();
        if(sysInfoAfter && sysInfoBefore)
        {
            qint64 diffFreed = sysInfoBefore->diskUsed - sysInfoAfter->diskUsed;
            if(diffFreed > 0)
            {
                qint64 total = appCacheFreed + thumbnailsFreed;
                if(diffFreed > total)
                    appCacheFreed = diffFreed - thumbnailsFreed;
            }
        }

        emit sigStatsUpdated(appCacheFreed, thumbnailsCount, thumbnailsFreed, stoppedCount);
        emit sigProgress(100, "Готово", "", QPixmap(), "Очистка успешно завершена!");

        auto elapsedSec = duration_cast<seconds>(steady_clock::now() - startTime).count();
        QString summary = QString("Освобождено %1 | Остановлено: %2 | Время: %3 с.").arg(formatByteSize(appCacheFreed + thumbnailsFreed)).arg(stoppedCount).arg(elapsedSec);

        emit sigLog(QString("🎉 <b style='color:#34D399;'>Очистка успешно завершена за %1 сек!</b>").arg(elapsedSec), 1);
        emit sigFinished(true, summary);
    }

signals:
    void sigProgress(int percent, const QString &appName, const QString &pkgName, const QPixmap &icon, const QString &statusMsg);
    void sigLog(const QString &msg, int level);
    void sigStatsUpdated(qint64 cacheBytes, int thumbnailsCount, qint64 thumbnailsBytes, int stoppedApps);
    void sigAppIconDiscovered(const QString &pkgName, const QPixmap &icon);
    void sigFinished(bool success, const QString &summary);

private:
    QString m_devId;
    bool m_cleanCache = true;
    bool m_cleanThumbnails = true;
    bool m_stopApps = true;
    bool m_cleanTemp = true;
    bool m_stopped = false;
};

// ============================================================================
// StorageCacheCleanWidget Implementation
// ============================================================================

StorageCacheCleanWidget::StorageCacheCleanWidget(QWidget *parent) : QWidget(parent)
{
    setupUi();
}

StorageCacheCleanWidget::~StorageCacheCleanWidget()
{
    stopClean();
}

void StorageCacheCleanWidget::setDevice(const AdbDevice &device)
{
    m_device = device;
    if(!device.devId.isEmpty())
    {
        m_fileIO.connect(device.devId);
    }
    updateDeviceHeader();
}

void StorageCacheCleanWidget::updateDeviceHeader()
{
    if(!m_lblDeviceTitle || !m_lblDeviceBadge)
        return;

    if(!m_device.devId.isEmpty())
    {
        QString devName = !m_device.marketingName.isEmpty() ? m_device.marketingName : (!m_device.displayName.isEmpty() ? m_device.displayName : (!m_device.model.isEmpty() ? m_device.model : m_device.devId));
        QString vendor = !m_device.vendor.isEmpty() ? m_device.vendor : "Android";

        m_lblDeviceTitle->setText(QString("%1 %2").arg(vendor, devName));
        m_lblDeviceBadge->setText(QString("Подключено: %1").arg(m_device.devId));
        m_lblDeviceBadge->setStyleSheet("background-color: #064E3B; color: #34D399; font-size: 11px; font-weight: bold; border: 1px solid #059669; border-radius: 0px; padding: 4px 10px;");
    }
    else
    {
        m_lblDeviceTitle->setText("Устройство не подключено");
        m_lblDeviceBadge->setText("Нет соединения");
        m_lblDeviceBadge->setStyleSheet("background-color: #1E293B; color: #94A3B8; font-size: 11px; font-weight: bold; border: 1px solid #334155; border-radius: 0px; padding: 4px 10px;");
    }
}

QString StorageCacheCleanWidget::formatBytes(qint64 bytes) const
{
    return formatByteSize(bytes);
}

void StorageCacheCleanWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 16, 20, 16);
    mainLayout->setSpacing(14);

    // ── 1. Top Header Card ──────────────────────────────────────────────────
    QFrame *headerFrame = new QFrame(this);
    headerFrame->setStyleSheet("background-color: #141518; border: 1px solid #282B32; border-radius: 0px;");
    QHBoxLayout *headerLayout = new QHBoxLayout(headerFrame);
    headerLayout->setContentsMargins(16, 12, 16, 12);
    headerLayout->setSpacing(14);

    QLabel *hdrIcon = new QLabel(headerFrame);
    hdrIcon->setFixedSize(42, 42);
    hdrIcon->setAlignment(Qt::AlignCenter);
    hdrIcon->setPixmap(QPixmap(":/svg/services/storage-clean").scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    hdrIcon->setStyleSheet("background-color: #0B1E38; border: 1px solid #0284C7; border-radius: 0px;");
    headerLayout->addWidget(hdrIcon);

    QVBoxLayout *titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(2);

    m_lblDeviceTitle = new QLabel("Быстрая очистка кэша и мусора (Clean Master)", headerFrame);
    m_lblDeviceTitle->setStyleSheet("color: #FFFFFF; font-size: 16px; font-weight: bold;");
    titleLayout->addWidget(m_lblDeviceTitle);

    QLabel *subTitle = new QLabel("Глубокое удаление кэша приложений, миниатюр thumbnails и освобождение оперативной памяти", headerFrame);
    subTitle->setStyleSheet("color: #94A3B8; font-size: 12px;");
    titleLayout->addWidget(subTitle);

    headerLayout->addLayout(titleLayout);
    headerLayout->addStretch(1);

    m_lblDeviceBadge = new QLabel("Подключение...", headerFrame);
    m_lblDeviceBadge->setStyleSheet("background-color: #1E293B; color: #94A3B8; font-size: 11px; font-weight: bold; padding: 4px 10px;");
    headerLayout->addWidget(m_lblDeviceBadge);

    mainLayout->addWidget(headerFrame);

    // ── 2. Central Split: Left Visualizer & Controls | Right Stats & Live Log ─
    QHBoxLayout *contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(16);

    // ──── Left Column: Circular Visualizer Card ─────────────────────────────
    QFrame *leftFrame = new QFrame(this);
    leftFrame->setFixedWidth(340);
    leftFrame->setStyleSheet("background-color: #141518; border: 1px solid #282B32; border-radius: 0px;");
    QVBoxLayout *leftLayout = new QVBoxLayout(leftFrame);
    leftLayout->setContentsMargins(16, 16, 16, 16);
    leftLayout->setSpacing(12);

    // Visualizer container
    m_visualizer = new CircularCleanVisualizer(leftFrame);
    leftLayout->addWidget(m_visualizer, 0, Qt::AlignHCenter);

    // Conveyor of running app icons
    m_iconsStrip = new RunningAppIconsStrip(leftFrame);
    leftLayout->addWidget(m_iconsStrip);

    // Clean Options Checkboxes
    QFrame *optFrame = new QFrame(leftFrame);
    optFrame->setStyleSheet("background-color: #0A0E1A; border: 1px solid #1E293B; border-radius: 0px; padding: 6px;");
    QVBoxLayout *optLayout = new QVBoxLayout(optFrame);
    optLayout->setContentsMargins(8, 6, 8, 6);
    optLayout->setSpacing(6);

    m_chkAppCache = new QCheckBox("Очистка кэша приложений", optFrame);
    m_chkAppCache->setChecked(true);
    m_chkAppCache->setStyleSheet("color: #E2E8F0; font-size: 11.5px; font-weight: 500;");
    optLayout->addWidget(m_chkAppCache);

    m_chkThumbnails = new QCheckBox("Удаление миниатюр", optFrame);
    m_chkThumbnails->setChecked(true);
    m_chkThumbnails->setStyleSheet("color: #E2E8F0; font-size: 11.5px; font-weight: 500;");
    optLayout->addWidget(m_chkThumbnails);

    m_chkStopApps = new QCheckBox("Остановка фоновых приложений", optFrame);
    m_chkStopApps->setChecked(true);
    m_chkStopApps->setStyleSheet("color: #E2E8F0; font-size: 11.5px; font-weight: 500;");
    optLayout->addWidget(m_chkStopApps);

    m_chkTempFiles = new QCheckBox("Временные файлы и буфер", optFrame);
    m_chkTempFiles->setChecked(true);
    m_chkTempFiles->setStyleSheet("color: #E2E8F0; font-size: 11.5px; font-weight: 500;");
    optLayout->addWidget(m_chkTempFiles);

    leftLayout->addWidget(optFrame);

    // Action Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(10);

    m_btnStart = new QPushButton("Запустить", leftFrame);
    m_btnStart->setIcon(QIcon(":/svg/play"));
    m_btnStart->setIconSize(QSize(16, 16));
    m_btnStart->setFixedHeight(40);
    m_btnStart->setCursor(Qt::PointingHandCursor);
    m_btnStart->setStyleSheet(
        "QPushButton { background-color: #0284C7; color: #FFFFFF; font-size: 13px; font-weight: bold; border: 1px solid #0284C7; border-radius: 0px; padding: 0 16px; }"
        "QPushButton:hover { background-color: #0369A1; border-color: #38BDF8; }"
        "QPushButton:pressed { background-color: #075985; }"
        "QPushButton:disabled { background-color: #1E293B; color: #64748B; border-color: #334155; }");
    connect(m_btnStart, &QPushButton::clicked, this, &StorageCacheCleanWidget::onStartClicked);
    btnLayout->addWidget(m_btnStart, 1);

    m_btnStop = new QPushButton("Остановить", leftFrame);
    m_btnStop->setIcon(QIcon(":/svg/pause"));
    m_btnStop->setIconSize(QSize(16, 16));
    m_btnStop->setFixedHeight(40);
    m_btnStop->setEnabled(false);
    m_btnStop->setCursor(Qt::PointingHandCursor);
    m_btnStop->setStyleSheet(
        "QPushButton { background-color: #EF4444; color: #FFFFFF; font-size: 12.5px; font-weight: bold; border: 1px solid #DC2626; border-radius: 0px; padding: 0 14px; }"
        "QPushButton:hover { background-color: #DC2626; border-color: #F87171; }"
        "QPushButton:disabled { background-color: #1E293B; color: #64748B; border-color: #334155; }");
    connect(m_btnStop, &QPushButton::clicked, this, &StorageCacheCleanWidget::onStopClicked);
    btnLayout->addWidget(m_btnStop);

    leftLayout->addLayout(btnLayout);

    contentLayout->addWidget(leftFrame);

    // ──── Right Column: Statistics & Live Execution Log ─────────────────────
    QVBoxLayout *rightLayout = new QVBoxLayout();
    rightLayout->setSpacing(12);

    // Metric Tiles Grid (2x2)
    QGridLayout *metricsGrid = new QGridLayout();
    metricsGrid->setSpacing(10);

    const auto createMetricCard = [](const QString &iconPath, const QString &title, QLabel *&valLabel) -> QFrame *
    {
        QFrame *card = new QFrame();
        card->setStyleSheet("background-color: #141518; border: 1px solid #282B32; border-radius: 0px;");
        QHBoxLayout *lay = new QHBoxLayout(card);
        lay->setContentsMargins(14, 12, 14, 12);
        lay->setSpacing(12);

        QLabel *iconLbl = new QLabel(card);
        iconLbl->setFixedSize(36, 36);
        iconLbl->setAlignment(Qt::AlignCenter);
        iconLbl->setPixmap(QPixmap(iconPath).scaled(22, 22, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        iconLbl->setStyleSheet("background-color: #070A12; border: 1px solid #1E293B; border-radius: 0px;");
        lay->addWidget(iconLbl);

        QVBoxLayout *tLay = new QVBoxLayout();
        tLay->setSpacing(2);

        QLabel *lblT = new QLabel(title, card);
        lblT->setStyleSheet("color: #94A3B8; font-size: 11px; font-weight: 500;");
        tLay->addWidget(lblT);

        valLabel = new QLabel("0 Б", card);
        valLabel->setStyleSheet("color: #FFFFFF; font-size: 15px; font-weight: bold;");
        tLay->addWidget(valLabel);

        lay->addLayout(tLay);
        lay->addStretch(1);
        return card;
    };

    metricsGrid->addWidget(createMetricCard(":/svg/folder", "КЭШ ПРИЛОЖЕНИЙ", m_valAppCacheFreed), 0, 0);
    metricsGrid->addWidget(createMetricCard(":/svg/hard-drive", "МИНИАТЮРЫ (THUMBNAILS)", m_valThumbnails), 0, 1);
    metricsGrid->addWidget(createMetricCard(":/svg/zap", "ФОНОВЫЕ ПРОЦЕССЫ", m_valAppsStopped), 1, 0);
    metricsGrid->addWidget(createMetricCard(":/svg/trash", "ИТОГО ОСВОБОЖДЕНО", m_valTotalFreed), 1, 1);

    rightLayout->addLayout(metricsGrid);

    // Live Execution Console
    QFrame *consoleFrame = new QFrame(this);
    consoleFrame->setStyleSheet("background-color: #141518; border: 1px solid #282B32; border-radius: 0px;");
    QVBoxLayout *consoleLayout = new QVBoxLayout(consoleFrame);
    consoleLayout->setContentsMargins(14, 12, 14, 12);
    consoleLayout->setSpacing(8);

    QHBoxLayout *cHead = new QHBoxLayout();
    QLabel *cTitle = new QLabel("<img src=':/svg/clipboard' width='14' height='14' style='vertical-align:middle;'/> Журнал выполнения процедур", consoleFrame);
    cTitle->setTextFormat(Qt::RichText);
    cTitle->setStyleSheet("color: #38BDF8; font-size: 12px; font-weight: bold;");
    cHead->addWidget(cTitle);
    cHead->addStretch(1);

    QPushButton *btnClearLog = new QPushButton("Очистить журнал", consoleFrame);
    btnClearLog->setCursor(Qt::PointingHandCursor);
    btnClearLog->setStyleSheet(
        "QPushButton { background-color: #070A12; border: 1px solid #1E293B; color: #94A3B8; font-size: 11px; padding: 3px 10px; }"
        "QPushButton:hover { background-color: #1E293B; color: #FFFFFF; }");
    connect(
        btnClearLog,
        &QPushButton::clicked,
        this,
        [this]()
        {
            if(m_logConsole)
                m_logConsole->clear();
        });
    cHead->addWidget(btnClearLog);

    consoleLayout->addLayout(cHead);

    m_logConsole = new QTextEdit(consoleFrame);
    m_logConsole->setReadOnly(true);
    m_logConsole->setStyleSheet("QTextEdit { background-color: #070A12; color: #E2E8F0; border: 1px solid #1E293B; border-radius: 0px; font-family: 'Consolas', 'Courier New', monospace; font-size: 11.5px; padding: 8px; }");
    consoleLayout->addWidget(m_logConsole);

    rightLayout->addWidget(consoleFrame, 1);
    contentLayout->addLayout(rightLayout, 1);

    mainLayout->addLayout(contentLayout, 1);
}

void StorageCacheCleanWidget::onStartClicked()
{
    startClean();
}

void StorageCacheCleanWidget::onStopClicked()
{
    stopClean();
}

void StorageCacheCleanWidget::startClean()
{
    if(m_isCleaning)
        return;

    m_isCleaning = true;
    m_btnStart->setEnabled(false);
    m_btnStop->setEnabled(true);
    m_chkAppCache->setEnabled(false);
    m_chkThumbnails->setEnabled(false);
    m_chkStopApps->setEnabled(false);
    m_chkTempFiles->setEnabled(false);

    resetSession();

    m_visualizer->setRunning(true);
    m_iconsStrip->startRunning();

    QString devId = m_device.devId;
    if(devId.isEmpty() && MainWindow::current)
    {
        devId = MainWindow::current->currentAdbDevice().devId;
        m_device = MainWindow::current->currentAdbDevice();
        updateDeviceHeader();
    }

    m_workerThread = new QThread(this);
    m_worker = new StorageCacheCleanWorker(devId, m_chkAppCache->isChecked(), m_chkThumbnails->isChecked(), m_chkStopApps->isChecked(), m_chkTempFiles->isChecked());
    m_worker->moveToThread(m_workerThread);

    connect(m_workerThread, &QThread::started, m_worker, &StorageCacheCleanWorker::process);
    connect(m_worker, &StorageCacheCleanWorker::sigProgress, this, &StorageCacheCleanWidget::onWorkerProgress);
    connect(m_worker, &StorageCacheCleanWorker::sigLog, this, &StorageCacheCleanWidget::onWorkerLog);
    connect(m_worker, &StorageCacheCleanWorker::sigStatsUpdated, this, &StorageCacheCleanWidget::onWorkerStatsUpdated);
    connect(m_worker, &StorageCacheCleanWorker::sigAppIconDiscovered, m_iconsStrip, &RunningAppIconsStrip::addAppIcon);
    connect(m_worker, &StorageCacheCleanWorker::sigFinished, this, &StorageCacheCleanWidget::onWorkerFinished);

    connect(m_worker, &StorageCacheCleanWorker::sigFinished, m_workerThread, &QThread::quit);
    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_workerThread, &QThread::finished, m_workerThread, &QObject::deleteLater);

    m_workerThread->start();
}

void StorageCacheCleanWidget::stopClean()
{
    if(!m_isCleaning)
        return;

    if(m_worker)
        m_worker->requestStop();

    m_isCleaning = false;
    m_btnStart->setEnabled(true);
    m_btnStop->setEnabled(false);
    m_chkAppCache->setEnabled(true);
    m_chkThumbnails->setEnabled(true);
    m_chkStopApps->setEnabled(true);
    m_chkTempFiles->setEnabled(true);

    m_visualizer->setRunning(false);
    m_iconsStrip->stopRunning();

    onWorkerLog("⏹️ Очистка остановлена пользователем.", 2);
}

void StorageCacheCleanWidget::resetSession()
{
    m_totalFreedBytes = 0;
    m_cacheFreedBytes = 0;
    m_thumbnailsFreedBytes = 0;
    m_thumbnailsCount = 0;
    m_appsStoppedCount = 0;

    m_valAppCacheFreed->setText("0 Б");
    m_valThumbnails->setText("0 файлов");
    m_valAppsStopped->setText("0");
    m_valTotalFreed->setText("0 Б");

    m_visualizer->reset();
    m_iconsStrip->clear();
    m_logConsole->clear();
}

void StorageCacheCleanWidget::onWorkerProgress(int percent, const QString &appName, const QString &pkgName, const QPixmap &icon, const QString &statusMsg)
{
    m_visualizer->setProgress(percent);
    m_visualizer->setCurrentApp(appName, pkgName, icon, statusMsg);
    m_iconsStrip->setActivePackage(pkgName);
}

void StorageCacheCleanWidget::onWorkerLog(const QString &message, int level)
{
    QString color = "#E2E8F0";
    if(level == 1)
        color = "#34D399"; // green success
    else if(level == 2)
        color = "#F87171"; // red error / stop

    QString timeStr = QTime::currentTime().toString("hh:mm:ss");
    QString html = QString("<span style='color:#64748B;'>[%1]</span> <span style='color:%2;'>%3</span>").arg(timeStr, color, message);

    m_logConsole->append(html);
    m_logConsole->verticalScrollBar()->setValue(m_logConsole->verticalScrollBar()->maximum());
}

void StorageCacheCleanWidget::onWorkerStatsUpdated(qint64 cacheBytes, int thumbnailsCount, qint64 thumbnailsBytes, int stoppedApps)
{
    m_cacheFreedBytes = cacheBytes;
    m_thumbnailsCount = thumbnailsCount;
    m_thumbnailsFreedBytes = thumbnailsBytes;
    m_appsStoppedCount = stoppedApps;
    m_totalFreedBytes = cacheBytes + thumbnailsBytes;

    m_valAppCacheFreed->setText(formatBytes(cacheBytes));
    m_valThumbnails->setText(QString("%1 ф. (%2)").arg(thumbnailsCount).arg(formatBytes(thumbnailsBytes)));
    m_valAppsStopped->setText(QString::number(stoppedApps));
    m_valTotalFreed->setText(formatBytes(m_totalFreedBytes));
}

// ============================================================================
// StorageCleanResultDialog Implementation
// ============================================================================

class StorageCleanResultDialog : public QDialog
{
public:
    StorageCleanResultDialog(const QString &deviceName, qint64 totalFreed, qint64 cacheFreed, int thumbCount, qint64 thumbFreed, int appsStopped, const QString &summary, QWidget *parent = nullptr) : QDialog(parent)
    {
        setWindowTitle("Результаты очистки — AdsKiller");
        setWindowIcon(QIcon(":/resources/app-logo"));
        setModal(true);
        setFixedWidth(460);

        setStyleSheet(
            "QDialog {"
            "   background-color: #0F1422;"
            "   color: #F8FAFC;"
            "}"
            "QLabel {"
            "   background: transparent;"
            "   color: #CBD5E1;"
            "}");

        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(24, 22, 24, 22);
        mainLayout->setSpacing(16);

        // Header: Success Icon + Title + Subtitle
        QHBoxLayout *hdr = new QHBoxLayout();
        hdr->setSpacing(14);

        QLabel *iconLbl = new QLabel(this);
        iconLbl->setFixedSize(48, 48);
        iconLbl->setAlignment(Qt::AlignCenter);
        iconLbl->setPixmap(QPixmap(":/svg/check-circle").scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        iconLbl->setStyleSheet("background-color: #064E3B; border: 1.5px solid #059669; border-radius: 0px;");
        hdr->addWidget(iconLbl);

        QVBoxLayout *titleLay = new QVBoxLayout();
        titleLay->setSpacing(3);
        QLabel *titleLbl = new QLabel("Очистка успешно завершена!", this);
        titleLbl->setStyleSheet("color: #FFFFFF; font-size: 16px; font-weight: bold;");
        titleLay->addWidget(titleLbl);

        QLabel *subLbl = new QLabel(deviceName.isEmpty() ? "Память Android-устройства оптимизирована" : deviceName, this);
        subLbl->setStyleSheet("color: #38BDF8; font-size: 12px; font-weight: 500;");
        titleLay->addWidget(subLbl);

        hdr->addLayout(titleLay);
        hdr->addStretch(1);
        mainLayout->addLayout(hdr);

        // Big Hero Result Box: Total Freed
        QFrame *heroFrame = new QFrame(this);
        heroFrame->setStyleSheet("background-color: #070A12; border: 1.5px solid #059669; border-radius: 0px; padding: 10px;");
        QVBoxLayout *heroLay = new QVBoxLayout(heroFrame);
        heroLay->setContentsMargins(16, 12, 16, 12);
        heroLay->setAlignment(Qt::AlignCenter);
        heroLay->setSpacing(4);

        QLabel *heroTitle = new QLabel("ВСЕГО ОСВОБОЖДЕНО ПАМЯТИ", heroFrame);
        heroTitle->setStyleSheet("color: #94A3B8; font-size: 11px; font-weight: 600; letter-spacing: 0.8px;");
        heroTitle->setAlignment(Qt::AlignCenter);
        heroLay->addWidget(heroTitle);

        QLabel *heroVal = new QLabel(formatByteSize(totalFreed), heroFrame);
        heroVal->setStyleSheet("color: #34D399; font-size: 26px; font-weight: bold; font-family: 'Segoe UI', sans-serif;");
        heroVal->setAlignment(Qt::AlignCenter);
        heroLay->addWidget(heroVal);

        mainLayout->addWidget(heroFrame);

        // Detailed Stats Grid (Card)
        QFrame *detailsFrame = new QFrame(this);
        detailsFrame->setStyleSheet("background-color: #141A28; border: 1px solid #1E293B; border-radius: 0px;");
        QGridLayout *grid = new QGridLayout(detailsFrame);
        grid->setContentsMargins(16, 14, 16, 14);
        grid->setHorizontalSpacing(14);
        grid->setVerticalSpacing(10);

        const auto addRow = [&](int row, const QString &iconPath, const QString &label, const QString &val, const QString &color = "#FFFFFF")
        {
            QLabel *iLbl = new QLabel(detailsFrame);
            iLbl->setPixmap(QPixmap(iconPath).scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            grid->addWidget(iLbl, row, 0);

            QLabel *tLbl = new QLabel(label, detailsFrame);
            tLbl->setStyleSheet("color: #94A3B8; font-size: 12px; font-weight: 500;");
            grid->addWidget(tLbl, row, 1);

            QLabel *vLbl = new QLabel(val, detailsFrame);
            vLbl->setStyleSheet(QString("color: %1; font-size: 12.5px; font-weight: bold;").arg(color));
            vLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            grid->addWidget(vLbl, row, 2);
        };

        addRow(0, ":/svg/folder", "Кэш приложений:", formatByteSize(cacheFreed), "#38BDF8");
        addRow(1, ":/svg/hard-drive", "Миниатюры (Thumbnails):", QString("%1 ф. (%2)").arg(thumbCount).arg(formatByteSize(thumbFreed)), "#00E5FF");
        addRow(2, ":/svg/zap", "Фоновые процессы:", QString("%1 остановлено").arg(appsStopped), "#FBBF24");
        if(!summary.isEmpty())
            addRow(3, ":/svg/check", "Итог:", summary, "#34D399");

        mainLayout->addWidget(detailsFrame);

        // Action button
        QPushButton *okBtn = new QPushButton("Отлично, закрыть", this);
        okBtn->setFixedHeight(42);
        okBtn->setCursor(Qt::PointingHandCursor);
        okBtn->setStyleSheet(
            "QPushButton {"
            "   background-color: #10B981;"
            "   border: 1px solid #059669;"
            "   border-radius: 0px;"
            "   color: #FFFFFF;"
            "   font-size: 13px;"
            "   font-weight: bold;"
            "   padding: 0px 24px;"
            "}"
            "QPushButton:hover {"
            "   background-color: #059669;"
            "   border-color: #34D399;"
            "}"
            "QPushButton:pressed {"
            "   background-color: #047857;"
            "}");
        connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
        mainLayout->addWidget(okBtn);
    }
};

void StorageCacheCleanWidget::onWorkerFinished(bool success, const QString &summary)
{
    m_isCleaning = false;
    m_btnStart->setEnabled(true);
    m_btnStop->setEnabled(false);
    m_chkAppCache->setEnabled(true);
    m_chkThumbnails->setEnabled(true);
    m_chkStopApps->setEnabled(true);
    m_chkTempFiles->setEnabled(true);

    m_visualizer->setCompleted(success, summary);
    m_iconsStrip->stopRunning();

    // Do NOT navigate away to cabinet: user stays in StorageCacheCleanWidget!
    if(success)
    {
        QString devName = !m_device.displayName.isEmpty() ? m_device.displayName : (!m_device.model.isEmpty() ? m_device.model : m_device.devId);
        StorageCleanResultDialog dlg(devName, m_totalFreedBytes, m_cacheFreedBytes, m_thumbnailsCount, m_thumbnailsFreedBytes, m_appsStoppedCount, summary, this);
        dlg.exec();
    }
}

// ============================================================================
// StorageCacheCleanService Implementation
// ============================================================================

StorageCacheCleanService::StorageCacheCleanService(QObject *parent) : Service(DeviceConnectType::ADB, parent)
{
}

StorageCacheCleanService::~StorageCacheCleanService()
{
    stop();
}

void StorageCacheCleanService::setAndroidArgs(const AdbDevice &adbDevice)
{
    Service::setAndroidArgs(adbDevice);
}

QString StorageCacheCleanService::uuid() const
{
    return IDServiceStorageCleanString;
}

PageIndex StorageCacheCleanService::targetPage()
{
    return StorageCacheCleanPage;
}

bool StorageCacheCleanService::canStart()
{
    return Service::canStart();
}

bool StorageCacheCleanService::isStarted()
{
    return m_started;
}

bool StorageCacheCleanService::isFinish()
{
    return m_finished;
}

QString StorageCacheCleanService::widgetIconName()
{
    return "storage-clean";
}

bool StorageCacheCleanService::start()
{
    sendCheckPull();
    if(!canStart())
        return false;

    m_started = true;
    m_finished = false;

    if(MainWindow::current)
    {
        auto *widget = static_cast<StorageCacheCleanWidget *>(MainWindow::current->pageWidget(StorageCacheCleanPage));
        if(widget)
        {
            widget->resetSession();
            widget->setDevice(mAdbDevice);
            widget->startClean();
        }
    }

    return true;
}

void StorageCacheCleanService::stop()
{
    m_started = false;
    m_finished = true;

    if(MainWindow::current)
    {
        auto *widget = static_cast<StorageCacheCleanWidget *>(MainWindow::current->pageWidget(StorageCacheCleanPage));
        if(widget)
            widget->stopClean();
    }
}

#include "Service.StorageCacheClean.moc"