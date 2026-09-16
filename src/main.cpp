#include <QApplication>
#include <QBuffer>
#include <QColor>
#include <QCursor>
#include <QDataStream>
#include <QDir>
#include <QEasingCurve>
#include <QFile>
#include <QFont>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPalette>
#include <QParallelAnimationGroup>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QScreen>
#include <QSharedMemory>
#include <QStyleFactory>
#include <QTimer>
#include <QWidget>
#include <cmath>

#include "begin.h"
#include "mainwindow.h"
#include "network.h"

constexpr auto ShowCommandPipe = "adskiller_window_show";
constexpr auto HideCommandPipe = "adskiller_window_hide";

bool checkout();
QWidget *createBanner();
void dismissBanner(QWidget *banner);

class CyberStreamOverlay : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal phase READ phase WRITE setPhase)

public:
    explicit CyberStreamOverlay(QWidget *parent = nullptr) : QWidget(parent), m_phase(0.0)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setStyleSheet("background: transparent; border: none;");

        auto *anim = new QPropertyAnimation(this, "phase", this);
        anim->setDuration(2200);
        anim->setStartValue(0.0);
        anim->setEndValue(1.0);
        anim->setLoopCount(-1);
        anim->start();
    }

    qreal phase() const
    {
        return m_phase;
    }
    void setPhase(qreal p)
    {
        m_phase = p;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);

        const qreal boundW = qMin<qreal>(300.0, width());
        const qreal totalH = height();

        QFont font("Consolas", 8, QFont::Bold);
        font.setStyleHint(QFont::Monospace);
        painter.setFont(font);
        QFontMetrics fm(font);

        const qreal charH = fm.height() * 0.95;
        const qreal charW = fm.horizontalAdvance(QLatin1Char('0'));
        const QString streamData = QStringLiteral("1010110010110100101001101010100101101010110100101010");

        const qreal colX[] = {18.0, 58.0, 110.0, 175.0, 235.0, 285.0};
        const qreal colSpeeds[] = {1.35, 0.95, 1.60, 1.15, 1.45, 0.85};
        const int colsCount = sizeof(colX) / sizeof(colX[0]);

        for(int c = 0; c < colsCount; ++c)
        {
            if(colX[c] > boundW)
                continue;

            qreal shift = std::fmod(m_phase * colSpeeds[c] * totalH, totalH);

            QLinearGradient lineGrad(colX[c], totalH, colX[c], 0);
            lineGrad.setColorAt(0.0, QColor(0, 245, 255, 0));
            lineGrad.setColorAt(0.5, QColor(0, 245, 255, 45));
            lineGrad.setColorAt(1.0, QColor(0, 245, 255, 10));

            painter.setPen(QPen(QBrush(lineGrad), 1.0, Qt::DashLine));
            painter.drawLine(QPointF(colX[c], totalH), QPointF(colX[c], 0));

            qreal pulseY = totalH - shift;
            QLinearGradient beamGrad(colX[c], pulseY + 45.0, colX[c], pulseY - 45.0);
            beamGrad.setColorAt(0.0, QColor(0, 245, 255, 0));
            beamGrad.setColorAt(0.5, QColor(255, 255, 255, 180));
            beamGrad.setColorAt(1.0, QColor(0, 245, 255, 0));

            painter.setPen(QPen(QBrush(beamGrad), 1.6));
            painter.drawLine(QPointF(colX[c], pulseY + 45.0), QPointF(colX[c], pulseY - 45.0));

            for(qreal y = totalH + charH; y >= -charH; y -= charH)
            {
                qreal curY = y - shift;
                while(curY < -charH)
                    curY += (totalH + charH * 2.0);

                qreal normY = 1.0 - (curY / totalH);
                qreal distPulse = std::abs(curY - pulseY);
                qreal pulseGlow = qMax<qreal>(0.0, 1.0 - (distPulse / 70.0));

                int charIdx = std::abs(static_cast<int>((curY + c * 37) / charH)) % streamData.length();
                QChar ch = streamData.at(charIdx);

                QColor textColor;
                if((c + charIdx) % 9 == 0)
                    textColor = QColor(255, 75, 75);
                else if((c + charIdx) % 5 == 0)
                    textColor = QColor(52, 211, 153);
                else
                    textColor = QColor(0, 245, 255);

                qreal lateralFade = 1.0 - (colX[c] / boundW);
                int alpha = qBound(0, static_cast<int>((normY * 90.0 + pulseGlow * 165.0) * lateralFade), 255);
                textColor.setAlpha(alpha);

                painter.setPen(textColor);
                painter.drawText(QPointF(colX[c] - charW / 2.0, curY), QString(ch));
            }
        }
    }

private:
    qreal m_phase;
};

