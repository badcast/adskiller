#include "FileManagerWidget.h"
#include "Services.h"
#include "mainwindow.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>

QString FileManagerWidget::formatBytes(qint64 bytes)
{
    if(bytes < 1024)
        return QString("%1 Б").arg(bytes);
    if(bytes < 1024 * 1024)
        return QString("%1 КБ").arg(QString::number(bytes / 1024.0, 'f', 1));
    if(bytes < 1024 * 1024 * 1024)
        return QString("%1 МБ").arg(QString::number(bytes / (1024.0 * 1024.0), 'f', 1));
    return QString("%1 ГБ").arg(QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2));
}

class FilePreviewDialog : public QDialog
{
public:
    FilePreviewDialog(const QString &fileName, const QByteArray &content, QWidget *parent = nullptr) : QDialog(parent)
    {
        setWindowTitle("Просмотр — " + fileName);
        resize(720, 480);
        setStyleSheet(
            "QDialog { background-color: #0A0E1A; color: #F8FAFC; }"
            "QTextEdit { background-color: #070A12; color: #F8FAFC; border: 1px solid #1E293B; border-radius: 0px; font-family: monospace; font-size: 12px; selection-background-color: #0284C7; }"
            "QPushButton { background-color: #0F172A; color: #E2E8F0; border: 1.5px solid #1E293B; border-radius: 0px; padding: 6px 16px; font-weight: 600; }"
            "QPushButton:hover { background-color: #131E35; border-color: #38BDF8; color: #38BDF8; }");

        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setContentsMargins(16, 14, 16, 14);
        layout->setSpacing(10);

        QHBoxLayout *topInfo = new QHBoxLayout();
        QLabel *title = new QLabel("<img src=\":/svg/clipboard\" width=\"14\" height=\"14\" style=\"vertical-align: middle;\"/> " + fileName, this);
        title->setTextFormat(Qt::RichText);
        title->setStyleSheet("font-size: 14px; font-weight: bold; color: #38BDF8;");
        QLabel *sizeLbl = new QLabel(QString("Размер: %1").arg(FileManagerWidget::formatBytes(content.size())), this);
        sizeLbl->setStyleSheet("color: #94A3B8; font-size: 12px;");
        topInfo->addWidget(title);
        topInfo->addStretch(1);
        topInfo->addWidget(sizeLbl);
        layout->addLayout(topInfo);

        QString ext = QFileInfo(fileName).suffix().toLower();
        bool isImage = (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "webp" || ext == "gif" || ext == "bmp");

        if(isImage)
        {
            QPixmap pix;
            if(pix.loadFromData(content))
            {
                QLabel *imgLabel = new QLabel(this);
                imgLabel->setAlignment(Qt::AlignCenter);
                imgLabel->setStyleSheet("background-color: #070A12; border: 1px solid #1E293B; border-radius: 0px; padding: 10px;");
                if(pix.width() > 680 || pix.height() > 360)
                    pix = pix.scaled(680, 360, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                imgLabel->setPixmap(pix);
                layout->addWidget(imgLabel, 1);
            }
            else
            {
                QTextEdit *edit = new QTextEdit(this);
                edit->setReadOnly(true);
                edit->setText("Не удалось декодировать изображение.");
                layout->addWidget(edit, 1);
            }
        }
        else
        {
            QTextEdit *edit = new QTextEdit(this);
            edit->setReadOnly(true);
            edit->setText(QString::fromUtf8(content));
            layout->addWidget(edit, 1);
        }

        QHBoxLayout *btnLayout = new QHBoxLayout();
        btnLayout->addStretch(1);
        QPushButton *btnClose = new QPushButton("Закрыть", this);
        connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
        btnLayout->addWidget(btnClose);
        layout->addLayout(btnLayout);
    }
};

FileManagerWidget::FileManagerWidget(QWidget *parent) : QWidget(parent)
{
    m_currentPath = "/sdcard";
    setupUi();
}

void FileManagerWidget::setDevice(const AdbDevice &device)
{
    m_device = device;
    m_fileIO.connect(device.devId);
}

void FileManagerWidget::setupUi()
{
    setStyleSheet(
        "QWidget { background-color: #0A0E1A; color: #F8FAFC; font-family: 'Segoe UI', 'Noto Sans', sans-serif; }"
        "QTableWidget { background-color: #0B0F19; alternate-background-color: #0F172A; color: #F8FAFC; border: 1px solid #1E293B; border-radius: 0px; gridline-color: #161F33; selection-background-color: #0284C7; selection-color: #FFFFFF; font-size: 12px; }"
        "QHeaderView::section { background-color: #0F172A; color: #94A3B8; font-weight: bold; font-size: 11px; border: none; border-bottom: 1px solid #1E293B; padding: 6px 8px; }"
        "QLineEdit { background-color: #0F172A; color: #F8FAFC; border: 1.5px solid #1E293B; border-radius: 0px; padding: 5px 10px; font-size: 12px; }"
        "QLineEdit:focus { border-color: #38BDF8; }"
        "QPushButton { background-color: #0F172A; color: #E2E8F0; border: 1.5px solid #1E293B; border-radius: 0px; padding: 5px 12px; font-size: 11.5px; font-weight: 600; }"
        "QPushButton:hover { background-color: #131E35; border-color: #38BDF8; color: #38BDF8; }"
        "QPushButton:pressed { background-color: #0B101D; border-color: #0284C7; }"
        "QPushButton:disabled { background-color: #0B101D; color: #475569; border-color: #1E293B; }"
        "QLabel { color: #94A3B8; font-size: 11.5px; }"
        "QListWidget { background-color: #0B0F19; border: 1px solid #1E293B; border-radius: 0px; padding: 4px; color: #E2E8F0; }"
        "QListWidget::item { padding: 8px 10px; border-radius: 0px; font-size: 12px; font-weight: 500; margin-bottom: 2px; }"
        "QListWidget::item:hover { background-color: #131E35; color: #38BDF8; }"
        "QListWidget::item:selected { background-color: #141B2D; border: 1px solid rgba(56, 189, 248, 0.35); color: #38BDF8; font-weight: bold; }");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(14, 10, 14, 12);
    mainLayout->setSpacing(8);

    // 1. Top Navigation Bar
    QHBoxLayout *navLayout = new QHBoxLayout();
    navLayout->setSpacing(6);

    m_btnBack = new QPushButton(this);
    m_btnBack->setIcon(QIcon(":/svg/arrow-left"));
    m_btnBack->setIconSize(QSize(14, 14));
    m_btnBack->setToolTip("Назад");
    m_btnBack->setFixedWidth(34);
    m_btnBack->setEnabled(false);
    connect(m_btnBack, &QPushButton::clicked, this, &FileManagerWidget::navigateBack);

    m_btnForward = new QPushButton(this);
    m_btnForward->setIcon(QIcon(":/svg/arrow-right"));
    m_btnForward->setIconSize(QSize(14, 14));
    m_btnForward->setToolTip("Вперёд");
    m_btnForward->setFixedWidth(34);
    m_btnForward->setEnabled(false);
    connect(m_btnForward, &QPushButton::clicked, this, &FileManagerWidget::navigateForward);

    m_btnUp = new QPushButton("Наверх", this);
    m_btnUp->setIcon(QIcon(":/svg/arrow-up"));
    m_btnUp->setIconSize(QSize(14, 14));
    m_btnUp->setToolTip("Перейти на уровень выше");
    connect(m_btnUp, &QPushButton::clicked, this, &FileManagerWidget::navigateUp);

    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setText(m_currentPath);
    m_pathEdit->setClearButtonEnabled(true);
    connect(
        m_pathEdit,
        &QLineEdit::returnPressed,
        this,
        [this]()
        {
            QString p = m_pathEdit->text().trimmed();
            if(!p.isEmpty())
                navigateTo(p);
        });

    m_btnCopyPath = new QPushButton(this);
    m_btnCopyPath->setIcon(QIcon(":/svg/copy"));
    m_btnCopyPath->setIconSize(QSize(13, 13));
    m_btnCopyPath->setToolTip("Скопировать путь");
    m_btnCopyPath->setFixedWidth(32);
    connect(
        m_btnCopyPath,
        &QPushButton::clicked,
        this,
        [this]()
        {
            QApplication::clipboard()->setText(m_currentPath);
            m_statusMsg->setText("Путь скопирован в буфер");
        });

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Поиск файлов...");
    m_searchEdit->setFixedWidth(190);
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &FileManagerWidget::onSearchFilterChanged);

