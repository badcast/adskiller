#include "Services.h"
#include "PixelBlastGame.h"
#include "MainWindow.h"

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QIcon>
#include <QMessageBox>
#include <QKeyEvent>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QLinearGradient>
#include <QScrollArea>
#include <QScrollBar>

#include "Extension.h"

// ============================================================================
// PBGInternal (In-Window Embedded Overlay for Pixel Blast Game)
// Obsidian / Metro UI Cyber Design Language
// ============================================================================

namespace
{

    class PixelBlastFrame : public QFrame
    {
    public:
        explicit PixelBlastFrame(QWidget *parent = nullptr) : QFrame(parent)
        {
            setAttribute(Qt::WA_StyledBackground, true);
        }

    protected:
        void paintEvent(QPaintEvent *event) override
        {
            QFrame::paintEvent(event);

            QPainter p(this);
            p.setRenderHint(QPainter::Antialiasing, false);

            const int w = width();
            const int h = height();
            const int bLen = 14;

            // AdsKiller signature cyber sci-fi corner bracket accents
            p.setPen(QPen(QColor(56, 189, 248, 220), 1.5));

            // Top-left
            p.drawLine(0, 0, bLen, 0);
            p.drawLine(0, 0, 0, bLen);

            // Top-right
            p.drawLine(w - bLen, 0, w - 1, 0);
            p.drawLine(w - 1, 0, w - 1, bLen);

            // Bottom-left
            p.drawLine(0, h - 1, bLen, h - 1);
            p.drawLine(0, h - bLen, 0, h - 1);

            // Bottom-right
            p.drawLine(w - bLen, h - 1, w - 1, h - 1);
            p.drawLine(w - 1, h - bLen, w - 1, h - 1);

            // Top cyan neon glow border line
            QLinearGradient topGlow(0, 0, w, 0);
            topGlow.setColorAt(0.0, QColor(56, 189, 248, 0));
            topGlow.setColorAt(0.5, QColor(56, 189, 248, 180));
            topGlow.setColorAt(1.0, QColor(56, 189, 248, 0));
            p.setPen(QPen(QBrush(topGlow), 1));
            p.drawLine(bLen + 2, 0, w - bLen - 2, 0);
        }
    };

} // namespace

class PBGInternal : public QWidget
{
    Q_OBJECT

public:
    explicit PBGInternal(PixelBlastGameService *service, QWidget *parent = nullptr);
    ~PBGInternal() override;

    PixelBlast *game() const
    {
        return mGame;
    }

    bool isFinished() const
    {
        return mFinished;
    }

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *ev) override;

private slots:
    void updateScores();
    void onGameOver();
    void onRestartClicked();
    void onExitClicked();

private:
    void setupUi();

    PixelBlastGameService *mService = nullptr;
    PixelBlast *mGame = nullptr;
    PixelBlastFrame *mArcadeCard = nullptr;
    QScrollArea *mScrollArea = nullptr;
    QLabel *mScoresLabel = nullptr;
    QLabel *mHighScoreLabel = nullptr;
    QPushButton *mRestartBtn = nullptr;
    QPushButton *mExitBtn = nullptr;
    QTimer *mScoreTimer = nullptr;
    int mHighScore = 0;
    bool mFinished = false;
};

PBGInternal::PBGInternal(PixelBlastGameService *service, QWidget *parent) : QWidget(parent), mService(service)
{
    setObjectName("PBGInternalOverlay");
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet("QWidget#PBGInternalOverlay { background-color: rgba(7, 10, 18, 0.92); }");

    if(parent)
    {
        parent->installEventFilter(this);
        setGeometry(parent->rect());
    }

    mHighScore = AppSetting::pixelBlastHighScore();

    setupUi();

    mScoreTimer = new QTimer(this);
    connect(mScoreTimer, &QTimer::timeout, this, &PBGInternal::updateScores);
}

PBGInternal::~PBGInternal()
{
    if(parent())
    {
        parent()->removeEventFilter(this);
    }
    if(mScoreTimer)
    {
        mScoreTimer->stop();
    }
    if(mGame)
    {
        mGame->stopGame();
    }
}

