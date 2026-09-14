#include "AITranslaterWidget.h"
#include "Services.h"
#include "MainWindow.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

// ──────────────────────────────────────────────────────────────────────────────
// Helpers / constants
// ──────────────────────────────────────────────────────────────────────────────

static constexpr int AT_POLL_INTERVAL_MS = 4000;
static constexpr int AT_POLL_MAX = 120; // 8 minutes max

static const QString AITT_STYLE_HEADER =
    "#at_header {"
    "  background-color: #0B1120;"
    "  border: 1px solid #1E293B;"
    "  border-left: 4px solid #38BDF8;"
    "  border-radius: 0px;"
    "}";

static const QString AITT_STYLE_CARD =
    "QFrame#at_card {"
    "  background-color: #0B1120;"
    "  border: 1px solid #1E293B;"
    "  border-radius: 0px;"
    "}";

static const QString AITT_STYLE_BTN_PRIMARY =
    "QPushButton {"
    "  background-color: #0284C7;"
    "  color: #FFFFFF;"
    "  border: 1px solid #0284C7;"
    "  border-radius: 0px;"
    "  padding: 0px 14px;"
    "  font-size: 11.5px;"
    "  font-weight: 700;"
    "  letter-spacing: 0.5px;"
    "  min-height: 28px;"
    "  max-height: 28px;"
    "}"
    "QPushButton:hover { background-color: #0369A1; border-color: #38BDF8; }"
    "QPushButton:pressed { background-color: #075985; }"
    "QPushButton:disabled { background-color: #1E293B; border-color: #1E293B; color: #475569; }";

static const QString AITT_STYLE_BTN_SEC =
    "QPushButton {"
    "  background-color: #0F172A;"
    "  color: #CBD5E1;"
    "  border: 1px solid #1E293B;"
    "  border-radius: 0px;"
    "  padding: 0px 12px;"
    "  font-size: 11.5px;"
    "  font-weight: 600;"
    "  min-height: 28px;"
    "  max-height: 28px;"
    "}"
    "QPushButton:hover { background-color: #1E293B; border-color: #38BDF8; color: #38BDF8; }"
    "QPushButton:pressed { background-color: #0B1120; }";

static const QString AITT_STYLE_BTN_GREEN =
    "QPushButton {"
    "  background-color: #059669;"
    "  color: #FFFFFF;"
    "  border: 1px solid #059669;"
    "  border-radius: 0px;"
    "  padding: 0px 14px;"
    "  font-size: 11.5px;"
    "  font-weight: 700;"
    "  letter-spacing: 0.5px;"
    "  min-height: 28px;"
    "  max-height: 28px;"
    "}"
    "QPushButton:hover { background-color: #10B981; border-color: #34D399; }"
    "QPushButton:pressed { background-color: #064E3B; }"
    "QPushButton:disabled { background-color: #1E293B; border-color: #1E293B; color: #475569; }";

static const QString AITT_STYLE_COMBO =
    "QComboBox {"
    "  background-color: #070A12;"
    "  color: #CBD5E1;"
    "  border: 1px solid #1E293B;"
    "  border-radius: 0px;"
    "  padding: 3px 8px;"
    "  font-size: 12px;"
    "  min-height: 28px;"
    "  max-height: 28px;"
    "}"
    "QComboBox:hover { border-color: #38BDF8; }"
    "QComboBox::drop-down { border: none; width: 18px; }"
    "QComboBox::down-arrow { image: none; }"
    "QComboBox QAbstractItemView {"
    "  background-color: #0B1120;"
    "  color: #CBD5E1;"
    "  selection-background-color: #0284C7;"
    "  selection-color: #FFFFFF;"
    "  border: 1px solid #1E293B;"
    "}";

static const QString AITT_STYLE_TEXTEDIT =
    "QTextEdit {"
    "  background-color: #070A12;"
    "  color: #E2E8F0;"
    "  border: 1px solid #1E293B;"
    "  border-radius: 0px;"
    "  font-size: 13px;"
    "  padding: 8px;"
    "  selection-background-color: #0284C7;"
    "}"
    "QTextEdit:focus { border: 1px solid #38BDF8; }";

static const QString AITT_STYLE_LINEEDIT =
    "QLineEdit {"
    "  background-color: #070A12;"
    "  color: #E2E8F0;"
    "  border: 1px solid #1E293B;"
    "  border-radius: 0px;"
    "  padding: 3px 8px;"
    "  font-size: 12px;"
    "  min-height: 28px;"
    "  max-height: 28px;"
    "}"
    "QLineEdit:focus { border: 1px solid #38BDF8; }";

// Built-in language list – extended via API when connection available
static const QList<QPair<QString, QString>> kBuiltinLanguages = {
    {"ru", QString::fromUtf8("Русский")},
    {"kk", QString::fromUtf8("Казахский")},
    {"en", QString::fromUtf8("Английский")},
    {"de", QString::fromUtf8("Немецкий")},
    {"fr", QString::fromUtf8("Французский")},
    {"es", QString::fromUtf8("Испанский")},
    {"zh", QString::fromUtf8("Китайский")},
    {"ar", QString::fromUtf8("Арабский")},
    {"tr", QString::fromUtf8("Турецкий")},
    {"pl", QString::fromUtf8("Польский")},
    {"uk", QString::fromUtf8("Украинский")},
    {"ro", QString::fromUtf8("Румынский")},
    {"it", QString::fromUtf8("Итальянский")},
    {"ja", QString::fromUtf8("Японский")},
    {"ko", QString::fromUtf8("Корейский")},
    {"uz", QString::fromUtf8("Узбекский")},
    {"ky", QString::fromUtf8("Кыргызский")},
};

// ──────────────────────────────────────────────────────────────────────────────
// AITranslaterWidget — ctor / dtor
// ──────────────────────────────────────────────────────────────────────────────

AITranslaterWidget::AITranslaterWidget(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet("AITranslaterWidget { background-color: #070A12; }");
    m_languages = kBuiltinLanguages;
    setupUi();
}

AITranslaterWidget::~AITranslaterWidget()
{
    if(m_pollTimer)
    {
        m_pollTimer->stop();
        m_pollTimer->deleteLater();
        m_pollTimer = nullptr;
    }
}