    m_btnNewDir = new QPushButton("Папка", this);
    m_btnNewDir->setIcon(QIcon(":/svg/plus"));
    m_btnNewDir->setIconSize(QSize(13, 13));
    m_btnNewDir->setToolTip("Создать новую папку");
    connect(m_btnNewDir, &QPushButton::clicked, this, &FileManagerWidget::createDirectory);

    m_btnUpload = new QPushButton("Загрузить", this);
    m_btnUpload->setIcon(QIcon(":/svg/upload"));
    m_btnUpload->setIconSize(QSize(13, 13));
    m_btnUpload->setToolTip("Загрузить файл с компьютера");
    m_btnUpload->setStyleSheet(
        "QPushButton { background: #164E63; color: #38BDF8; font-weight: bold; border: 1px solid #0891B2; }"
        "QPushButton:hover { background: #0E7490; color: #FFFFFF; }");
    connect(m_btnUpload, &QPushButton::clicked, this, &FileManagerWidget::uploadFile);

    m_btnRefresh = new QPushButton(this);
    m_btnRefresh->setIcon(QIcon(":/svg/refresh-cw"));
    m_btnRefresh->setIconSize(QSize(13, 13));
    m_btnRefresh->setToolTip("Обновить список");
    m_btnRefresh->setFixedWidth(34);
    connect(m_btnRefresh, &QPushButton::clicked, this, &FileManagerWidget::refreshList);

    navLayout->addWidget(m_btnBack);
    navLayout->addWidget(m_btnForward);
    navLayout->addWidget(m_btnUp);
    navLayout->addWidget(m_pathEdit, 1);
    navLayout->addWidget(m_btnCopyPath);
    navLayout->addWidget(m_searchEdit);
    navLayout->addWidget(m_btnNewDir);
    navLayout->addWidget(m_btnUpload);
    navLayout->addWidget(m_btnRefresh);
    mainLayout->addLayout(navLayout);

    // 2. Central Splitter: Sidebar + Main Table + Inspector
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(4);
    splitter->setStyleSheet("QSplitter::handle { background-color: #1E293B; border-radius: 0px; }");

