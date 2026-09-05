#include <functional>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <list>
#include <memory>

#include <QCloseEvent>
#include <QDesktopServices>
#include <QEasingCurve>
#include <QEventLoop>
#include <QFontDatabase>
#include <QFuture>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStringListModel>
#include <QTableView>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QVector>
#include <QWheelEvent>
#include <QRandomGenerator>

#include "AppSystemTray.h"
#include "Services.h"
#include "Strings.h"
#include "extension.h"
#include "mainwindow.h"
#include "network.h"
#include "AIChatView.h"
#include "about_dialog.h"
#include "ui_mainwindow.h"

constexpr struct
{
    PageIndex index;
    const char *widgetName;
} PageConstNames[LengthPages] = {{AuthPage, "page_auth"}, {CabinetPage, "page_cabinet"}, {LongInfoPage, "page_adsmalware"}, {LoaderPage, "page_loader"}, {DevicesPage, "page_devices"}, {MyDevicesPage, "page_mydevices"}, {BuyVIPPage, "page_buyvip"}};

namespace
{
    class HorizontalWheelFilter : public QObject
    {
    public:
        explicit HorizontalWheelFilter(QScrollArea *scrollArea) : QObject(scrollArea), m_scrollArea(scrollArea)
        {
        }

    protected:
        bool eventFilter(QObject *obj, QEvent *event) override
        {
            if(event->type() == QEvent::Wheel && m_scrollArea)
            {
                QWheelEvent *wheelEvent = static_cast<QWheelEvent *>(event);
                int delta = wheelEvent->angleDelta().y();
                if(delta == 0)
                    delta = wheelEvent->angleDelta().x();
                if(delta != 0)
                {
                    QScrollBar *hBar = m_scrollArea->horizontalScrollBar();
                    if(hBar)
                        hBar->setValue(hBar->value() - delta);
                    return true;
                }
            }
            return QObject::eventFilter(obj, event);
        }

    private:
        QScrollArea *m_scrollArea;
    };

    class CapsuleFocusFilter : public QObject
    {
    public:
        explicit CapsuleFocusFilter(QWidget *capsule) : QObject(capsule), m_capsule(capsule)
        {
        }

    protected:
        bool eventFilter(QObject *obj, QEvent *event) override
        {
            if(!m_capsule)
                return QObject::eventFilter(obj, event);

            if(event->type() == QEvent::FocusIn)
            {
                m_capsule->setStyleSheet(
                    "QFrame#aiInputCapsule {"
                    "   background-color: #191B21;"
                    "   border: 1px solid #38BDF8;"
                    "   border-radius: 12px;"
                    "}");
            }
            else if(event->type() == QEvent::FocusOut)
            {
                m_capsule->setStyleSheet(
                    "QFrame#aiInputCapsule {"
                    "   background-color: #16181D;"
                    "   border: 1px solid #2B2F38;"
                    "   border-radius: 12px;"
                    "}");
            }
            return QObject::eventFilter(obj, event);
        }

    private:
        QWidget *m_capsule;
    };

    class AdbDeviceVisualizer;
    static AdbDeviceVisualizer *s_adbVisualizer = nullptr;

    class AdbDeviceVisualizer : public QWidget
    {
    public:
        explicit AdbDeviceVisualizer(QWidget *parent = nullptr)
            : QWidget(parent), m_status(UNKNOWN), m_time(0.0f), m_connectedTime(0.0f)
        {
            s_adbVisualizer = this;
            setAttribute(Qt::WA_OpaquePaintEvent, false);
            setMinimumSize(280, 420);
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

            m_animTimer = new QTimer(this);
            connect(m_animTimer, &QTimer::timeout, this, [this]() {
                m_time += 0.035f;
                if(m_status == DEVICE)
                    m_connectedTime += 0.035f;
                update();
            });
            m_animTimer->start(25); // ~40 FPS
        }

        ~AdbDeviceVisualizer() override
        {
            if(s_adbVisualizer == this)
                s_adbVisualizer = nullptr;
        }

        void setStatus(AdbConStatus s, const QString &name = QString(), const QString &sub = QString())
        {
            if(m_status != s || m_devName != name || m_devSub != sub)
            {
                if(m_status != DEVICE && s == DEVICE)
                    m_connectedTime = 0.0f;
                m_status = s;
                m_devName = name;
                m_devSub = sub;
                update();
            }
        }

        AdbConStatus status() const { return m_status; }

        void startAnimation()
        {
            if(!m_animTimer->isActive())
                m_animTimer->start(25);
        }

        void stopAnimation()
        {
            m_animTimer->stop();
        }