void AITranslaterWidget::setNetwork(Network *net)
{
    m_net = net;
    if(m_net)
    {
        requestLanguages();
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// UI construction
// ──────────────────────────────────────────────────────────────────────────────

void AITranslaterWidget::setupUi()
{
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 16, 18, 16);
    root->setSpacing(12);

    setupHeaderFrame();
    root->addWidget(findChild<QFrame *>("at_header"));

    // Tabs: Text / Document / Async Queue
    QTabWidget *tabs = new QTabWidget(this);
    tabs->setObjectName("at_tabs");
    tabs->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #1E293B; background-color: #0A0E1A; top: -1px; }"
        "QTabBar::tab { background: transparent; color: #64748B; border: none; border-bottom: 2px solid transparent;"
        "               padding: 8px 22px; font-size: 11.5px; font-weight: 700; text-transform: uppercase; letter-spacing: 0.5px; margin-right: 4px; }"
        "QTabBar::tab:selected { background-color: #0B1120; color: #38BDF8; border-bottom: 2px solid #38BDF8; }"
        "QTabBar::tab:hover:!selected { color: #CBD5E1; border-bottom: 2px solid #334155; }");

    // ── Tab 1: Text translation ──
    QWidget *textTab = new QWidget(this);
    QVBoxLayout *textTabLayout = new QVBoxLayout(textTab);
    textTabLayout->setContentsMargins(14, 14, 14, 14);
    textTabLayout->setSpacing(10);
    setupTextTranslatePanel();
    // move children built in setupTextTranslatePanel into textTabLayout
    for(auto *ch : findChildren<QWidget *>("at_textpanel_inner"))
    {
        textTabLayout->addWidget(ch);
        break;
    }
    tabs->addTab(textTab, QIcon(":/svg/message-circle"), QString::fromUtf8("Текст"));

    // ── Tab 2: Document translation ──
    QWidget *docTab = new QWidget(this);
    QVBoxLayout *docTabLayout = new QVBoxLayout(docTab);
    docTabLayout->setContentsMargins(14, 14, 14, 14);
    docTabLayout->setSpacing(10);
    setupDocumentPanel();
    for(auto *ch : findChildren<QWidget *>("at_docpanel_inner"))
    {
        docTabLayout->addWidget(ch);
        break;
    }
    tabs->addTab(docTab, QIcon(":/svg/clipboard"), QString::fromUtf8("Документ"));

    // ── Tab 3: Async queue ──
    QWidget *queueTab = new QWidget(this);
    QVBoxLayout *queueTabLayout = new QVBoxLayout(queueTab);
    queueTabLayout->setContentsMargins(14, 14, 14, 14);
    queueTabLayout->setSpacing(10);
    setupQueuePanel();
    for(auto *ch : findChildren<QWidget *>("at_queuepanel_inner"))
    {
        queueTabLayout->addWidget(ch);
        break;
    }
    tabs->addTab(queueTab, QIcon(":/svg/clock"), QString::fromUtf8("Очередь"));

    root->addWidget(tabs, 1);

    // Status bar
    m_lblStatus = new QLabel(this);
    m_lblStatus->setObjectName("at_statusbar");
    m_lblStatus->setStyleSheet(
        "font-size: 11px; color: #94A3B8; padding: 5px 10px;"
        "background-color: #0B1120; border: 1px solid #1E293B; border-left: 3px solid #38BDF8; border-radius: 0px;");
    m_lblStatus->setText(QString::fromUtf8("Готово к работе."));
    m_lblStatus->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    root->addWidget(m_lblStatus);
}

void AITranslaterWidget::setupHeaderFrame()
{
    QFrame *hdr = new QFrame(this);
    hdr->setObjectName("at_header");
    hdr->setStyleSheet(AITT_STYLE_HEADER);
    QHBoxLayout *hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(14, 10, 14, 10);
    hl->setSpacing(12);

    // Left: icon + title + subtitle
    QVBoxLayout *titleBox = new QVBoxLayout();
    titleBox->setSpacing(2);
    QLabel *titleLbl = new QLabel(QString::fromUtf8("<img src=\":/svg/globe\" width=\"16\" height=\"16\" style=\"vertical-align: middle;\"/>  ИИ-Переводчик документов"), hdr);
    titleLbl->setTextFormat(Qt::RichText);
    titleLbl->setStyleSheet("font-size: 15px; font-weight: bold; color: #38BDF8;");
    QLabel *subLbl = new QLabel(QString::fromUtf8("Перевод текстов и файлов с помощью AI (Gemini). Поддерживаются PDF, DOCX, TXT и другие форматы."), hdr);
    subLbl->setStyleSheet("font-size: 11px; color: #64748B;");
    subLbl->setWordWrap(true);
    titleBox->addWidget(titleLbl);
    titleBox->addWidget(subLbl);
    hl->addLayout(titleBox, 1);

    // Right: refresh languages button
    QPushButton *btnLangs = new QPushButton(QString::fromUtf8("Обновить языки"), hdr);
    btnLangs->setIcon(QIcon(":/svg/refresh-cw"));
    btnLangs->setIconSize(QSize(13, 13));
    btnLangs->setStyleSheet(AITT_STYLE_BTN_SEC);
    btnLangs->setCursor(Qt::PointingHandCursor);
    connect(btnLangs, &QPushButton::clicked, this, &AITranslaterWidget::requestLanguages);
    hl->addWidget(btnLangs, 0, Qt::AlignVCenter);
}