    // Left Sidebar: Quick Access
    QWidget *sidebarWidget = new QWidget(splitter);
    QVBoxLayout *sidebarLayout = new QVBoxLayout(sidebarWidget);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);
    sidebarLayout->setSpacing(6);

    QLabel *sidebarTitle = new QLabel("БЫСТРЫЙ ДОСТУП", sidebarWidget);
    sidebarTitle->setStyleSheet("color: #94A3B8; font-size: 10px; font-weight: bold; letter-spacing: 0.8px; padding: 2px 4px;");
    sidebarLayout->addWidget(sidebarTitle);

    m_quickAccessList = new QListWidget(sidebarWidget);
    auto addShortcut = [this](const QString &label, const QString &path, const QString &iconPath)
    {
        QListWidgetItem *it = new QListWidgetItem(QIcon(iconPath), label, m_quickAccessList);
        it->setData(Qt::UserRole, path);
    };
    addShortcut("Внутренняя память", "/sdcard", ":/svg/smartphone");
    addShortcut("Загрузки", "/sdcard/Download", ":/svg/download");
    addShortcut("Фото (DCIM)", "/sdcard/DCIM", ":/svg/folder");
    addShortcut("Изображения", "/sdcard/Pictures", ":/svg/folder");
    addShortcut("Музыка", "/sdcard/Music", ":/svg/music");
    addShortcut("Видео", "/sdcard/Movies", ":/svg/folder");
    addShortcut("Документы", "/sdcard/Documents", ":/svg/clipboard");
    addShortcut("Android Data", "/sdcard/Android/data", ":/svg/settings");
    addShortcut("Корень устройства", "/", ":/svg/hard-drive");
    connect(m_quickAccessList, &QListWidget::itemClicked, this, &FileManagerWidget::onQuickAccessClicked);
    sidebarLayout->addWidget(m_quickAccessList);
    splitter->addWidget(sidebarWidget);

    // Center: File Table
    QWidget *centerWidget = new QWidget(splitter);
    QVBoxLayout *centerLayout = new QVBoxLayout(centerWidget);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(4);

    m_table = new QTableWidget(centerWidget);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels(QStringList() << "Имя" << "Тип" << "Размер" << "Дата изменения" << "Права");
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->setShowGrid(false);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(36);
    m_table->setIconSize(QSize(22, 22));
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(m_table, &QTableWidget::cellDoubleClicked, this, &FileManagerWidget::onItemDoubleClicked);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &FileManagerWidget::onSelectionChanged);
    connect(m_table, &QTableWidget::customContextMenuRequested, this, &FileManagerWidget::showContextMenu);

    centerLayout->addWidget(m_table);
    splitter->addWidget(centerWidget);

    // Right Inspector Pane
    m_inspectorPane = new QWidget(splitter);
    QVBoxLayout *inspLayout = new QVBoxLayout(m_inspectorPane);
    inspLayout->setContentsMargins(4, 0, 0, 0);
    inspLayout->setSpacing(8);

    QFrame *card = new QFrame(m_inspectorPane);
    card->setObjectName("fmInspectorCard");
    card->setStyleSheet("QFrame#fmInspectorCard { background-color: #0F172A; border: 1px solid #1E293B; border-radius: 0px; padding: 8px; }");
    QVBoxLayout *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(10, 10, 10, 10);
    cardLayout->setSpacing(8);

    m_inspIcon = new QLabel(card);
    m_inspIcon->setAlignment(Qt::AlignCenter);
    m_inspIcon->setFixedSize(52, 52);

    QHBoxLayout *iconCenter = new QHBoxLayout();
    iconCenter->addStretch(1);
    iconCenter->addWidget(m_inspIcon);
    iconCenter->addStretch(1);
    cardLayout->addLayout(iconCenter);

    m_inspName = new QLabel("Выберите объект", card);
    m_inspName->setStyleSheet("color: #FFFFFF; font-size: 13px; font-weight: bold;");
    m_inspName->setAlignment(Qt::AlignCenter);
    m_inspName->setWordWrap(true);
    m_inspName->setTextInteractionFlags(Qt::TextSelectableByMouse);
    cardLayout->addWidget(m_inspName);

    QHBoxLayout *badges = new QHBoxLayout();
    badges->setSpacing(6);
    m_inspTypeBadge = new QLabel("—", card);
    m_inspTypeBadge->setStyleSheet("background-color: #1E293B; color: #94A3B8; border: 1px solid #334155; border-radius: 0px; padding: 2px 8px; font-size: 10px; font-weight: 600;");
    m_inspSizeBadge = new QLabel("—", card);
    m_inspSizeBadge->setStyleSheet("background-color: #1E293B; color: #38BDF8; border: 1px solid #334155; border-radius: 0px; padding: 2px 8px; font-size: 10px; font-weight: 600;");
    badges->addStretch(1);
    badges->addWidget(m_inspTypeBadge);
    badges->addWidget(m_inspSizeBadge);
    badges->addStretch(1);
    cardLayout->addLayout(badges);

    QFrame *sep = new QFrame(card);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color: #1E293B;");
    cardLayout->addWidget(sep);

    m_inspPathLabel = new QLabel("—", card);
    m_inspPathLabel->setStyleSheet("color: #64748B; font-size: 11px;");
    m_inspPathLabel->setWordWrap(true);
    m_inspPathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    cardLayout->addWidget(m_inspPathLabel);

    auto makeDetailRow = [card, cardLayout](const QString &title, QLabel *&valLabel)
    {
        QHBoxLayout *row = new QHBoxLayout();
        row->setSpacing(6);
        QLabel *titleLbl = new QLabel(title, card);
        titleLbl->setStyleSheet("color: #64748B; font-size: 11px;");
        valLabel = new QLabel("—", card);
        valLabel->setStyleSheet("color: #E2E8F0; font-size: 11px; font-weight: 500;");
        row->addWidget(titleLbl);
        row->addStretch(1);
        row->addWidget(valLabel);
        cardLayout->addLayout(row);
    };

    makeDetailRow("Изменён:", m_inspDateVal);
    makeDetailRow("Права:", m_inspPermsVal);

    cardLayout->addSpacing(8);

    m_btnDownload = new QPushButton("Скачать на ПК", card);
    m_btnDownload->setIcon(QIcon(":/svg/download"));
    m_btnDownload->setIconSize(QSize(14, 14));
    m_btnDownload->setStyleSheet(
        "QPushButton { background-color: #0284C7; color: white; font-weight: bold; padding: 7px; border: none; border-radius: 0px; }"
        "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0369A1, stop:1 #38BDF8); }"
        "QPushButton:disabled { background: #1E293B; color: #475569; }");
    connect(m_btnDownload, &QPushButton::clicked, this, &FileManagerWidget::downloadSelected);
    cardLayout->addWidget(m_btnDownload);

    m_btnPreview = new QPushButton("Быстрый просмотр", card);
    m_btnPreview->setIcon(QIcon(":/svg/eye"));
    m_btnPreview->setIconSize(QSize(13, 13));
    connect(m_btnPreview, &QPushButton::clicked, this, &FileManagerWidget::previewSelected);
    cardLayout->addWidget(m_btnPreview);

    m_btnRename = new QPushButton("Переименовать", card);
    m_btnRename->setIcon(QIcon(":/svg/edit"));
    m_btnRename->setIconSize(QSize(13, 13));
    connect(m_btnRename, &QPushButton::clicked, this, &FileManagerWidget::renameSelected);
    cardLayout->addWidget(m_btnRename);

    m_btnDelete = new QPushButton("Удалить объект", card);
    m_btnDelete->setIcon(QIcon(":/svg/trash"));
    m_btnDelete->setIconSize(QSize(13, 13));
    m_btnDelete->setStyleSheet(
        "QPushButton { background: rgba(239, 68, 68, 0.15); color: #F87171; border: 1px solid rgba(239, 68, 68, 0.35); font-weight: bold; padding: 6px; border-radius: 0px; }"
        "QPushButton:hover { background: rgba(239, 68, 68, 0.25); color: #FFA3A3; border-color: #EF4444; }"
        "QPushButton:disabled { background: #0B101D; color: #475569; border-color: #1E293B; }");
    connect(m_btnDelete, &QPushButton::clicked, this, &FileManagerWidget::deleteSelected);
    cardLayout->addWidget(m_btnDelete);

    cardLayout->addStretch(1);
    inspLayout->addWidget(card);
    splitter->addWidget(m_inspectorPane);

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    splitter->setSizes(QList<int>() << 170 << 540 << 230);

    mainLayout->addWidget(splitter, 1);

    // 3. Bottom Status Bar
    QHBoxLayout *statusLayout = new QHBoxLayout();
    statusLayout->setSpacing(12);

    m_statusSummary = new QLabel("0 элементов", this);
    m_statusSummary->setStyleSheet("color: #94A3B8; font-size: 11px; font-weight: 500;");

    m_statusSelected = new QLabel("Ничего не выбрано", this);
    m_statusSelected->setStyleSheet("color: #64748B; font-size: 11px;");

    m_statusMsg = new QLabel("Готово", this);
    m_statusMsg->setStyleSheet("color: #38BDF8; font-size: 11px; font-weight: 500;");

    statusLayout->addWidget(m_statusSummary);
    statusLayout->addWidget(m_statusSelected);
    statusLayout->addStretch(1);
    statusLayout->addWidget(m_statusMsg);
    mainLayout->addLayout(statusLayout);

    updateInspector();
}