void PBGInternal::setupUi()
{
    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setAlignment(Qt::AlignCenter);

    mArcadeCard = new PixelBlastFrame(this);
    mArcadeCard->setObjectName("arcadeCard");
    mArcadeCard->setFixedWidth(640);
    mArcadeCard->setStyleSheet(
        "QFrame#arcadeCard {"
        "  background-color: #0B0F19;"
        "  border: 1px solid #1E293B;"
        "  border-radius: 0px;"
        "}");

    QVBoxLayout *cardLayout = new QVBoxLayout(mArcadeCard);
    cardLayout->setContentsMargins(1, 1, 1, 1);
    cardLayout->setSpacing(0);

    // 1. Header HUD Frame
    QFrame *topCard = new QFrame(mArcadeCard);
    topCard->setObjectName("hudHeader");
    topCard->setStyleSheet(
        "QFrame#hudHeader {"
        "  background-color: #070A12;"
        "  border-bottom: 1px solid #1E293B;"
        "  border-radius: 0px;"
        "}");
    QHBoxLayout *topLayout = new QHBoxLayout(topCard);
    topLayout->setContentsMargins(12, 8, 12, 8);
    topLayout->setSpacing(8);

    // Service icon inside sharp cyber badge
    QLabel *iconLabel = new QLabel(topCard);
    iconLabel->setFixedSize(28, 28);
    iconLabel->setPixmap(QIcon(":/svg/services/pixelblast").pixmap(22, 22));
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setStyleSheet(
        "QLabel {"
        "  background-color: #0B0F19;"
        "  border: 1px solid #1E293B;"
        "  border-radius: 0px;"
        "}");
    topLayout->addWidget(iconLabel);

    // Title & subtitle
    QLabel *titleLabel = new QLabel(topCard);
    titleLabel->setTextFormat(Qt::RichText);
    titleLabel->setText(
        QString::fromUtf8(
            "<div style='line-height: 110%;'>"
            "<span style='color: #38BDF8; font-family: \"Segoe UI Variable\", \"Segoe UI\", sans-serif; font-weight: 800; font-size: 12.5px; letter-spacing: 0.8px;'>PIXEL BLAST GAME</span><br/>"
            "<span style='color: #64748B; font-family: \"Segoe UI Variable\", \"Segoe UI\", sans-serif; font-size: 9px; font-weight: 600; letter-spacing: 0.5px;'>by badcast</span>"
            "</div>"));
    titleLabel->setStyleSheet("background: transparent; border: none;");
    topLayout->addWidget(titleLabel);

    topLayout->addStretch(1);

    // High Score Badge
    mHighScoreLabel = new QLabel(topCard);
    mHighScoreLabel->setTextFormat(Qt::RichText);
    mHighScoreLabel->setText(
        QString::fromUtf8(
            "<span style='color: #FBBF24; font-size: 9.5px; font-weight: bold;'>🏆 РЕКОРД</span> "
            "<span style='color: #FFFFFF; font-family: \"Cascadia Code\", Consolas, monospace; font-size: 13px; font-weight: bold;'>%1</span>")
            .arg(mHighScore));
    mHighScoreLabel->setStyleSheet(
        "QLabel {"
        "  background-color: rgba(245, 158, 11, 0.08);"
        "  border: 1px solid rgba(245, 158, 11, 0.35);"
        "  border-radius: 0px;"
        "  padding: 4px 10px;"
        "}");
    topLayout->addWidget(mHighScoreLabel);

    // Current Score Badge
    mScoresLabel = new QLabel(topCard);
    mScoresLabel->setTextFormat(Qt::RichText);
    mScoresLabel->setText(
        QString::fromUtf8(
            "<span style='color: #38BDF8; font-size: 9.5px; font-weight: bold;'>⚡ СЧЁТ</span> "
            "<span style='color: #FFFFFF; font-family: \"Cascadia Code\", Consolas, monospace; font-size: 13px; font-weight: bold;'>0</span>"));
    mScoresLabel->setStyleSheet(
        "QLabel {"
        "  background-color: rgba(56, 189, 248, 0.08);"
        "  border: 1px solid rgba(56, 189, 248, 0.35);"
        "  border-radius: 0px;"
        "  padding: 4px 10px;"
        "}");
    topLayout->addWidget(mScoresLabel);

    // Restart Button
    mRestartBtn = new QPushButton(QString::fromUtf8("Заново"), topCard);
    mRestartBtn->setIcon(QIcon(":/svg/refresh-cw"));
    mRestartBtn->setCursor(Qt::PointingHandCursor);
    mRestartBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #1E293B;"
        "  color: #F8FAFC;"
        "  font-family: \"Segoe UI Variable\", \"Segoe UI\", sans-serif;"
        "  font-weight: 600;"
        "  font-size: 11px;"
        "  border: 1px solid #334155;"
        "  border-radius: 0px;"
        "  padding: 4px 10px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #0284C7;"
        "  color: #FFFFFF;"
        "  border-color: #38BDF8;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #0369A1;"
        "  border-color: #0284C7;"
        "}");
    connect(mRestartBtn, &QPushButton::clicked, this, &PBGInternal::onRestartClicked);
    topLayout->addWidget(mRestartBtn);

    // Exit Button
    mExitBtn = new QPushButton(QString::fromUtf8("Закрыть"), topCard);
    mExitBtn->setCursor(Qt::PointingHandCursor);
    mExitBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #1E293B;"
        "  color: #94A3B8;"
        "  font-family: \"Segoe UI Variable\", \"Segoe UI\", sans-serif;"
        "  font-weight: 600;"
        "  font-size: 11px;"
        "  border: 1px solid #334155;"
        "  border-radius: 0px;"
        "  padding: 4px 10px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #DC2626;"
        "  color: #FFFFFF;"
        "  border-color: #EF4444;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #991B1B;"
        "  border-color: #DC2626;"
        "}");
    connect(mExitBtn, &QPushButton::clicked, this, &PBGInternal::onExitClicked);
    topLayout->addWidget(mExitBtn);

    cardLayout->addWidget(topCard);

    // 2. Central Game Viewport in Scroll Area
    mScrollArea = new QScrollArea(mArcadeCard);
    mScrollArea->setObjectName("gameScrollArea");
    mScrollArea->setWidgetResizable(false);
    mScrollArea->setFrameShape(QFrame::NoFrame);
    mScrollArea->setAlignment(Qt::AlignCenter);
    mScrollArea->setStyleSheet(
        "QScrollArea#gameScrollArea { background-color: #050811; border: none; border-radius: 0px; }"
        "QScrollBar:vertical { background-color: #070A12; width: 6px; margin: 0px; border: none; }"
        "QScrollBar::handle:vertical { background-color: #1E293B; min-height: 24px; border-radius: 0px; }"
        "QScrollBar::handle:vertical:hover { background-color: #38BDF8; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; background: none; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }");

    mGame = new PixelBlast(mScrollArea);
    mGame->setCellSize(1);
    mGame->setFixedSize(530, 700);
    mGame->connect(mGame, &PixelBlast::endOfGame, this, &PBGInternal::onGameOver);

    mScrollArea->setWidget(mGame);
    cardLayout->addWidget(mScrollArea, 1);

    // 3. Bottom Hotkey Legend & Status Frame
    QFrame *footerCard = new QFrame(mArcadeCard);
    footerCard->setObjectName("hudFooter");
    footerCard->setStyleSheet(
        "QFrame#hudFooter {"
        "  background-color: #070A12;"
        "  border-top: 1px solid #1E293B;"
        "  border-radius: 0px;"
        "}");
    QHBoxLayout *footerLayout = new QHBoxLayout(footerCard);
    footerLayout->setContentsMargins(12, 6, 12, 6);
    footerLayout->setSpacing(8);

    QLabel *tipLabel = new QLabel(footerCard);
    tipLabel->setTextFormat(Qt::RichText);
    tipLabel->setText(
        QString::fromUtf8(
            "<span style='color: #64748B; font-family: \"Segoe UI Variable\", \"Segoe UI\", sans-serif; font-size: 10px; font-weight: 600; letter-spacing: 0.5px;'>УПРАВЛЕНИЕ:</span> &nbsp;"
            "<span style='color: #38BDF8; border: 1px solid #334155; padding: 2px 6px; font-family: \"Cascadia Code\", Consolas, monospace; font-size: 9.5px; font-weight: bold;'>ЛКМ</span> <span style='color: #94A3B8; font-size: 10.5px;'>Блоки на поле</span> &nbsp;|&nbsp; "
            "<span style='color: #F59E0B; border: 1px solid #334155; padding: 2px 6px; font-family: \"Cascadia Code\", Consolas, monospace; font-size: 9.5px; font-weight: bold;'>ПКМ</span> <span style='color: #94A3B8; font-size: 10.5px;'>Сброс выбора</span> &nbsp;|&nbsp; "
            "<span style='color: #EF4444; border: 1px solid #334155; padding: 2px 6px; font-family: \"Cascadia Code\", Consolas, monospace; font-size: 9.5px; font-weight: bold;'>ESC</span> <span style='color: #94A3B8; font-size: 10.5px;'>Закрыть оверлей</span>"));
    tipLabel->setStyleSheet("background: transparent; border: none;");
    tipLabel->setAlignment(Qt::AlignCenter);
    footerLayout->addWidget(tipLabel, 1, Qt::AlignCenter);

    cardLayout->addWidget(footerCard);

    // Ambient Obsidian Drop Shadow
    QGraphicsDropShadowEffect *cardShadow = new QGraphicsDropShadowEffect(mArcadeCard);
    cardShadow->setBlurRadius(28);
    cardShadow->setColor(QColor(0, 0, 0, 220));
    cardShadow->setOffset(0, 4);
    mArcadeCard->setGraphicsEffect(cardShadow);

    rootLayout->addWidget(mArcadeCard);
}