void AITranslaterWidget::setupTextTranslatePanel()
{
    QWidget *inner = new QWidget(this);
    inner->setObjectName("at_textpanel_inner");
    QVBoxLayout *vl = new QVBoxLayout(inner);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(10);

    // ── Language selector row ──
    QHBoxLayout *langRow = new QHBoxLayout();
    langRow->setSpacing(8);

    QLabel *srcLangLbl = new QLabel(QString::fromUtf8("Исходный язык:"), inner);
    srcLangLbl->setStyleSheet("font-size: 12px; color: #94A3B8;");
    langRow->addWidget(srcLangLbl);

    m_cbSrcLang = new QComboBox(inner);
    m_cbSrcLang->setStyleSheet(AITT_STYLE_COMBO);
    m_cbSrcLang->setMinimumWidth(160);
    fillLanguageCombo(m_cbSrcLang, "ru");
    langRow->addWidget(m_cbSrcLang);

    m_btnSwapLang = new QPushButton(QString::fromUtf8("⇄"), inner);
    m_btnSwapLang->setStyleSheet(AITT_STYLE_BTN_SEC);
    m_btnSwapLang->setFixedWidth(36);
    m_btnSwapLang->setToolTip(QString::fromUtf8("Поменять языки местами"));
    m_btnSwapLang->setCursor(Qt::PointingHandCursor);
    connect(
        m_btnSwapLang,
        &QPushButton::clicked,
        this,
        [this]()
        {
            int si = m_cbSrcLang->currentIndex();
            int di = m_cbDstLang->currentIndex();
            m_cbSrcLang->setCurrentIndex(di);
            m_cbDstLang->setCurrentIndex(si);
        });
    langRow->addWidget(m_btnSwapLang);

    QLabel *dstLangLbl = new QLabel(QString::fromUtf8("Целевой язык:"), inner);
    dstLangLbl->setStyleSheet("font-size: 12px; color: #94A3B8;");
    langRow->addWidget(dstLangLbl);

    m_cbDstLang = new QComboBox(inner);
    m_cbDstLang->setStyleSheet(AITT_STYLE_COMBO);
    m_cbDstLang->setMinimumWidth(160);
    fillLanguageCombo(m_cbDstLang, "kk");
    langRow->addWidget(m_cbDstLang);

    langRow->addStretch(1);

    m_btnTranslateText = new QPushButton(QString::fromUtf8("Перевести"), inner);
    m_btnTranslateText->setIcon(QIcon(":/svg/play"));
    m_btnTranslateText->setIconSize(QSize(14, 14));
    m_btnTranslateText->setStyleSheet(AITT_STYLE_BTN_PRIMARY);
    m_btnTranslateText->setMinimumWidth(130);
    m_btnTranslateText->setCursor(Qt::PointingHandCursor);
    connect(m_btnTranslateText, &QPushButton::clicked, this, &AITranslaterWidget::sendTextTranslate);
    langRow->addWidget(m_btnTranslateText);

    vl->addLayout(langRow);

    // ── Text editors: input / output ──
    QSplitter *splitter = new QSplitter(Qt::Horizontal, inner);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(6);

    // Input
    QFrame *inputCard = new QFrame(splitter);
    inputCard->setObjectName("at_card");
    inputCard->setStyleSheet(AITT_STYLE_CARD);
    QVBoxLayout *inputVl = new QVBoxLayout(inputCard);
    inputVl->setContentsMargins(8, 8, 8, 8);
    inputVl->setSpacing(6);
    QHBoxLayout *inputHeader = new QHBoxLayout();
    QLabel *inputTitle = new QLabel(QString::fromUtf8("Исходный текст"), inputCard);
    inputTitle->setStyleSheet("font-size: 12px; font-weight: 600; color: #94A3B8;");
    inputHeader->addWidget(inputTitle);
    inputHeader->addStretch(1);
    QPushButton *btnClearIn = new QPushButton(QString::fromUtf8("Очистить"), inputCard);
    btnClearIn->setStyleSheet(AITT_STYLE_BTN_SEC);
    btnClearIn->setFixedHeight(24);
    btnClearIn->setCursor(Qt::PointingHandCursor);
    connect(btnClearIn, &QPushButton::clicked, this, [this]() { m_editInput->clear(); });
    inputHeader->addWidget(btnClearIn);
    inputVl->addLayout(inputHeader);
    m_editInput = new QTextEdit(inputCard);
    m_editInput->setStyleSheet(AITT_STYLE_TEXTEDIT);
    m_editInput->setPlaceholderText(QString::fromUtf8("Введите текст для перевода..."));
    inputVl->addWidget(m_editInput, 1);
    splitter->addWidget(inputCard);

    // Output
    QFrame *outputCard = new QFrame(splitter);
    outputCard->setObjectName("at_card");
    outputCard->setStyleSheet(AITT_STYLE_CARD);
    QVBoxLayout *outputVl = new QVBoxLayout(outputCard);
    outputVl->setContentsMargins(8, 8, 8, 8);
    outputVl->setSpacing(6);
    QHBoxLayout *outputHeader = new QHBoxLayout();
    QLabel *outputTitle = new QLabel(QString::fromUtf8("Перевод"), outputCard);
    outputTitle->setStyleSheet("font-size: 12px; font-weight: 600; color: #10B981;");
    outputHeader->addWidget(outputTitle);
    outputHeader->addStretch(1);
    QPushButton *btnCopyOut = new QPushButton(QString::fromUtf8("Копировать"), outputCard);
    btnCopyOut->setIcon(QIcon(":/svg/copy"));
    btnCopyOut->setIconSize(QSize(12, 12));
    btnCopyOut->setStyleSheet(AITT_STYLE_BTN_SEC);
    btnCopyOut->setFixedHeight(24);
    btnCopyOut->setCursor(Qt::PointingHandCursor);
    connect(
        btnCopyOut,
        &QPushButton::clicked,
        this,
        [this]()
        {
            QApplication::clipboard()->setText(m_editOutput->toPlainText());
            showStatus(QString::fromUtf8("Перевод скопирован в буфер обмена."));
        });
    outputHeader->addWidget(btnCopyOut);
    outputVl->addLayout(outputHeader);
    m_editOutput = new QTextEdit(outputCard);
    m_editOutput->setStyleSheet(AITT_STYLE_TEXTEDIT);
    m_editOutput->setReadOnly(true);
    m_editOutput->setPlaceholderText(QString::fromUtf8("Результат перевода появится здесь..."));
    outputVl->addWidget(m_editOutput, 1);
    splitter->addWidget(outputCard);

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    vl->addWidget(splitter, 1);

    // Info label
    m_lblTextInfo = new QLabel(inner);
    m_lblTextInfo->setStyleSheet("font-size: 11px; color: #475569;");
    m_lblTextInfo->setAlignment(Qt::AlignRight);
    vl->addWidget(m_lblTextInfo);
}