QIcon FileManagerWidget::getFileIcon(bool isDir, const QString &fileName)
{
    static QMap<QString, QIcon> iconCache;
    QString ext = QFileInfo(fileName).suffix().toLower();
    QString key = isDir ? (fileName == ".." ? "up" : "dir") : (ext.isEmpty() ? "file" : ext);

    if(iconCache.contains(key))
        return iconCache.value(key);

    QPixmap pixmap(28, 28);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    if(isDir)
    {
        if(fileName == "..")
        {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor("#0284C7"));
            p.drawRoundedRect(2, 2, 24, 24, 6, 6);
            p.setPen(QPen(Qt::white, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawLine(14, 8, 14, 20);
            p.drawLine(14, 8, 9, 13);
            p.drawLine(14, 8, 19, 13);
        }
        else
        {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor("#D97706"));
            p.drawRoundedRect(3, 6, 10, 6, 2, 2);
            p.setBrush(QColor("#F59E0B"));
            p.drawRoundedRect(3, 9, 22, 14, 3, 3);
            p.setBrush(QColor("#FBBF24"));
            p.drawRoundedRect(5, 11, 18, 10, 2, 2);
        }
    }
    else
    {
        QColor badgeBg = QColor("#334155");
        QString badge = "DOC";
        if(ext == "apk")
        {
            badgeBg = QColor("#059669");
            badge = "APK";
        }
        else if(ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "webp" || ext == "gif" || ext == "bmp")
        {
            badgeBg = QColor("#0284C7");
            badge = "IMG";
        }
        else if(ext == "mp4" || ext == "mkv" || ext == "avi" || ext == "mov")
        {
            badgeBg = QColor("#7C3AED");
            badge = "VID";
        }
        else if(ext == "mp3" || ext == "wav" || ext == "flac" || ext == "ogg")
        {
            badgeBg = QColor("#DB2777");
            badge = "AUD";
        }
        else if(ext == "zip" || ext == "rar" || ext == "7z" || ext == "tar" || ext == "gz")
        {
            badgeBg = QColor("#D97706");
            badge = "ZIP";
        }
        else if(ext == "pdf")
        {
            badgeBg = QColor("#DC2626");
            badge = "PDF";
        }
        else if(ext == "txt" || ext == "log" || ext == "md")
        {
            badgeBg = QColor("#475569");
            badge = "TXT";
        }
        else if(ext == "json" || ext == "xml" || ext == "prop" || ext == "conf")
        {
            badgeBg = QColor("#0891B2");
            badge = "CFG";
        }

        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#1E293B"));
        p.drawRoundedRect(4, 3, 20, 22, 3, 3);

        p.setBrush(QColor("#334155"));
        QPolygon corner;
        corner << QPoint(17, 3) << QPoint(24, 10) << QPoint(17, 10);
        p.drawPolygon(corner);

        p.setBrush(badgeBg);
        p.drawRoundedRect(5, 13, 18, 10, 2, 2);

        p.setPen(Qt::white);
        QFont f = p.font();
        f.setPixelSize(7);
        f.setBold(true);
        p.setFont(f);
        p.drawText(QRect(5, 13, 18, 10), Qt::AlignCenter, badge);
    }
    p.end();

    QIcon icon(pixmap);
    iconCache.insert(key, icon);
    return icon;
}

