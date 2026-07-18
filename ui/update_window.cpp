#include "update_window.h"
#include <QVBoxLayout>
#include <QScreen>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QGraphicsDropShadowEffect>
#include <QPainterPath>
#include <QLinearGradient>
#include <random>
#include <numeric>
#include <QRandomGenerator>

RandomBlockProgress::RandomBlockProgress(QWidget *parent) : QWidget(parent), m_value(0)
{
    setFixedSize(120, 120);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_randomIndices.resize(100);
    std::iota(m_randomIndices.begin(), m_randomIndices.end(), 0);
    
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(m_randomIndices.begin(), m_randomIndices.end(), g);
}

void RandomBlockProgress::setValue(int value)
{
    if (value < 0) value = 0;
    if (value > 100) value = 100;
    if (m_value != value) {
        m_value = value;
        update();
    }
}

void RandomBlockProgress::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Block logic: 10x10 grid, 2px spacing between them
    int cols = 10;
    int rows = 10;
    int spacing = 2;
    int totalWidth = width();
    int totalHeight = height();
    
    // We calculate block size so it fits perfectly
    float blockWidth = (float)(totalWidth - spacing * (cols - 1)) / cols;
    float blockHeight = (float)(totalHeight - spacing * (rows - 1)) / rows;

    QLinearGradient activeGradient(0, 0, totalWidth, totalHeight);
    activeGradient.setColorAt(0, QColor("#4776E6"));
    activeGradient.setColorAt(1, QColor("#8E54E9"));

    QBrush inactiveBrush(QColor("#1A2639"));

    painter.setPen(Qt::NoPen);

    for (int i = 0; i < 100; ++i) {
        int index = m_randomIndices[i];
        int row = index / cols;
        int col = index % cols;

        QRectF blockRect(col * (blockWidth + spacing), row * (blockHeight + spacing), blockWidth, blockHeight);

        // Round corners slightly for better aesthetic
        QPainterPath path;
        path.addRoundedRect(blockRect, 2, 2);

        if (i < m_value) {
            painter.fillPath(path, activeGradient);
        } else {
            painter.fillPath(path, inactiveBrush);
        }
    }
}

UpdateWindow::UpdateWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(480, 340);

    QWidget *centralWidget = new QWidget(this);
    centralWidget->setObjectName("mainContainer");
    setCentralWidget(centralWidget);

    QVBoxLayout *layout = new QVBoxLayout(centralWidget);
    layout->setContentsMargins(30, 30, 30, 30);
    layout->setSpacing(15);

    titleLabel = new QLabel("ADSKILLER UPDATE", this);
    titleLabel->setObjectName("titleLabel");
    titleLabel->setAlignment(Qt::AlignHCenter);

    label = new QLabel(this);
    label->setObjectName("infoLabel");
    label->setAlignment(Qt::AlignHCenter);

    jokeLabel = new QLabel(this);
    jokeLabel->setObjectName("jokeLabel");
    jokeLabel->setAlignment(Qt::AlignHCenter);

    progressBarCurrent = new QProgressBar(this);
    progressBarCurrent->setObjectName("progCurrent");
    progressBarCurrent->setTextVisible(false);

    blockProgressTotal = new RandomBlockProgress(this);

    statsLabel = new QLabel(this);
    statsLabel->setObjectName("statsLabel");
    statsLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    QHBoxLayout *blockLayout = new QHBoxLayout();
    blockLayout->addStretch();
    blockLayout->addWidget(blockProgressTotal);
    blockLayout->addSpacing(20);
    blockLayout->addWidget(statsLabel);
    blockLayout->addStretch();

    layout->addWidget(titleLabel);
    layout->addWidget(jokeLabel);
    layout->addStretch();
    layout->addWidget(label);
    layout->addWidget(progressBarCurrent);
    layout->addLayout(blockLayout);

    // Apply beautiful non-standard dark theme QSS
    this->setStyleSheet(R"(
        #mainContainer {
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #0B192C, stop:1 #050B14);
            border: 1px solid #33FFFFFF;
            border-radius: 16px;
        }
        #titleLabel {
            color: #FF416C;
            font-size: 18px;
            font-weight: 900;
            letter-spacing: 2px;
        }
        #infoLabel {
            color: #E0E0E0;
            font-size: 13px;
        }
        #jokeLabel {
            color: #8E54E9;
            font-size: 14px;
            font-style: italic;
            margin-top: 5px;
        }
        #statsLabel {
            color: #A0AABF;
            font-size: 12px;
            line-height: 1.5;
        }
        QProgressBar {
            background-color: #1A2639;
            border: none;
            border-radius: 6px;
            height: 12px;
        }
        QProgressBar::chunk {
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #FF416C, stop:1 #FF4B2B);
            border-radius: 6px;
        }
        #progTotal::chunk {
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #4776E6, stop:1 #8E54E9);
        }
    )");

    // Joke timer
    QStringList jokes = {
        "Android: свобода. iPhone: дорогой плен.",
        "Зарядка iPhone снова потерялась?",
        "Android кастомизируется, iPhone просто работает.",
        "Зеленый пузырь? Ну и что!",
        "Снова покупаем переходник для яблока?",
        "Твой Android умеет варить кофе?",
        "iPhone — статус, Android — жизнь.",
        "Продал почку ради нового iPhone.",
        "Android: настройки, которые не трогаешь.",
        "Экосистема Apple не отпускает!"
    };
    jokeLabel->setText(jokes.first());
    
    QTimer *jokeTimer = new QTimer(this);
    connect(jokeTimer, &QTimer::timeout, this, [this, jokes]() {
        int r = QRandomGenerator::global()->bounded(jokes.size());
        jokeLabel->setText(jokes[r]);
    });
    jokeTimer->start(3500);

    // Center window
    if (const QScreen *screen = QGuiApplication::primaryScreen()) {
        QRect screenGeometry = screen->geometry();
        int x = (screenGeometry.width() - width()) / 2;
        int y = (screenGeometry.height() - height()) / 2;
        move(x, y);
    }
}

void UpdateWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void UpdateWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPosition().toPoint() - m_dragPosition);
        event->accept();
    }
}

void UpdateWindow::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    m_drag = false;
}

UpdateWindow::~UpdateWindow()
{
}

void UpdateWindow::setText(const QString &value)
{
    label->setText(value);
}

void UpdateWindow::setStats(const QString &stats)
{
    statsLabel->setText(stats);
}

void UpdateWindow::setProgress(int v1, int v2)
{
    progressBarCurrent->setValue(v1);
    blockProgressTotal->setValue(v2);
}

void UpdateWindow::delayPush(int ms, std::function<void()> call, bool loop)
{
    QTimer *qtimer = new QTimer(this);
    qtimer->setSingleShot(!loop);
    qtimer->setInterval(ms);
    connect(
        qtimer,
        &QTimer::timeout,
        [qtimer, call]()
        {
            call();
            if(qtimer->isSingleShot())
            {
                qtimer->stop();
                qtimer->deleteLater();
            }
        });
    qtimer->start();
}