void PBGInternal::showEvent(QShowEvent *event)
{
    if(parentWidget())
    {
        setGeometry(parentWidget()->rect());
        if(mArcadeCard)
        {
            const int targetW = qMin(640, parentWidget()->width() - 24);
            const int targetH = qBound(420, parentWidget()->height() - 20, 750);
            mArcadeCard->setFixedSize(targetW, targetH);
        }
    }
    raise();
    setFocus();
    if(mGame)
    {
        mGame->setFocus();
        if(!mGame->isPlaying())
        {
            mGame->startGame();
        }
    }
    if(mScrollArea)
    {
        QTimer::singleShot(
            50,
            this,
            [this]()
            {
                if(mScrollArea->verticalScrollBar())
                {
                    mScrollArea->verticalScrollBar()->setValue(mScrollArea->verticalScrollBar()->maximum());
                }
            });
    }
    if(mScoreTimer && !mScoreTimer->isActive())
    {
        mScoreTimer->start(100);
    }
    QWidget::showEvent(event);
}

void PBGInternal::hideEvent(QHideEvent *event)
{
    if(mScoreTimer)
    {
        mScoreTimer->stop();
    }
    if(mGame)
    {
        mGame->stopGame();
    }
    QWidget::hideEvent(event);
}

void PBGInternal::keyPressEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_Escape)
    {
        onExitClicked();
        return;
    }
    QWidget::keyPressEvent(event);
}