    protected:
        void paintEvent(QPaintEvent *event) override
        {
            Q_UNUSED(event);
            QPainter p(this);
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setRenderHint(QPainter::SmoothPixmapTransform, true);
            p.setRenderHint(QPainter::TextAntialiasing, true);

            const int w = width();
            const int h = height();
            const float cx = w * 0.5f;
            const float cy = h * 0.44f;

            // Palette per status
            QColor primaryColor;
            QColor glowColor;
            QColor accentColor;

            if(m_status == DEVICE)
            {
                primaryColor = QColor(16, 185, 129); // Emerald #10B981
                glowColor = QColor(52, 211, 153, 90);
                accentColor = QColor(110, 231, 183);
            }
            else if(m_status == UNAUTH)
            {
                primaryColor = QColor(245, 158, 11); // Amber #F59E0B
                glowColor = QColor(251, 191, 36, 100);
                accentColor = QColor(253, 230, 138);
            }
            else
            {
                primaryColor = QColor(56, 189, 248); // Sky Cyan #38BDF8
                glowColor = QColor(14, 165, 233, 70);
                accentColor = QColor(186, 230, 253);
            }

            // 1. Ambient Background Glow
            QRadialGradient ambientGlow(cx, cy, qMax(w, h) * 0.55);
            ambientGlow.setColorAt(0.0, QColor(glowColor.red(), glowColor.green(), glowColor.blue(), 30));
            ambientGlow.setColorAt(0.65, QColor(glowColor.red(), glowColor.green(), glowColor.blue(), 5));
            ambientGlow.setColorAt(1.0, QColor(11, 15, 25, 0));
            p.fillRect(rect(), ambientGlow);

            // 2. Tech Orbit Ring & Crosshairs
            const float orbitR = qMin(w, h) * 0.46f;
            p.setPen(QPen(QColor(primaryColor.red(), primaryColor.green(), primaryColor.blue(), 30), 1.0f));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(QPointF(cx, cy), orbitR, orbitR);

            // Rotating tech tick marks on orbit
            p.save();
            p.translate(cx, cy);
            p.rotate(std::fmod(m_time * 25.0f, 360.0f));
            p.setPen(QPen(QColor(accentColor.red(), accentColor.green(), accentColor.blue(), 80), 2.0f));
            for(int a = 0; a < 4; ++a)
            {
                p.drawLine(QPointF(orbitR - 6.0f, 0), QPointF(orbitR + 6.0f, 0));
                p.rotate(90.0);
            }
            p.restore();

            // 3. Animated Concentric Radar Waves (Expanding)
            const float maxRadarR = qMin(w, h) * 0.44f;
            const int ringCount = 3;
            for(int i = 0; i < ringCount; ++i)
            {
                float waveT = std::fmod(m_time * 0.75f + (float)i / (float)ringCount, 1.0f);
                float ringR = waveT * maxRadarR;
                int ringAlpha = static_cast<int>((1.0f - waveT) * (m_status == DEVICE ? 160 : 110));
                if(ringAlpha > 0)
                {
                    QPen wavePen(QColor(primaryColor.red(), primaryColor.green(), primaryColor.blue(), ringAlpha));
                    wavePen.setWidthF(1.2f);
                    if(i % 2 == 1)
                        wavePen.setStyle(Qt::DashLine);
                    p.setPen(wavePen);
                    p.setBrush(Qt::NoBrush);
                    p.drawEllipse(QPointF(cx, cy), ringR, ringR);
                }
            }

            // 4. Rotating Scan Beam (when searching)
            if(m_status == UNKNOWN)
            {
                p.save();
                p.translate(cx, cy);
                p.rotate(std::fmod(m_time * 100.0f, 360.0f));
                QConicalGradient sweep(0, 0, 0);
                sweep.setColorAt(0.0, QColor(56, 189, 248, 50));
                sweep.setColorAt(0.15, QColor(56, 189, 248, 10));
                sweep.setColorAt(0.3, QColor(56, 189, 248, 0));
                sweep.setColorAt(1.0, QColor(56, 189, 248, 0));
                p.setBrush(sweep);
                p.setPen(Qt::NoPen);
                p.drawEllipse(QPointF(0, 0), maxRadarR * 0.9f, maxRadarR * 0.9f);
                p.restore();
            }

            // 5. Phone Dimensions & Floating Motion
            const float phoneW = 184.0f;
            const float phoneH = 326.0f;
            float floatY = (m_status == DEVICE) ? 0.0f : (std::sin(m_time * 2.2f) * 5.0f);
            const float px = cx - phoneW * 0.5f;
            const float py = cy - phoneH * 0.5f + floatY;

            // 6. USB Cable & Data Stream
            const float cableStartX = cx;
            const float cableStartY = h;
            const float cableEndY = py + phoneH;

            // Cable Base line
            p.setPen(QPen(QColor(30, 41, 59, 220), 6.0f, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(QPointF(cableStartX, cableStartY), QPointF(cableStartX, cableEndY));

            // Cable Core Glow line
            p.setPen(QPen(QColor(primaryColor.red(), primaryColor.green(), primaryColor.blue(), 180), 2.5f, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(QPointF(cableStartX, cableStartY), QPointF(cableStartX, cableEndY));

            // Animated light pulses traveling upward into USB port
            const int pulseCount = 4;
            for(int i = 0; i < pulseCount; ++i)
            {
                float speedMult = (m_status == DEVICE) ? 2.5f : 1.2f;
                float pulseT = std::fmod(m_time * speedMult + (float)i / (float)pulseCount, 1.0f);
                float pulseY = cableStartY - pulseT * (cableStartY - cableEndY);
                float pulseAlpha = (pulseT < 0.15f) ? (pulseT / 0.15f) : (pulseT > 0.85f ? (1.0f - pulseT) / 0.15f : 1.0f);
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(accentColor.red(), accentColor.green(), accentColor.blue(), static_cast<int>(pulseAlpha * 255)));
                p.drawEllipse(QPointF(cableStartX, pulseY), 3.5f, 6.0f);
            }

            // USB-C Plug Head
            QRectF usbPlugRect(cx - 11.0f, cableEndY - 1.0f, 22.0f, 15.0f);
            QLinearGradient plugGrad(usbPlugRect.topLeft(), usbPlugRect.bottomRight());
            plugGrad.setColorAt(0.0, QColor(71, 85, 105));
            plugGrad.setColorAt(1.0, QColor(30, 41, 59));
            p.setPen(QPen(primaryColor, 1.2f));
            p.setBrush(plugGrad);
            p.drawRoundedRect(usbPlugRect, 3.0f, 3.0f);

            // 7. Outer Phone Chassis (Metallic bevel & breathing neon glow)
            QRectF phoneRect(px, py, phoneW, phoneH);
            float glowBreathing = 0.7f + 0.3f * std::sin(m_time * 3.0f);
            QPen neonPen(QColor(primaryColor.red(), primaryColor.green(), primaryColor.blue(), static_cast<int>(160 * glowBreathing)));
            neonPen.setWidthF(2.5f);

            QLinearGradient chassisGrad(phoneRect.topLeft(), phoneRect.bottomRight());
            chassisGrad.setColorAt(0.0, QColor(30, 41, 59));
            chassisGrad.setColorAt(0.5, QColor(15, 23, 42));
            chassisGrad.setColorAt(1.0, QColor(2, 6, 23));

            p.setPen(neonPen);
            p.setBrush(chassisGrad);
            p.drawRoundedRect(phoneRect, 26.0f, 26.0f);

            // 8. Phone Screen (AMOLED Glass)
            const float screenMargin = 7.0f;
            QRectF screenRect(px + screenMargin, py + screenMargin, phoneW - screenMargin * 2.0f, phoneH - screenMargin * 2.0f);
            p.setPen(QPen(QColor(51, 65, 85, 160), 1.0f));
            p.setBrush(QColor(8, 12, 20));
            p.drawRoundedRect(screenRect, 20.0f, 20.0f);

            // Speaker slit & front camera punch-hole
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(51, 65, 85));
            p.drawRoundedRect(QRectF(cx - 16.0f, py + 12.0f, 32.0f, 3.5f), 1.5f, 1.5f);
            p.setBrush(QColor(30, 41, 59));
            p.drawEllipse(QPointF(cx + 25.0f, py + 13.5f), 3.0f, 3.0f);

            // Mini Status Bar inside Phone
            p.setPen(QColor(148, 163, 184));
            QFont statusFont = p.font();
            statusFont.setPointSize(8);
            statusFont.setBold(true);
            p.setFont(statusFont);
            p.drawText(QRectF(screenRect.left() + 10.0f, screenRect.top() + 8.0f, 50.0f, 14.0f), Qt::AlignLeft | Qt::AlignVCenter, "ADB 3.0");

            // Battery icon
            QRectF battRect(screenRect.right() - 24.0f, screenRect.top() + 10.0f, 14.0f, 8.0f);
            p.setPen(QPen(QColor(148, 163, 184), 1.0f));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(battRect, 1.5f, 1.5f);
            p.fillRect(QRectF(battRect.left() + 2.0f, battRect.top() + 2.0f, 7.0f, 4.0f), primaryColor);

            // 9. Screen Contents by State
            p.save();
            p.setClipRect(screenRect);

            if(m_status == DEVICE)
            {
                // --- CONNECTED STATE ---
                float successPulse = qMin(1.0f, m_connectedTime * 2.0f);
                float checkCenterY = py + phoneH * 0.38f;
                float checkR = 34.0f * successPulse;

                QRadialGradient succGrad(cx, checkCenterY, checkR * 1.5f);
                succGrad.setColorAt(0.0, QColor(16, 185, 129, 70));
                succGrad.setColorAt(1.0, QColor(16, 185, 129, 0));
                p.setBrush(succGrad);
                p.setPen(Qt::NoPen);
                p.drawEllipse(QPointF(cx, checkCenterY), checkR * 1.5f, checkR * 1.5f);

                p.setPen(QPen(QColor(16, 185, 129), 2.5f));
                p.setBrush(QColor(6, 78, 59, 180));
                p.drawEllipse(QPointF(cx, checkCenterY), checkR, checkR);

                // Checkmark path
                QPainterPath checkPath;
                checkPath.moveTo(cx - 12.0f, checkCenterY);
                checkPath.lineTo(cx - 3.0f, checkCenterY + 9.0f);
                checkPath.lineTo(cx + 14.0f, checkCenterY - 8.0f);
                QPen checkPen(QColor(240, 253, 244), 3.0f, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
                p.strokePath(checkPath, checkPen);

                // Device Title
                QFont titleFont = p.font();
                titleFont.setPointSize(11);
                titleFont.setBold(true);
                p.setFont(titleFont);
                p.setPen(QColor(240, 253, 244));
                QString displayName = m_devName.isEmpty() ? QString::fromUtf8("Android Устройство") : m_devName;
                p.drawText(QRectF(screenRect.left() + 6.0f, checkCenterY + 44.0f, screenRect.width() - 12.0f, 22.0f),
                           Qt::AlignCenter | Qt::AlignVCenter, displayName);

                // Subtitle (Model / Vendor)
                QFont subFont = p.font();
                subFont.setPointSize(8);
                subFont.setBold(false);
                p.setFont(subFont);
                p.setPen(QColor(110, 231, 183));
                QString subText = m_devSub.isEmpty() ? QString::fromUtf8("USB Подключен") : m_devSub;
                p.drawText(QRectF(screenRect.left() + 6.0f, checkCenterY + 66.0f, screenRect.width() - 12.0f, 18.0f),
                           Qt::AlignCenter | Qt::AlignVCenter, subText);

                // Ready Badge
                QRectF badgeRect(cx - 58.0f, checkCenterY + 92.0f, 116.0f, 22.0f);
                p.setPen(QPen(QColor(16, 185, 129), 1.0f));
                p.setBrush(QColor(16, 185, 129, 45));
                p.drawRoundedRect(badgeRect, 11.0f, 11.0f);

                QFont badgeFont = p.font();
                badgeFont.setPointSize(8);
                badgeFont.setBold(true);
                p.setFont(badgeFont);
                p.setPen(QColor(52, 211, 153));
                p.drawText(badgeRect, Qt::AlignCenter, QString::fromUtf8("● АВТОРИЗОВАНО"));
            }
            else if(m_status == UNAUTH)
            {
                // --- UNAUTHORIZED / PERMISSION REQUIRED ---
                float alertY = py + phoneH * 0.32f;

                // Warning Icon
                p.setPen(QPen(QColor(245, 158, 11), 2.2f));
                p.setBrush(QColor(120, 53, 15, 160));
                p.drawEllipse(QPointF(cx, alertY), 22.0f, 22.0f);

                QFont warnIconFont = p.font();
                warnIconFont.setPointSize(13);
                warnIconFont.setBold(true);
                p.setFont(warnIconFont);
                p.setPen(QColor(253, 230, 138));
                p.drawText(QRectF(cx - 15.0f, alertY - 15.0f, 30.0f, 30.0f), Qt::AlignCenter, "!");

                // Simulated Prompt Dialog
                QRectF promptCard(screenRect.left() + 8.0f, alertY + 30.0f, screenRect.width() - 16.0f, 116.0f);
                p.setPen(QPen(QColor(245, 158, 11, 160), 1.0f));
                p.setBrush(QColor(30, 25, 18, 230));
                p.drawRoundedRect(promptCard, 8.0f, 8.0f);

                QFont pTitle = p.font();
                pTitle.setPointSize(8);
                pTitle.setBold(true);
                p.setFont(pTitle);
                p.setPen(QColor(251, 191, 36));
                p.drawText(QRectF(promptCard.left() + 4.0f, promptCard.top() + 6.0f, promptCard.width() - 8.0f, 28.0f),
                           Qt::AlignCenter | Qt::TextWordWrap, QString::fromUtf8("Разрешить отладку\nпо USB?"));

                QFont pDesc = p.font();
                pDesc.setPointSize(7);
                pDesc.setBold(false);
                p.setFont(pDesc);
                p.setPen(QColor(209, 213, 219));
                p.drawText(QRectF(promptCard.left() + 6.0f, promptCard.top() + 38.0f, promptCard.width() - 12.0f, 28.0f),
                           Qt::AlignCenter | Qt::TextWordWrap, QString::fromUtf8("Всегда разрешать с этого компьютера"));

                // Pulsing button with touch ripple
                float btnPulse = 0.8f + 0.2f * std::sin(m_time * 4.0f);
                QRectF okBtnRect(cx - 46.0f, promptCard.bottom() - 32.0f, 92.0f, 24.0f);

                // Touch ripple expanding from button
                float ripT = std::fmod(m_time * 1.5f, 1.0f);
                p.setPen(QPen(QColor(245, 158, 11, static_cast<int>((1.0f - ripT) * 160)), 1.5f));
                p.setBrush(Qt::NoBrush);
                p.drawRoundedRect(okBtnRect.adjusted(-ripT * 8.0f, -ripT * 5.0f, ripT * 8.0f, ripT * 5.0f), 6.0f, 6.0f);

                p.setPen(QPen(QColor(245, 158, 11), 1.2f));
                p.setBrush(QColor(217, 119, 6, static_cast<int>(200 * btnPulse)));
                p.drawRoundedRect(okBtnRect, 5.0f, 5.0f);

                QFont btnFont = p.font();
                btnFont.setPointSize(8);
                btnFont.setBold(true);
                p.setFont(btnFont);
                p.setPen(QColor(255, 255, 255));
                p.drawText(okBtnRect, Qt::AlignCenter, QString::fromUtf8("✓ РАЗРЕШИТЬ"));
            }
            else
            {
                // --- SEARCHING / SCANNING STATE ---
                float iconCenterY = py + phoneH * 0.40f;

                // Radar scan circles inside screen
                float inRadius = 45.0f;
                p.setPen(QPen(QColor(56, 189, 248, 60), 1.0f, Qt::DashLine));
                p.setBrush(Qt::NoBrush);
                p.drawEllipse(QPointF(cx, iconCenterY), inRadius, inRadius);

                float innerPulseR = 24.0f + 12.0f * std::sin(m_time * 2.5f);
                p.setPen(QPen(QColor(56, 189, 248, 90), 1.2f));
                p.drawEllipse(QPointF(cx, iconCenterY), innerPulseR, innerPulseR);

                // Center Icon Glyph
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(14, 165, 233, 50));
                p.drawEllipse(QPointF(cx, iconCenterY), 20.0f, 20.0f);

                QFont symbolFont = p.font();
                symbolFont.setPointSize(15);
                p.setFont(symbolFont);
                p.setPen(QColor(56, 189, 248));
                p.drawText(QRectF(cx - 15.0f, iconCenterY - 15.0f, 30.0f, 30.0f), Qt::AlignCenter, "⚡");

                // Text: ПОИСК УСТРОЙСТВА
                QFont sTitle = p.font();
                sTitle.setPointSize(9);
                sTitle.setBold(true);
                p.setFont(sTitle);
                p.setPen(QColor(240, 249, 255));
                p.drawText(QRectF(screenRect.left() + 6.0f, iconCenterY + 42.0f, screenRect.width() - 12.0f, 20.0f),
                           Qt::AlignCenter, QString::fromUtf8("ПОИСК УСТРОЙСТВА"));

                // Subtitle
                QFont sDesc = p.font();
                sDesc.setPointSize(8);
                sDesc.setBold(false);
                p.setFont(sDesc);
                p.setPen(QColor(148, 163, 184));
                p.drawText(QRectF(screenRect.left() + 6.0f, iconCenterY + 62.0f, screenRect.width() - 12.0f, 18.0f),
                           Qt::AlignCenter, QString::fromUtf8("Подключите USB-кабель"));

                // Animated Dots: ● ● ○
                int dotIdx = static_cast<int>(m_time * 2.5f) % 4;
                QString dots;
                for(int d = 0; d < 3; ++d)
                {
                    if(d < dotIdx)
                        dots += "● ";
                    else
                        dots += "○ ";
                }
                QFont dotsFont = p.font();
                dotsFont.setPointSize(9);
                p.setFont(dotsFont);
                p.setPen(QColor(56, 189, 248));
                p.drawText(QRectF(screenRect.left() + 6.0f, iconCenterY + 84.0f, screenRect.width() - 12.0f, 18.0f),
                           Qt::AlignCenter, dots.trimmed());
            }

            p.restore();
        }

    private:
        QTimer *m_animTimer;
        AdbConStatus m_status;
        QString m_devName;
        QString m_devSub;
        float m_time;
        float m_connectedTime;
    };
} // namespace

MainWindow *MainWindow::current;

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow), timerAuthAnim(nullptr)
{
    int x;
    QStringListModel *model;
    ui->setupUi(this);

    this->setMinimumSize(1060, 600);
    this->resize(1180, 680);

    // Load settings
    AppSetting::load();

    bool paramCheck;
    QVariant value;

    // V1 Old Token ID
    AppSetting::removeEncToken();
    // value = AppSetting::encryptedToken(&paramCheck);

    // V2 - newer JWT
    std::tuple<QString, QString> _ps = AppSetting::loginAndPass(&paramCheck);
    if(paramCheck)
    {
        ui->lineLoginEdit->setText(std::get<0>(_ps));
        ui->linePassEdit->setText(std::get<1>(_ps));
    }

    value = AppSetting::autoLogin(&paramCheck);
    if(paramCheck)
    {
        ui->checkAutoLogin->setChecked(value.toBool());
    }
    else
    {
        ui->checkAutoLogin->setChecked(true);
    }

    value = AppSetting::networkTimeout(&paramCheck);
    if(paramCheck)
    {
        value = value.toInt() < 1000 ? 1000 : value.toInt() > 60000 ? 60000 : value;
    }
    else
    {
        value = NetworkTimeoutDefault;
    }

    AppSetting::networkTimeout(nullptr, value);
    network.setTimeout(value.toInt());

    // Refresh TabPages to Content widget (Selective)
    QList<QWidget *> _w;
    for(x = 0; x < ui->tabWidget->count(); ++x)
        _w << ui->tabWidget->widget(x);

    for(const auto &item : std::as_const(PageConstNames))
    {
        auto iter = std::find_if(_w.begin(), _w.end(), [&item](const QWidget *it) { return it->objectName() == item.widgetName; });
        if(iter != std::end(_w))
            pages.insert(item.index, *iter);
    }

    vPageSpacer = ui->topcontent;
    vPageSpacer->setMaximumHeight(400);
    vPageSpacerAnimator = new QPropertyAnimation(vPageSpacer, "maximumHeight", this);
    vPageSpacerAnimator->setDuration(500);
    vPageSpacerAnimator->setStartValue(400);
    vPageSpacerAnimator->setEndValue(0);

    QGraphicsOpacityEffect *effect = new QGraphicsOpacityEffect(ui->contentLayout);
    ui->contentLayout->setGraphicsEffect(effect);

    contentOpacityAnimator = new QPropertyAnimation(effect, "opacity", this);
    contentOpacityAnimator->setDuration(1000);
    contentOpacityAnimator->setStartValue(0);
    contentOpacityAnimator->setEndValue(1.0);

    deviceLeftAnimator = new QPropertyAnimation(ui->device_left_group, "maximumWidth", this);
    deviceLeftAnimator->setDuration(1000);
    deviceLeftAnimator->setStartValue(1000);
    deviceLeftAnimator->setEndValue(0);
    deviceLeftAnimator->setEasingCurve(QEasingCurve::InOutCubic);

    // Top header back to main page.
    ui->contentLayout->layout()->addWidget(ui->toplevel_backpage);

    for(x = 0; x < _w.count(); ++x)
        ui->contentLayout->layout()->addWidget(_w[x]);

    ui->tabWidget->deleteLater();

    malwareProgressCircle = new ProgressCircle(this);
    malwareProgressCircle->setInfinilyMode(false);
    ui->progressCircleLayout->addWidget(malwareProgressCircle);

    loaderProgressCircle = new ProgressCircle(this);
    loaderProgressCircle->setInfinilyMode(true);
    loaderProgressCircle->setVisibleText(false);
    loaderProgressCircle->setInnerRadius(0);
    loaderProgressCircle->setColor(Qt::darkRed);
    loaderProgressCircle->setInnerRadius(.5);
    loaderProgressCircle->setMinimumHeight(225);
    ui->loaderLayout->addWidget(loaderProgressCircle);

    QList<QAction *> menusTheme {ui->mThemeSystem, ui->mThemeLight, ui->mThemeDark};
    for(QAction *q : menusTheme)
    {
        q->setChecked(false);
        QObject::connect(q, &QAction::triggered, this, &MainWindow::setThemeAction);
    }

    model = new QStringListModel(ui->processLogStatus);
    ui->processLogStatus->setModel(model);

    versionChecker = new QTimer(this);
    versionChecker->setSingleShot(true);
    versionChecker->setInterval(VersionCheckRate);

    // Signals
    QObject::connect(&network, &Network::sLoginFinish, this, &MainWindow::slotAuthFinish);
    QObject::connect(&network, &Network::sFetchingVersion, this, &MainWindow::slotFetchVersionFinish);
    QObject::connect(&network, &Network::sPullServiceList, this, &MainWindow::slotPullServiceList);

    QObject::connect(ui->authpageUpdate, &QPushButton::clicked, this, &MainWindow::updateCabinet);
    QObject::connect(ui->buttonBackTo, &QPushButton::clicked, this, &MainWindow::updateCabinet);
    QObject::connect(ui->logoutButton, &QPushButton::clicked, this, &MainWindow::logoutSystem);
    QObject::connect(
        ui->malwareReRun,
        &QPushButton::clicked,
        [this]()
        {
            if(ServiceProvider::currentService() && !ServiceProvider::currentService()->isStarted())
                ServiceProvider::currentService()->start();
        });
    QObject::connect(versionChecker, &QTimer::timeout, this, [this]() { checkVersion(false); });

    // Font init
    int fontId = QFontDatabase::addApplicationFont(":/resources/font-DigitalNumbers");
    QStringList fontFamils = QFontDatabase::applicationFontFamilies(fontId);
    if(!fontFamils.isEmpty())
    {
        QString fontFamily = fontFamils.first();
        malwareProgressCircle->setStyleSheet(QString("QWidget { Font-family: '%1'; }").arg(fontFamily));
    }

    // Set Default Theme DARK ONLY

    ui->menu_4->deleteLater();

    setTheme(ThemeScheme::Dark);
    // setTheme(static_cast<ThemeScheme>(static_cast<ThemeScheme>(std::clamp<int>(AppSetting::themeIndex(), 0, 2))));

    QString _version;
    _version += QString::number(AppVerMajor);
    _version += ".";
    _version += QString::number(AppVerMinor);
    _version += ".";
    _version += QString::number(AppVerPatch);

    runtimeVersion = {_version, {}, 0};

    // Run check version
#ifdef NDEBUG
    verChansesAvailable = -1;
#endif
    checkVersion(true);

    snows = nullptr;

    QDate d = QDate::currentDate();
    if(d >= QDate(d.year(), 12, 20) || d <= QDate(d.year(), 2, 1))
    {
        // ADD Snowflakes
        snows = new Snowflake(this, 50);
        ui->centralwidget_Layout->addWidget(snows, 0, 0, 0, 0);
        snows->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        snows->setSnowPixmap(QPixmap(":/resources/snowflake-image"));
        ui->mainapplogo->setStyleSheet("image: url(:/resources/app-logo-merry);");
        this->setWindowIcon(QIcon(":/resources/app-logo-merry"));
    }
    else
    {
        ui->mainapplogo->setStyleSheet("image: url(:/resources/app-logo);");
        this->setWindowIcon(QIcon(":/resources/app-logo"));
    }

    // Init tray
    tray = new AdsAppSystemTray(this);

    // Apply convenient modern design for all pages
    setupPagesDesign();

    // Modern AI Panel configuration
    if(ui->aiToolBoxToggle && ui->aiToolBoxContainer)
    {
        if(ui->horizontalLayout_ai)
        {
            ui->horizontalLayout_ai->setContentsMargins(0, 0, 0, 0);
            ui->horizontalLayout_ai->setSpacing(2);
        }

        ui->aiToolBoxContainer->setStyleSheet(
            "QFrame#aiToolBoxContainer {"
            "   background-color: #141518;"
            "   border: none;"
            "}");

        // Dynamic toggle button styling lambda
        auto updateToggleButtonStyle = [this](bool expanded)
        {
            if(expanded)
            {
                ui->aiToolBoxToggle->setText(QString::fromUtf8("›"));
                ui->aiToolBoxToggle->setToolTip("Свернуть панель ИИ");
                ui->aiToolBoxToggle->setFixedWidth(18);
                ui->aiToolBoxToggle->setStyleSheet(
                    "QPushButton#aiToolBoxToggle {"
                    "   background: #181A20;"
                    "   color: #64748B;"
                    "   border: 1px solid #232730;"
                    "   border-radius: 4px;"
                    "   font-size: 14px;"
                    "   font-weight: bold;"
                    "   padding: 0px;"
                    "}"
                    "QPushButton#aiToolBoxToggle:hover {"
                    "   background: #222631;"
                    "   border-color: #38BDF8;"
                    "   color: #38BDF8;"
                    "}"
                    "QPushButton#aiToolBoxToggle:pressed {"
                    "   background: #121418;"
                    "}");
            }
            else
            {
                ui->aiToolBoxToggle->setText(QString::fromUtf8("И\nИ\n\n‹"));
                ui->aiToolBoxToggle->setToolTip("Развернуть панель AdsKiller AI");
                ui->aiToolBoxToggle->setFixedWidth(32);
                ui->aiToolBoxToggle->setStyleSheet(
                    "QPushButton#aiToolBoxToggle {"
                    "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #1E2330, stop:1 #141720);"
                    "   color: #38BDF8;"
                    "   border: 1px solid #2B3950;"
                    "   border-top-left-radius: 8px;"
                    "   border-bottom-left-radius: 8px;"
                    "   border-top-right-radius: 0px;"
                    "   border-bottom-right-radius: 0px;"
                    "   font-size: 11px;"
                    "   font-weight: bold;"
                    "   padding: 6px 0px;"
                    "}"
                    "QPushButton#aiToolBoxToggle:hover {"
                    "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #26334A, stop:1 #1A2233);"
                    "   border: 1px solid #38BDF8;"
                    "   color: #FFFFFF;"
                    "}"
                    "QPushButton#aiToolBoxToggle:pressed {"
                    "   background: #0F172A;"
                    "}");
            }
        };

        ui->aiToolBoxToggle->setCursor(Qt::PointingHandCursor);
        ui->aiToolBoxContainer->setFixedWidth(350);
        updateToggleButtonStyle(true);

        // Detach pages from legacy QToolBox and embed modern aiPanel into horizontalLayout_ai
        if(ui->aiToolBox)
        {
            while(ui->aiToolBox->count() > 0)
            {
                ui->aiToolBox->removeItem(0);
            }
            if(ui->horizontalLayout_ai)
            {
                ui->horizontalLayout_ai->removeWidget(ui->aiToolBox);
                ui->aiToolBox->hide();
            }
        }

        // Modern unified panel wrapper (replaces clunky QToolBox accordion)
        QWidget *aiPanel = new QWidget(ui->aiToolBoxContainer);
        aiPanel->setObjectName("aiMainPanel");
        aiPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        aiPanel->setMinimumHeight(520);
        aiPanel->setStyleSheet(
            "QWidget#aiMainPanel {"
            "   background-color: #141518;"
            "   border: 1px solid #23262E;"
            "   border-radius: 10px;"
            "}");

        if(ui->horizontalLayout_ai)
        {
            ui->horizontalLayout_ai->insertWidget(0, aiPanel, 1);
            ui->horizontalLayout_ai->setStretch(0, 1);
            ui->horizontalLayout_ai->setStretch(1, 0);
        }

        QVBoxLayout *aiPanelLayout = new QVBoxLayout(aiPanel);
        aiPanelLayout->setContentsMargins(0, 0, 0, 0);
        aiPanelLayout->setSpacing(0);

        // Top Header Bar with title, online indicator, and segmented tab pill switcher
        QWidget *aiHeaderBar = new QWidget(aiPanel);
        aiHeaderBar->setObjectName("aiHeaderBar");
        aiHeaderBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        aiHeaderBar->setFixedHeight(40);
        aiHeaderBar->setStyleSheet(
            "QWidget#aiHeaderBar {"
            "   background-color: #17191E;"
            "   border-top-left-radius: 9px;"
            "   border-top-right-radius: 9px;"
            "   border-bottom: 1px solid #23262E;"
            "}");

        QHBoxLayout *headerLayout = new QHBoxLayout(aiHeaderBar);
        headerLayout->setContentsMargins(10, 0, 6, 0);
        headerLayout->setSpacing(6);

        QLabel *aiTitle = new QLabel(aiHeaderBar);
        aiTitle->setText("<b>AdsKiller AI</b>");
        aiTitle->setStyleSheet("color: #FFFFFF; font-size: 12px; font-weight: 600;");

        QLabel *aiStatus = new QLabel(aiHeaderBar);
        aiStatus->setText(QString::fromUtf8("●"));
        aiStatus->setToolTip("Ассистент активен");
        aiStatus->setStyleSheet("color: #10B981; font-size: 8px; margin-top: 1px;");

        headerLayout->addWidget(aiTitle);
        headerLayout->addWidget(aiStatus);
        headerLayout->addStretch(1);

        QFrame *segmentedBar = new QFrame(aiHeaderBar);
        segmentedBar->setObjectName("aiSegmentedBar");
        segmentedBar->setFixedHeight(26);
        segmentedBar->setStyleSheet(
            "QFrame#aiSegmentedBar {"
            "   background-color: #1E2128;"
            "   border: 1px solid #2B2F38;"
            "   border-radius: 6px;"
            "}");
        QHBoxLayout *segLayout = new QHBoxLayout(segmentedBar);
        segLayout->setContentsMargins(2, 2, 2, 2);
        segLayout->setSpacing(2);

        QPushButton *tabChatBtn = new QPushButton("💬 Чат", segmentedBar);
        QPushButton *tabInfoBtn = new QPushButton("ℹ Инфо", segmentedBar);

        const QString segBtnStyle =
            "QPushButton {"
            "   background: transparent;"
            "   color: #8E9297;"
            "   border: none;"
            "   border-radius: 4px;"
            "   padding: 2px 7px;"
            "   font-size: 10.5px;"
            "   font-weight: 500;"
            "}"
            "QPushButton:hover {"
            "   color: #FFFFFF;"
            "   background: rgba(255, 255, 255, 0.05);"
            "}"
            "QPushButton:checked {"
            "   background: #2D3340;"
            "   color: #38BDF8;"
            "   font-weight: bold;"
            "}";

        tabChatBtn->setStyleSheet(segBtnStyle);
        tabInfoBtn->setStyleSheet(segBtnStyle);
        tabChatBtn->setCheckable(true);
        tabInfoBtn->setCheckable(true);
        tabChatBtn->setChecked(true);
        tabChatBtn->setCursor(Qt::PointingHandCursor);
        tabInfoBtn->setCursor(Qt::PointingHandCursor);

        segLayout->addWidget(tabChatBtn);
        segLayout->addWidget(tabInfoBtn);
        headerLayout->addWidget(segmentedBar);

        QPushButton *headerCollapseBtn = new QPushButton(QString::fromUtf8("✕"), aiHeaderBar);
        headerCollapseBtn->setToolTip("Свернуть панель ИИ");
        headerCollapseBtn->setFixedSize(22, 22);
        headerCollapseBtn->setCursor(Qt::PointingHandCursor);
        headerCollapseBtn->setStyleSheet(
            "QPushButton {"
            "   background: transparent;"
            "   color: #64748B;"
            "   border: none;"
            "   border-radius: 4px;"
            "   font-size: 11px;"
            "   font-weight: bold;"
            "}"
            "QPushButton:hover {"
            "   background: rgba(239, 68, 68, 0.15);"
            "   color: #F87171;"
            "}"
            "QPushButton:pressed {"
            "   background: rgba(239, 68, 68, 0.25);"
            "}");
        headerLayout->addWidget(headerCollapseBtn);
        QObject::connect(headerCollapseBtn, &QPushButton::clicked, ui->aiToolBoxToggle, &QPushButton::click);

        aiPanelLayout->addWidget(aiHeaderBar);

        // QStackedWidget for switching between Chat and Info
        QStackedWidget *aiStack = new QStackedWidget(aiPanel);
        aiStack->setObjectName("aiStack");
        aiStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        aiStack->setStyleSheet("QStackedWidget#aiStack { background: transparent; border: none; }");

        aiStack->addWidget(ui->aiToolBoxPage1);
        aiStack->addWidget(ui->aiToolBoxPage2);
        aiPanelLayout->addWidget(aiStack, 1);

        QObject::connect(
            tabChatBtn,
            &QPushButton::clicked,
            this,
            [aiStack, tabChatBtn, tabInfoBtn]()
            {
                aiStack->setCurrentIndex(0);
                tabChatBtn->setChecked(true);
                tabInfoBtn->setChecked(false);
            });
        QObject::connect(
            tabInfoBtn,
            &QPushButton::clicked,
            this,
            [aiStack, tabChatBtn, tabInfoBtn]()
            {
                aiStack->setCurrentIndex(1);
                tabChatBtn->setChecked(false);
                tabInfoBtn->setChecked(true);
            });

        // Collapse / Expand toggle logic
        QObject::connect(
            ui->aiToolBoxToggle,
            &QPushButton::clicked,
            this,
            [this, aiPanel, updateToggleButtonStyle]()
            {
                if(aiPanel->isVisible())
                {
                    aiPanel->setVisible(false);
                    updateToggleButtonStyle(false);
                    ui->aiToolBoxContainer->setFixedWidth(36);
                }
                else
                {
                    aiPanel->setVisible(true);
                    updateToggleButtonStyle(true);
                    ui->aiToolBoxContainer->setFixedWidth(350);
                }
            });

        // Setup Page 2: About AI info
        ui->aiToolBoxPage2->setStyleSheet("QWidget#aiToolBoxPage2 { background-color: #141518; border: none; }");
        if(ui->aboutAi_edit)
        {
            ui->aboutAi_edit->setStyleSheet(
                "QTextEdit#aboutAi_edit {"
                "   background-color: #141518;"
                "   color: #D1D5DB;"
                "   border: none;"
                "   padding: 14px 14px;"
                "   font-size: 11px;"
                "}");
            ui->aboutAi_edit->setHtml(
                "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">"
                "<html><head><meta name=\"qrichtext\" content=\"1\" /><meta charset=\"utf-8\" />"
                "<style type=\"text/css\">p, li { white-space: pre-wrap; line-height: 1.5; }</style></head>"
                "<body style=\"font-family:'Segoe UI', 'Noto Sans', sans-serif; font-size:10pt; color:#D1D5DB;\">"
                "<div style=\"text-align:center; padding:12px 0 8px 0;\">"
                "<span style=\"font-size:30px;\">🤖</span><br/>"
                "<b style=\"color:#38BDF8; font-size:13pt;\">AdsKiller AI Assistant</b><br/>"
                "<span style=\"color:#8E9297; font-size:9pt;\">Интеллектуальный помощник</span><br/>"
                "<span style=\"display:inline-block; margin-top:6px; background-color:#1E293B; color:#38BDF8; font-size:8.5pt; font-weight:600; padding:2px 8px; border-radius:10px;\">● В сети</span>"
                "</div>"
                "<hr style=\"border:none; border-top:1px solid #252830; margin:10px 0;\"/>"
                "<p style=\"font-size:9.5pt;\">"
                "<b style=\"color:#FFFFFF;\">Возможности модуля:</b><br/>"
                "&nbsp;• Диагностика и блокировка рекламы<br/>"
                "&nbsp;• Управление подключенными устройствами<br/>"
                "&nbsp;• Проверка статуса подписки и кредитов<br/>"
                "&nbsp;• Быстрые ответы и оптимизация ОС"
                "</p>"
                "<hr style=\"border:none; border-top:1px solid #252830; margin:10px 0;\"/>"
                "<p style=\"font-size:9.5pt;\">"
                "<b style=\"color:#FFFFFF;\">Автор модуля ИИ:</b><br/>"
                "&nbsp;&nbsp;Команда <span style=\"color:#38BDF8;\">imister.tech</span><br/><br/>"
                "<b style=\"color:#FFFFFF;\">Разработчик:</b><br/>"
                "&nbsp;&nbsp;Нурсеит К. (<span style=\"color:#38BDF8;\">badcast</span>)<br/><br/>"
                "<b style=\"color:#FFFFFF;\">Дизайн:</b><br/>"
                "&nbsp;&nbsp;Владимир (<span style=\"color:#38BDF8;\">LeoJames</span>)"
                "</p>"
                "</body></html>");
        }

        // Setup Page 1: Chat interface
        ui->aiToolBoxPage1->setStyleSheet("QWidget#aiToolBoxPage1 { background-color: #141518; border: none; }");
        ui->aiToolBoxPage1->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        // Clean out legacy designer layout immediately before creating Page 1 widgets
        QLayout *oldPage1Layout = ui->aiToolBoxPage1->layout();
        if(oldPage1Layout)
        {
            QLayoutItem *item;
            while((item = oldPage1Layout->takeAt(0)) != nullptr)
            {
                delete item;
            }
            delete oldPage1Layout;
        }

        QVBoxLayout *page1Layout = new QVBoxLayout(ui->aiToolBoxPage1);
        page1Layout->setContentsMargins(6, 6, 6, 6);
        page1Layout->setSpacing(6);

        // Create custom widget-based AIChatView
        AIChatView *chatView = new AIChatView(ui->aiToolBoxPage1);
        chatView->setObjectName("aiChatView");
        chatView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        chatView->setMinimumWidth(0);
        chatView->setMaximumWidth(16777215);
        chatView->setMinimumHeight(300);
        chatView->setMaximumHeight(16777215);
        page1Layout->addWidget(chatView, 1);
        ui->aiChatMessages->hide();
        chatView->showLocked();

        // Quick suggestions single-row carousel + shuffle button
        QWidget *quickBarWidget = new QWidget(ui->aiToolBoxPage1);
        quickBarWidget->setStyleSheet("background: transparent;");
        quickBarWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        quickBarWidget->setFixedHeight(38);
        page1Layout->addWidget(quickBarWidget, 0);
        QHBoxLayout *quickBarLayout = new QHBoxLayout(quickBarWidget);
        quickBarLayout->setContentsMargins(0, 2, 0, 2);
        quickBarLayout->setSpacing(4);

        QPushButton *shuffleBtn = new QPushButton(QString::fromUtf8("🔀"), quickBarWidget);
        shuffleBtn->setToolTip("Перемешать подсказки");
        shuffleBtn->setFixedSize(26, 26);
        shuffleBtn->setCursor(Qt::PointingHandCursor);
        shuffleBtn->setStyleSheet(
            "QPushButton {"
            "   background: #1C1E24;"
            "   color: #8E9297;"
            "   border: 1px solid #2B2F38;"
            "   border-radius: 13px;"
            "   font-size: 11px;"
            "   padding: 0px;"
            "}"
            "QPushButton:hover {"
            "   background: #252A34;"
            "   border-color: #38BDF8;"
            "   color: #38BDF8;"
            "}"
            "QPushButton:pressed {"
            "   background: #131417;"
            "}");

        QScrollArea *scrollArea = new QScrollArea(quickBarWidget);
        scrollArea->setObjectName("aiQuickScrollArea");
        scrollArea->setWidgetResizable(true);
        scrollArea->setFixedHeight(34);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scrollArea->setStyleSheet(
            "QScrollArea#aiQuickScrollArea {"
            "   border: none;"
            "   background: transparent;"
            "}");

        scrollArea->viewport()->installEventFilter(new HorizontalWheelFilter(scrollArea));
        scrollArea->installEventFilter(new HorizontalWheelFilter(scrollArea));

        QWidget *quickButtonsWidget = new QWidget(scrollArea);
        quickButtonsWidget->setStyleSheet("background: transparent;");
        QHBoxLayout *quickButtonsLayout = new QHBoxLayout(quickButtonsWidget);
        quickButtonsLayout->setContentsMargins(0, 2, 0, 2);
        quickButtonsLayout->setSpacing(5);

        struct QuickQuestion
        {
            QString icon;
            QStringList variations;
        };

        QList<QuickQuestion> quickQuestions = {
            {"💳", {"Мои кредиты", "Сколько кредитов?", "Остаток баланса?", "Показать баланс"}},
            {"👑", {"VIP статус", "Остаток VIP дней", "Сколько VIP дней?", "Когда истекает VIP?"}},
            {"📱", {"Мои устройства", "Список устройств", "Активные девайсы", "Привязанные устройства"}},
            {"🛡️", {"Удаление рекламы", "Запусти удаление рекламы", "Какие есть сервисы?", "Открой окно покупки VIP"}},
            {"⚡", {"Быстрая очистка", "Остановить приложения", "Очистить кэш", "Как закрыть вирусы?"}},
            {"🚀", {"Ускорить телефон", "Как очистить ОЗУ?", "Оптимизация системы", "Ускорить работу"}},
            {"💡", {"Что ты умеешь?", "Возможности AdsKiller", "Справка по функциям", "Чем можешь помочь?"}},
            {"📧", {"Моя почта", "Мой email", "Какая у меня почта?", "Адрес эл. почты"}},
            {"🛒", {"Купить кредиты", "Как купить VIP?", "Пополнение баланса", "Тарифы и цены"}},
            {"🔒", {"Безопасность", "Безопасно ли это?", "Как включить отладку?", "Как подключить телефон?"}},
            {"📊", {"Статистика", "Заблокированная реклама", "Отчет блокировки", "Сколько рекламы скрыто?"}},
            {"❓", {"Как пользоваться?", "Инструкция для новичка", "Быстрый старт", "Помощь по приложению"}}};

        auto questionsPtr = std::make_shared<QList<QuickQuestion>>(quickQuestions);

        auto populateRandomButtons = [this, quickButtonsWidget, quickButtonsLayout, scrollArea, questionsPtr]()
        {
            // Clear existing buttons from layout
            QLayoutItem *child;
            while((child = quickButtonsLayout->takeAt(0)) != nullptr)
            {
                if(child->widget())
                    delete child->widget();
                delete child;
            }

            // Shuffle questions randomly
            std::shuffle(questionsPtr->begin(), questionsPtr->end(), *QRandomGenerator::global());

            for(int i = 0; i < questionsPtr->size(); ++i)
            {
                const auto &qData = (*questionsPtr)[i];
                int initialIdx = QRandomGenerator::global()->bounded(qData.variations.size());
                QString initialText = qData.variations[initialIdx];

                QPushButton *btn = new QPushButton(QString("%1 %2").arg(qData.icon, initialText), quickButtonsWidget);
                btn->setFixedHeight(26);
                btn->setStyleSheet(
                    "QPushButton {"
                    "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #22252C, stop:1 #1A1C22);"
                    "   color: #D1D5DB;"
                    "   border: 1px solid #2F333E;"
                    "   border-radius: 13px;"
                    "   padding: 2px 10px;"
                    "   font-size: 11px;"
                    "   font-weight: 500;"
                    "}"
                    "QPushButton:hover {"
                    "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #2A303C, stop:1 #202630);"
                    "   border: 1px solid #38BDF8;"
                    "   color: #38BDF8;"
                    "}"
                    "QPushButton:pressed {"
                    "   background-color: #141518;"
                    "   color: #FFFFFF;"
                    "}");
                btn->setCursor(Qt::PointingHandCursor);
                quickButtonsLayout->addWidget(btn);

                QObject::connect(
                    btn,
                    &QPushButton::clicked,
                    this,
                    [this, btn, icon = qData.icon, variations = qData.variations, lastIdx = initialIdx]() mutable
                    {
                        if(!ui->aiChatSend->isEnabled())
                            return;
                        QString textToSend = variations[lastIdx];
                        ui->aiChatEdit->setText(textToSend);
                        ui->aiChatSend->click();

                        if(variations.size() > 1)
                        {
                            int r;
                            do
                            {
                                r = QRandomGenerator::global()->bounded(variations.size());
                            } while(r == lastIdx);
                            lastIdx = r;
                            btn->setText(QString("%1 %2").arg(icon, variations[r]));
                        }
                    });
            }
            if(scrollArea->horizontalScrollBar())
                scrollArea->horizontalScrollBar()->setValue(0);
        };

        populateRandomButtons();
        scrollArea->setWidget(quickButtonsWidget);

        QObject::connect(shuffleBtn, &QPushButton::clicked, this, populateRandomButtons);

        quickBarLayout->addWidget(shuffleBtn, 0);
        quickBarLayout->addWidget(scrollArea, 1);

        // Integrated modern input capsule
        QFrame *inputCapsule = new QFrame(ui->aiToolBoxPage1);
        inputCapsule->setObjectName("aiInputCapsule");
        inputCapsule->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        inputCapsule->setMinimumHeight(44);
        inputCapsule->setMaximumHeight(56);
        inputCapsule->setStyleSheet(
            "QFrame#aiInputCapsule {"
            "   background-color: #16181D;"
            "   border: 1px solid #2B2F38;"
            "   border-radius: 12px;"
            "}");

        QHBoxLayout *capsuleLayout = new QHBoxLayout(inputCapsule);
        capsuleLayout->setContentsMargins(10, 4, 6, 4);
        capsuleLayout->setSpacing(6);

        ui->aiChatEdit->setStyleSheet(
            "QTextEdit#aiChatEdit {"
            "   background: transparent;"
            "   color: #F3F4F6;"
            "   border: none;"
            "   padding: 4px 2px;"
            "   font-size: 11.5px;"
            "   selection-background-color: #0078D4;"
            "}");
        ui->aiChatEdit->setPlaceholderText("Спросите у AdsKiller AI...");
        ui->aiChatEdit->setMinimumHeight(32);
        ui->aiChatEdit->setMaximumHeight(48);
        ui->aiChatEdit->installEventFilter(new CapsuleFocusFilter(inputCapsule));

        ui->aiChatSend->setStyleSheet(
            "QPushButton#aiChatSend {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #0078D4, stop:1 #005A9E);"
            "   color: #FFFFFF;"
            "   border: none;"
            "   border-radius: 15px;"
            "   font-size: 13px;"
            "   font-weight: bold;"
            "}"
            "QPushButton#aiChatSend:hover {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #1088E8, stop:1 #0066BA);"
            "}"
            "QPushButton#aiChatSend:pressed {"
            "   background: #004D80;"
            "}"
            "QPushButton#aiChatSend:disabled {"
            "   background: #23262E;"
            "   color: #4B515D;"
            "}");
        ui->aiChatSend->setFixedSize(30, 30);
        ui->aiChatSend->setCursor(Qt::PointingHandCursor);
        ui->aiChatSend->setText(QString::fromUtf8("➤"));
        ui->aiChatSend->setToolTip("Отправить сообщение (Enter)");

        capsuleLayout->addWidget(ui->aiChatEdit, 1);
        capsuleLayout->addWidget(ui->aiChatSend, 0, Qt::AlignVCenter);

        page1Layout->addWidget(inputCapsule, 0);

        // Activate container layout and ensure all components are visible
        if(ui->aiToolBoxContainer->layout())
            ui->aiToolBoxContainer->layout()->activate();

        // Explicitly show all components to ensure nothing remains hidden
        chatView->show();
        quickBarWidget->show();
        inputCapsule->show();
        ui->aiToolBoxPage1->show();
        aiStack->show();
        aiPanel->show();
    }
}

MainWindow::~MainWindow()
{
    ServiceProvider::closeService();
    Adb::killServer();
    AppSetting::save();
    delete ui;
}

void MainWindow::setupPagesDesign()
{
    // ==========================================
    // 0. Global Window and Controls Styling
    // ==========================================
    this->setStyleSheet(
        "QMainWindow {"
        "   background-color: #141517;"
        "}"
        "QWidget#centralwidget {"
        "   background-color: #141517;"
        "}"
        "QScrollBar:vertical {"
        "   background: transparent;"
        "   width: 8px;"
        "   margin: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "   background: #363940;"
        "   border-radius: 4px;"
        "   min-height: 24px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "   background: #4E525C;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "   height: 0px;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "   background: transparent;"
        "}"
        "QScrollBar:horizontal {"
        "   background: transparent;"
        "   height: 8px;"
        "   margin: 0px;"
        "}"
        "QScrollBar::handle:horizontal {"
        "   background: #363940;"
        "   border-radius: 4px;"
        "   min-width: 24px;"
        "}"
        "QScrollBar::handle:horizontal:hover {"
        "   background: #4E525C;"
        "}"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
        "   width: 0px;"
        "}"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {"
        "   background: transparent;"
        "}"
    );

    // ==========================================
    // 1. Auth Page (page_auth)
    // ==========================================
    if(ui->page_auth)
    {
        ui->page_auth->setAttribute(Qt::WA_StyledBackground, true);
        ui->page_auth->setStyleSheet(
            "QWidget#page_auth {"
            "   background: qradialgradient(cx:0.5, cy:0.45, radius:0.8, fx:0.5, fy:0.4, "
            "       stop:0 #111A2E, stop:0.55 #0A0E1A, stop:1 #04060A);"
            "}"
        );
    }
    if(ui->gridLayout_asd2)
    {
        ui->gridLayout_asd2->setAlignment(Qt::AlignCenter);
    }

    if(ui->frame_4)
    {
        ui->frame_4->setAttribute(Qt::WA_StyledBackground, true);
        ui->frame_4->setFixedSize(440, 560);
        ui->frame_4->setStyleSheet(
            "QFrame#frame_4 {"
            "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #141B2D, stop:1 #0B0F19);"
            "   border: 1px solid rgba(56, 189, 248, 0.28);"
            "   border-radius: 20px;"
            "}"
        );
    }

    if(ui->mainapplogo)
    {
        ui->mainapplogo->setFixedSize(68, 68);
        QString logoUrl = (QDate::currentDate().month() == 12 || QDate::currentDate().month() == 1)
            ? ":/resources/app-logo-merry" : ":/resources/app-logo";
        ui->mainapplogo->setStyleSheet(QString(
            "QFrame#mainapplogo {"
            "   image: url(%1);"
            "   background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #1E293B, stop:1 #0F172A);"
            "   border: 1.5px solid rgba(56, 189, 248, 0.35);"
            "   border-radius: 20px;"
            "   padding: 10px;"
            "}"
        ).arg(logoUrl));
    }

    if(ui->label_4)
    {
        ui->label_4->setTextFormat(Qt::RichText);
        ui->label_4->setAlignment(Qt::AlignCenter);
        ui->label_4->setStyleSheet("background: transparent;");
        ui->label_4->setText(
            "<div align='center'>"
            "<span style='font-size: 20px; font-weight: 700; color: #F8FAFC; letter-spacing: 0.5px;'>AdsKiller Desktop</span><br>"
            "<span style='font-size: 13px; font-weight: 400; color: #94A3B8;'>Авторизуйтесь для доступа к сервисам</span>"
            "</div>"
        );
    }

    if(ui->label_12)
    {
        ui->label_12->setText("ЛОГИН ИЛИ СЕРИЙНЫЙ НОМЕР");
        ui->label_12->setStyleSheet("color: #94A3B8; font-size: 11px; font-weight: 700; letter-spacing: 0.8px; background: transparent;");
    }

    if(ui->lineLoginEdit)
    {
        ui->lineLoginEdit->setFixedHeight(42);
        ui->lineLoginEdit->setPlaceholderText("Логин или имя пользователя");
        ui->lineLoginEdit->setStyleSheet(
            "QLineEdit {"
            "   background-color: #0F172A;"
            "   color: #F8FAFC;"
            "   border: 1.5px solid #1E293B;"
            "   border-radius: 10px;"
            "   padding: 0 14px;"
            "   font-size: 13px;"
            "   selection-background-color: #0284C7;"
            "}"
            "QLineEdit:hover {"
            "   border: 1.5px solid #334155;"
            "   background-color: #131E35;"
            "}"
            "QLineEdit:focus {"
            "   border: 1.5px solid #38BDF8;"
            "   background-color: #0F172A;"
            "}"
            "QLineEdit:disabled {"
            "   background-color: #0B101D;"
            "   color: #475569;"
            "   border-color: #1E293B;"
            "}"
        );
    }

    if(ui->label_14)
    {
        ui->label_14->setText("ПАРОЛЬ ИЛИ ТОКЕН ДОСТУПА");
        ui->label_14->setStyleSheet("color: #94A3B8; font-size: 11px; font-weight: 700; letter-spacing: 0.8px; background: transparent;");
    }

    if(ui->linePassEdit)
    {
        ui->linePassEdit->setFixedHeight(42);
        ui->linePassEdit->setPlaceholderText("Пароль или токен");
        ui->linePassEdit->setStyleSheet(
            "QLineEdit {"
            "   background-color: #0F172A;"
            "   color: #F8FAFC;"
            "   border: 1.5px solid #1E293B;"
            "   border-radius: 10px;"
            "   padding: 0 14px;"
            "   font-size: 13px;"
            "   selection-background-color: #0284C7;"
            "}"
            "QLineEdit:hover {"
            "   border: 1.5px solid #334155;"
            "   background-color: #131E35;"
            "}"
            "QLineEdit:focus {"
            "   border: 1.5px solid #38BDF8;"
            "   background-color: #0F172A;"
            "}"
            "QLineEdit:disabled {"
            "   background-color: #0B101D;"
            "   color: #475569;"
            "   border-color: #1E293B;"
            "}"
        );
    }

    if(ui->butShowPass)
    {
        ui->butShowPass->setText("👁");
        ui->butShowPass->setToolTip("Показать / скрыть пароль");
        ui->butShowPass->setCursor(Qt::PointingHandCursor);
        ui->butShowPass->setFixedSize(42, 42);
        ui->butShowPass->setStyleSheet(
            "QPushButton {"
            "   background-color: #0F172A;"
            "   color: #94A3B8;"
            "   border: 1.5px solid #1E293B;"
            "   border-radius: 10px;"
            "   font-size: 15px;"
            "}"
            "QPushButton:hover {"
            "   background-color: #1E293B;"
            "   border-color: #38BDF8;"
            "   color: #38BDF8;"
            "}"
            "QPushButton:pressed {"
            "   background-color: #0B101D;"
            "   border-color: #0284C7;"
            "}"
        );
    }

    if(ui->checkAutoLogin)
    {
        ui->checkAutoLogin->setCursor(Qt::PointingHandCursor);
        ui->checkAutoLogin->setText("Войти при запуске");
        ui->checkAutoLogin->setStyleSheet(
            "QCheckBox {"
            "   color: #94A3B8;"
            "   font-size: 12px;"
            "   font-weight: 500;"
            "   spacing: 8px;"
            "   background: transparent;"
            "}"
            "QCheckBox:hover {"
            "   color: #E2E8F0;"
            "}"
            "QCheckBox::indicator {"
            "   width: 17px;"
            "   height: 17px;"
            "   border: 1.5px solid #334155;"
            "   border-radius: 5px;"
            "   background-color: #0F172A;"
            "}"
            "QCheckBox::indicator:hover {"
            "   border-color: #38BDF8;"
            "   background-color: #131E35;"
            "}"
            "QCheckBox::indicator:checked {"
            "   background-color: #0284C7;"
            "   border-color: #38BDF8;"
            "}"
        );
    }

    if(ui->label_2)
    {
        ui->label_2->setCursor(Qt::PointingHandCursor);
        ui->label_2->setText("<a href=\"index\" style=\"color: #38BDF8; text-decoration: none;\">Забыли свой токен?</a>");
        ui->label_2->setStyleSheet("QLabel#label_2 { color: #38BDF8; font-size: 12px; font-weight: 500; background: transparent; }");
    }

    if(ui->authButton)
    {
        ui->authButton->setText("Войти в систему");
        ui->authButton->setFixedHeight(44);
        ui->authButton->setCursor(Qt::PointingHandCursor);
        ui->authButton->setStyleSheet(
            "QPushButton {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0284C7, stop:1 #0EA5E9);"
            "   color: #FFFFFF;"
            "   border: 1px solid rgba(56, 189, 248, 0.4);"
            "   border-radius: 10px;"
            "   font-size: 14px;"
            "   font-weight: 700;"
            "   letter-spacing: 0.5px;"
            "}"
            "QPushButton:hover {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0369A1, stop:1 #38BDF8);"
            "   border-color: #7DD3FC;"
            "}"
            "QPushButton:pressed {"
            "   background: #0284C7;"
            "   border-color: #0284C7;"
            "}"
            "QPushButton:disabled {"
            "   background-color: #1E293B;"
            "   border-color: #334155;"
            "   color: #64748B;"
            "}"
        );
    }

    if(ui->statusAuthText)
    {
        ui->statusAuthText->setAlignment(Qt::AlignCenter);
        ui->statusAuthText->setWordWrap(true);
        ui->statusAuthText->setStyleSheet(
            "QLabel#statusAuthText {"
            "   color: #38BDF8;"
            "   font-size: 12px;"
            "   font-weight: 500;"
            "   background: transparent;"
            "   padding: 2px 8px;"
            "}"
        );
    }

    if(ui->label_auth_ver)
    {
        ui->label_auth_ver->setAlignment(Qt::AlignCenter);
        ui->label_auth_ver->setStyleSheet("color: #475569; font-size: 11px; font-weight: 500; letter-spacing: 0.3px; background: transparent;");
        ui->label_auth_ver->setText("Версия: " + runtimeVersion.mVersion.toString());
    }

    // Assemble modern responsive layout for frame_4
    if(ui->frame_4 && !ui->frame_4->layout())
    {
        QVBoxLayout *cardLayout = new QVBoxLayout(ui->frame_4);
        cardLayout->setContentsMargins(36, 32, 36, 26);
        cardLayout->setSpacing(0);

        // 1. App logo badge
        if(ui->mainapplogo)
            cardLayout->addWidget(ui->mainapplogo, 0, Qt::AlignHCenter);

        cardLayout->addSpacing(14);

        // 2. Title and Subtitle
        if(ui->label_4)
            cardLayout->addWidget(ui->label_4);

        cardLayout->addSpacing(22);

        // 3. Login label
        if(ui->label_12)
            cardLayout->addWidget(ui->label_12);

        cardLayout->addSpacing(6);

        // 4. Login input
        if(ui->lineLoginEdit)
            cardLayout->addWidget(ui->lineLoginEdit);

        cardLayout->addSpacing(14);

        // 5. Password label
        if(ui->label_14)
            cardLayout->addWidget(ui->label_14);

        cardLayout->addSpacing(6);

        // 6. Password row (input + show pass button)
        QHBoxLayout *passRow = new QHBoxLayout();
        passRow->setContentsMargins(0, 0, 0, 0);
        passRow->setSpacing(8);
        if(ui->linePassEdit)
            passRow->addWidget(ui->linePassEdit, 1);
        if(ui->butShowPass)
            passRow->addWidget(ui->butShowPass, 0);
        cardLayout->addLayout(passRow);

        cardLayout->addSpacing(14);

        // 7. Options row (Auto login + Forgot token)
        QHBoxLayout *optRow = new QHBoxLayout();
        optRow->setContentsMargins(0, 0, 0, 0);
        if(ui->checkAutoLogin)
            optRow->addWidget(ui->checkAutoLogin, 0, Qt::AlignVCenter);
        optRow->addStretch(1);
        if(ui->label_2)
            optRow->addWidget(ui->label_2, 0, Qt::AlignVCenter);
        cardLayout->addLayout(optRow);

        cardLayout->addSpacing(20);

        // 8. Sign In CTA button
        if(ui->authButton)
            cardLayout->addWidget(ui->authButton);

        cardLayout->addSpacing(10);

        // 9. Status text banner
        if(ui->statusAuthText)
            cardLayout->addWidget(ui->statusAuthText);

        cardLayout->addStretch(1);

        // 10. Version footer
        if(ui->label_auth_ver)
            cardLayout->addWidget(ui->label_auth_ver);

        // Connect Enter key navigation
        if(ui->lineLoginEdit && ui->linePassEdit)
            connect(ui->lineLoginEdit, &QLineEdit::returnPressed, [this]() { ui->linePassEdit->setFocus(); });
        if(ui->linePassEdit && ui->authButton)
            connect(ui->linePassEdit, &QLineEdit::returnPressed, ui->authButton, &QPushButton::click);
    }

    // ==========================================
    // 2. Cabinet Page (page_cabinet)
    // ==========================================
    if(ui->toplevel_up)
    {
        ui->toplevel_up->setStyleSheet(
            "QFrame#toplevel_up {"
            "   background-color: #1A1D21;"
            "   border-bottom: 1px solid #282B30;"
            "}"
        );
    }
    if(ui->label_6)
    {
        ui->label_6->setStyleSheet(
            "color: #FFFFFF;"
            "font-size: 14px;"
            "font-weight: bold;"
            "letter-spacing: 0.5px;"
            "font-style: normal;"
            "background: transparent;"
        );
    }
    if(ui->logoutButton)
    {
        ui->logoutButton->setCursor(Qt::PointingHandCursor);
        ui->logoutButton->setStyleSheet(
            "QPushButton {"
            "   background-color: rgba(239, 68, 68, 0.12);"
            "   color: #F87171;"
            "   border: 1px solid rgba(239, 68, 68, 0.3);"
            "   border-radius: 6px;"
            "   font-size: 12px;"
            "   font-weight: 600;"
            "   padding: 4px 12px;"
            "}"
            "QPushButton:hover {"
            "   background-color: rgba(239, 68, 68, 0.22);"
            "   color: #FFA3A3;"
            "   border-color: rgba(239, 68, 68, 0.5);"
            "}"
            "QPushButton:pressed {"
            "   background-color: rgba(239, 68, 68, 0.35);"
            "}"
        );
    }
    if(ui->authpageUpdate)
    {
        ui->authpageUpdate->setText(QString::fromUtf8("🔄 Обновить"));
        ui->authpageUpdate->setMinimumSize(105, 30);
        ui->authpageUpdate->setMaximumSize(125, 30);
        ui->authpageUpdate->setCursor(Qt::PointingHandCursor);
        ui->authpageUpdate->setToolTip(QString::fromUtf8("Обновить данные кабинета и доступность сервисов"));
        ui->authpageUpdate->setStyleSheet(
            "QPushButton {"
            "   background-color: #222630;"
            "   border: 1px solid #363D4E;"
            "   border-radius: 6px;"
            "   color: #38BDF8;"
            "   font-size: 12px;"
            "   font-weight: 600;"
            "   padding: 4px 12px;"
            "}"
            "QPushButton:hover {"
            "   background-color: #2D3342;"
            "   border-color: #38BDF8;"
            "   color: #FFFFFF;"
            "}"
            "QPushButton:pressed {"
            "   background-color: #171A21;"
            "   border-color: #0284C7;"
            "}"
        );
    }
    if(ui->frame_7)
    {
        ui->frame_7->setMaximumSize(16777215, 16777215);
        ui->frame_7->setMinimumHeight(140);
        ui->frame_7->setStyleSheet(
            "QFrame#frame_7 {"
            "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #1C1E24, stop:1 #131519);"
            "   border: 1px solid #2B2E36;"
            "   border-radius: 14px;"
            "}"
        );

        if(!ui->frame_7->layout() && ui->frame_6 && ui->authedMainWin && ui->frame_5)
        {
            QHBoxLayout *f7Layout = new QHBoxLayout(ui->frame_7);
            f7Layout->setContentsMargins(16, 12, 16, 12);
            f7Layout->setSpacing(14);
            f7Layout->addWidget(ui->frame_6, 1);
            f7Layout->addWidget(ui->authedMainWin, 1);
            f7Layout->addWidget(ui->frame_5, 1);
        }
    }
    if(ui->authedMainWin)
    {
        ui->authedMainWin->setMaximumSize(16777215, 16777215);
        ui->authedMainWin->setMinimumHeight(110);
        ui->authedMainWin->setStyleSheet(
            "QFrame#authedMainWin {"
            "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #222630, stop:1 #171A21);"
            "   border: 1px solid #2F3543;"
            "   border-radius: 12px;"
            "}"
        );
        if(ui->authedMainWin->layout())
        {
            ui->authedMainWin->layout()->setContentsMargins(10, 8, 10, 8);
            ui->authedMainWin->layout()->setSpacing(4);
            if(ui->frame_3)
                ui->authedMainWin->layout()->setAlignment(ui->frame_3, Qt::AlignCenter);
            if(ui->labelLoginAuthed)
                ui->authedMainWin->layout()->setAlignment(ui->labelLoginAuthed, Qt::AlignCenter);
        }
    }
    if(ui->frame_3)
    {
        ui->frame_3->setFixedSize(58, 58);
        ui->frame_3->setStyleSheet(
            "QFrame#frame_3 {"
            "   image: url(:/resources/no-avatar);"
            "   border: 2px solid #38BDF8;"
            "   border-radius: 29px;"
            "   background-color: #0F1216;"
            "   padding: 3px;"
            "}"
        );
    }
    if(ui->labelLoginAuthed)
    {
        QFont font = ui->labelLoginAuthed->font();
        font.setUnderline(false);
        ui->labelLoginAuthed->setFont(font);
        ui->labelLoginAuthed->setStyleSheet(
            "color: #FFFFFF;"
            "font-size: 15px;"
            "font-weight: bold;"
            "text-decoration: none;"
            "background: transparent;"
        );
        ui->labelLoginAuthed->setAlignment(Qt::AlignCenter);
    }
    if(ui->frame_6)
    {
        ui->frame_6->setMaximumSize(16777215, 16777215);
        ui->frame_6->setMinimumHeight(110);
        ui->frame_6->setStyleSheet(
            "QFrame#frame_6 {"
            "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #2A2113, stop:1 #1A150D);"
            "   border: 1px solid rgba(245, 158, 11, 0.4);"
            "   border-radius: 12px;"
            "}"
        );

        if(!ui->frame_6->findChild<QPushButton *>("buttonAddVip"))
        {
            if(ui->frame_6->layout())
            {
                if(ui->labelVipDays)
                    ui->frame_6->layout()->removeWidget(ui->labelVipDays);
                delete ui->frame_6->layout();
            }

            QVBoxLayout *f6Layout = new QVBoxLayout(ui->frame_6);
            f6Layout->setContentsMargins(10, 8, 10, 8);
            f6Layout->setSpacing(6);
            f6Layout->setAlignment(Qt::AlignCenter);

            if(ui->labelVipDays)
            {
                f6Layout->addWidget(ui->labelVipDays, 0, Qt::AlignCenter);
            }

            QPushButton *btnAddVip = new QPushButton(QString::fromUtf8("+ Добавить"), ui->frame_6);
            btnAddVip->setObjectName("buttonAddVip");
            btnAddVip->setCursor(Qt::PointingHandCursor);
            btnAddVip->setFixedSize(115, 25);
            btnAddVip->setToolTip(QString::fromUtf8("Пополнить или продлить VIP-статус"));
            btnAddVip->setStyleSheet(
                "QPushButton {"
                "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #D97706, stop:1 #F59E0B);"
                "   color: #FFFFFF;"
                "   font-size: 11px;"
                "   font-weight: bold;"
                "   border: none;"
                "   border-radius: 6px;"
                "   padding: 2px 8px;"
                "}"
                "QPushButton:hover {"
                "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #F59E0B, stop:1 #FBBF24);"
                "}"
                "QPushButton:pressed {"
                "   background: #B45309;"
                "}"
            );
            f6Layout->addWidget(btnAddVip, 0, Qt::AlignCenter);

            QObject::connect(btnAddVip, &QPushButton::clicked, this, [this]() {
                for(const auto &s : std::as_const(services))
                {
                    if(s && s->uuid() == IDServiceVIPBuyString)
                    {
                        if(s->active)
                        {
                            this->runService(s);
                        }
                        else
                        {
                            QMessageBox::warning(this, QString::fromUtf8("Пополнение VIP"), QString::fromUtf8("Сервис пополнения VIP временно недоступен."));
                        }
                        return;
                    }
                }
                QMessageBox::warning(this, QString::fromUtf8("Пополнение VIP"), QString::fromUtf8("Сервис пополнения VIP недоступен."));
            });
        }
    }
    if(ui->labelVipDays)
    {
        QFont font = ui->labelVipDays->font();
        font.setUnderline(false);
        ui->labelVipDays->setFont(font);
        ui->labelVipDays->setStyleSheet(
            "color: #FBBF24;"
            "font-size: 13.5px;"
            "font-weight: bold;"
            "text-decoration: none;"
            "line-height: 1.4;"
            "background: transparent;"
        );
        ui->labelVipDays->setAlignment(Qt::AlignCenter);
    }
    if(ui->frame_5)
    {
        ui->frame_5->setMaximumSize(16777215, 16777215);
        ui->frame_5->setMinimumHeight(110);
        ui->frame_5->setStyleSheet(
            "QFrame#frame_5 {"
            "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #14241B, stop:1 #0E1A13);"
            "   border: 1px solid rgba(52, 211, 153, 0.4);"
            "   border-radius: 12px;"
            "}"
        );
    }
    if(ui->labelCredits)
    {
        QFont font = ui->labelCredits->font();
        font.setUnderline(false);
        ui->labelCredits->setFont(font);
        ui->labelCredits->setStyleSheet(
            "color: #34D399;"
            "font-size: 13.5px;"
            "font-weight: bold;"
            "text-decoration: none;"
            "line-height: 1.4;"
            "background: transparent;"
        );
        ui->labelCredits->setAlignment(Qt::AlignCenter);
    }
    if(ui->toplevel_up_2)
    {
        ui->toplevel_up_2->setStyleSheet(
            "QFrame#toplevel_up_2 {"
            "   background-color: #17191E;"
            "   border-top: 1px solid #262930;"
            "   border-bottom: 1px solid #262930;"
            "   border-radius: 6px;"
            "}"
        );
    }
    if(ui->label_7)
    {
        ui->label_7->setText(QString::fromUtf8("ДОСТУПНЫЕ СЕРВИСЫ"));
        ui->label_7->setStyleSheet(
            "color: #38BDF8;"
            "font-size: 12px;"
            "font-weight: bold;"
            "letter-spacing: 1.2px;"
            "background: transparent;"
        );
    }
    if(ui->serviceContents)
    {
        ui->serviceContents->setStyleSheet("QFrame#serviceContents { background: transparent; border: none; }");
    }
    if(ui->authInfo)
    {
        ui->authInfo->setVisible(false);
    }
    if(ui->sss && ui->serviceContents && !ui->scrollAreaWidgetContents_3->findChild<QFrame *>("cabinetSideInfoPanel"))
    {
        for(int i = ui->sss->count() - 1; i >= 0; --i)
        {
            QLayoutItem *item = ui->sss->itemAt(i);
            if(item && item->widget() != ui->serviceContents)
            {
                ui->sss->takeAt(i);
                delete item;
            }
        }

        // Side reference card / Справочник аккаунта
        QFrame *sidePanel = new QFrame(ui->scrollAreaWidgetContents_3);
        sidePanel->setObjectName("cabinetSideInfoPanel");
        sidePanel->setFixedWidth(290);
        sidePanel->setStyleSheet(
            "QFrame#cabinetSideInfoPanel {"
            "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #1D2028, stop:1 #13151B);"
            "   border: 1px solid #2C313E;"
            "   border-radius: 14px;"
            "}"
        );

        QVBoxLayout *sideLayout = new QVBoxLayout(sidePanel);
        sideLayout->setContentsMargins(14, 14, 14, 14);
        sideLayout->setSpacing(7);

        QLabel *sideTitle = new QLabel(QString::fromUtf8("📋 СПРАВОЧНИК АККАУНТА"), sidePanel);
        sideTitle->setStyleSheet(
            "color: #38BDF8;"
            "font-size: 11px;"
            "font-weight: bold;"
            "letter-spacing: 1.1px;"
            "background: transparent;"
            "padding-bottom: 6px;"
            "border-bottom: 1px solid #282C38;"
        );
        sideLayout->addWidget(sideTitle);

        auto createRow = [sidePanel, sideLayout](const QString &icon, const QString &caption, const QString &valObjName) {
            QFrame *row = new QFrame(sidePanel);
            row->setStyleSheet(
                "QFrame {"
                "   background-color: #171922;"
                "   border: 1px solid #242836;"
                "   border-radius: 8px;"
                "}"
            );
            QHBoxLayout *rl = new QHBoxLayout(row);
            rl->setContentsMargins(8, 5, 10, 5);
            rl->setSpacing(8);

            QLabel *iconLbl = new QLabel(icon, row);
            iconLbl->setFixedSize(26, 26);
            iconLbl->setAlignment(Qt::AlignCenter);
            iconLbl->setStyleSheet(
                "background-color: #212635;"
                "border-radius: 6px;"
                "font-size: 13px;"
            );
            rl->addWidget(iconLbl);

            QVBoxLayout *col = new QVBoxLayout();
            col->setContentsMargins(0, 0, 0, 0);
            col->setSpacing(1);

            QLabel *capLbl = new QLabel(caption, row);
            capLbl->setStyleSheet("color: #64748B; font-size: 10px; font-weight: 500; background: transparent; border: none;");
            col->addWidget(capLbl);

            QLabel *valLbl = new QLabel("-", row);
            valLbl->setObjectName(valObjName);
            valLbl->setStyleSheet("color: #F1F5F9; font-size: 12px; font-weight: bold; background: transparent; border: none;");
            col->addWidget(valLbl);

            rl->addLayout(col, 1);
            sideLayout->addWidget(row);
        };

        createRow("👤", QString::fromUtf8("Логин аккаунта"), "cabinetVal_login");
        createRow("🕒", QString::fromUtf8("Время входа"), "cabinetVal_loginTime");
        createRow("💳", QString::fromUtf8("Баланс кредитов"), "cabinetVal_credits");
        createRow("👑", QString::fromUtf8("VIP-статус"), "cabinetVal_vip");
        createRow("📱", QString::fromUtf8("Подключено устройств"), "cabinetVal_devices");
        createRow("🌐", QString::fromUtf8("Локация"), "cabinetVal_location");
        createRow("🛡️", QString::fromUtf8("Статус безопасности"), "cabinetVal_status");

        sideLayout->addStretch(1);

        ui->sss->setContentsMargins(10, 8, 10, 14);
        ui->sss->setSpacing(16);
        ui->sss->setAlignment(ui->serviceContents, Qt::AlignTop);
        ui->sss->insertStretch(0, 1);
        ui->sss->addWidget(sidePanel, 0, Qt::AlignTop);
        ui->sss->addStretch(1);
    }

    // ==========================================
    // 3. Devices Connection Page (page_devices)
    // ==========================================
    if(auto *hLayout = qobject_cast<QHBoxLayout *>(ui->page_devices->layout()))
    {
        hLayout->setContentsMargins(18, 14, 18, 14);
        hLayout->setSpacing(20);
        hLayout->setStretch(0, 0); // horizontalSpacer_2
        hLayout->setStretch(1, 5); // device_left_group
        hLayout->setStretch(2, 6); // device_right_group
        hLayout->setStretch(3, 0); // horizontalSpacer
    }

    if(ui->scrollArea_2)
    {
        ui->scrollArea_2->setFrameShape(QFrame::NoFrame);
        ui->scrollArea_2->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        ui->scrollArea_2->setStyleSheet("QScrollArea { background: transparent; border: none; } QWidget#scrollAreaWidgetContents_2 { background: transparent; }");
    }

    if(ui->device_left_group)
    {
        ui->device_left_group->setStyleSheet(
            "QFrame#device_left_group {"
            "   background-color: #0F172A;"
            "   border: 1px solid #1E293B;"
            "   border-radius: 16px;"
            "}"
        );
    }

    if(ui->label)
    {
        ui->label->setText(
            "<html><head/><body>"
            "<div style=\"font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; color: #E2E8F0; padding: 2px;\">"
            "  <div style=\"margin-bottom: 12px;\">"
            "    <span style=\"font-size: 16px; font-weight: 700; color: #F8FAFC;\">Подключение устройства (ADB)</span>"
            "  </div>"
            "  <p style=\"color: #94A3B8; font-size: 11.5px; margin: 0 0 12px 0; line-height: 1.4;\">"
            "    Для выполнения процедур активируйте <b>Отладку по USB</b> на вашем Android-смартфоне:"
            "  </p>"
            "  <div style=\"background: #1E293B; border: 1px solid #334155; border-radius: 9px; padding: 9px 12px; margin-bottom: 8px;\">"
            "    <span style=\"background: #0284C7; color: #FFFFFF; border-radius: 10px; padding: 2px 8px; font-weight: bold; font-size: 11px;\">1</span>"
            "    <strong style=\"color: #F8FAFC; font-size: 12.5px; margin-left: 6px;\">Режим разработчика</strong>"
            "    <p style=\"margin: 4px 0 0 24px; color: #94A3B8; font-size: 11.5px; line-height: 1.4;\">"
            "      Откройте <b>Настройки</b> &rarr; <b>О телефоне</b>. Найдите <b>Номер сборки</b> (или версию MIUI/HyperOS) и нажмите на него <b>7 раз</b> подряд."
            "    </p>"
            "  </div>"
            "  <div style=\"background: #1E293B; border: 1px solid #334155; border-radius: 9px; padding: 9px 12px; margin-bottom: 8px;\">"
            "    <span style=\"background: #0284C7; color: #FFFFFF; border-radius: 10px; padding: 2px 8px; font-weight: bold; font-size: 11px;\">2</span>"
            "    <strong style=\"color: #F8FAFC; font-size: 12.5px; margin-left: 6px;\">Включите отладку по USB</strong>"
            "    <p style=\"margin: 4px 0 0 24px; color: #94A3B8; font-size: 11.5px; line-height: 1.4;\">"
            "      Перейдите в <b>Настройки</b> &rarr; <b>Для разработчиков</b> и активируйте тумблер <b>Отладка по USB</b> (для Xiaomi также «Установка через USB»)."
            "    </p>"
            "  </div>"
            "  <div style=\"background: #1E293B; border: 1px solid #334155; border-radius: 9px; padding: 9px 12px; margin-bottom: 8px;\">"
            "    <span style=\"background: #0284C7; color: #FFFFFF; border-radius: 10px; padding: 2px 8px; font-weight: bold; font-size: 11px;\">3</span>"
            "    <strong style=\"color: #F8FAFC; font-size: 12.5px; margin-left: 6px;\">Подключите кабель к ПК</strong>"
            "    <p style=\"margin: 4px 0 0 24px; color: #94A3B8; font-size: 11.5px; line-height: 1.4;\">"
            "      Соедините устройство кабелем. На экране телефона появится запрос &mdash; отметьте <b>«Всегда разрешать с этого компьютера»</b> и нажмите <b>ОК</b>."
            "    </p>"
            "  </div>"
            "  <div style=\"background: rgba(30, 41, 59, 0.4); border: 1px dashed #334155; border-radius: 8px; padding: 8px 12px; margin-top: 6px;\">"
            "    <span style=\"color: #38BDF8; font-size: 11.5px; font-weight: 600;\">💡 Телефон не определяется?</span>"
            "    <p style=\"margin: 3px 0 0 0; color: #64748B; font-size: 11px; line-height: 1.35;\">"
            "      Смените режим подключения USB на <b>«Передача файлов (MTP)»</b> либо подключите кабель в другой USB-порт на ПК."
            "    </p>"
            "  </div>"
            "</div>"
            "</body></html>"
        );
    }
    if(ui->label_3)
    {
        ui->label_3->setText("<a style=\"color: #38BDF8; text-decoration: none; font-size: 12px; font-weight: 500;\" href=\"https://www.anymp4.com/ru/faq/enable-usb-debugging-for-android.html\">📖 Подробная пошаговая инструкция с иллюстрациями &rarr;</a>");
    }
    if(ui->label_5)
    {
        ui->label_5->setStyleSheet(
            "background-color: #0B1120;"
            "border: 1px solid #1E3A5F;"
            "border-radius: 10px;"
            "color: #38BDF8;"
            "font-size: 12.5px;"
            "font-weight: 600;"
            "padding: 10px;"
        );
        ui->label_5->setText(QString::fromUtf8("📡 Поиск подключенного Android-устройства..."));
    }

    if(ui->device_right_group && !s_adbVisualizer)
    {
        while(auto item = ui->device_right_group->takeAt(0))
        {
            if(item->widget())
            {
                item->widget()->hide();
                item->widget()->deleteLater();
            }
            delete item;
        }

        AdbDeviceVisualizer *visualizer = new AdbDeviceVisualizer(ui->page_devices);
        visualizer->setObjectName("adbDeviceVisualizer");
        ui->device_right_group->addWidget(visualizer);
    }

    // ==========================================
    // 4. Procedures & Scan Execution Page (page_adsmalware)
    // ==========================================
    if(ui->deviceLabelName)
    {
        ui->deviceLabelName->setStyleSheet(
            "background-color: #1E2229;"
            "border: 1px solid #2E333D;"
            "border-radius: 8px;"
            "color: #4CC2FF;"
            "font-size: 14px;"
            "font-weight: bold;"
            "font-style: normal;"
            "padding: 8px 16px;"
        );
    }
    if(ui->processLogStatus)
    {
        ui->processLogStatus->setStyleSheet(
            "QListView {"
            "   background-color: #121316;"
            "   border: 1px solid #282A2E;"
            "   border-radius: 8px;"
            "   color: #9CA3AF;"
            "   font-family: 'Consolas', 'DejaVu Sans Mono', 'Courier New', monospace;"
            "   font-size: 11px;"
            "   padding: 8px;"
            "}"
            "QListView::item:selected {"
            "   background-color: #26292F;"
            "   color: #4CC2FF;"
            "}"
        );
    }
    if(ui->processBarStatus)
    {
        ui->processBarStatus->setStyleSheet(
            "QProgressBar {"
            "   background-color: #1A1C20;"
            "   border: 1px solid #2E3238;"
            "   border-radius: 5px;"
            "   height: 16px;"
            "   text-align: center;"
            "   color: #FFFFFF;"
            "   font-size: 11px;"
            "   font-weight: bold;"
            "}"
            "QProgressBar::chunk {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0078D4, stop:1 #4CC2FF);"
            "   border-radius: 4px;"
            "}"
        );
    }
    if(ui->malwareStatusText0)
    {
        ui->malwareStatusText0->setStyleSheet(
            "color: #E5E7EB;"
            "font-size: 14px;"
            "font-weight: 600;"
            "padding: 6px;"
            "font-style: normal;"
            "text-decoration: none;"
        );
    }
    if(ui->malwareReRun)
    {
        ui->malwareReRun->setCursor(Qt::PointingHandCursor);
        ui->malwareReRun->setStyleSheet(
            "QPushButton {"
            "   background-color: #0078D4;"
            "   border: 1px solid #005A9E;"
            "   border-radius: 8px;"
            "   color: #FFFFFF;"
            "   font-size: 13px;"
            "   font-weight: bold;"
            "   padding: 10px 20px;"
            "}"
            "QPushButton:hover {"
            "   background-color: #1084D9;"
            "   border-color: #4CC2FF;"
            "}"
            "QPushButton:pressed {"
            "   background-color: #004C87;"
            "}"
        );
    }

    // ==========================================
    // 5. Loader Page (page_loader)
    // ==========================================
    if(ui->frame_loader)
    {
        ui->frame_loader->setStyleSheet(
            "QFrame#frame_loader {"
            "   background-color: #1E2024;"
            "   border: 1px solid #2D3139;"
            "   border-radius: 12px;"
            "}"
        );
    }
    if(ui->loaderPageText)
    {
        ui->loaderPageText->setStyleSheet(
            "color: #9CA3AF;"
            "font-size: 14px;"
            "font-weight: 600;"
            "letter-spacing: 0.5px;"
        );
    }

    // ==========================================
    // 6. My Devices & Warranty Page (page_mydevices)
    // ==========================================
    if(ui->label_9)
    {
        ui->label_9->setStyleSheet("color: #9CA3AF; font-size: 12px; font-weight: 600;");
    }
    if(ui->label_10)
    {
        ui->label_10->setStyleSheet("color: #9CA3AF; font-size: 12px; font-weight: 600;");
    }
    if(ui->myDeviceFilterDateStart)
    {
        ui->myDeviceFilterDateStart->setStyleSheet(
            "QDateEdit {"
            "   background-color: #1E2228;"
            "   border: 1px solid #363A42;"
            "   border-radius: 6px;"
            "   color: #FFFFFF;"
            "   font-size: 12px;"
            "   padding: 4px 8px;"
            "}"
            "QDateEdit:focus {"
            "   border: 1px solid #4CC2FF;"
            "}"
        );
    }
    if(ui->myDeviceFilterDateEnd)
    {
        ui->myDeviceFilterDateEnd->setStyleSheet(
            "QDateEdit {"
            "   background-color: #1E2228;"
            "   border: 1px solid #363A42;"
            "   border-radius: 6px;"
            "   color: #FFFFFF;"
            "   font-size: 12px;"
            "   padding: 4px 8px;"
            "}"
            "QDateEdit:focus {"
            "   border: 1px solid #4CC2FF;"
            "}"
        );
    }
    if(ui->myDeviceQuaranteeFilter)
    {
        ui->myDeviceQuaranteeFilter->setCursor(Qt::PointingHandCursor);
        ui->myDeviceQuaranteeFilter->setStyleSheet(
            "QCheckBox {"
            "   color: #D1D5DB;"
            "   font-size: 12px;"
            "   font-weight: 500;"
            "   spacing: 6px;"
            "}"
            "QCheckBox::indicator {"
            "   width: 16px;"
            "   height: 16px;"
            "   border: 1px solid #3E434D;"
            "   border-radius: 4px;"
            "   background-color: #1E2024;"
            "}"
            "QCheckBox::indicator:hover {"
            "   border-color: #4CC2FF;"
            "}"
            "QCheckBox::indicator:checked {"
            "   background-color: #0078D4;"
            "   border-color: #0078D4;"
            "}"
        );
    }
    if(ui->myDeviceSend)
    {
        ui->myDeviceSend->setCursor(Qt::PointingHandCursor);
        ui->myDeviceSend->setStyleSheet(
            "QPushButton {"
            "   background-color: #0078D4;"
            "   border: 1px solid #005A9E;"
            "   border-radius: 6px;"
            "   color: #FFFFFF;"
            "   font-size: 12px;"
            "   font-weight: 600;"
            "   padding: 5px 16px;"
            "}"
            "QPushButton:hover {"
            "   background-color: #1084D9;"
            "   border-color: #4CC2FF;"
            "}"
            "QPushButton:pressed {"
            "   background-color: #004C87;"
            "}"
        );
    }
    if(ui->myDeviceActual)
    {
        ui->myDeviceActual->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        ui->myDeviceActual->horizontalHeader()->setStretchLastSection(true);
        ui->myDeviceActual->setStyleSheet(
            "QTableView {"
            "   background-color: #1A1C20;"
            "   alternate-background-color: #16181B;"
            "   gridline-color: #282A2E;"
            "   border: 1px solid #282A2E;"
            "   border-radius: 8px;"
            "   color: #D1D5DB;"
            "   font-size: 12px;"
            "   selection-background-color: #0078D4;"
            "   selection-color: #FFFFFF;"
            "}"
            "QHeaderView::section {"
            "   background-color: #22252B;"
            "   color: #9CA3AF;"
            "   font-size: 12px;"
            "   font-weight: bold;"
            "   border: none;"
            "   border-bottom: 1px solid #2E3238;"
            "   border-right: 1px solid #282A2E;"
            "   padding: 6px 8px;"
            "}"
        );
    }
    if(ui->myDevicePageLabel)
    {
        ui->myDevicePageLabel->setStyleSheet("color: #6B7280; font-size: 11px; padding: 4px;");
    }

    // ==========================================
    // 7. VIP Subscription Page (page_buyvip)
    // ==========================================
    if(ui->groupBox)
    {
        ui->groupBox->setStyleSheet(
            "QGroupBox {"
            "   background-color: #1E2024;"
            "   border: 1px solid #2D3139;"
            "   border-radius: 12px;"
            "   margin-top: 24px;"
            "   padding: 24px 20px 20px 20px;"
            "   font-size: 15px;"
            "   font-weight: bold;"
            "   color: #FFFFFF;"
            "}"
            "QGroupBox::title {"
            "   subcontrol-origin: margin;"
            "   subcontrol-position: top center;"
            "   padding: 4px 16px;"
            "   background-color: #26292F;"
            "   border: 1px solid #363940;"
            "   border-radius: 6px;"
            "   color: #FFFFFF;"
            "}"
        );
    }
    if(ui->label_13)
    {
        ui->label_13->setStyleSheet("color: #9CA3AF; font-size: 13px; font-weight: 500; margin-bottom: 4px;");
    }
    if(ui->labelVipBalance)
    {
        ui->labelVipBalance->setStyleSheet(
            "background-color: #13271D;"
            "border: 1px solid #16532E;"
            "border-radius: 8px;"
            "color: #4ADE80;"
            "font-size: 14px;"
            "font-weight: bold;"
            "padding: 10px 14px;"
        );
    }
    if(ui->comboBoxSelectVIPDays)
    {
        ui->comboBoxSelectVIPDays->setStyleSheet(
            "QComboBox {"
            "   background-color: #18191C;"
            "   border: 1px solid #32353B;"
            "   border-radius: 6px;"
            "   color: #FFFFFF;"
            "   font-size: 13px;"
            "   padding: 6px 12px;"
            "   min-height: 24px;"
            "}"
            "QComboBox:hover {"
            "   border-color: #4CC2FF;"
            "}"
            "QComboBox::drop-down {"
            "   border: none;"
            "   width: 24px;"
            "}"
            "QComboBox QAbstractItemView {"
            "   background-color: #1E2024;"
            "   border: 1px solid #32353B;"
            "   selection-background-color: #0078D4;"
            "   selection-color: #FFFFFF;"
            "   color: #FFFFFF;"
            "   padding: 4px;"
            "}"
        );
    }
    if(ui->frame)
    {
        ui->frame->setStyleSheet(
            "QFrame#frame {"
            "   background-color: #18191C;"
            "   border: 1px solid #2A2D33;"
            "   border-radius: 8px;"
            "   padding: 10px;"
            "}"
        );
    }
    if(ui->labelInfoVip)
    {
        ui->labelInfoVip->setStyleSheet("color: #F3F4F6; font-size: 13px; font-weight: 600;");
    }
    if(ui->buttonBuyVip)
    {
        ui->buttonBuyVip->setCursor(Qt::PointingHandCursor);
        ui->buttonBuyVip->setStyleSheet(
            "QPushButton {"
            "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #10B981, stop:1 #059669);"
            "   border: 1px solid #059669;"
            "   border-radius: 6px;"
            "   color: #FFFFFF;"
            "   font-size: 13px;"
            "   font-weight: bold;"
            "   padding: 8px 20px;"
            "}"
            "QPushButton:hover {"
            "   background: #10B981;"
            "   border-color: #34D399;"
            "}"
            "QPushButton:pressed {"
            "   background: #047857;"
            "}"
        );
    }

    // ==========================================
    // 8. Top Back Bar (toplevel_backpage)
    // ==========================================
    if(ui->toplevel_backpage)
    {
        ui->toplevel_backpage->setStyleSheet(
            "QFrame#toplevel_backpage {"
            "   background-color: #1A1D21;"
            "   border-bottom: 1px solid #282B30;"
            "}"
        );
    }
    if(ui->buttonBackTo)
    {
        ui->buttonBackTo->setText("‹ Назад");
        ui->buttonBackTo->setCursor(Qt::PointingHandCursor);
        ui->buttonBackTo->setStyleSheet(
            "QPushButton {"
            "   background-color: #26292F;"
            "   border: 1px solid #363940;"
            "   border-radius: 6px;"
            "   color: #E5E7EB;"
            "   font-size: 12px;"
            "   font-weight: 600;"
            "   padding: 5px 14px;"
            "}"
            "QPushButton:hover {"
            "   background-color: #32363E;"
            "   border-color: #4CC2FF;"
            "   color: #4CC2FF;"
            "}"
            "QPushButton:pressed {"
            "   background-color: #1A1B1E;"
            "}"
        );
    }
    if(ui->label_8)
    {
        ui->label_8->setStyleSheet(
            "color: #FFFFFF;"
            "font-size: 14px;"
            "font-weight: bold;"
            "letter-spacing: 0.5px;"
            "font-style: normal;"
            "background: transparent;"
        );
    }
}

void MainWindow::initServiceModules()
{
    QString tmp0;
    int x, y;

    if(!services.isEmpty() || !serverServices)
        return;

    std::shared_ptr<Service> instance = nullptr;
    std::list<std::shared_ptr<Service>> buildServices = Service::EnumAppServices(this);

    for(x = 0, y = serverServices->size(); x < y; ++x)
    {
        const ServiceItemInfo *remoteService = &(serverServices->at(x));
        if(remoteService->hide)
            continue;

        // Find build uuid service.
        for(auto iter = std::begin(buildServices); iter != std::end(buildServices); ++iter)
        {
            if(remoteService->uuid == (*iter)->uuid())
            {
                instance = std::move(*iter);
                buildServices.erase(iter);
                break;
            }
        }

        if(!instance)
            instance = std::make_shared<UnavailableService>(this);

#if SHOW_SERVICE_BY_DEBUG
        instance->active = instance->isAvailable(); // EVERYTHING TRUE
#else
        instance->active = remoteService->active && instance->isAvailable();
#endif

        QString badgeText;
        QString styleSheet;
        uint32_t effectivePrice = (x == 0 && network.authedId.basePrice > 0 ? network.authedId.basePrice : remoteService->price);
        bool hasVIP = network.authedId.hasVipAccount();
        bool isDynamic = (remoteService->price == static_cast<std::uint32_t>(-1));
        bool isFree = (effectivePrice == 0);
        bool canBypassWithVip = (remoteService->needVIP && hasVIP);

        if(!instance->active)
        {
            if(!instance->isAvailable())
                badgeText = QString::fromUtf8("🔧 В разработке");
            else if(!remoteService->active)
                badgeText = QString::fromUtf8("⛔ Не доступен");
            else
                badgeText = QString::fromUtf8("⛔ Отключен");

            styleSheet =
                "QPushButton {"
                "   text-align: left;"
                "   padding: 10px 14px 10px 12px;"
                "   font-size: 12px;"
                "   font-weight: 500;"
                "   background-color: #17181C;"
                "   color: #64748B;"
                "   border-radius: 12px;"
                "   border: 1px solid #282A30;"
                "   border-left: 5px solid #475569;"
                "}";
        }
        else if(canBypassWithVip)
        {
            // Service supports VIP and user HAS VIP account -> Unlimited access
            badgeText = QString::fromUtf8("👑 VIP • БЕЗЛИМИТ");

            styleSheet =
                "QPushButton {"
                "   text-align: left;"
                "   padding: 10px 14px 10px 12px;"
                "   font-size: 12px;"
                "   font-weight: bold;"
                "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #2C2213, stop:0.5 #231B0E, stop:1 #1A140A);"
                "   color: #FEF3C7;"
                "   border-radius: 12px;"
                "   border: 1px solid rgba(245, 158, 11, 0.45);"
                "   border-left: 5px solid #F59E0B;"
                "}"
                "QPushButton:hover {"
                "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #3D2E17, stop:0.5 #312411, stop:1 #241A0B);"
                "   border-color: #FBBF24;"
                "   border-left: 5px solid #FCD34D;"
                "   color: #FFFFFF;"
                "}"
                "QPushButton:pressed {"
                "   background: #140E05;"
                "   border-color: #D97706;"
                "}";
        }
        else if(isFree)
        {
            // Completely free service for everyone
            badgeText = QString::fromUtf8("БЕСПЛАТНО");

            styleSheet =
                "QPushButton {"
                "   text-align: left;"
                "   padding: 10px 14px 10px 12px;"
                "   font-size: 12px;"
                "   font-weight: bold;"
                "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #15271D, stop:0.5 #112017, stop:1 #0D1912);"
                "   color: #D1FAE5;"
                "   border-radius: 12px;"
                "   border: 1px solid rgba(52, 211, 153, 0.4);"
                "   border-left: 5px solid #10B981;"
                "}"
                "QPushButton:hover {"
                "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #1E382A, stop:0.5 #182D21, stop:1 #122219);"
                "   border-color: #34D399;"
                "   border-left: 5px solid #6EE7B7;"
                "   color: #FFFFFF;"
                "}"
                "QPushButton:pressed {"
                "   background: #09120D;"
                "   border-color: #059669;"
                "}";
        }
        else if(isDynamic)
        {
            badgeText = remoteService->needVIP ? QString::fromUtf8("⚙ ТАРИФ НА ВЫБОР (или 👑 VIP)") : QString::fromUtf8("⚙ ТАРИФ НА ВЫБОР");

            styleSheet =
                "QPushButton {"
                "   text-align: left;"
                "   padding: 10px 14px 10px 12px;"
                "   font-size: 12px;"
                "   font-weight: bold;"
                "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #1A2232, stop:0.5 #141B28, stop:1 #0F141F);"
                "   color: #E0F2FE;"
                "   border-radius: 12px;"
                "   border: 1px solid rgba(56, 189, 248, 0.4);"
                "   border-left: 5px solid #38BDF8;"
                "}"
                "QPushButton:hover {"
                "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #233047, stop:0.5 #1C2638, stop:1 #151D2C);"
                "   border-color: #38BDF8;"
                "   border-left: 5px solid #7DD3FC;"
                "   color: #FFFFFF;"
                "}"
                "QPushButton:pressed {"
                "   background: #0B0E16;"
                "   border-color: #0284C7;"
                "}";
        }
        else if(remoteService->needVIP)
        {
            // Service supports VIP, but user has NO VIP -> Show price in credits with VIP alternative
            badgeText = QString::fromUtf8("💳 %1 %2  (или 👑 VIP)").arg(effectivePrice).arg(network.authedId.currencyType);

            styleSheet =
                "QPushButton {"
                "   text-align: left;"
                "   padding: 10px 14px 10px 12px;"
                "   font-size: 12px;"
                "   font-weight: bold;"
                "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #1C2333, stop:0.5 #161C2A, stop:1 #111520);"
                "   color: #F0F9FF;"
                "   border-radius: 12px;"
                "   border: 1px solid rgba(245, 158, 11, 0.4);"
                "   border-left: 5px solid #F59E0B;"
                "}"
                "QPushButton:hover {"
                "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #253147, stop:0.5 #1D2638, stop:1 #161D2B);"
                "   border-color: #FBBF24;"
                "   border-left: 5px solid #FCD34D;"
                "   color: #FFFFFF;"
                "}"
                "QPushButton:pressed {"
                "   background: #0D121B;"
                "   border-color: #D97706;"
                "}";
        }
        else
        {
            // Service does NOT require/support VIP -> Standard credit price for all users
            badgeText = QString::fromUtf8("💳 %1 %2").arg(effectivePrice).arg(network.authedId.currencyType);

            styleSheet =
                "QPushButton {"
                "   text-align: left;"
                "   padding: 10px 14px 10px 12px;"
                "   font-size: 12px;"
                "   font-weight: bold;"
                "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #1A2232, stop:0.5 #141B28, stop:1 #0F141F);"
                "   color: #E0F2FE;"
                "   border-radius: 12px;"
                "   border: 1px solid rgba(56, 189, 248, 0.4);"
                "   border-left: 5px solid #0284C7;"
                "}"
                "QPushButton:hover {"
                "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #233047, stop:0.5 #1C2638, stop:1 #151D2C);"
                "   border-color: #38BDF8;"
                "   border-left: 5px solid #38BDF8;"
                "   color: #FFFFFF;"
                "}"
                "QPushButton:pressed {"
                "   background: #0B0E16;"
                "   border-color: #0284C7;"
                "}";
        }

        tmp0 = remoteService->name + "\n" + badgeText;

        if(instance->uuid() != IDServiceAIAgentString)
        {
            QPushButton *button = new QPushButton(QIcon(":/service-icons/" + instance->widgetIconName()), tmp0, ui->serviceContents);
            button->setCursor(instance->active ? Qt::PointingHandCursor : Qt::ForbiddenCursor);
            button->setStyleSheet(styleSheet);
            button->setIconSize({52, 52});
            button->setFixedSize(260, 80);
            button->setEnabled(instance->active);

            QString tip = remoteService->name;
            if(!remoteService->description.isEmpty())
                tip += "\n" + remoteService->description;
            if(instance->active)
            {
                if(canBypassWithVip)
                {
                    tip += "\n\n• Включено в VIP-подписку (безлимитно, без списания кредитов)";
                }
                else if(isFree)
                {
                    tip += "\n\n• Бесплатная услуга (0 кредитов)";
                }
                else if(remoteService->needVIP)
                {
                    tip += QString("\n\n• Стоимость: %1 %2 (или бесплатно с активным VIP-аккаунтом)").arg(effectivePrice).arg(network.authedId.currencyType);
                }
                else if(!isDynamic)
                {
                    tip += QString("\n\n• Стоимость: %1 %2 (оплата кредитами)").arg(effectivePrice).arg(network.authedId.currencyType);
                }
            }
            button->setToolTip(tip);

            // Target service by slot with credit verification
            QObject::connect(
                button,
                &QPushButton::clicked,
                this,
                [this, instance, effectivePrice, isDynamic, needVIP = remoteService->needVIP, name = remoteService->name]()
                {
                    bool bypass = (needVIP && network.authedId.hasVipAccount());
                    if(!bypass && effectivePrice > 0 && !isDynamic && network.authedId.credits < effectivePrice)
                    {
                        QMessageBox msgBox(this);
                        msgBox.setWindowTitle(QString::fromUtf8("Недостаточно кредитов"));
                        msgBox.setIcon(QMessageBox::Warning);

                        const int shortage = effectivePrice - static_cast<int>(network.authedId.credits);
                        const QString &currency = network.authedId.currencyType;

                        QString infoHtml = QString::fromUtf8(
                            "<div style='min-width: 330px; font-family: Segoe UI, sans-serif; font-size: 13px; color: #E2E8F0;'>"
                            "<p style='font-size: 15px; font-weight: bold; color: #F87171; margin-bottom: 8px;'>Недостаточно кредитов для запуска</p>"
                            "<p style='margin-bottom: 10px;'>Для запуска сервиса <b style='color: #38BDF8;'>«%1»</b> на вашем балансе не хватает средств.</p>"
                            "<table style='width: 100%; border-collapse: collapse; margin-bottom: 12px; background: rgba(30, 41, 59, 0.7); border: 1px solid rgba(148, 163, 184, 0.2); border-radius: 6px;'>"
                            "<tr><td style='padding: 6px 10px; color: #94A3B8;'>Стоимость услуги:</td><td style='padding: 6px 10px; text-align: right; font-weight: bold; color: #F8FAFC;'>%2 %3</td></tr>"
                            "<tr><td style='padding: 6px 10px; color: #94A3B8;'>На вашем балансе:</td><td style='padding: 6px 10px; text-align: right; font-weight: bold; color: #FBBF24;'>%4 %3</td></tr>"
                            "<tr style='border-top: 1px solid rgba(148, 163, 184, 0.2);'><td style='padding: 6px 10px; color: #EF4444; font-weight: bold;'>Не хватает:</td><td style='padding: 6px 10px; text-align: right; font-weight: bold; color: #EF4444;'>%5 %3</td></tr>"
                            "</table>"
                            "%6"
                            "</div>"
                        ).arg(name)
                         .arg(effectivePrice)
                         .arg(currency)
                         .arg(network.authedId.credits)
                         .arg(shortage > 0 ? shortage : 0)
                         .arg(needVIP
                              ? QString::fromUtf8("<p style='margin: 4px 0 0 0; color: #CBD5E1; line-height: 1.4;'>💡 <i>Вы можете активировать <b>VIP-статус</b> для безлимитного доступа без списания кредитов, либо пополнить баланс через службу поддержки.</i></p>")
                              : QString::fromUtf8("<p style='margin: 4px 0 0 0; color: #CBD5E1; line-height: 1.4;'>💳 <i>Данная услуга оплачивается только кредитами (VIP-статус не поддерживается). Пополните баланс через раздел Поддержка.</i></p>"));

                        msgBox.setText(infoHtml);

                        msgBox.setStyleSheet(
                            "QMessageBox {"
                            "   background-color: #0F172A;"
                            "   color: #F8FAFC;"
                            "   border: 1px solid #334155;"
                            "   border-radius: 12px;"
                            "}"
                            "QLabel {"
                            "   color: #F8FAFC;"
                            "   background: transparent;"
                            "}"
                            "QPushButton {"
                            "   background-color: #334155;"
                            "   color: #F8FAFC;"
                            "   border: 1px solid #475569;"
                            "   border-radius: 6px;"
                            "   padding: 6px 16px;"
                            "   font-size: 12px;"
                            "   font-weight: bold;"
                            "   min-width: 85px;"
                            "}"
                            "QPushButton:hover {"
                            "   background-color: #475569;"
                            "   border-color: #64748B;"
                            "}"
                            "QPushButton:pressed {"
                            "   background-color: #1E293B;"
                            "}"
                        );

                        QPushButton *btnVip = nullptr;
                        QPushButton *btnSupport = nullptr;

                        if(needVIP)
                        {
                            btnVip = msgBox.addButton(QString::fromUtf8("👑 Оформить VIP"), QMessageBox::ActionRole);
                            btnVip->setCursor(Qt::PointingHandCursor);
                            btnVip->setStyleSheet(
                                "QPushButton {"
                                "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #D97706, stop:1 #F59E0B);"
                                "   color: #FFFFFF;"
                                "   font-size: 12px;"
                                "   font-weight: bold;"
                                "   border: none;"
                                "   border-radius: 6px;"
                                "   padding: 6px 16px;"
                                "   min-width: 125px;"
                                "}"
                                "QPushButton:hover {"
                                "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #F59E0B, stop:1 #FBBF24);"
                                "}"
                                "QPushButton:pressed {"
                                "   background: #B45309;"
                                "}"
                            );
                        }
                        else
                        {
                            btnSupport = msgBox.addButton(QString::fromUtf8("💬 Поддержка"), QMessageBox::ActionRole);
                            btnSupport->setCursor(Qt::PointingHandCursor);
                            btnSupport->setStyleSheet(
                                "QPushButton {"
                                "   background-color: #0284C7;"
                                "   color: #FFFFFF;"
                                "   font-size: 12px;"
                                "   font-weight: bold;"
                                "   border: none;"
                                "   border-radius: 6px;"
                                "   padding: 6px 16px;"
                                "   min-width: 110px;"
                                "}"
                                "QPushButton:hover {"
                                "   background-color: #0EA5E9;"
                                "}"
                                "QPushButton:pressed {"
                                "   background-color: #0369A1;"
                                "}"
                            );
                        }

                        QPushButton *btnClose = msgBox.addButton(needVIP ? QString::fromUtf8("Закрыть") : QString::fromUtf8("Понятно"), QMessageBox::RejectRole);
                        btnClose->setCursor(Qt::PointingHandCursor);
                        msgBox.setDefaultButton(btnVip ? btnVip : (btnSupport ? btnSupport : btnClose));

                        msgBox.exec();

                        if(needVIP && msgBox.clickedButton() == btnVip)
                        {
                            for(const auto &s : std::as_const(services))
                            {
                                if(s && s->uuid() == IDServiceVIPBuyString)
                                {
                                    if(s->active)
                                    {
                                        this->runService(s);
                                    }
                                    else
                                    {
                                        QMessageBox::warning(this, QString::fromUtf8("Пополнение VIP"), QString::fromUtf8("Сервис пополнения VIP временно недоступен."));
                                    }
                                    break;
                                }
                            }
                        }
                        else if(btnSupport && msgBox.clickedButton() == btnSupport)
                        {
                            this->on_action_WhatsApp_triggered();
                        }
                        return;
                    }
                    this->runService(instance);
                }
            );

            instance->title = remoteService->name;
            instance->ownerWidget = button;
        }
        else
        {
            if(auto *aiService = qobject_cast<AIAgentService *>(instance.get()))
            {
                connect(
                    aiService,
                    &AIAgentService::onRunService,
                    this,
                    [this](const QString &service_uuid)
                    {
                        for(const auto &s : std::as_const(services))
                        {
                            if(s && s->uuid() == service_uuid)
                            {
                                if(s->active)
                                {
                                    runService(s);
                                }
                                break;
                            }
                        }
                    });
            }

            if(!instance->active)
            {
                ui->aiChatEdit->setDisabled(true);
                ui->aiChatSend->setDisabled(true);
                auto *chatView = findChild<AIChatView *>("aiChatView");
                if(chatView)
                    chatView->addAIMessage("К сожалению Сервис ИИ не доступен. Попробуйте позднее.");
            }
            else
            {
                instance->start(); // Auto start for AI
            }
        }

        services << std::move(instance);
    }
    std::sort(std::begin(services), std::end(services), [](const std::shared_ptr<Service> &lhs, const std::shared_ptr<Service> &rhs) { return static_cast<int>(lhs->active) > static_cast<int>(rhs->active); });

    QGridLayout *layoutSpace = qobject_cast<QGridLayout *>(ui->serviceContents->layout());
    if(layoutSpace)
    {
        layoutSpace->setSpacing(14);
        layoutSpace->setContentsMargins(10, 10, 10, 10);
    }

    x = 0;
    for(const std::shared_ptr<Service> &item : std::as_const(services))
    {
        // Adds widget to a grid
        if(item->uuid() != IDServiceAIAgentString)
        {
            static_cast<QGridLayout *>(ui->serviceContents->layout())->addWidget(item->ownerWidget, x / 2, x % 2);
            ++x;
        }
    }
    serverServices.reset();
}

void MainWindow::on_actionAboutUs_triggered()
{
    AboutDialog dlg(this);
    dlg.setCurrentTab(AboutDialog::TabAbout);
    dlg.exec();
}

void MainWindow::on_actionUsLic_triggered()
{
    AboutDialog dlg(this);
    dlg.setCurrentTab(AboutDialog::TabGplV3);
    dlg.exec();
}

void MainWindow::on_action_WhatsApp_triggered()
{
    QString dec = acceptLinkWaMe;
    dec = QByteArray::fromBase64(dec.toUtf8());
    QDesktopServices::openUrl(QUrl(dec));
}

void MainWindow::on_action_Qt_triggered()
{
    QMessageBox::aboutQt(this);
}

void MainWindow::checkVersion(bool firstRun)
{
#ifdef NDEBUG

    network.pullFetchVersion(firstRun);

    // Show First Page
    if(firstRun)
    {
        ui->loaderPageText->setText("Проверка обновления");
        showPageLoader(
            startPage,
            100,
            [this]() -> bool
            {
                if(actualVersion.empty())
                    return false;
                if(lastPage == AuthPage)
                {
                    if(actualVersion.mStatus != NetworkStatus::OK)
                    {
                        ui->loaderPageText->setText("Проблема с интернетом?");
                        ui->loaderPageText->update();
                        lastPage = PageIndex(-1);
                        QTimer::singleShot(2000, this, [this]() { willTerminate(); });
                        return false;
                    }
                    else
                    {
                        verChansesAvailable = ChansesRunInvalid;
                        ui->loaderPageText->setText("Ваша версия актуальная!");
                        ui->loaderPageText->update();
                        versionChecker->start();
                        return true;
                    }
                }
                return false;
            });
    }
    else if(verChansesAvailable > -1)
    {
        delayUICallLoop(
            70,
            [this]()
            {
                if(actualVersion.empty())
                    return true;
                if(verChansesAvailable > -1 && !isHidden())
                {
                    if(actualVersion.mStatus != NetworkStatus::OK)
                    {
                        if(verChansesAvailable == 0)
                        {
                            // Will terminate
                            verChansesAvailable = -1;
                            willTerminate();
                            versionChecker->stop();
                            return false;
                        }
                        else
                        {
                            QString warnMessage = "У вас осталось попыток (%1), срочно восстановите связь, иначе "
                                                  "приложение аварийно завершится.";
                            warnMessage = warnMessage.arg(verChansesAvailable);
                            verChansesAvailable = qMax<int>(verChansesAvailable - 1, 0);
                            QMessageBox::warning(this, "Отсутствие соединение с интернетом.", warnMessage);
                        }
                    }
                    else
                    {
                        verChansesAvailable = ChansesRunInvalid;
                    }
                }
                versionChecker->start();
                return false;
            });
    }

#else
    if(firstRun)
        showPageLoader(AuthPage);
#endif
}

void MainWindow::willTerminate()
{
    setEnabled(false);
    showMessageFromStatus(NetworkError);
    delayUICall(5000, std::bind(&MainWindow::close, this));
    QMessageBox::critical(this, "Нет соединение с интернетом", "Программа будет аварийно завершена через 5 секунд.", QMessageBox::Ok);
}

void MainWindow::showPage(PageIndex pageNum)
{
    if(curPage != LoaderPage)
        lastPage = curPage;
    if(pages.contains(curPage))
    {
        pages[curPage]->setEnabled(false);
        pages[curPage]->setVisible(false);
    }
    curPage = pageNum;
    if(pages.contains(curPage))
    {
        pages[curPage]->setVisible(true);
        pages[curPage]->setEnabled(true);
    }

    vPageSpacerAnimator->start();
    contentOpacityAnimator->start();

    ui->toplevel_backpage->setVisible(pageNum > CabinetPage);
    if(ui->toplevel_backpage->isVisible())
    {
        switch(pageNum)
        {
            case DevicesPage:
                ui->label_8->setText("Подключение Android-устройства (ADB)");
                break;
            case LongInfoPage:
                ui->label_8->setText(ServiceProvider::currentService() ? ServiceProvider::currentService()->title : "Выполнение процедуры");
                break;
            case MyDevicesPage:
                ui->label_8->setText("История устройств и гарантия");
                break;
            case BuyVIPPage:
                ui->label_8->setText("Оформление VIP-подписки");
                break;
            default:
                ui->label_8->setText("Назад в личный кабинет");
                break;
        }
    }
    pageShownPreStart(curPage);
}

void MainWindow::pageShownPreStart(int page)
{
    switch(page)
    {
            // WELCOME
        case AuthPage:
            ui->statusAuthText->setText("Выполните аутентификацию");
            ui->authButton->setEnabled(true);
            clearAuthInfoPage();
            if(lastPage == AuthPage && AppSetting::autoLogin() && !ui->linePassEdit->text().isEmpty() && ui->checkAutoLogin->isChecked())
                ui->authButton->click();
            break;
        case DevicesPage:
            if(nullptr == ServiceProvider::currentService())
            {
                QMessageBox::warning(this, "Service is not connected", "Service module is no load.");
                logoutSystem();
                return;
            }

            ServiceProvider::currentService()->stop();

            // Unset
            deviceSelectSwitched = false;
            deviceLeftAnimator->setDirection(QPropertyAnimation::Forward);

            if(s_adbVisualizer)
            {
                s_adbVisualizer->setStatus(UNKNOWN);
                s_adbVisualizer->startAnimation();
            }

            delayUI(500);

            delayUICallLoop(
                300,
                [this]() -> bool
                {
                    auto *vis = s_adbVisualizer;
                    if(!deviceSelectSwitched)
                    {
                        QList<AdbDevice> devices = Adb::getDevices();
                        if(devices.isEmpty())
                        {
                            if(vis && vis->status() != UNKNOWN)
                            {
                                vis->setStatus(UNKNOWN);
                                ui->label_5->setStyleSheet(
                                    "background-color: #0B1120;"
                                    "border: 1px solid #1E3A5F;"
                                    "border-radius: 10px;"
                                    "color: #38BDF8;"
                                    "font-size: 12.5px;"
                                    "font-weight: 600;"
                                    "padding: 10px;"
                                );
                                ui->label_5->setText(QString::fromUtf8("📡 Поиск подключенного Android-устройства..."));
                            }
                        }
                        else
                        {
                            bool hasAuth = false;
                            bool hasUnauth = false;
                            AdbDevice authDev;
                            AdbDevice unauthDev;

                            for(const AdbDevice &device : std::as_const(devices))
                            {
                                AdbConStatus status = Adb::deviceStatus(device.devId);
                                if(status == DEVICE)
                                {
                                    hasAuth = true;
                                    authDev = device;
                                    break;
                                }
                                else if(status == UNAUTH)
                                {
                                    hasUnauth = true;
                                    unauthDev = device;
                                }
                            }

                            if(hasAuth)
                            {
                                connectPhone.isAuthed = true;
                                connectPhone.adbDevice = authDev;
                                ServiceProvider::currentService()->setArgs(authDev);

                                QString devName = !authDev.marketingName.isEmpty() ? authDev.marketingName :
                                                  (!authDev.displayName.isEmpty() ? authDev.displayName :
                                                  (!authDev.model.isEmpty() ? authDev.model : authDev.devId));
                                QString devSub = !authDev.vendor.isEmpty() ? (authDev.vendor + " (" + authDev.model + ")") : authDev.devId;

                                if(vis && vis->status() != DEVICE)
                                {
                                    vis->setStatus(DEVICE, devName, devSub);
                                }
                                ui->label_5->setStyleSheet(
                                    "background-color: #064E3B;"
                                    "border: 1px solid #059669;"
                                    "border-radius: 10px;"
                                    "color: #34D399;"
                                    "font-size: 12.5px;"
                                    "font-weight: 600;"
                                    "padding: 10px;"
                                );
                                ui->label_5->setText(QString::fromUtf8("✅ Устройство подключено: %1! Запуск...").arg(devName));
                            }
                            else if(hasUnauth)
                            {
                                QString devName = !unauthDev.marketingName.isEmpty() ? unauthDev.marketingName :
                                                  (!unauthDev.displayName.isEmpty() ? unauthDev.displayName :
                                                  (!unauthDev.model.isEmpty() ? unauthDev.model : unauthDev.devId));

                                if(vis && vis->status() != UNAUTH)
                                {
                                    vis->setStatus(UNAUTH, devName);
                                }
                                ui->label_5->setStyleSheet(
                                    "background-color: #451A03;"
                                    "border: 1px solid #D97706;"
                                    "border-radius: 10px;"
                                    "color: #FBBF24;"
                                    "font-size: 12.5px;"
                                    "font-weight: 600;"
                                    "padding: 10px;"
                                );
                                ui->label_5->setText(QString::fromUtf8("⚠️ Нажмите «Разрешить отладку по USB» на экране телефона"));
                            }
                        }
                    }
                    if(ServiceProvider::currentService()->canStart() && !deviceSelectSwitched)
                    {
                        deviceSelectSwitched = true;
                        deviceLeftAnimator->start();
                        delayUI(1800);
                        showPageLoader(ServiceProvider::currentService()->targetPage());
                    }
                    if(curPage != DevicesPage)
                    {
                        deviceLeftAnimator->stop();
                        ui->device_left_group->setMaximumWidth(QWIDGETSIZE_MAX);
                        if(vis)
                            vis->stopAnimation();
                    }
                    return curPage == DevicesPage;
                });

            break;
        case CabinetPage:
        {
            ui->scrollArea_3->verticalScrollBar()->setValue(0);
            fillAuthInfoPage();
            break;
        }
        case LongInfoPage:
        {
            QStringList place {};
            QStringListModel *model = static_cast<QStringListModel *>(ui->processLogStatus->model());
            ui->processBarStatus->setValue(0);
            ui->malwareStatusText0->setText("Ожидание запуска сервиса.");
            malwareProgressCircle->setValue(0);
            malwareProgressCircle->setMaximum(100);
            malwareProgressCircle->setInfinilyMode(false);

            place << "<< Во время процесса не отсоединяйте устройство от компьютера >>";

            // TODO: set auto start mode flag.
            // IF THERE AUTO_START = YES?

            if(!ServiceProvider::currentService()->canStart())
            {
                place << "Внутреняя ошибка, сервис не может быть запущен. Нажмите назад "
                         "и повторите попытку.";
            }
            else
            {
                place << QString("<< Ожидаем >>").arg(ServiceProvider::currentService()->title);

                delayUICall(500, [this]() { ServiceProvider::currentService()->start(); });
            }

            model->setStringList(place);
            break;
        }
        default:
            break;
    }
}

void MainWindow::runService(std::shared_ptr<Service> service)
{
    if(!ServiceProvider::runService(service))
    {
        QMessageBox::warning(this, "Service is shutdown", "Service module is no load or disabled by server.");
        logoutSystem();
    }
}

void MainWindow::closeService(std::shared_ptr<Service> service)
{
    if(service != nullptr)
        ServiceProvider::closeService();
    updateCabinet();
}

void MainWindow::clearAuthInfoPage()
{
    int x, y;
    delete ui->authInfo->model(); // fix: delete old model before replacing to avoid accumulation
    QStandardItemModel *model = new QStandardItemModel(ui->authInfo);
    model->setRowCount(7);
    model->setColumnCount(2);

    model->setHorizontalHeaderItem(0, new QStandardItem("Параметр"));
    model->setHorizontalHeaderItem(1, new QStandardItem("Значение"));

    model->setItem(0, 0, new QStandardItem("Логин"));
    model->setItem(0, 1, new QStandardItem("-"));

    model->setItem(1, 0, new QStandardItem("Последний вход"));
    model->setItem(1, 1, new QStandardItem("-"));

    model->setItem(2, 0, new QStandardItem("Баланс"));
    model->setItem(2, 1, new QStandardItem("-"));

    model->setItem(3, 0, new QStandardItem("VIP дней"));
    model->setItem(3, 1, new QStandardItem("-"));

    model->setItem(4, 0, new QStandardItem("Подключений"));
    model->setItem(4, 1, new QStandardItem("-"));

    model->setItem(5, 0, new QStandardItem("Расположение"));
    model->setItem(5, 1, new QStandardItem("-"));

    model->setItem(6, 0, new QStandardItem("Заблокирован"));
    model->setItem(6, 1, new QStandardItem("-"));

    ui->authInfo->setModel(model);
    ui->authInfo->horizontalHeader()->setStretchLastSection(true);
    ui->authInfo->verticalHeader()->setVisible(false);
    ui->authInfo->resizeColumnToContents(0);

    for(x = 0, y = ui->serviceContents->layout()->count(); x < y; ++x)
        ui->serviceContents->layout()->takeAt(0)->widget()->deleteLater();

    for(x = 0; x < services.count(); ++x)
        if(services[x]->ownerWidget != nullptr)
            services[x]->ownerWidget->deleteLater();

    serverServices.reset();
    services.clear();

    ui->aiChatEdit->clear();
    ui->aiChatSend->setText(QString::fromUtf8("➤"));

    ui->labelLoginAuthed->setText("-");
    ui->labelCredits->setText(QString::fromUtf8("💳 0\nБаланс"));
    ui->labelVipDays->setText(QString::fromUtf8("👑 0 ДНЕЙ\nVIP статус"));

    // Reset Side Reference Panel labels
    QLabel *lbl = nullptr;
    if((lbl = findChild<QLabel *>("cabinetVal_login"))) lbl->setText("-");
    if((lbl = findChild<QLabel *>("cabinetVal_loginTime"))) lbl->setText("-");
    if((lbl = findChild<QLabel *>("cabinetVal_credits"))) lbl->setText("-");
    if((lbl = findChild<QLabel *>("cabinetVal_vip"))) {
        lbl->setText("-");
        lbl->setStyleSheet("color: #F1F5F9; font-size: 12px; font-weight: bold; background: transparent; border: none;");
    }
    if((lbl = findChild<QLabel *>("cabinetVal_devices"))) lbl->setText("-");
    if((lbl = findChild<QLabel *>("cabinetVal_location"))) lbl->setText("-");
    if((lbl = findChild<QLabel *>("cabinetVal_status"))) {
        lbl->setText("-");
        lbl->setStyleSheet("color: #F1F5F9; font-size: 12px; font-weight: bold; background: transparent; border: none;");
    }

    AIAgentService::resetHistory();
    ui->aiChatEdit->clear();
    ui->aiChatSend->setText(QString::fromUtf8("➤"));

    ui->aiChatEdit->setDisabled(true);
    ui->aiChatSend->setDisabled(true);

    auto *chatView = findChild<AIChatView *>("aiChatView");
    if(chatView)
        chatView->showLocked();
}

void MainWindow::fillAuthInfoPage()
{
    QString value;
    int x, y;
    QStandardItemModel *model = qobject_cast<QStandardItemModel *>(ui->authInfo->model());
    if(model)
    {
        value = network.authedId.idName;
        model->item(0, 1)->setText(value);

        value = QDateTime::currentDateTime().toString(Qt::TextDate);
        model->item(1, 1)->setText(value);

        value = QString::number(network.authedId.credits) + " кредитов";
        model->item(2, 1)->setText(value);

        value = QString::number(network.authedId.vipDays);
        model->item(3, 1)->setText(value);

        value = QString::number(network.authedId.connectedDevices);
        model->item(4, 1)->setText(value);

        value = network.authedId.location;
        model->item(5, 1)->setText(value);

        value = network.authedId.blocked ? "Да" : "Нет";
        model->item(6, 1)->setText(value);
    }

    ui->labelLoginAuthed->setText(network.authedId.idName);
    ui->labelCredits->setText(QString("💳 %1 %2\nБаланс").arg(network.authedId.credits).arg(network.authedId.currencyType));
    if(network.authedId.hasVipAccount())
        ui->labelVipDays->setText(QString("👑 %1 ДНЕЙ\nVIP активен").arg(network.authedId.vipDays));
    else
        ui->labelVipDays->setText(QString("👑 Нет VIP\nVIP статус"));

    // Populate Side Reference Panel (Справочник аккаунта)
    QLabel *sLbl = nullptr;
    if((sLbl = findChild<QLabel *>("cabinetVal_login")))
        sLbl->setText(network.authedId.idName.isEmpty() ? "-" : network.authedId.idName);

    if((sLbl = findChild<QLabel *>("cabinetVal_loginTime")))
        sLbl->setText(QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm"));

    if((sLbl = findChild<QLabel *>("cabinetVal_credits")))
        sLbl->setText(QString("%1 %2").arg(network.authedId.credits).arg(network.authedId.currencyType));

    if((sLbl = findChild<QLabel *>("cabinetVal_vip")))
    {
        if(network.authedId.hasVipAccount())
        {
            sLbl->setText(QString("👑 %1 дн.").arg(network.authedId.vipDays));
            sLbl->setStyleSheet("color: #FBBF24; font-size: 12px; font-weight: bold; background: transparent; border: none;");
        }
        else
        {
            sLbl->setText(QString::fromUtf8("Не активен"));
            sLbl->setStyleSheet("color: #94A3B8; font-size: 12px; font-weight: 500; background: transparent; border: none;");
        }
    }

    if((sLbl = findChild<QLabel *>("cabinetVal_devices")))
        sLbl->setText(QString("%1 шт.").arg(network.authedId.connectedDevices));

    if((sLbl = findChild<QLabel *>("cabinetVal_location")))
        sLbl->setText(network.authedId.location.isEmpty() ? QString::fromUtf8("Не определено") : network.authedId.location);

    if((sLbl = findChild<QLabel *>("cabinetVal_status")))
    {
        if(network.authedId.blocked)
        {
            sLbl->setText(QString::fromUtf8("⛔ Заблокирован"));
            sLbl->setStyleSheet("color: #F87171; font-size: 12px; font-weight: bold; background: transparent; border: none;");
        }
        else
        {
            sLbl->setText(QString::fromUtf8("✅ Активен"));
            sLbl->setStyleSheet("color: #34D399; font-size: 12px; font-weight: bold; background: transparent; border: none;");
        }
    }

    initServiceModules();
}

void MainWindow::delayUI(int ms)
{
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    timer.start(ms);
    loop.exec();
}

void MainWindow::delayUICallLoop(int ms, std::function<bool()> callFalseEnd)
{
    QTimer *qtimer = new QTimer(this);
    qtimer->setSingleShot(false);
    qtimer->setInterval(ms);
    auto isRunning = std::make_shared<bool>(false);
    QObject::connect(
        qtimer,
        &QTimer::timeout,
        [qtimer, callFalseEnd, isRunning]()
        {
            if(*isRunning)
                return;
            *isRunning = true;
            if(!callFalseEnd())
            {
                qtimer->stop();
                qtimer->deleteLater();
            }
            else
            {
                *isRunning = false;
            }
        });
    qtimer->start();
}

void MainWindow::delayUICall(int ms, std::function<void()> call)
{
    delayUICallLoop(
        ms,
        [call]() -> bool
        {
            call();
            return false;
        });
}

bool MainWindow::accessUi_page_longinfo(QListView *&processLogStatusV, QLabel *&malareStatusText0V, QLabel *&deviceLabelNameV, QProgressBar *&processBarStatusV, QPushButton *&pushButtonReRun)
{
    processLogStatusV = ui->processLogStatus;
    malareStatusText0V = ui->malwareStatusText0;
    deviceLabelNameV = ui->deviceLabelName;
    processBarStatusV = ui->processBarStatus;
    pushButtonReRun = ui->malwareReRun;
    return true;
}

bool MainWindow::accessUi_page_devices(QTableView *&tableActual, QDateEdit *&dateEditStart, QDateEdit *&dateEditEnd, QPushButton *&refreshButton, QCheckBox *&quaranteeFilter)
{
    tableActual = ui->myDeviceActual;
    dateEditStart = ui->myDeviceFilterDateStart;
    dateEditEnd = ui->myDeviceFilterDateEnd;
    refreshButton = ui->myDeviceSend;
    quaranteeFilter = ui->myDeviceQuaranteeFilter;
    return true;
}

bool MainWindow::accessUi_page_buyvip(QComboBox *&listVariants, QLabel *&balanceText, QLabel *&infoAfterPeriod, QPushButton *&buyButton)
{
    ui->comboBoxSelectVIPDays->disconnect();
    ui->labelVipBalance->disconnect();
    ui->buttonBuyVip->disconnect();
    ui->labelInfoVip->disconnect();

    listVariants = ui->comboBoxSelectVIPDays;
    balanceText = ui->labelVipBalance;
    buyButton = ui->buttonBuyVip;
    infoAfterPeriod = ui->labelInfoVip;

    infoAfterPeriod->clear();
    listVariants->clear();
    balanceText->clear();

    return true;
}

void MainWindow::on_authButton_clicked()
{
    network.forclyExit = false;
    if(network.pending() || network.isAuthed())
        return;

    if(ui->lineLoginEdit->text().isEmpty() && ui->linePassEdit->text().isEmpty())
    {
        QMessageBox::warning(this, "Предупреждение", "Поле авторизаций не заполнено.");
        return;
    }

    network.pushLoginPass(ui->lineLoginEdit->text(), ui->linePassEdit->text());
    ui->statusAuthText->setStyleSheet("color: #38BDF8; font-size: 12px; font-weight: 500; background: transparent; padding: 2px 8px;");
    ui->statusAuthText->setText("Авторизация");

    if(ui->authButton)
        ui->authButton->setEnabled(false);
    ui->lineLoginEdit->setEnabled(false);
    ui->linePassEdit->setEnabled(false);

    if(timerAuthAnim != nullptr)
    {
        delete timerAuthAnim;
        timerAuthAnim = nullptr;
    }

    constexpr int Dots = 3;
    timerAuthAnim = new QTimer(this);
    timerAuthAnim->start(350);
    QObject::connect(
        timerAuthAnim,
        &QTimer::timeout,
        this,
        [this]()
        {
            QString temp = ui->statusAuthText->text();
            int dotCount = std::accumulate(temp.begin(), temp.end(), 0, [](int count, const QChar &c) { return count += (c == '.' ? 1 : 0); });
            if(dotCount >= Dots)
                temp.remove(temp.length() - Dots, Dots);
            else
                temp += '.';
            ui->statusAuthText->setText(temp);
        });

    AppSetting::autoLogin(nullptr, ui->checkAutoLogin->isChecked());

    delayUICallLoop(
        100,
        [this]()
        {
            if(!network.pending() && network.isAuthed())
            {
                delayUICall(
                    2000,
                    [this]()
                    {
                        showPage(CabinetPage);
                        updateCabinet();
                    });
            }
            return network.pending();
        });
}

void MainWindow::setThemeAction()
{
    // QList<QAction *> virtualSelectItems {ui->mThemeSystem, ui->mThemeLight, ui->mThemeDark};
    // QAction *selfSender = qobject_cast<QAction *>(sender());
    // int scheme;
    // for(scheme = (0); scheme < virtualSelectItems.size() && selfSender != virtualSelectItems[scheme]; ++scheme)
    //     ;
    // setTheme(static_cast<ThemeScheme>(scheme));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    tray->deleteLater();
    event->accept();
}

void MainWindow::slotAuthFinish(int status, bool ok)
{
    delayUICall(
        1000,
        [ok, status, this]() -> void
        {
            QString resText;
            int _status = status;

            if(!network.isAuthed())
            {
                _status = NetworkStatus::NetworkError;
            }

            if(timerAuthAnim)
                timerAuthAnim->stop();
            switch(_status)
            {
                case 0:
                    resText = "Токен успешно прошел проверку. Добро пожаловать!";

                    if(!ui->lineLoginEdit->text().isEmpty() && !ui->linePassEdit->text().isEmpty())
                    {
                        AppSetting::loginAndPass(nullptr, ui->lineLoginEdit->text(), ui->linePassEdit->text());

                        // Only get token.
                        break;
                    }

                    if(network.authedId.isNotValidBalance())
                    {
                        ui->statusAuthText->setStyleSheet("color: #F87171; font-size: 12px; font-weight: 500; background: transparent; padding: 2px 8px;");
                        ui->statusAuthText->setText("Закончился баланс, пополните, чтобы продолжить.");
                        showMessageFromStatus(NetworkStatus::NoEnoughMoney);
                    }
                    else if(network.authedId.blocked)
                    {
                        ui->statusAuthText->setStyleSheet("color: #F87171; font-size: 12px; font-weight: 500; background: transparent; padding: 2px 8px;");
                        ui->statusAuthText->setText("Аккаунт заблокирован");
                        showMessageFromStatus(NetworkStatus::AccountBlocked);
                    }
                    else
                    {
                        ui->statusAuthText->setStyleSheet("color: #34D399; font-size: 12px; font-weight: 500; background: transparent; padding: 2px 8px;");
                        ui->statusAuthText->setText("Аутентификация прошла успешно.");
                    }

                    break;
                case 401:
                    resText = infoServer401;
                    break;
                case NetworkStatus::NoEnoughMoney:
                    resText = infoNoBalance;
                    break;
                default:
                    resText = infoNoInternet;
                    break;
            }

            ui->lineLoginEdit->setEnabled(true);
            ui->linePassEdit->setEnabled(true);
            ui->authButton->setEnabled(true);
            if(_status != 0)
            {
                ui->statusAuthText->setStyleSheet("color: #F87171; font-size: 12px; font-weight: 500; background: transparent; padding: 2px 8px;");
                ui->statusAuthText->setText(resText);
            }
        });
}

void MainWindow::slotPullServiceList(const QList<ServiceItemInfo> &services, bool ok)
{
    serverServices.reset();

    if(ok)
    {
        serverServices = std::move(std::make_shared<QList<ServiceItemInfo>>(services));
    }
    else
    {
        logoutSystem();
    }
}

void MainWindow::slotFetchVersionFinish(int status, const QString &version, const QString &url, bool ok)
{
    VersionInfo actualVersion = {{}, {}, status};
    if(status == NetworkStatus::NetworkError)
    {
        this->actualVersion = actualVersion;
        return;
    }

    actualVersion = {version, url, status};
    this->actualVersion = actualVersion;
    if(runtimeVersion.mVersion >= actualVersion.mVersion)
    {
        return;
    }

    QString text;
    text = "Обнаружена новая версия программного обеспечения. После нажатия "
           "кнопки \"ОК\" ";
#ifdef WIN32
    text += "будет запущена обновление ПО.";
#else
    text += "откроется ссылка в вашем браузере.\n"
            "Пожалуйста, скачайте обновление по прямой ссылке.\n";
#endif
    text += "\nС уважением ваша команда Adskiller Team.";
    text += "\n\nВаша версия: v";
    text += runtimeVersion.mVersion.toString();
    text += "\nВерсия на сервере: v";
    text += actualVersion.mVersion.toString();
    // TURNED OFF INFO ABOUT UPDATE
    // QMessageBox::information(this, "Обнаружена новая версия", text);
    this->close();

#ifdef WIN32
    QTemporaryDir tempdir;
    tempdir.setAutoRemove(false);
    QDir appDir(QCoreApplication::applicationDirPath());
    QStringList entries = appDir.entryList(QStringList() << "*.dll" << UpdateManagerExecute, QDir::Files);
    for(const QString &e : entries)
    {
        QFile::copy(appDir.filePath(e), tempdir.filePath(e));
    }
    appDir.mkdir(tempdir.filePath("platforms"));
    QFile::copy(appDir.filePath("platforms/qwindows.dll"), tempdir.filePath("platforms/qwindows.dll"));
    appDir.mkdir(tempdir.filePath("networkinformation"));
    QFile::copy(appDir.filePath("networkinformation/qnetworklistmanager.dll"), tempdir.filePath("networkinformation/qnetworklistmanager.dll"));
    appDir.mkdir(tempdir.filePath("tls"));
    QFile::copy(appDir.filePath("tls/qcertonlybackend.dll"), tempdir.filePath("tls/qcertonlybackend.dll"));
    QFile::copy(appDir.filePath("tls/qschannelbackend.dll"), tempdir.filePath("tls/qschannelbackend.dll"));
    if(QProcess::startDetached(tempdir.filePath(UpdateManagerExecute), QStringList() << QString("--dir") << appDir.path() << QString("--exec") << QCoreApplication::applicationFilePath()))
    {
        QApplication::quit();
        return;
    }
#endif
    QDesktopServices::openUrl(QUrl(url));
}

void MainWindow::showEvent(QShowEvent *event)
{
    if(snows)
        delayUICall(50, [this]() { snows->start(); });
    event->accept();
}

void MainWindow::setTheme(ThemeScheme theme)
{
    int scheme;
    const char *resourceName;
    QList<QAction *> menus {ui->mThemeSystem, ui->mThemeLight, ui->mThemeDark};
    for(scheme = (0); scheme < menus.size(); ++scheme)
    {
        menus[scheme]->setChecked(theme == scheme);
    }

    switch(theme)
    {
        case System:
            resourceName = nullptr;
            break;
        case Dark:
            resourceName = ":/resources/app-style-dark";
            break;
        case Light:
        default:
            resourceName = ":/resources/app-style-light";
            break;
    }

    QFile styleRes {};
    QString styleSheet {};
    if(resourceName)
    {
        styleRes.setFileName(resourceName);
        if(!styleRes.open(QFile::ReadOnly | QFile::Text))
        {
            QMessageBox::warning(this, "FAIL", "Set theme failed. Default to SYSTEM theme");
        }
        else
        {
            styleSheet = styleRes.readAll();
        }
        styleRes.close();
    }

    // Set application Design
    app->setStyleSheet(styleSheet);
    AppSetting::themeIndex(nullptr, static_cast<int>(theme));
}

ThemeScheme MainWindow::getTheme()
{
    ThemeScheme scheme;
    scheme = static_cast<ThemeScheme>(AppSetting::themeIndex());
    return scheme;
}

void MainWindow::showMessageFromStatus(int statusCode)
{
    if(statusCode == NetworkStatus::NetworkError)
        QMessageBox::warning(this, "Ошибка подключения", infoNoNetworkUpdate);

    if(statusCode == NetworkStatus::NoEnoughMoney)
        QMessageBox::warning(this, "Сервер отклонил запрос", infoNoBalance);

    if(statusCode == NetworkStatus::AccountBlocked)
        QMessageBox::warning(this, "Сервер отклонил запрос", infoAccountBlocked);
}

void MainWindow::updateCabinet()
{
    if(!network.isAuthed())
    {
        logoutSystem();
        return;
    }

    network.pushAuthToken();

    for(int i = 0; i < services.count(); ++i)
        if(services[i]->ownerWidget != nullptr)
            services[i]->ownerWidget->deleteLater();

    if(ui->serviceContents->layout())
    {
        while(ui->serviceContents->layout()->count() > 0)
        {
            QLayoutItem *item = ui->serviceContents->layout()->takeAt(0);
            if(item->widget())
                item->widget()->deleteLater();
            delete item;
        }
    }

    services.clear();
    serverServices.reset();

    showPageLoader(
        CabinetPage,
        1000,
        [this]() -> bool
        {
            const char *str = "Обновление странницы";
            bool status = network.isAuthed() && !network.pending();

            if(network.forclyExit)
            {
                logoutSystem();
                return true;
            }

            if(network.isAuthed() && !serverServices)
            {
                str = "Еще чуть-чуть";
            }

            if(status && !serverServices)
            {
                network.pullServiceList();
                status = !network.pending();
            }

            ui->loaderPageText->setText(str);

            if(status && !serverServices)
            {
                delayUICall(1, [this]() { logoutSystem(); });
            }
            else if(status && network.authedId.vipDays < 5 && network.authedId.vipDays > 0)
            {
                delayUICall(300, [this]() { QMessageBox::warning(this, "Уведомление", infoVipExpire); });
            }
            return status;
        });
}

void MainWindow::logoutSystem()
{
    network.forclyExit = true;
    if(network.isAuthed())
    {
        network._token = {};
        network.authedId = {};
        clearAuthInfoPage();
        showPageLoader(AuthPage, 500, QString("Выход из системы"));
    }
    else
    {
        showPageLoader(AuthPage, 0, QString("Выход из системы"));
    }
}

void MainWindow::showPageLoader(PageIndex pageNum, int msWait, std::function<bool()> predFalseEnd, QString text)
{
    if(pageNum == LoaderPage)
        return;

    if(text.isEmpty())
        text = "Ожидайте";

    ui->loaderPageText->setText(text);

    showPage(LoaderPage);
    delayUICallLoop(
        msWait,
        [this, pageNum, predFalseEnd]()
        {
            if(predFalseEnd())
            {
                QTimer::singleShot(1500, this, [this, pageNum]() { showPage(pageNum); });
                return false;
            }
            return true;
        });
}

void MainWindow::on_butShowPass_clicked()
{
    if(ui->linePassEdit->echoMode() == QLineEdit::EchoMode::Password)
    {
        ui->linePassEdit->setEchoMode(QLineEdit::EchoMode::Normal);
        ui->butShowPass->setText("🔒");
        ui->butShowPass->setToolTip("Скрыть пароль");
    }
    else
    {
        ui->linePassEdit->setEchoMode(QLineEdit::EchoMode::Password);
        ui->butShowPass->setText("👁");
        ui->butShowPass->setToolTip("Показать пароль");
    }
}