QString FileManagerWidget::getFileType(bool isDir, const QString &fileName)
{
    if(isDir)
        return (fileName == "..") ? "Родительская папка" : "Папка с файлами";

    QString ext = QFileInfo(fileName).suffix().toLower();
    if(ext == "apk")
        return "Пакет Android (APK)";
    if(ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "webp" || ext == "gif" || ext == "bmp")
        return "Изображение " + ext.toUpper();
    if(ext == "mp4" || ext == "mkv" || ext == "avi" || ext == "mov")
        return "Видеофайл " + ext.toUpper();
    if(ext == "mp3" || ext == "wav" || ext == "flac" || ext == "ogg")
        return "Аудиозапись " + ext.toUpper();
    if(ext == "zip" || ext == "rar" || ext == "7z" || ext == "tar" || ext == "gz")
        return "Архив " + ext.toUpper();
    if(ext == "pdf")
        return "Документ PDF";
    if(ext == "txt" || ext == "log" || ext == "md")
        return "Текстовый документ";
    if(ext == "json" || ext == "xml")
        return "Файл разметки (" + ext.toUpper() + ")";
    if(ext == "sh")
        return "Shell-скрипт";
    if(ext == "prop")
        return "Свойства Android (build.prop)";
    if(ext.isEmpty())
        return "Файл";
    return "Файл " + ext.toUpper();
}

void FileManagerWidget::navigateTo(const QString &path)
{
    QString target = QDir::cleanPath(path.trimmed());
    if(target.isEmpty())
        target = "/sdcard";

    if(m_currentPath != target && !m_currentPath.isEmpty())
    {
        m_historyBack.append(m_currentPath);
        m_historyForward.clear();
        m_btnBack->setEnabled(true);
        m_btnForward->setEnabled(false);
    }

    m_currentPath = target;
    refreshList();
}

void FileManagerWidget::navigateBack()
{
    if(m_historyBack.isEmpty())
        return;

    m_historyForward.append(m_currentPath);
    m_currentPath = m_historyBack.takeLast();
    m_btnBack->setEnabled(!m_historyBack.isEmpty());
    m_btnForward->setEnabled(true);
    refreshList();
}

void FileManagerWidget::navigateForward()
{
    if(m_historyForward.isEmpty())
        return;

    m_historyBack.append(m_currentPath);
    m_currentPath = m_historyForward.takeLast();
    m_btnBack->setEnabled(true);
    m_btnForward->setEnabled(!m_historyForward.isEmpty());
    refreshList();
}

void FileManagerWidget::navigateUp()
{
    if(m_currentPath == "/" || m_currentPath.isEmpty())
        return;

    QString clean = QDir::cleanPath(m_currentPath);
    int lastSlash = clean.lastIndexOf('/');
    QString parentPath = (lastSlash <= 0) ? "/" : clean.left(lastSlash);
    navigateTo(parentPath);
}

void FileManagerWidget::refreshList()
{
    if(m_device.devId.isEmpty() && MainWindow::current && !MainWindow::current->currentAdbDevice().isEmpty())
        setDevice(MainWindow::current->currentAdbDevice());

    if(!m_fileIO.isConnect() && !m_device.devId.isEmpty())
        m_fileIO.connect(m_device.devId);

    if(!m_fileIO.isConnect())
    {
        m_statusMsg->setText("Нет подключения к устройству");
        return;
    }

    m_pathEdit->setText(m_currentPath);
    m_statusMsg->setText("Загрузка файлов...");
    qApp->processEvents();

    m_currentItems = m_fileIO.getFileList(m_currentPath);

    // Sort: directories first, then files alphabetically
    std::sort(
        m_currentItems.begin(),
        m_currentItems.end(),
        [](const AdbFileInfo &a, const AdbFileInfo &b)
        {
            if(a.isDir != b.isDir)
                return a.isDir > b.isDir;
            return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
        });

    populateTable();
    m_statusMsg->setText("Готово");
}