void PBGInternal::mousePressEvent(QMouseEvent *event)
{
    // Consume mouse clicks outside the card to prevent pass-through to background widgets
    event->accept();
}

bool PBGInternal::eventFilter(QObject *obj, QEvent *ev)
{
    if(obj == parent() && ev->type() == QEvent::Resize)
    {
        if(parentWidget())
        {
            setGeometry(parentWidget()->rect());
            if(mArcadeCard)
            {
                const int targetW = qMin(640, parentWidget()->width() - 24);
                const int targetH = qBound(420, parentWidget()->height() - 20, 750);
                mArcadeCard->setFixedSize(targetW, targetH);
            }
            raise();
        }
    }
    return QWidget::eventFilter(obj, ev);
}

void PBGInternal::updateScores()
{
    if(!mGame)
        return;

    int currentScore = mGame->getScores();
    if(mScoresLabel)
    {
        mScoresLabel->setText(
            QString::fromUtf8(
                "<span style='color: #38BDF8; font-size: 9.5px; font-weight: bold;'>⚡ СЧЁТ</span> "
                "<span style='color: #FFFFFF; font-family: \"Cascadia Code\", Consolas, monospace; font-size: 13.5px; font-weight: bold;'>%1</span>")
                .arg(currentScore));
    }

    if(currentScore > mHighScore)
    {
        mHighScore = currentScore;
        if(mHighScoreLabel)
        {
            mHighScoreLabel->setText(
                QString::fromUtf8(
                    "<span style='color: #FBBF24; font-size: 9.5px; font-weight: bold;'>🏆 РЕКОРД</span> "
                    "<span style='color: #FFFFFF; font-family: \"Cascadia Code\", Consolas, monospace; font-size: 12.5px; font-weight: bold;'>%1</span>")
                    .arg(mHighScore));
        }
        AppSetting::pixelBlastHighScore(nullptr, mHighScore);
        AppSetting::save();
    }
}