void AITranslaterWidget::setupDocumentPanel()
{
    QWidget *inner = new QWidget(this);
    inner->setObjectName("at_docpanel_inner");
    QVBoxLayout *vl = new QVBoxLayout(inner);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(12);

    // ── File selector card ──
    QFrame *fileCard = new QFrame(inner);
    fileCard->setObjectName("at_card");
    fileCard->setStyleSheet(AITT_STYLE_CARD);
    QVBoxLayout *fcVl = new QVBoxLayout(fileCard);
    fcVl->setContentsMargins(14, 12, 14, 12);
    fcVl->setSpacing(10);

    QLabel *fcTitle = new QLabel(QString::fromUtf8("Выбор документа"), fileCard);
    fcTitle->setStyleSheet("font-size: 13px; font-weight: 600; color: #38BDF8;");
    fcVl->addWidget(fcTitle);

    QHBoxLayout *fileRow = new QHBoxLayout();
    fileRow->setSpacing(8);
    m_editDocPath = new QLineEdit(fileCard);
    m_editDocPath->setStyleSheet(AITT_STYLE_LINEEDIT);
    m_editDocPath->setPlaceholderText(QString::fromUtf8("Путь к файлу (PDF, DOCX, TXT, XLSX, ODT...)"));
    m_editDocPath->setReadOnly(true);
    fileRow->addWidget(m_editDocPath, 1);
    m_btnChooseDoc = new QPushButton(QString::fromUtf8("Обзор..."), fileCard);
    m_btnChooseDoc->setIcon(QIcon(":/svg/folder"));
    m_btnChooseDoc->setIconSize(QSize(14, 14));
    m_btnChooseDoc->setStyleSheet(AITT_STYLE_BTN_SEC);
    m_btnChooseDoc->setCursor(Qt::PointingHandCursor);
    m_btnChooseDoc->setMinimumWidth(100);
    connect(
        m_btnChooseDoc,
        &QPushButton::clicked,
        this,
        [this]()
        {
            QString path = QFileDialog::getOpenFileName(this, QString::fromUtf8("Выбрать документ для перевода"), QDir::homePath(), QString::fromUtf8("Документы (*.pdf *.docx *.doc *.txt *.xlsx *.xls *.odt *.ods *.pptx *.ppt *.rtf);;Все файлы (*)"));
            if(path.isEmpty())
                return;
            QFile f(path);
            if(!f.open(QIODevice::ReadOnly))
            {
                showStatus(QString::fromUtf8("Не удалось открыть файл: ") + path, true);
                return;
            }
            m_pendingDocBytes = f.readAll();
            f.close();
            m_pendingDocName = QFileInfo(path).fileName();
            m_editDocPath->setText(path);
            showStatus(QString::fromUtf8("Файл загружен: %1 (%2 байт)").arg(m_pendingDocName).arg(m_pendingDocBytes.size()));
        });
    fileRow->addWidget(m_btnChooseDoc);
    fcVl->addLayout(fileRow);

    // Language + format row
    QHBoxLayout *optRow = new QHBoxLayout();
    optRow->setSpacing(10);

    QLabel *srcL = new QLabel(QString::fromUtf8("Исходный:"), fileCard);
    srcL->setStyleSheet("font-size: 12px; color: #94A3B8;");
    optRow->addWidget(srcL);
    m_cbDocSrcLang = new QComboBox(fileCard);
    m_cbDocSrcLang->setStyleSheet(AITT_STYLE_COMBO);
    m_cbDocSrcLang->setMinimumWidth(140);
    fillLanguageCombo(m_cbDocSrcLang, "ru");
    optRow->addWidget(m_cbDocSrcLang);

    QLabel *arrL = new QLabel(QString::fromUtf8("→"), fileCard);
    arrL->setStyleSheet("font-size: 14px; color: #64748B;");
    optRow->addWidget(arrL);

    QLabel *dstL = new QLabel(QString::fromUtf8("Перевести в:"), fileCard);
    dstL->setStyleSheet("font-size: 12px; color: #94A3B8;");
    optRow->addWidget(dstL);
    m_cbDocDstLang = new QComboBox(fileCard);
    m_cbDocDstLang->setStyleSheet(AITT_STYLE_COMBO);
    m_cbDocDstLang->setMinimumWidth(140);
    fillLanguageCombo(m_cbDocDstLang, "kk");
    optRow->addWidget(m_cbDocDstLang);

    QLabel *fmtL = new QLabel(QString::fromUtf8("Формат:"), fileCard);
    fmtL->setStyleSheet("font-size: 12px; color: #94A3B8;");
    optRow->addWidget(fmtL);
    m_cbTargetFmt = new QComboBox(fileCard);
    m_cbTargetFmt->setStyleSheet(AITT_STYLE_COMBO);
    m_cbTargetFmt->setMinimumWidth(90);
    m_cbTargetFmt->addItem("docx");
    m_cbTargetFmt->addItem("pdf");
    m_cbTargetFmt->addItem("txt");
    m_cbTargetFmt->addItem("odt");
    optRow->addWidget(m_cbTargetFmt);

    optRow->addStretch(1);
    fcVl->addLayout(optRow);

    // Action buttons
    QHBoxLayout *actRow = new QHBoxLayout();
    actRow->setSpacing(8);
    m_btnTranslateDoc = new QPushButton(QString::fromUtf8("Перевести сейчас"), fileCard);
    m_btnTranslateDoc->setIcon(QIcon(":/svg/play"));
    m_btnTranslateDoc->setIconSize(QSize(14, 14));
    m_btnTranslateDoc->setStyleSheet(AITT_STYLE_BTN_PRIMARY);
    m_btnTranslateDoc->setMinimumWidth(160);
    m_btnTranslateDoc->setCursor(Qt::PointingHandCursor);
    connect(m_btnTranslateDoc, &QPushButton::clicked, this, [this]() { sendDocumentTranslate(false); });
    actRow->addWidget(m_btnTranslateDoc);

    m_btnTranslateDocAsync = new QPushButton(QString::fromUtf8("В очередь (async)"), fileCard);
    m_btnTranslateDocAsync->setIcon(QIcon(":/svg/clock"));
    m_btnTranslateDocAsync->setIconSize(QSize(14, 14));
    m_btnTranslateDocAsync->setStyleSheet(AITT_STYLE_BTN_GREEN);
    m_btnTranslateDocAsync->setMinimumWidth(160);
    m_btnTranslateDocAsync->setCursor(Qt::PointingHandCursor);
    connect(m_btnTranslateDocAsync, &QPushButton::clicked, this, [this]() { sendDocumentTranslate(true); });
    actRow->addWidget(m_btnTranslateDocAsync);

    actRow->addStretch(1);
    fcVl->addLayout(actRow);

    vl->addWidget(fileCard);

    // Progress
    m_progressDoc = new QProgressBar(inner);
    m_progressDoc->setStyleSheet(
        "QProgressBar { background-color: #0B1120; border: 1px solid #1E293B; border-radius: 0px; height: 6px; text-align: center; color: transparent; }"
        "QProgressBar::chunk { background-color: #38BDF8; }");
    m_progressDoc->setTextVisible(false);
    m_progressDoc->setRange(0, 0);
    m_progressDoc->setFixedHeight(6);
    m_progressDoc->setVisible(false);
    vl->addWidget(m_progressDoc);

    // Info
    m_lblDocInfo = new QLabel(inner);
    m_lblDocInfo->setStyleSheet("font-size: 11px; color: #64748B;");
    m_lblDocInfo->setAlignment(Qt::AlignLeft);
    m_lblDocInfo->setWordWrap(true);
    vl->addWidget(m_lblDocInfo);

    // Hint card
    QFrame *hintCard = new QFrame(inner);
    hintCard->setObjectName("at_card");
    hintCard->setStyleSheet(
        "QFrame#at_card { background-color: #0B1120; border: 1px solid #1E293B; border-left: 3px solid #F59E0B; border-radius: 0px; }");
    QVBoxLayout *hintVl = new QVBoxLayout(hintCard);
    hintVl->setContentsMargins(14, 10, 14, 10);
    hintVl->setSpacing(4);
    QLabel *hintTitle = new QLabel(QString::fromUtf8("Рекомендации"), hintCard);
    hintTitle->setStyleSheet("font-size: 11.5px; font-weight: 700; color: #F59E0B; text-transform: uppercase;");
    hintVl->addWidget(hintTitle);
    QLabel *hintText = new QLabel(
        QString::fromUtf8(
            "• Для документов до 2 МБ используйте синхронный перевод.\n"
            "• Для больших файлов используйте асинхронный режим и отслеживайте прогресс на вкладке «Очередь».\n"
            "• Поддерживаемые форматы: PDF, DOCX, DOC, TXT, XLSX, ODS, ODT, PPTX, RTF."),
        hintCard);
    hintText->setStyleSheet("font-size: 11.5px; color: #94A3B8;");
    hintText->setWordWrap(true);
    hintVl->addWidget(hintText);
    vl->addWidget(hintCard);

    vl->addStretch(1);
}