int main(int argc, char **argv)
{
    int exitCode;
    QApplication app(argc, argv);

    QSharedMemory sharedMemUpdate("imister.kz-app_adskiller_v1_update");
    if(sharedMemUpdate.attach() || !checkout())
    {
        return EXIT_FAILURE;
    }
    QSharedMemory sharedMem("imister.kz-app_adskiller_v1");
    if(sharedMem.attach())
    {
        sharedMem.lock();
        int len = qMin<int>(strlen(ShowCommandPipe), sharedMem.size());
        memcpy(sharedMem.data(), ShowCommandPipe, len);
        sharedMem.unlock();
        sharedMem.detach();
        return EXIT_SUCCESS;
    }
    if(!sharedMem.create(128))
    {
        return EXIT_FAILURE;
    }
    sharedMem.lock();
    memset(sharedMem.data(), 0, sharedMem.size());
    sharedMem.unlock();

    QWidget *banner = createBanner();

    app.setStyle(QStyleFactory::create("Fusion"));

    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(10, 14, 26));
    darkPalette.setColor(QPalette::WindowText, QColor(248, 250, 252));
    darkPalette.setColor(QPalette::Base, QColor(7, 10, 18));
    darkPalette.setColor(QPalette::AlternateBase, QColor(15, 23, 42));
    darkPalette.setColor(QPalette::ToolTipBase, QColor(15, 23, 42));
    darkPalette.setColor(QPalette::ToolTipText, QColor(248, 250, 252));
    darkPalette.setColor(QPalette::Text, QColor(248, 250, 252));
    darkPalette.setColor(QPalette::Button, QColor(15, 23, 42));
    darkPalette.setColor(QPalette::ButtonText, QColor(248, 250, 252));
    darkPalette.setColor(QPalette::BrightText, QColor(56, 189, 248));
    darkPalette.setColor(QPalette::Link, QColor(56, 189, 248));
    darkPalette.setColor(QPalette::Highlight, QColor(2, 132, 199));
    darkPalette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    darkPalette.setColor(QPalette::PlaceholderText, QColor(100, 116, 139));

    darkPalette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(71, 85, 105));
    darkPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(71, 85, 105));
    darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(71, 85, 105));
    darkPalette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(30, 41, 59));
    darkPalette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(71, 85, 105));

    app.setPalette(darkPalette);

    MainWindow *w = new MainWindow;
    w->current = w;
    w->app = &app;
    w->delayUICallLoop(
        100,
        [&sharedMem, &w]() -> bool
        {
            if(!sharedMem.lock())
                return true;

            const char *data = reinterpret_cast<const char *>(sharedMem.constData());
            int len = 0;
            while(len < sharedMem.size() && data[len] != '\0')
            {
                len++;
            }
            QString cmd = QString::fromLatin1(data, len);

            if(cmd == ShowCommandPipe)
            {
                w->showNormal();
            }
            if(cmd == HideCommandPipe)
            {
                w->hide();
            }
            memset(sharedMem.data(), 0, 1);
            sharedMem.unlock();
            return true;
        });

    QTimer::singleShot(
        3000,
        [&]()
        {
            if(w)
            {
                QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
                if(!screen)
                    screen = QGuiApplication::primaryScreen();
                if(screen)
                {
                    QRect screenGeo = screen->availableGeometry();
                    int x = qMax(screenGeo.x(), screenGeo.x() + (screenGeo.width() - w->width()) / 2);
                    int y = qMax(screenGeo.y(), screenGeo.y() + (screenGeo.height() - w->height()) / 2);
                    w->move(x, y);
                }
                w->show();
            }
            if(banner)
            {
                dismissBanner(banner);
                banner = nullptr;
            }
        });

    exitCode = app.exec();
    if(w != nullptr)
        delete w;
    sharedMem.detach();
    return exitCode;
}

bool checkout()
{
    QDir qdir;
    QString adbfile = AdbExecutableFilename();
    if(!qdir.exists(adbfile))
    {
        qDebug() << "Adb not found";
        return false;
    }
    return true;
}

QWidget *createBanner()
{
    QWidget *banner = new QWidget;
    banner->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    banner->setAttribute(Qt::WA_TranslucentBackground);
    banner->setCursor(Qt::WaitCursor);
    banner->setStyleSheet("background: transparent;");

    QPixmap pixmap(":/resources/banner");
    QLabel *lab = new QLabel(banner);
    lab->setStyleSheet("background: transparent; border: none;");

    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if(!screen)
    {
        screen = QGuiApplication::primaryScreen();
    }

    QRect screenGeo = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);

    if(!pixmap.isNull())
    {
        int maxH = static_cast<int>(screenGeo.height() * 0.80);
        if(pixmap.height() > maxH && maxH > 200)
        {
            pixmap = pixmap.scaledToHeight(maxH, Qt::SmoothTransformation);
        }
        lab->setPixmap(pixmap);
        banner->setFixedSize(pixmap.size());
    }
    else
    {
        banner->setFixedSize(500, 478);
    }

    lab->setGeometry(0, 0, banner->width(), banner->height());

    auto *streamOverlay = new CyberStreamOverlay(banner);
    streamOverlay->setGeometry(0, 0, banner->width(), banner->height());
    streamOverlay->raise();

    int targetX = qMax(screenGeo.x(), screenGeo.x() + (screenGeo.width() - banner->width()) / 2);
    int targetY = qMax(screenGeo.y(), screenGeo.y() + (screenGeo.height() - banner->height()) / 2);
    banner->move(targetX, targetY);

    banner->show();

    return banner;
}

void dismissBanner(QWidget *banner)
{
    if(!banner)
        return;

    banner->close();
    banner->deleteLater();
}

#include "main.moc"