void FileManagerWidget::populateTable()
{
    m_table->setRowCount(0);

    QString filter = m_searchEdit->text().trimmed();

    int row = 0;

    // Show ".." parent entry if not in root
    if(m_currentPath != "/" && !m_currentPath.isEmpty() && filter.isEmpty())
    {
        m_table->insertRow(row);
        QTableWidgetItem *itemUp = new QTableWidgetItem(".. [Наверх]");
        itemUp->setIcon(getFileIcon(true, ".."));
        itemUp->setData(Qt::UserRole, true);
        itemUp->setData(Qt::UserRole + 1, "..");
        itemUp->setData(Qt::UserRole + 2, "");
        m_table->setItem(row, 0, itemUp);

        QTableWidgetItem *typeUp = new QTableWidgetItem("Папка");
        typeUp->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 1, typeUp);

        QTableWidgetItem *dash1 = new QTableWidgetItem("—");
        dash1->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(row, 2, dash1);

        QTableWidgetItem *dash2 = new QTableWidgetItem("—");
        dash2->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 3, dash2);

        QTableWidgetItem *dash3 = new QTableWidgetItem("—");
        dash3->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 4, dash3);
        row++;
    }

    int dirCount = 0;
    int fileCount = 0;
    qint64 totalSize = 0;

    for(const AdbFileInfo &info : m_currentItems)
    {
        if(!filter.isEmpty() && !info.name.contains(filter, Qt::CaseInsensitive))
            continue;

        m_table->insertRow(row);

        QTableWidgetItem *nameItem = new QTableWidgetItem(info.name);
        nameItem->setIcon(getFileIcon(info.isDir, info.name));
        nameItem->setData(Qt::UserRole, info.isDir);
        nameItem->setData(Qt::UserRole + 1, info.name);
        nameItem->setData(Qt::UserRole + 2, info.fullPath);

        QTableWidgetItem *typeItem = new QTableWidgetItem(getFileType(info.isDir, info.name));
        typeItem->setForeground(QColor("#94A3B8"));

        QTableWidgetItem *sizeItem = new QTableWidgetItem(info.isDir ? "—" : formatBytes(info.size));
        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        sizeItem->setForeground(info.isDir ? QColor("#64748B") : QColor("#38BDF8"));

        QTableWidgetItem *dateItem = new QTableWidgetItem(info.modifyTime.isEmpty() ? "—" : info.modifyTime);
        dateItem->setTextAlignment(Qt::AlignCenter);
        dateItem->setForeground(QColor("#94A3B8"));

        QTableWidgetItem *permItem = new QTableWidgetItem(info.permissions.isEmpty() ? "—" : info.permissions);
        permItem->setTextAlignment(Qt::AlignCenter);
        permItem->setForeground(QColor("#64748B"));

        m_table->setItem(row, 0, nameItem);
        m_table->setItem(row, 1, typeItem);
        m_table->setItem(row, 2, sizeItem);
        m_table->setItem(row, 3, dateItem);
        m_table->setItem(row, 4, permItem);

        if(info.isDir)
            dirCount++;
        else
        {
            fileCount++;
            totalSize += info.size;
        }

        row++;
    }

    m_statusSummary->setText(QString("%1 папок  •  %2 файлов  •  %3").arg(dirCount).arg(fileCount).arg(formatBytes(totalSize)));
    onSelectionChanged();
}

void FileManagerWidget::onSelectionChanged()
{
    updateInspector();
}

void FileManagerWidget::updateInspector()
{
    int row = m_table->currentRow();
    if(row < 0 || row >= m_table->rowCount())
    {
        m_inspIcon->setPixmap(getFileIcon(true, "folder").pixmap(48, 48));
        m_inspName->setText("Объект не выбран");
        m_inspTypeBadge->setText("—");
        m_inspSizeBadge->setText("—");
        m_inspPathLabel->setText("Текущий каталог: " + m_currentPath);
        m_inspDateVal->setText("—");
        m_inspPermsVal->setText("—");

        m_btnDownload->setEnabled(false);
        m_btnPreview->setEnabled(false);
        m_btnRename->setEnabled(false);
        m_btnDelete->setEnabled(false);
        m_statusSelected->setText("Ничего не выбрано");
        return;
    }

    QTableWidgetItem *item = m_table->item(row, 0);
    if(!item)
        return;

    QString name = item->data(Qt::UserRole + 1).toString();
    if(name == "..")
    {
        m_inspIcon->setPixmap(getFileIcon(true, "..").pixmap(48, 48));
        m_inspName->setText(".. [На уровень выше]");
        m_inspTypeBadge->setText("Папка");
        m_inspSizeBadge->setText("—");
        m_inspPathLabel->setText("Переход в родительский каталог");
        m_inspDateVal->setText("—");
        m_inspPermsVal->setText("—");

        m_btnDownload->setEnabled(false);
        m_btnPreview->setEnabled(false);
        m_btnRename->setEnabled(false);
        m_btnDelete->setEnabled(false);
        m_statusSelected->setText("Переход наверх");
        return;
    }

    bool isDir = item->data(Qt::UserRole).toBool();
    QString fullPath = item->data(Qt::UserRole + 2).toString();
    QString type = m_table->item(row, 1) ? m_table->item(row, 1)->text() : "";
    QString size = m_table->item(row, 2) ? m_table->item(row, 2)->text() : "";
    QString date = m_table->item(row, 3) ? m_table->item(row, 3)->text() : "";
    QString perms = m_table->item(row, 4) ? m_table->item(row, 4)->text() : "";

    m_inspIcon->setPixmap(getFileIcon(isDir, name).pixmap(48, 48));
    m_inspName->setText(name);
    m_inspTypeBadge->setText(isDir ? "Папка" : QFileInfo(name).suffix().toUpper());
    m_inspSizeBadge->setText(size);
    m_inspPathLabel->setText(fullPath);
    m_inspDateVal->setText(date);
    m_inspPermsVal->setText(perms);

    m_btnDownload->setEnabled(!isDir);
    m_btnPreview->setEnabled(!isDir);
    m_btnRename->setEnabled(true);
    m_btnDelete->setEnabled(true);

    m_statusSelected->setText(QString("Выбрано: %1 (%2)").arg(name, isDir ? "Папка" : size));
}