void AITranslaterWidget::setupQueuePanel()
{
    QWidget *inner = new QWidget(this);
    inner->setObjectName("at_queuepanel_inner");
    QVBoxLayout *vl = new QVBoxLayout(inner);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(10);

    // Controls row
    QHBoxLayout *ctrlRow = new QHBoxLayout();
    ctrlRow->setSpacing(8);
    QLabel *queueTitle = new QLabel(QString::fromUtf8("Задачи перевода документов"), inner);
    queueTitle->setStyleSheet("font-size: 12px; font-weight: 700; color: #38BDF8; text-transform: uppercase;");
    ctrlRow->addWidget(queueTitle);
    ctrlRow->addStretch(1);
    m_btnRefreshQueue = new QPushButton(QString::fromUtf8("Обновить"), inner);
    m_btnRefreshQueue->setIcon(QIcon(":/svg/refresh-cw"));
    m_btnRefreshQueue->setIconSize(QSize(13, 13));
    m_btnRefreshQueue->setStyleSheet(AITT_STYLE_BTN_SEC);
    m_btnRefreshQueue->setCursor(Qt::PointingHandCursor);
    connect(m_btnRefreshQueue, &QPushButton::clicked, this, &AITranslaterWidget::pollAsyncTask);
    ctrlRow->addWidget(m_btnRefreshQueue);
    vl->addLayout(ctrlRow);

    // Table
    m_queueTable = new QTableWidget(0, 6, inner);
    m_queueTable->setStyleSheet(
        "QTableWidget { background-color: #070A12; color: #CBD5E1; border: 1px solid #1E293B;"
        "  border-radius: 0px; gridline-color: #1E293B; font-size: 12px; }"
        "QTableWidget::item { padding: 6px 10px; border-bottom: 1px solid #1E293B; }"
        "QTableWidget::item:selected { background-color: #0284C7; color: #FFFFFF; }"
        "QHeaderView::section { background-color: #0B1120; color: #94A3B8; border: none;"
        "  border-bottom: 2px solid #1E293B; padding: 6px 8px; font-size: 11px; font-weight: 700; text-transform: uppercase; }");
    m_queueTable->setHorizontalHeaderLabels({QString::fromUtf8("Task ID"), QString::fromUtf8("Файл"), QString::fromUtf8("Языки"), QString::fromUtf8("Статус"), QString::fromUtf8("Прогресс"), QString::fromUtf8("Действие")});
    m_queueTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_queueTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_queueTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_queueTable->verticalHeader()->setVisible(false);
    m_queueTable->verticalHeader()->setDefaultSectionSize(36);
    m_queueTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_queueTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_queueTable->setAlternatingRowColors(false);
    vl->addWidget(m_queueTable, 1);

    QLabel *queueHint = new QLabel(QString::fromUtf8("Для отслеживания используйте кнопку «Обновить». Когда задача завершится, нажмите «Скачать»."), inner);
    queueHint->setStyleSheet("font-size: 11px; color: #475569;");
    queueHint->setWordWrap(true);
    vl->addWidget(queueHint);
}

// ──────────────────────────────────────────────────────────────────────────────
// Language combo helpers
// ──────────────────────────────────────────────────────────────────────────────

void AITranslaterWidget::fillLanguageCombo(QComboBox *cb, const QString &selectCode)
{
    cb->blockSignals(true);
    cb->clear();
    int selIdx = 0;
    for(int i = 0; i < m_languages.size(); ++i)
    {
        const auto &p = m_languages[i];
        cb->addItem(QString("%1 (%2)").arg(p.second, p.first), p.first);
        if(p.first == selectCode)
            selIdx = i;
    }
    cb->setCurrentIndex(selIdx);
    cb->blockSignals(false);
}

QString AITranslaterWidget::selectedSourceLang() const
{
    return m_cbSrcLang ? m_cbSrcLang->currentData().toString() : "ru";
}

QString AITranslaterWidget::selectedTargetLang() const
{
    return m_cbDstLang ? m_cbDstLang->currentData().toString() : "kk";
}

// ──────────────────────────────────────────────────────────────────────────────
// UI state helpers
// ──────────────────────────────────────────────────────────────────────────────

void AITranslaterWidget::setUiBusy(bool busy)
{
    if(m_btnTranslateText)
        m_btnTranslateText->setEnabled(!busy);
    if(m_btnTranslateDoc)
        m_btnTranslateDoc->setEnabled(!busy);
    if(m_btnTranslateDocAsync)
        m_btnTranslateDocAsync->setEnabled(!busy);
    if(m_progressDoc)
        m_progressDoc->setVisible(busy);
}