void PBGInternal::onGameOver()
{
    updateScores();
    int finalScore = mGame ? mGame->getScores() : 0;
    bool isRecord = (finalScore > 0 && finalScore >= mHighScore);

    QString msg;
    if(isRecord)
    {
        msg = QString::fromUtf8("Поздравляем! Новый рекорд: %1 очков!\n\nХотите сыграть ещё раз?").arg(finalScore);
    }
    else
    {
        msg = QString::fromUtf8("Игра окончена!\nВаш результат: %1 очков (Рекорд: %2).\n\nХотите сыграть ещё раз?").arg(finalScore).arg(mHighScore);
    }

    QMessageBox box(this);
    box.setWindowTitle(QString::fromUtf8("Pixel Blast — Результат"));
    box.setText(msg);
    box.setIconPixmap(QIcon(":/svg/services/pixelblast").pixmap(48, 48));
    QPushButton *btnRestart = box.addButton(QString::fromUtf8("Сыграть снова"), QMessageBox::AcceptRole);
    QPushButton *btnExit = box.addButton(QString::fromUtf8("В кабинет"), QMessageBox::RejectRole);
    btnRestart->setCursor(Qt::PointingHandCursor);
    btnExit->setCursor(Qt::PointingHandCursor);
    btnRestart->setStyleSheet(
        "QPushButton {"
        "  background-color: #0284C7;"
        "  color: #FFFFFF;"
        "  font-family: \"Segoe UI Variable\", \"Segoe UI\", sans-serif;"
        "  font-weight: 700;"
        "  font-size: 11.5px;"
        "  border: 1px solid #38BDF8;"
        "  border-radius: 0px;"
        "  padding: 6px 18px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #0369A1;"
        "  border-color: #7DD3FC;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #075985;"
        "}");
    btnExit->setStyleSheet(
        "QPushButton {"
        "  background-color: #1E293B;"
        "  color: #94A3B8;"
        "  font-family: \"Segoe UI Variable\", \"Segoe UI\", sans-serif;"
        "  font-weight: 600;"
        "  font-size: 11.5px;"
        "  border: 1px solid #334155;"
        "  border-radius: 0px;"
        "  padding: 6px 18px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #334155;"
        "  border-color: #64748B;"
        "  color: #FFFFFF;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #0F172A;"
        "}");
    box.setStyleSheet(
        "QMessageBox {"
        "  background-color: #070A12;"
        "  border: 1px solid #1E293B;"
        "}"
        "QLabel {"
        "  color: #F8FAFC;"
        "  font-family: \"Segoe UI Variable\", \"Segoe UI\", sans-serif;"
        "  font-size: 12.5px;"
        "}");

    box.exec();

    if(box.clickedButton() == btnRestart)
    {
        onRestartClicked();
    }
    else
    {
        onExitClicked();
    }
}

void PBGInternal::onRestartClicked()
{
    if(mGame)
    {
        mGame->resetGame();
        mGame->startGame();
    }
    updateScores();
}

void PBGInternal::onExitClicked()
{
    if(mService)
    {
        mService->stop();
    }
}

// ============================================================================
// PixelBlastGameService Implementation
// ============================================================================

PixelBlastGameService::PixelBlastGameService(QObject *parent) : Service(DeviceConnectType::None, parent), mInternal(nullptr)
{
    title = QString::fromUtf8("Pixel Blast");
    m_sortScore = 120;
    m_flag = ServiceFlag::New;
}

PixelBlastGameService::~PixelBlastGameService()
{
    stop();
}

QString PixelBlastGameService::uuid() const
{
    return IDServicePixelBlastString;
}

QString PixelBlastGameService::getInternalData()
{
    return Service::getInternalData() + ":Game = PixelBlast currentScore = " + QString::number(mInternal && mInternal->game() ? mInternal->game()->getScores() : 0) + ", highScore = " + QString::number(AppSetting::pixelBlastHighScore());
}

PageIndex PixelBlastGameService::targetPage()
{
    return LengthPages;
}

bool PixelBlastGameService::canStart()
{
    return Service::canStart();
}

bool PixelBlastGameService::isStarted()
{
    return mInternal != nullptr && mInternal->isVisible();
}

bool PixelBlastGameService::isFinish()
{
    return mInternal ? mInternal->isFinished() : false;
}

bool PixelBlastGameService::start()
{
    sendCheckPull();
    if(!canStart())
        return false;

    if(!mInternal)
    {
        QWidget *parentWidget = MainWindow::current;
        mInternal = new PBGInternal(this, parentWidget);
    }

    if(MainWindow::current)
    {
        mInternal->setGeometry(MainWindow::current->rect());
    }
    mInternal->show();
    mInternal->raise();
    if(mInternal->game())
    {
        mInternal->game()->startGame();
    }

    return true;
}

void PixelBlastGameService::stop()
{
    if(!mInternal)
        return;

    PBGInternal *internal = mInternal;
    mInternal = nullptr;
    internal->hide();
    internal->deleteLater();
}

QString PixelBlastGameService::widgetIconName()
{
    return "pixelblast";
}

#include "Service.PixelBlastGame.moc"