void FileManagerWidget::onItemDoubleClicked(int row, int column)
{
    Q_UNUSED(column);
    if(row < 0 || row >= m_table->rowCount())
        return;

    QTableWidgetItem *item = m_table->item(row, 0);
    if(!item)
        return;

    QString name = item->data(Qt::UserRole + 1).toString();
    if(name == "..")
    {
        navigateUp();
        return;
    }

    bool isDir = item->data(Qt::UserRole).toBool();
    QString fullPath = item->data(Qt::UserRole + 2).toString();

    if(isDir)
    {
        navigateTo(fullPath);
    }
    else
    {
        previewSelected();
    }
}

void FileManagerWidget::onQuickAccessClicked(QListWidgetItem *item)
{
    if(!item)
        return;
    QString path = item->data(Qt::UserRole).toString();
    if(!path.isEmpty())
        navigateTo(path);
}

void FileManagerWidget::onSearchFilterChanged(const QString &filter)
{
    Q_UNUSED(filter);
    populateTable();
}

void FileManagerWidget::showContextMenu(const QPoint &pos)
{
    QTableWidgetItem *item = m_table->itemAt(pos);
    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: #0F172A; color: #F8FAFC; border: 1px solid #1E293B; border-radius: 0px; padding: 4px; }"
        "QMenu::item { padding: 6px 24px 6px 12px; border-radius: 0px; font-size: 11.5px; }"
        "QMenu::item:selected { background-color: #131E35; color: #38BDF8; }");

    if(item)
    {
        int row = item->row();
        m_table->selectRow(row);
        QString name = m_table->item(row, 0)->data(Qt::UserRole + 1).toString();
        bool isDir = m_table->item(row, 0)->data(Qt::UserRole).toBool();
        QString fullPath = m_table->item(row, 0)->data(Qt::UserRole + 2).toString();

        if(name != "..")
        {
            if(!isDir)
            {
                menu.addAction(QIcon(":/svg/download"), "Скачать на ПК", this, &FileManagerWidget::downloadSelected);
                menu.addAction(QIcon(":/svg/eye"), "Быстрый просмотр", this, &FileManagerWidget::previewSelected);
            }
            menu.addAction(QIcon(":/svg/edit"), "Переименовать", this, &FileManagerWidget::renameSelected);
            menu.addAction(QIcon(":/svg/copy"), "Копировать путь", [fullPath]() { QApplication::clipboard()->setText(fullPath); });
            menu.addSeparator();
            menu.addAction(QIcon(":/svg/trash"), "Удалить", this, &FileManagerWidget::deleteSelected);
            menu.addSeparator();
        }
    }

    menu.addAction(QIcon(":/svg/plus"), "Создать папку", this, &FileManagerWidget::createDirectory);
    menu.addAction(QIcon(":/svg/upload"), "Загрузить файл на телефон", this, &FileManagerWidget::uploadFile);
    menu.addAction(QIcon(":/svg/refresh-cw"), "Обновить", this, &FileManagerWidget::refreshList);

    menu.exec(m_table->viewport()->mapToGlobal(pos));
}

void FileManagerWidget::downloadSelected()
{
    int row = m_table->currentRow();
    if(row < 0)
        return;

    QTableWidgetItem *item = m_table->item(row, 0);
    if(!item)
        return;

    QString fileName = item->data(Qt::UserRole + 1).toString();
    QString remotePath = item->data(Qt::UserRole + 2).toString();
    if(remotePath.isEmpty() || item->data(Qt::UserRole).toBool())
        return;

    QString localPath = QFileDialog::getSaveFileName(this, "Сохранить файл на компьютер", QDir::homePath() + "/" + fileName);
    if(localPath.isEmpty())
        return;

    m_statusMsg->setText("Скачивание: " + fileName + "...");
    qApp->processEvents();

    if(m_fileIO.pullFile(remotePath, localPath))
    {
        m_statusMsg->setText("Файл сохранён");
        QMessageBox::information(this, "Успешно", "Файл успешно сохранён на компьютер:\n" + localPath);
    }
    else
    {
        m_statusMsg->setText("Ошибка скачивания");
        QMessageBox::warning(this, "Ошибка", "Не удалось скачать файл с устройства.");
    }
}

void FileManagerWidget::uploadFile()
{
    QString localPath = QFileDialog::getOpenFileName(this, "Выберите файл для загрузки на телефон", QDir::homePath());
    if(localPath.isEmpty())
        return;

    QFileInfo fi(localPath);
    QString remoteTarget = m_currentPath;
    if(!remoteTarget.endsWith('/'))
        remoteTarget += '/';
    remoteTarget += fi.fileName();

    m_statusMsg->setText("Загрузка: " + fi.fileName() + "...");
    qApp->processEvents();

    if(m_fileIO.pushFile(localPath, remoteTarget))
    {
        m_statusMsg->setText("Файл загружен");
        refreshList();
    }
    else
    {
        m_statusMsg->setText("Ошибка загрузки");
        QMessageBox::warning(this, "Ошибка", "Не удалось загрузить файл на устройство.");
    }
}