void AITranslaterWidget::showStatus(const QString &msg, bool error)
{
    if(!m_lblStatus)
        return;
    m_lblStatus->setText(msg);
    if(error)
        m_lblStatus->setStyleSheet("font-size: 11.5px; color: #F87171; padding: 4px 8px; background-color: #2D0B0B; border: 1px solid #7F1D1D; border-radius: 0px;");
    else
        m_lblStatus->setStyleSheet("font-size: 11.5px; color: #94A3B8; padding: 4px 8px; background-color: #0B1120; border: 1px solid #1E293B; border-radius: 0px;");
}

void AITranslaterWidget::clearTextResults()
{
    if(m_editOutput)
        m_editOutput->clear();
    if(m_lblTextInfo)
        m_lblTextInfo->clear();
}

// ──────────────────────────────────────────────────────────────────────────────
// Network: request languages list
// ──────────────────────────────────────────────────────────────────────────────

void AITranslaterWidget::requestLanguages()
{
    if(!m_net)
    {
        showStatus(QString::fromUtf8("Нет соединения с сетью."), true);
        return;
    }

    Network *net = new Network(*m_net);
    connect(
        net,
        &Network::sPullServiceUUID,
        this,
        [this, net](const QJsonObject resp, const QString &, ServiceOperation so, bool ok)
        {
            net->deleteLater();
            if(!ok)
            {
                showStatus(QString::fromUtf8("Не удалось получить список языков."), true);
                return;
            }
            const QJsonObject result = resp.contains("result") && resp["result"].isObject() ? resp["result"].toObject() : resp;

            QJsonArray langs;
            if(result.contains("languages_info") && result["languages_info"].isObject() && result["languages_info"].toObject().contains("languages"))
                langs = result["languages_info"].toObject()["languages"].toArray();
            else if(result.contains("languages") && result["languages"].isArray())
                langs = result["languages"].toArray();
            else if(result.contains("data") && result["data"].isObject() && result["data"].toObject().contains("languages"))
                langs = result["data"].toObject()["languages"].toArray();
            else if(result.contains("data") && result["data"].isArray())
                langs = result["data"].toArray();

            if(langs.isEmpty())
            {
                showStatus(QString::fromUtf8("Список языков пуст. Используется встроенный список."));
                return;
            }
            m_languages.clear();
            for(const QJsonValue &v : langs)
            {
                QJsonObject lo = v.toObject();
                QString code = lo["code"].toString();
                QString name = lo["name"].toString();
                if(name.isEmpty())
                    name = lo["name_ru"].toString();
                if(name.isEmpty())
                    name = lo["name_en"].toString();
                if(!code.isEmpty() && !name.isEmpty())
                    m_languages.append({code, name});
            }
            // Refresh all combos
            QString curSrc = m_cbSrcLang ? m_cbSrcLang->currentData().toString() : "ru";
            QString curDst = m_cbDstLang ? m_cbDstLang->currentData().toString() : "kk";
            fillLanguageCombo(m_cbSrcLang, curSrc);
            fillLanguageCombo(m_cbDstLang, curDst);
            fillLanguageCombo(m_cbDocSrcLang, curSrc);
            fillLanguageCombo(m_cbDocDstLang, curDst);
            showStatus(QString::fromUtf8("Список языков обновлён: %1 языков.").arg(m_languages.size()));
        });

    QJsonObject req;
    net->pullServiceUUID(IDServiceAITranslaterString, req, ServiceOperation::Get);
}

// ──────────────────────────────────────────────────────────────────────────────
// Network: translate text
// ──────────────────────────────────────────────────────────────────────────────

void AITranslaterWidget::sendTextTranslate()
{
    if(!m_net)
    {
        showStatus(QString::fromUtf8("Нет соединения с сетью."), true);
        return;
    }

    QString text = m_editInput ? m_editInput->toPlainText().trimmed() : QString();
    if(text.isEmpty())
    {
        showStatus(QString::fromUtf8("Введите текст для перевода."), true);
        return;
    }

    QString srcLang = m_cbSrcLang ? m_cbSrcLang->currentData().toString() : "ru";
    QString dstLang = m_cbDstLang ? m_cbDstLang->currentData().toString() : "kk";

    clearTextResults();
    setUiBusy(true);
    showStatus(QString::fromUtf8("Перевод текста... Пожалуйста, подождите."));

    Network *net = new Network(*m_net);
    connect(
        net,
        &Network::sPullServiceUUID,
        this,
        [this, net](const QJsonObject resp, const QString &, ServiceOperation, bool ok)
        {
            net->deleteLater();
            setUiBusy(false);
            const QJsonObject result = resp.contains("result") && resp["result"].isObject() ? resp["result"].toObject() : resp;
            if(!ok || (!result["success"].isUndefined() && !result["success"].toBool()))
            {
                QString err = result["error"].toString();
                showStatus(QString::fromUtf8("Ошибка: ") + (err.isEmpty() ? QString::fromUtf8("не удалось выполнить перевод текста.") : err), true);
                return;
            }
            QString translation = result["translation"].toString();
            if(translation.isEmpty() && result.contains("translation_base64"))
            {
                QByteArray b64 = result["translation_base64"].toString().toLatin1();
                translation = QString::fromUtf8(QByteArray::fromBase64(b64));
            }
            if(m_editOutput)
                m_editOutput->setPlainText(translation);

            QString provider = result["provider"].toString();
            double duration = result["duration_seconds"].toDouble();
            QString srcL = result["source_lang"].toString();
            QString dstL = result["target_lang"].toString();
            if(m_lblTextInfo)
                m_lblTextInfo->setText(QString::fromUtf8("Провайдер: %1 | %2→%3 | %.2f с").arg(provider, srcL, dstL).arg(duration));
            showStatus(QString::fromUtf8("Текст успешно переведён."));
        });

    QJsonObject svc;
    svc["action"] = "translate_text";
    svc["text"] = text;
    svc["source_lang"] = srcLang;
    svc["target_lang"] = dstLang;
    net->pullServiceUUID(IDServiceAITranslaterString, svc, ServiceOperation::Set);
}

// ──────────────────────────────────────────────────────────────────────────────
// Network: translate document (sync or async)
// ──────────────────────────────────────────────────────────────────────────────

void AITranslaterWidget::sendDocumentTranslate(bool async)
{
    if(!m_net)
    {
        showStatus(QString::fromUtf8("Нет соединения с сетью."), true);
        return;
    }
    if(m_pendingDocBytes.isEmpty() || m_pendingDocName.isEmpty())
    {
        showStatus(QString::fromUtf8("Сначала выберите файл документа."), true);
        return;
    }

    QString srcLang = m_cbDocSrcLang ? m_cbDocSrcLang->currentData().toString() : "ru";
    QString dstLang = m_cbDocDstLang ? m_cbDocDstLang->currentData().toString() : "kk";
    QString fmt = m_cbTargetFmt ? m_cbTargetFmt->currentText() : "docx";

    setUiBusy(true);
    if(m_lblDocInfo)
        m_lblDocInfo->setText(QString::fromUtf8("Отправка документа на сервер... (%1 байт)").arg(m_pendingDocBytes.size()));
    showStatus(async ? QString::fromUtf8("Документ отправлен в очередь перевода...") : QString::fromUtf8("Синхронный перевод документа... Подождите."));

    Network *net = new Network(*m_net);
    // Allow up to 5 minutes for large document translation
    net->setTimeout(300000);

    connect(
        net,
        &Network::sPullServiceUUID,
        this,
        [this, net, async](const QJsonObject resp, const QString &, ServiceOperation, bool ok)
        {
            net->deleteLater();
            setUiBusy(false);

            const QJsonObject result = resp.contains("result") && resp["result"].isObject() ? resp["result"].toObject() : resp;
            if(!ok || (!result["success"].isUndefined() && !result["success"].toBool()))
            {
                QString err = result["error"].toString();
                showStatus(QString::fromUtf8("Ошибка: ") + (err.isEmpty() ? QString::fromUtf8("сервер не вернул ответ или соединение прервано.") : err), true);
                if(m_lblDocInfo)
                    m_lblDocInfo->setText(err.isEmpty() ? QString::fromUtf8("Ошибка передачи документа.") : err);
                return;
            }

            if(async)
            {
                // Async: got task_id
                QJsonObject taskObj = result.contains("task") && result["task"].isObject() ? result["task"].toObject() : result;
                m_asyncTaskId = result.contains("task_id") ? result["task_id"].toString() : taskObj["task_id"].toString();
                QString status = result.contains("status") ? result["status"].toString() : taskObj["status"].toString();
                if(status.isEmpty()) status = "queued";

                QString srcL = result.contains("source_lang") ? result["source_lang"].toString() : taskObj["source_lang"].toString();
                QString dstL = result.contains("target_lang") ? result["target_lang"].toString() : taskObj["target_lang"].toString();

                if(m_lblDocInfo)
                    m_lblDocInfo->setText(QString::fromUtf8("Задача принята. ID: %1 | Статус: %2").arg(m_asyncTaskId, status));
                showStatus(QString::fromUtf8("Задача #%1 добавлена в очередь. Отслеживайте прогресс на вкладке «Очередь».").arg(m_asyncTaskId));

                // Add row to queue table
                if(m_queueTable && !m_asyncTaskId.isEmpty())
                {
                    int row = m_queueTable->rowCount();
                    m_queueTable->insertRow(row);
                    m_queueTable->setItem(row, 0, new QTableWidgetItem(m_asyncTaskId));
                    m_queueTable->setItem(row, 1, new QTableWidgetItem(m_pendingDocName));
                    m_queueTable->setItem(row, 2, new QTableWidgetItem(QString("%1 → %2").arg(srcL, dstL)));
                    m_queueTable->setItem(row, 3, new QTableWidgetItem(status));
                    m_queueTable->setItem(row, 4, new QTableWidgetItem("—"));

                    QPushButton *dlBtn = new QPushButton(QString::fromUtf8("Скачать"), m_queueTable);
                    dlBtn->setIcon(QIcon(":/svg/download"));
                    dlBtn->setIconSize(QSize(13, 13));
                    dlBtn->setEnabled(false);
                    dlBtn->setStyleSheet(AITT_STYLE_BTN_PRIMARY);
                    QString capturedTaskId = m_asyncTaskId;
                    connect(
                        dlBtn,
                        &QPushButton::clicked,
                        this,
                        [this, capturedTaskId]()
                        {
                            // Build get request to fetch completed file
                            if(!m_net)
                                return;
                            Network *getNet = new Network(*m_net);
                            connect(
                                getNet,
                                &Network::sPullServiceUUID,
                                this,
                                [this, getNet](const QJsonObject resp2, const QString &, ServiceOperation, bool ok2)
                                {
                                    getNet->deleteLater();
                                    if(!ok2)
                                    {
                                        showStatus(QString::fromUtf8("Не удалось получить файл задачи."), true);
                                        return;
                                    }
                                    const QJsonObject r2 = resp2.contains("result") && resp2["result"].isObject() ? resp2["result"].toObject() : resp2;
                                    if(!r2["success"].toBool())
                                    {
                                        QString err = r2["error"].toString();
                                        showStatus(QString::fromUtf8("Ошибка: ") + (err.isEmpty() ? QString::fromUtf8("файл ещё не готов или произошла ошибка.") : err), true);
                                        return;
                                    }
                                    QString b64 = r2["file_base64"].toString();
                                    QString fname = r2["filename"].toString();
                                    if(b64.isEmpty())
                                    {
                                        showStatus(QString::fromUtf8("Сервер вернул пустой файл."), true);
                                        return;
                                    }
                                    QByteArray fileData = QByteArray::fromBase64(b64.toLatin1());
                                    QString savePath = QFileDialog::getSaveFileName(this, QString::fromUtf8("Сохранить переведённый документ"), QDir::homePath() + "/" + fname);
                                    if(savePath.isEmpty())
                                        return;
                                    QFile out(savePath);
                                    if(out.open(QIODevice::WriteOnly))
                                    {
                                        out.write(fileData);
                                        out.close();
                                        showStatus(QString::fromUtf8("Файл сохранён: ") + savePath);
                                    }
                                    else
                                        showStatus(QString::fromUtf8("Не удалось сохранить файл."), true);
                                });
                            QJsonObject svcGet;
                            svcGet["task_id"] = capturedTaskId;
                            svcGet["include_file"] = true;
                            getNet->pullServiceUUID(IDServiceAITranslaterString, svcGet, ServiceOperation::Get);
                        });
                    m_queueTable->setCellWidget(row, 5, dlBtn);
                    m_queueTable->verticalHeader()->setSectionResizeMode(row, QHeaderView::Fixed);
                    m_queueTable->setRowHeight(row, 36);
                }
            }
            else
            {
                // Sync: got translated file in base64
                QString b64 = result["file_base64"].toString();
                QString fname = result["filename"].toString();
                int units = result["units_total"].toInt();
                int unitsOk = result["units_translated"].toInt();

                if(b64.isEmpty())
                {
                    showStatus(QString::fromUtf8("Сервер вернул пустой результат."), true);
                    return;
                }

                QByteArray fileData = QByteArray::fromBase64(b64.toLatin1());
                QString savePath = QFileDialog::getSaveFileName(this, QString::fromUtf8("Сохранить переведённый документ"), QDir::homePath() + "/" + fname, QString::fromUtf8("Документы (*.docx *.pdf *.txt *.odt);;Все файлы (*)"));

                if(savePath.isEmpty())
                {
                    showStatus(QString::fromUtf8("Сохранение отменено."));
                    return;
                }

                QFile out(savePath);
                if(!out.open(QIODevice::WriteOnly))
                {
                    showStatus(QString::fromUtf8("Не удалось создать файл: ") + savePath, true);
                    return;
                }
                out.write(fileData);
                out.close();

                if(m_lblDocInfo)
                    m_lblDocInfo->setText(QString::fromUtf8("Переведено фрагментов: %1 из %2 | Файл: %3 (%4 байт)").arg(unitsOk).arg(units).arg(fname).arg(fileData.size()));
                showStatus(QString::fromUtf8("Документ переведён и сохранён: ") + savePath);
            }
        });

    QJsonObject svc;
    svc["filename"] = m_pendingDocName;
    svc["file_base64"] = QString::fromLatin1(m_pendingDocBytes.toBase64());
    svc["source_lang"] = srcLang;
    svc["target_lang"] = dstLang;
    svc["target_format"] = fmt;
    if(async)
        svc["async"] = true;

    net->pullServiceUUID(IDServiceAITranslaterString, svc, ServiceOperation::Set);
}