void FileManagerWidget::previewSelected()
{
    int row = m_table->currentRow();
    if(row < 0)
        return;

    QTableWidgetItem *item = m_table->item(row, 0);
    if(!item)
        return;

    bool isDir = item->data(Qt::UserRole).toBool();
    if(isDir)
        return;

    QString fileName = item->data(Qt::UserRole + 1).toString();
    QString remotePath = item->data(Qt::UserRole + 2).toString();

    m_statusMsg->setText("Чтение: " + fileName + "...");
    qApp->processEvents();

    QByteArray content = m_fileIO.read(remotePath);
    FilePreviewDialog dlg(fileName, content, this);
    dlg.exec();
    m_statusMsg->setText("Готово");
}

void FileManagerWidget::renameSelected()
{
    int row = m_table->currentRow();
    if(row < 0)
        return;

    QTableWidgetItem *item = m_table->item(row, 0);
    if(!item)
        return;

    QString oldName = item->data(Qt::UserRole + 1).toString();
    if(oldName == "..")
        return;

    QString oldPath = item->data(Qt::UserRole + 2).toString();
    bool ok = false;
    QString newName = QInputDialog::getText(this, "Переименование", "Введите новое имя:", QLineEdit::Normal, oldName, &ok);

    if(!ok || newName.trimmed().isEmpty() || newName.trimmed() == oldName)
        return;

    newName = newName.trimmed();
    QString newPath = m_currentPath;
    if(!newPath.endsWith('/'))
        newPath += '/';
    newPath += newName;

    m_statusMsg->setText("Переименование...");
    qApp->processEvents();

    auto reply = m_fileIO.commandQueueWaits(QString("mv \"%1\" \"%2\"").arg(oldPath, newPath));
    if(reply.first)
    {
        m_statusMsg->setText("Переименовано в " + newName);
        refreshList();
    }
    else
    {
        QMessageBox::warning(this, "Ошибка", "Не удалось переименовать:\n" + reply.second);
    }
}

void FileManagerWidget::deleteSelected()
{
    int row = m_table->currentRow();
    if(row < 0)
        return;

    QTableWidgetItem *item = m_table->item(row, 0);
    if(!item)
        return;

    QString name = item->data(Qt::UserRole + 1).toString();
    if(name == "..")
        return;

    QString fullPath = item->data(Qt::UserRole + 2).toString();
    bool isDir = item->data(Qt::UserRole).toBool();

    auto ans = QMessageBox::question(this, "Удаление", QString("Вы уверены, что хотите удалить %1 \"%2\"?").arg(isDir ? "папку" : "файл", name), QMessageBox::Yes | QMessageBox::No);

    if(ans != QMessageBox::Yes)
        return;

    m_statusMsg->setText("Удаление: " + name + "...");
    qApp->processEvents();

    bool success = false;
    if(isDir)
    {
        auto reply = m_fileIO.commandQueueWaits("rm -rf \"" + fullPath + "\"");
        success = reply.first;
    }
    else
    {
        success = m_fileIO.deleteFile(fullPath);
    }

    if(success)
    {
        m_statusMsg->setText("Удалено: " + name);
        refreshList();
    }
    else
    {
        m_statusMsg->setText("Ошибка удаления");
        QMessageBox::warning(this, "Ошибка", "Не удалось удалить выбранный объект.");
    }
}

void FileManagerWidget::createDirectory()
{
    bool ok = false;
    QString dirName = QInputDialog::getText(this, "Новая папка", "Введите имя новой папки:", QLineEdit::Normal, "", &ok);
    if(!ok || dirName.trimmed().isEmpty())
        return;

    dirName = dirName.trimmed();
    QString fullTarget = m_currentPath;
    if(!fullTarget.endsWith('/'))
        fullTarget += '/';
    fullTarget += dirName;

    if(m_fileIO.makeDir(fullTarget))
    {
        m_statusMsg->setText("Папка создана: " + dirName);
        refreshList();
    }
    else
    {
        QMessageBox::warning(this, "Ошибка", "Не удалось создать папку на устройстве.");
    }
}

FileManagerService::FileManagerService(QObject *parent) : Service(DeviceConnectType::ADB, parent)
{
    title = "Проводник ADB";
    active = true;
}

FileManagerService::~FileManagerService()
{
    stop();
}

QString FileManagerService::uuid() const
{
    return IDServiceFileManagerString;
}

PageIndex FileManagerService::targetPage()
{
    return FileManagerPage;
}

QString FileManagerService::widgetIconName()
{
    return "file-manager";
}

bool FileManagerService::canStart()
{
    return Service::canStart();
}

bool FileManagerService::isStarted()
{
    return m_started;
}

bool FileManagerService::isFinish()
{
    return m_finished;
}

bool FileManagerService::start()
{
    sendCheckPull();
    if(!canStart())
        return false;

    m_started = true;
    m_finished = false;

    if(MainWindow::current)
    {
        auto *widget = static_cast<FileManagerWidget *>(MainWindow::current->pageWidget(FileManagerPage));
        if(widget)
        {
            widget->setDevice(mAdbDevice);
            widget->refreshList();
        }
    }

    return true;
}

void FileManagerService::stop()
{
    m_started = false;
    m_finished = true;
}