// ──────────────────────────────────────────────────────────────────────────────
// Network: poll async task status
// ──────────────────────────────────────────────────────────────────────────────

void AITranslaterWidget::pollAsyncTask()
{
    if(m_asyncTaskId.isEmpty())
    {
        showStatus(QString::fromUtf8("Нет активной асинхронной задачи для проверки."));
        return;
    }
    if(!m_net)
        return;

    showStatus(QString::fromUtf8("Проверка статуса задачи #%1...").arg(m_asyncTaskId));

    Network *net = new Network(*m_net);
    connect(
        net,
        &Network::sPullServiceUUID,
        this,
        [this, net](const QJsonObject resp, const QString &, ServiceOperation, bool ok)
        {
            net->deleteLater();
            if(!ok)
            {
                showStatus(QString::fromUtf8("Ошибка проверки статуса задачи."), true);
                return;
            }
            const QJsonObject result = resp.contains("result") && resp["result"].isObject() ? resp["result"].toObject() : resp;
            QJsonObject taskObj = result.contains("task") && result["task"].isObject() ? result["task"].toObject() : result;

            QString status = taskObj.contains("status") ? taskObj["status"].toString() : result["status"].toString();
            int progress = taskObj.contains("progress") ? taskObj["progress"].toInt(0) : result["progress"].toInt(0);

            // Update table row if exists
            if(m_queueTable)
            {
                for(int r = 0; r < m_queueTable->rowCount(); ++r)
                {
                    auto *idItem = m_queueTable->item(r, 0);
                    if(idItem && idItem->text() == m_asyncTaskId)
                    {
                        m_queueTable->item(r, 3)->setText(status);
                        m_queueTable->item(r, 4)->setText(progress > 0 ? QString("%1%").arg(progress) : (status == "completed" ? "100%" : "—"));

                        if(status == "completed")
                        {
                            auto *dlBtn = qobject_cast<QPushButton *>(m_queueTable->cellWidget(r, 5));
                            if(dlBtn)
                                dlBtn->setEnabled(true);
                            showStatus(QString::fromUtf8("Задача #%1 завершена! Нажмите «Скачать».").arg(m_asyncTaskId));
                        }
                        break;
                    }
                }
            }

            if(status != "completed" && status != "failed")
                showStatus(QString::fromUtf8("Статус #%1: %2 (%3%)").arg(m_asyncTaskId, status).arg(progress));
            else if(status == "failed")
            {
                QString err = result.contains("error") ? result["error"].toString() : taskObj["error"].toString();
                showStatus(QString::fromUtf8("Ошибка: Задача #%1 завершилась с ошибкой: %2").arg(m_asyncTaskId, err.isEmpty() ? QString::fromUtf8("сбой") : err), true);
            }
        });

    QJsonObject svc;
    svc["task_id"] = m_asyncTaskId;
    svc["include_file"] = false;
    net->pullServiceUUID(IDServiceAITranslaterString, svc, ServiceOperation::Get);
}

// ══════════════════════════════════════════════════════════════════════════════
//  AITranslaterService  —  Service shell
// ══════════════════════════════════════════════════════════════════════════════

struct ATSInternalData
{
    bool started = false;
    bool finished = false;
};

AITranslaterService::AITranslaterService(QObject *parent) : Service(DeviceConnectType::None, parent), mInternal(new ATSInternalData)
{
    title = QString::fromUtf8("Переводчик документов (ИИ)");
    active = true;
}

AITranslaterService::~AITranslaterService()
{
    stop();
    delete mInternal;
}

QString AITranslaterService::uuid() const
{
    return IDServiceAITranslaterString;
}

PageIndex AITranslaterService::targetPage()
{
    return AITranslaterPage;
}

QString AITranslaterService::widgetIconName()
{
    return "ai-translator";
}

bool AITranslaterService::canStart()
{
    return true; // DeviceConnectType::None — no ADB required
}

bool AITranslaterService::isStarted()
{
    return mInternal && mInternal->started;
}

bool AITranslaterService::isFinish()
{
    return mInternal && mInternal->finished;
}

bool AITranslaterService::start()
{
    if(mInternal)
    {
        mInternal->started = true;
        mInternal->finished = false;
    }

    if(MainWindow::current)
    {
        auto *widget = static_cast<AITranslaterWidget *>(MainWindow::current->pageWidget(AITranslaterPage));
        if(widget)
            widget->setNetwork(&MainWindow::current->network);
    }

    return true;
}

void AITranslaterService::stop()
{
    if(mInternal)
    {
        mInternal->started = false;
        mInternal->finished = true;
    }
}
