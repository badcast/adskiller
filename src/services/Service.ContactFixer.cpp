#include "ContactFixerWidget.h"
#include "Services.h"
#include "mainwindow.h"

#include "NumberPreview.h"
#include "vcard.h"
#include "text_io.h"

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
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QTableWidget>
#include <QTextEdit>
#include <QTextStream>
#include <QVBoxLayout>

#include <fstream>
#include <sstream>

struct ContactFixerPrivate
{
    std::vector<vCard> vcards;
};

ContactFixerWidget::ContactFixerWidget(QWidget *parent) : QWidget(parent), d(new ContactFixerPrivate)
{
    setupUi();
}

ContactFixerWidget::~ContactFixerWidget()
{
    delete d;
    d = nullptr;
}

void ContactFixerWidget::setDevice(const AdbDevice &device)
{
    m_device = device;
    if(!device.devId.isEmpty())
    {
        m_fileIO.connect(device.devId);
    }
    updateDeviceUi();

    if(m_autoLoadOnConnect && !device.devId.isEmpty())
    {
        m_autoLoadOnConnect = false;
        QTimer::singleShot(300, this, &ContactFixerWidget::loadFromDevice);
    }
}

void ContactFixerWidget::updateDeviceUi()
{
    if(!m_lblDeviceStatus)
        return;

    if(!m_device.devId.isEmpty())
    {
        QString name = !m_device.marketingName.isEmpty() ? m_device.marketingName : (!m_device.displayName.isEmpty() ? m_device.displayName : (!m_device.model.isEmpty() ? m_device.model : m_device.devId));
        m_lblDeviceStatus->setText(name + " (Подключен)");
        m_lblDeviceStatus->setStyleSheet("background-color: #064E3B; color: #34D399; font-size: 12px; font-weight: bold; border: 1px solid #059669; border-radius: 0px; padding: 6px 12px;");
        if(m_btnConnectDevice)
        {
            m_btnConnectDevice->setText("Сменить устройство");
            m_btnConnectDevice->setIcon(QIcon(":/svg/refresh-cw"));
            m_btnConnectDevice->setIconSize(QSize(13, 13));
        }
    }
    else
    {
        m_lblDeviceStatus->setText("Телефон не подключен");
        m_lblDeviceStatus->setStyleSheet("background-color: #1E293B; color: #94A3B8; font-size: 12px; font-weight: 500; border: 1px solid #334155; border-radius: 0px; padding: 6px 12px;");
        if(m_btnConnectDevice)
        {
            m_btnConnectDevice->setText("Подключить ADB");
            m_btnConnectDevice->setIcon(QIcon(":/svg/wifi"));
            m_btnConnectDevice->setIconSize(QSize(13, 13));
        }
    }
}

void ContactFixerWidget::requestDeviceConnect()
{
    // 1. First check if any authorized ADB device is already plugged in
    QList<AdbDevice> devices = Adb::getDevices();
    for(const auto &dev : devices)
    {
        if(Adb::deviceStatus(dev.devId) == DEVICE)
        {
            setDevice(dev);
            if(MainWindow::current)
            {
                MainWindow::current->connectPhone.isAuthed = true;
                MainWindow::current->connectPhone.adbDevice = dev;
                if(ServiceProvider::currentService())
                    ServiceProvider::currentService()->setArgs(dev);
            }
            loadFromDevice();
            return;
        }
    }

    // 2. Launch device connection sequence via MainWindow
    m_autoLoadOnConnect = true;
    if(MainWindow::current)
    {
        MainWindow::current->connectPhone = {};
        MainWindow::current->connectPhone.connectionType = DeviceConnectType::ADB;
        MainWindow::current->showPageLoader(DevicesPage, 1200, []() { return true; }, "Подключение к устройству через ADB...");
    }
    else
    {
        QMessageBox::warning(this, "ADB", "Устройство не подключено по USB. Подключите телефон и включите «Отладку по USB».");
    }
}

void ContactFixerWidget::setupUi()
{
    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setObjectName("cf_scrollArea");
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setStyleSheet("#cf_scrollArea { background-color: transparent; border: none; }");

    QWidget *contentWidget = new QWidget(scrollArea);
    contentWidget->setObjectName("cf_contentWidget");
    contentWidget->setStyleSheet("#cf_contentWidget { background-color: transparent; }");

    QVBoxLayout *mainLayout = new QVBoxLayout(contentWidget);
    mainLayout->setContentsMargins(18, 16, 18, 16);
    mainLayout->setSpacing(12);

    // ================= 1. TOP HEADER & DEVICE BAR =================
    QFrame *headerFrame = new QFrame(contentWidget);
    headerFrame->setObjectName("cf_header");
    headerFrame->setStyleSheet("#cf_header { background-color: #0F172A; border: 1px solid #1E293B; border-radius: 0px; }");
    QVBoxLayout *headerVBox = new QVBoxLayout(headerFrame);
    headerVBox->setContentsMargins(12, 8, 12, 8);
    headerVBox->setSpacing(6);

    // Row A: Title & Device Status
    QHBoxLayout *headerRowA = new QHBoxLayout();
    headerRowA->setSpacing(10);

    QVBoxLayout *titleBox = new QVBoxLayout();
    titleBox->setSpacing(1);
    QLabel *titleLbl = new QLabel("<img src=\":/svg/users\" width=\"16\" height=\"16\" style=\"vertical-align: middle;\"/>  Исправление и нормализация контактов", headerFrame);
    titleLbl->setTextFormat(Qt::RichText);
    titleLbl->setStyleSheet("font-size: 14px; font-weight: bold; color: #38BDF8;");
    QLabel *subtitleLbl = new QLabel("Автоматическое форматирование телефонных номеров в телефонной книге vCard / Android", headerFrame);
    subtitleLbl->setStyleSheet("font-size: 11px; color: #64748B;");
    titleBox->addWidget(titleLbl);
    titleBox->addWidget(subtitleLbl);
    headerRowA->addLayout(titleBox);

    headerRowA->addStretch(1);

    m_lblDeviceStatus = new QLabel("Телефон не подключен", headerFrame);
    m_lblDeviceStatus->setStyleSheet("background-color: #1E293B; color: #94A3B8; font-size: 11px; font-weight: 500; border: 1px solid #334155; border-radius: 0px; padding: 3px 8px;");
    headerRowA->addWidget(m_lblDeviceStatus);

    m_btnConnectDevice = new QPushButton("Подключить ADB", headerFrame);
    m_btnConnectDevice->setIcon(QIcon(":/svg/wifi"));
    m_btnConnectDevice->setIconSize(QSize(13, 13));
    m_btnConnectDevice->setStyleSheet(
        "QPushButton { background-color: #1E293B; color: #38BDF8; font-weight: 600; border-radius: 0px; padding: 4px 10px; min-height: 26px; border: 1px solid #0284C7; font-size: 11.5px; }"
        "QPushButton:hover { background-color: #0284C7; color: #FFFFFF; }");
    connect(m_btnConnectDevice, &QPushButton::clicked, this, &ContactFixerWidget::requestDeviceConnect);
    headerRowA->addWidget(m_btnConnectDevice);

    headerVBox->addLayout(headerRowA);

    // Row B: Active Source Field (Поле активного источника)
    QHBoxLayout *sourceRow = new QHBoxLayout();
    sourceRow->setSpacing(8);
    QLabel *srcLbl = new QLabel("Активный источник:", headerFrame);
    srcLbl->setStyleSheet("color: #94A3B8; font-size: 11.5px; font-weight: 600;");
    sourceRow->addWidget(srcLbl);

    m_loadedPathEdit = new QLineEdit(headerFrame);
    m_loadedPathEdit->setReadOnly(true);
    m_loadedPathEdit->setPlaceholderText("Файл или устройство ещё не загружены. Откройте .vcf с ПК или нажмите «Считать с телефона»...");
    m_loadedPathEdit->setStyleSheet("QLineEdit { background-color: #070A12; color: #38BDF8; border: 1px solid #1E293B; border-radius: 0px; padding: 3px 8px; font-family: monospace; font-size: 11.5px; min-height: 26px; }");
    sourceRow->addWidget(m_loadedPathEdit, 1);

    headerVBox->addLayout(sourceRow);
    mainLayout->addWidget(headerFrame);

    // ================= 2. ACTIONS & TOOLBAR CARD =================
    QFrame *actionFrame = new QFrame(contentWidget);
    actionFrame->setObjectName("cf_actions");
    actionFrame->setStyleSheet("#cf_actions { background-color: #0F172A; border: 1px solid #1E293B; border-radius: 0px; }");
    QVBoxLayout *actionVBox = new QVBoxLayout(actionFrame);
    actionVBox->setContentsMargins(12, 8, 12, 8);
    actionVBox->setSpacing(6);

    // Action Row 1: Source & Output
    QHBoxLayout *actRow1 = new QHBoxLayout();
    actRow1->setSpacing(8);

    m_btnOpenVcf = new QPushButton("Открыть .vcf с ПК", actionFrame);
    m_btnOpenVcf->setIcon(QIcon(":/svg/folder"));
    m_btnOpenVcf->setIconSize(QSize(13, 13));
    m_btnOpenVcf->setStyleSheet(
        "QPushButton { background-color: #0284C7; color: #FFFFFF; font-weight: 600; font-size: 11.5px; border-radius: 0px; padding: 3px 10px; height: 26px; min-height: 26px; max-height: 26px; border: none; }"
        "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0369A1, stop:1 #0284C7); }");
    connect(m_btnOpenVcf, &QPushButton::clicked, this, &ContactFixerWidget::openVcfFile);
    actRow1->addWidget(m_btnOpenVcf);

    m_btnLoadDevice = new QPushButton("Считать с телефона", actionFrame);
    m_btnLoadDevice->setIcon(QIcon(":/svg/smartphone"));
    m_btnLoadDevice->setIconSize(QSize(13, 13));
    m_btnLoadDevice->setStyleSheet(
        "QPushButton { background-color: #1E293B; color: #38BDF8; font-weight: 600; font-size: 11.5px; border-radius: 0px; padding: 3px 10px; height: 26px; min-height: 26px; max-height: 26px; border: 1px solid #0284C7; }"
        "QPushButton:hover { background-color: #0284C7; color: #FFFFFF; }");
    connect(m_btnLoadDevice, &QPushButton::clicked, this, &ContactFixerWidget::loadFromDevice);
    actRow1->addWidget(m_btnLoadDevice);

    QLabel *remotePathLbl = new QLabel("Путь на телефоне:", actionFrame);
    remotePathLbl->setStyleSheet("color: #94A3B8; font-size: 11.5px; margin-left: 4px;");
    actRow1->addWidget(remotePathLbl);

    m_remotePathEdit = new QLineEdit("/sdcard/Download/contacts.vcf", actionFrame);
    m_remotePathEdit->setToolTip("Путь к файлу .vcf на устройстве для чтения и записи");
    m_remotePathEdit->setStyleSheet(
        "QLineEdit { background-color: #070A12; color: #F8FAFC; border: 1px solid #1E293B; border-radius: 0px; padding: 2px 8px; font-family: monospace; font-size: 11.5px; height: 26px; min-height: 26px; max-height: 26px; min-width: 170px; }"
        "QLineEdit:focus { border-color: #38BDF8; }");
    actRow1->addWidget(m_remotePathEdit);

    actRow1->addStretch(1);

    m_btnSaveVcf = new QPushButton("Сохранить .vcf", actionFrame);
    m_btnSaveVcf->setIcon(QIcon(":/svg/download"));
    m_btnSaveVcf->setIconSize(QSize(13, 13));
    m_btnSaveVcf->setStyleSheet(
        "QPushButton { background-color: #1E293B; color: #E2E8F0; font-weight: 600; font-size: 11.5px; border-radius: 0px; padding: 3px 10px; height: 26px; min-height: 26px; max-height: 26px; border: 1px solid #334155; }"
        "QPushButton:hover { background-color: #27354A; border-color: #38BDF8; color: #38BDF8; }");
    connect(m_btnSaveVcf, &QPushButton::clicked, this, &ContactFixerWidget::saveVcfFile);
    actRow1->addWidget(m_btnSaveVcf);

    m_btnPushDevice = new QPushButton("Отправить на телефон", actionFrame);
    m_btnPushDevice->setIcon(QIcon(":/svg/send"));
    m_btnPushDevice->setIconSize(QSize(13, 13));
    m_btnPushDevice->setStyleSheet(
        "QPushButton { background-color: #1E293B; color: #E2E8F0; font-weight: 600; font-size: 11.5px; border-radius: 0px; padding: 3px 10px; height: 26px; min-height: 26px; max-height: 26px; border: 1px solid #334155; }"
        "QPushButton:hover { background-color: #27354A; border-color: #38BDF8; color: #38BDF8; }");
    connect(m_btnPushDevice, &QPushButton::clicked, this, &ContactFixerWidget::pushToDevice);
    actRow1->addWidget(m_btnPushDevice);

    m_btnExportCsv = new QPushButton("Экспорт в CSV", actionFrame);
    m_btnExportCsv->setIcon(QIcon(":/svg/clipboard"));
    m_btnExportCsv->setIconSize(QSize(13, 13));
    m_btnExportCsv->setStyleSheet(
        "QPushButton { background-color: #1E293B; color: #94A3B8; font-weight: 500; font-size: 11.5px; border-radius: 0px; padding: 3px 8px; height: 26px; min-height: 26px; max-height: 26px; border: 1px solid #334155; }"
        "QPushButton:hover { color: #F8FAFC; border-color: #64748B; }");
    connect(m_btnExportCsv, &QPushButton::clicked, this, &ContactFixerWidget::exportCsv);
    actRow1->addWidget(m_btnExportCsv);

    actionVBox->addLayout(actRow1);

    // Action Row 2: Format Rules & Search
    QHBoxLayout *actRow2 = new QHBoxLayout();
    actRow2->setSpacing(8);

    QLabel *ruleLbl = new QLabel("Формат номеров:", actionFrame);
    ruleLbl->setStyleSheet("color: #94A3B8; font-size: 11.5px; font-weight: 600;");
    actRow2->addWidget(ruleLbl);

    m_comboFormatRule = new QComboBox(actionFrame);
    m_comboFormatRule->addItem("Международный красивый: +7 (900) 000-00-00");
    m_comboFormatRule->addItem("Локальный красивый: 8 (900) 000-00-00");
    m_comboFormatRule->addItem("Международный компактный: +79000000000");
    m_comboFormatRule->addItem("Локальный компактный: 89000000000");
    m_comboFormatRule->setStyleSheet(
        "QComboBox { background-color: #070A12; color: #F8FAFC; border: 1px solid #1E293B; border-radius: 0px; padding: 2px 24px 2px 8px; height: 26px; min-height: 26px; max-height: 26px; min-width: 280px; font-size: 11.5px; }"
        "QComboBox:hover { border-color: #38BDF8; }"
        "QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: top right; border: none; width: 20px; background-color: transparent; }"
        "QComboBox::down-arrow { image: url(:/svg/arrow-down); width: 12px; height: 12px; }"
        "QComboBox::down-arrow:hover { image: url(:/svg/arrow-down-hover); }"
        "QComboBox QAbstractItemView { background-color: #0F172A; color: #F8FAFC; selection-background-color: #0284C7; border: 1px solid #1E293B; }");
    connect(m_comboFormatRule, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ContactFixerWidget::onFormatRuleChanged);
    actRow2->addWidget(m_comboFormatRule);

    m_chkFixLeading8 = new QCheckBox("Заменять 8 на +7 (РФ/СНГ)", actionFrame);
    m_chkFixLeading8->setChecked(true);
    m_chkFixLeading8->setStyleSheet("QCheckBox { color: #E2E8F0; font-size: 11.5px; margin-left: 4px; } QCheckBox::indicator { width: 14px; height: 14px; }");
    connect(m_chkFixLeading8, &QCheckBox::toggled, this, &ContactFixerWidget::applyFixToAll);
    actRow2->addWidget(m_chkFixLeading8);

    m_btnApplyFix = new QPushButton("Применить формат", actionFrame);
    m_btnApplyFix->setIcon(QIcon(":/svg/zap"));
    m_btnApplyFix->setIconSize(QSize(13, 13));
    m_btnApplyFix->setStyleSheet(
        "QPushButton { background-color: #059669; color: #FFFFFF; font-weight: bold; font-size: 11.5px; border-radius: 0px; padding: 3px 12px; height: 26px; min-height: 26px; max-height: 26px; border: none; }"
        "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #047857, stop:1 #059669); }");
    connect(m_btnApplyFix, &QPushButton::clicked, this, &ContactFixerWidget::applyFixToAll);
    actRow2->addWidget(m_btnApplyFix);

    actRow2->addStretch(1);

    m_searchEdit = new QLineEdit(actionFrame);
    m_searchEdit->setPlaceholderText("Поиск по имени или номеру...");
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setStyleSheet(
        "QLineEdit { background-color: #070A12; color: #F8FAFC; border: 1px solid #1E293B; border-radius: 0px; padding: 2px 8px; font-size: 11.5px; height: 26px; min-height: 26px; max-height: 26px; min-width: 190px; }"
        "QLineEdit:focus { border-color: #38BDF8; }");
    connect(m_searchEdit, &QLineEdit::textChanged, this, &ContactFixerWidget::onSearchFilterChanged);
    actRow2->addWidget(m_searchEdit);

    actionVBox->addLayout(actRow2);
    mainLayout->addWidget(actionFrame);

    // ================= 3. MAIN SPLITTER (Center Table + Right Inspector) =================
    QSplitter *splitter = new QSplitter(Qt::Horizontal, contentWidget);
    splitter->setChildrenCollapsible(false);
    splitter->setMinimumHeight(420);

    // Left Container: Selection Controls & Table
    QWidget *leftContainer = new QWidget(splitter);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftContainer);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(6);

    QHBoxLayout *selectionBar = new QHBoxLayout();
    m_btnSelectAll = new QPushButton("Выбрать все", leftContainer);
    m_btnSelectAll->setStyleSheet(
        "QPushButton { background-color: #1E293B; color: #E2E8F0; border: 1px solid #334155; border-radius: 0px; padding: 3px 10px; min-height: 24px; font-size: 11px; font-weight: 500; }"
        "QPushButton:hover { color: #38BDF8; border-color: #38BDF8; }");
    connect(m_btnSelectAll, &QPushButton::clicked, this, &ContactFixerWidget::onSelectAllClicked);
    selectionBar->addWidget(m_btnSelectAll);

    m_btnDeselectAll = new QPushButton("Снять выбор", leftContainer);
    m_btnDeselectAll->setStyleSheet(
        "QPushButton { background-color: #1E293B; color: #E2E8F0; border: 1px solid #334155; border-radius: 0px; padding: 3px 10px; min-height: 24px; font-size: 11px; font-weight: 500; }"
        "QPushButton:hover { color: #38BDF8; border-color: #38BDF8; }");
    connect(m_btnDeselectAll, &QPushButton::clicked, this, &ContactFixerWidget::onDeselectAllClicked);
    selectionBar->addWidget(m_btnDeselectAll);

    selectionBar->addStretch(1);

    m_statNeedsFix = new QLabel("Требуют исправления: 0", leftContainer);
    m_statNeedsFix->setStyleSheet("color: #F59E0B; font-weight: 600; font-size: 11.5px;");
    selectionBar->addWidget(m_statNeedsFix);

    leftLayout->addLayout(selectionBar);

    m_table = new QTableWidget(leftContainer);
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({"Выбор", "Имя контакта", "Исходный номер", "Страна", "Код", "Исправленный номер", "Тип"});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(28);
    m_table->horizontalHeader()->setMinimumHeight(28);
    m_table->setShowGrid(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);

    m_table->setStyleSheet(
        "QTableWidget { background-color: #070A12; border: 1px solid #1E293B; border-radius: 0px; gridline-color: #1E293B; color: #F8FAFC; selection-background-color: #1E293B; selection-color: #38BDF8; font-size: 11.5px; }"
        "QTableWidget::item { padding: 4px 8px; border-bottom: 1px solid #0F172A; }"
        "QTableWidget::item:hover { background-color: #0F172A; }"
        "QTableWidget::item:selected { background-color: #141E33; color: #38BDF8; }"
        "QHeaderView::section { background-color: #0F172A; color: #94A3B8; font-weight: bold; border: none; border-bottom: 1px solid #1E293B; padding: 4px 8px; font-size: 11px; }");
    connect(m_table, &QTableWidget::itemChanged, this, &ContactFixerWidget::onTableItemChanged);

    leftLayout->addWidget(m_table, 1);
    splitter->addWidget(leftContainer);

    // Right Container: Inspector & Live Number Tester
    QWidget *rightContainer = new QWidget(splitter);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightContainer);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(10);

    QFrame *testerCard = new QFrame(rightContainer);
    testerCard->setObjectName("testerCard");
    testerCard->setStyleSheet("#testerCard { background-color: #0F172A; border: 1px solid #1E293B; border-radius: 0px; padding: 6px; }");

    QVBoxLayout *cardLayout = new QVBoxLayout(testerCard);
    cardLayout->setContentsMargins(10, 10, 10, 10);
    cardLayout->setSpacing(6);

    QLabel *cardTitle = new QLabel("<img src=\":/svg/bot\" width=\"14\" height=\"14\" style=\"vertical-align: middle;\"/>  Интерактивный тестер номеров", testerCard);
    cardTitle->setTextFormat(Qt::RichText);
    cardTitle->setStyleSheet("font-size: 13px; font-weight: bold; color: #38BDF8; margin-bottom: 1px;");
    cardLayout->addWidget(cardTitle);

    QLabel *inputHint = new QLabel("Введите номер телефона для проверки форматирования:", testerCard);
    inputHint->setStyleSheet("font-size: 11px; color: #94A3B8;");
    cardLayout->addWidget(inputHint);

    m_testNumberInput = new QLineEdit(testerCard);
    m_testNumberInput->setText("+890000000000");
    m_testNumberInput->setPlaceholderText("Например: +890000000000 или 89001234567");
    m_testNumberInput->setStyleSheet(
        "QLineEdit { background-color: #070A12; color: #F8FAFC; border: 1px solid #1E293B; border-radius: 0px; padding: 4px 8px; font-family: monospace; font-size: 12px; font-weight: bold; min-height: 28px; } "
        "QLineEdit:focus { border-color: #38BDF8; }");
    connect(
        m_testNumberInput,
        &QLineEdit::textChanged,
        this,
        [this](const QString &text)
        {
            onTestNumberInputChanged(text);
            if(m_quickNumberEdit && m_quickNumberEdit->text().isEmpty())
            {
                m_quickNumberEdit->setPlaceholderText(text.trimmed());
            }
        });
    cardLayout->addWidget(m_testNumberInput);

    // Meta details: Country & Code
    QHBoxLayout *metaLayout = new QHBoxLayout();
    QLabel *cKey = new QLabel("Страна:", testerCard);
    cKey->setStyleSheet("color: #64748B; font-size: 11px;");
    m_testCountryLabel = new QLabel("—", testerCard);
    m_testCountryLabel->setStyleSheet("color: #E2E8F0; font-weight: bold; font-size: 11px;");

    QLabel *dKey = new QLabel("Код:", testerCard);
    dKey->setStyleSheet("color: #64748B; font-size: 11px; margin-left: 6px;");
    m_testDialCodeLabel = new QLabel("—", testerCard);
    m_testDialCodeLabel->setStyleSheet("color: #38BDF8; font-weight: bold; font-size: 11px;");

    metaLayout->addWidget(cKey);
    metaLayout->addWidget(m_testCountryLabel);
    metaLayout->addWidget(dKey);
    metaLayout->addWidget(m_testDialCodeLabel);
    metaLayout->addStretch(1);

    m_testValidLabel = new QLabel("—", testerCard);
    m_testValidLabel->setStyleSheet("font-size: 11px; font-weight: 600;");
    metaLayout->addWidget(m_testValidLabel);
    cardLayout->addLayout(metaLayout);

    QFrame *divLine = new QFrame(testerCard);
    divLine->setFrameShape(QFrame::HLine);
    divLine->setStyleSheet("color: #1E293B; margin: 2px 0;");
    cardLayout->addWidget(divLine);

    QLabel *formatHeader = new QLabel("Варианты форматирования:", testerCard);
    formatHeader->setStyleSheet("color: #94A3B8; font-size: 11px; font-weight: 600; margin-top: 1px;");
    cardLayout->addWidget(formatHeader);

    auto makeFormatRow = [this, testerCard](const QString &name, QLabel *&labelPtr) -> QWidget *
    {
        QWidget *w = new QWidget(testerCard);
        QVBoxLayout *v = new QVBoxLayout(w);
        v->setContentsMargins(0, 1, 0, 2);
        v->setSpacing(2);

        QLabel *lbl = new QLabel(name, w);
        lbl->setStyleSheet("color: #64748B; font-size: 10px; font-weight: 600;");
        v->addWidget(lbl);

        QHBoxLayout *h = new QHBoxLayout();
        h->setSpacing(5);
        labelPtr = new QLabel("—", w);
        labelPtr->setStyleSheet("background-color: #070A12; color: #F8FAFC; border: 1px solid #1E293B; border-radius: 0px; padding: 3px 6px; font-family: monospace; font-size: 11px; min-height: 24px;");
        h->addWidget(labelPtr, 1);

        QPushButton *btnCopy = new QPushButton(w);
        btnCopy->setIcon(QIcon(":/svg/copy"));
        btnCopy->setIconSize(QSize(12, 12));
        btnCopy->setFixedSize(24, 24);
        btnCopy->setToolTip("Копировать в буфер");
        btnCopy->setStyleSheet(
            "QPushButton { background-color: #1E293B; border: 1px solid #334155; border-radius: 0px; color: #38BDF8; font-size: 11px; } "
            "QPushButton:hover { background-color: #0284C7; color: white; }");
        connect(btnCopy, &QPushButton::clicked, [labelPtr]() { QApplication::clipboard()->setText(labelPtr->text()); });
        h->addWidget(btnCopy);

        v->addLayout(h);
        return w;
    };

    cardLayout->addWidget(makeFormatRow("1. Международный красивый (Global Beauty)", m_valBeautyGlobal));
    cardLayout->addWidget(makeFormatRow("2. Локальный красивый (Local Beauty)", m_valBeautyLocal));
    cardLayout->addWidget(makeFormatRow("3. Международный компактный (Global Compact)", m_valCompactGlobal));
    cardLayout->addWidget(makeFormatRow("4. Локальный компактный (Local Compact)", m_valCompactLocal));

    cardLayout->addStretch(1);

    // Quick Add Contact Frame with dedicated fields
    QFrame *addFrame = new QFrame(testerCard);
    addFrame->setStyleSheet("background-color: #070A12; border: 1px solid #1E293B; border-radius: 0px; padding: 6px;");
    QVBoxLayout *addLayout = new QVBoxLayout(addFrame);
    addLayout->setContentsMargins(6, 6, 6, 6);
    addLayout->setSpacing(5);

    QLabel *addTitle = new QLabel("<img src=\":/svg/plus\" width=\"12\" height=\"12\" style=\"vertical-align: middle;\"/>  Быстрое добавление контакта", addFrame);
    addTitle->setTextFormat(Qt::RichText);
    addTitle->setStyleSheet("color: #38BDF8; font-size: 11.5px; font-weight: bold;");
    addLayout->addWidget(addTitle);

    m_quickNameEdit = new QLineEdit(addFrame);
    m_quickNameEdit->setPlaceholderText("Имя контакта (напр. Иван Иванов)");
    m_quickNameEdit->setStyleSheet(
        "QLineEdit { background-color: #0F172A; color: #F8FAFC; border: 1px solid #1E293B; border-radius: 0px; padding: 3px 8px; font-size: 11.5px; min-height: 26px; } "
        "QLineEdit:focus { border-color: #38BDF8; }");
    addLayout->addWidget(m_quickNameEdit);

    m_quickNumberEdit = new QLineEdit(addFrame);
    m_quickNumberEdit->setPlaceholderText("Номер (напр. +7 900 123-45-67)");
    m_quickNumberEdit->setStyleSheet(
        "QLineEdit { background-color: #0F172A; color: #F8FAFC; border: 1px solid #1E293B; border-radius: 0px; padding: 3px 8px; font-size: 11.5px; min-height: 26px; font-family: monospace; } "
        "QLineEdit:focus { border-color: #38BDF8; }");
    addLayout->addWidget(m_quickNumberEdit);

    m_btnQuickAdd = new QPushButton("Добавить в список", addFrame);
    m_btnQuickAdd->setIcon(QIcon(":/svg/plus"));
    m_btnQuickAdd->setIconSize(QSize(13, 13));
    m_btnQuickAdd->setStyleSheet(
        "QPushButton { background-color: #1E293B; color: #E2E8F0; border: 1px solid #334155; border-radius: 0px; padding: 4px 10px; font-size: 11.5px; font-weight: 600; min-height: 26px; } "
        "QPushButton:hover { background-color: #0284C7; color: #FFFFFF; border-color: #38BDF8; }");
    connect(m_btnQuickAdd, &QPushButton::clicked, this, &ContactFixerWidget::onQuickAddContact);
    addLayout->addWidget(m_btnQuickAdd);

    cardLayout->addWidget(addFrame);

    rightLayout->addWidget(testerCard, 1);
    splitter->addWidget(rightContainer);

    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    mainLayout->addWidget(splitter, 1);

    // ================= 4. BOTTOM STATUS BAR =================
    QHBoxLayout *statusBar = new QHBoxLayout();
    statusBar->setContentsMargins(4, 2, 4, 2);

    m_statTotalContacts = new QLabel("Всего контактов: 0", contentWidget);
    m_statTotalContacts->setStyleSheet("color: #94A3B8; font-size: 12.5px; font-weight: bold; margin-right: 14px;");
    statusBar->addWidget(m_statTotalContacts);

    m_statTotalNumbers = new QLabel("Номеров: 0", contentWidget);
    m_statTotalNumbers->setStyleSheet("color: #94A3B8; font-size: 12.5px; margin-right: 14px;");
    statusBar->addWidget(m_statTotalNumbers);

    m_statusMsg = new QLabel("Готов к работе. Откройте файл контактов .vcf или считайте с телефона.", contentWidget);
    m_statusMsg->setStyleSheet("color: #38BDF8; font-size: 12.5px; font-style: italic;");
    statusBar->addWidget(m_statusMsg, 1);

    mainLayout->addLayout(statusBar);

    scrollArea->setWidget(contentWidget);
    rootLayout->addWidget(scrollArea);

    // Initialize live tester with sample number
    onTestNumberInputChanged("+890000000000");
    updateDeviceUi();
}

void ContactFixerWidget::onTestNumberInputChanged(const QString &text)
{
    QString input = text.trimmed();
    if(input.isEmpty())
    {
        m_testCountryLabel->setText("—");
        m_testDialCodeLabel->setText("—");
        m_testValidLabel->setText("—");
        m_valBeautyGlobal->setText("—");
        m_valBeautyLocal->setText("—");
        m_valCompactGlobal->setText("—");
        m_valCompactLocal->setText("—");
        return;
    }

    NumberPreview np(input.toStdString());
    m_testCountryLabel->setText(np.country().empty() ? "Не определена" : QString::fromStdString(np.country()));
    m_testDialCodeLabel->setText(np.dialCode().empty() ? "—" : QString::fromStdString(np.dialCode()));
    m_testValidLabel->setText(np.isGenericNumber() ? "Корректный" : "Нестандартный");
    m_testValidLabel->setStyleSheet(np.isGenericNumber() ? "font-size: 11px; font-weight: 600; color: #10B981;" : "font-size: 11px; font-weight: 600; color: #F59E0B;");

    m_valBeautyGlobal->setText(QString::fromStdString(np.format(NumberFormat::Beauty | NumberFormat::Global)));
    m_valBeautyLocal->setText(QString::fromStdString(np.format(NumberFormat::Beauty | NumberFormat::Local)));
    m_valCompactGlobal->setText(QString::fromStdString(np.format(NumberFormat::Compact | NumberFormat::Global)));
    m_valCompactLocal->setText(QString::fromStdString(np.format(NumberFormat::Compact | NumberFormat::Local)));
}

QString ContactFixerWidget::computeFixedNumber(const QString &rawNumber) const
{
    QString num = rawNumber.trimmed();
    if(num.isEmpty())
        return {};

    // Check if leading 8 should be replaced for Russian numbers
    if(m_chkFixLeading8 && m_chkFixLeading8->isChecked())
    {
        QString digitsOnly;
        for(const QChar &c : num)
        {
            if(c.isDigit())
                digitsOnly.append(c);
        }

        if(digitsOnly.length() == 11 && digitsOnly.startsWith('8'))
        {
            num = "+7" + digitsOnly.mid(1);
        }
        else if(digitsOnly.length() == 10)
        {
            num = "+7" + digitsOnly;
        }
    }

    NumberPreview np(num.toStdString());
    if(np.isEmpty())
        return rawNumber;

    int rule = m_comboFormatRule ? m_comboFormatRule->currentIndex() : 0;
    std::string res;
    switch(rule)
    {
        case 0:
            res = np.format(NumberFormat::Beauty | NumberFormat::Global);
            break;
        case 1:
            res = np.format(NumberFormat::Beauty | NumberFormat::Local);
            break;
        case 2:
            res = np.format(NumberFormat::Compact | NumberFormat::Global);
            break;
        case 3:
            res = np.format(NumberFormat::Compact | NumberFormat::Local);
            break;
        default:
            res = np.format(NumberFormat::Beauty | NumberFormat::Global);
            break;
    }

    return QString::fromStdString(res);
}

void ContactFixerWidget::openVcfFile()
{
    QString path = QFileDialog::getOpenFileName(this, "Открыть файл контактов vCard", QDir::homePath(), "vCard Files (*.vcf *.vcard);;All Files (*)");
    if(path.isEmpty())
        return;

    std::ifstream file(path.toStdString(), std::ios::binary);
    if(!file.is_open())
    {
        QMessageBox::warning(this, "Ошибка", "Не удалось открыть файл: " + path);
        return;
    }

    TextReader reader(file);
    std::vector<vCard> cards;
    reader >> cards;
    file.close();

    if(cards.empty())
    {
        QMessageBox::warning(this, "Предупреждение", "В файле не найдено контактов формата vCard.");
        return;
    }

    m_loadedFilePath = path;
    if(m_loadedPathEdit)
        m_loadedPathEdit->setText("Файл ПК: " + path);
    if(d)
        d->vcards = std::move(cards);
    parseVcards();
    m_statusMsg->setText(QString("Загружено из файла: %1 (%2 контактов)").arg(QFileInfo(path).fileName()).arg(d ? d->vcards.size() : 0));
}

static std::vector<vCard> parseContactsFromAdbContent(const QString &rawOutput)
{
    std::vector<vCard> cards;
    const QStringList lines = rawOutput.split('\n', Qt::SkipEmptyParts);
    for(const QString &rawLine : lines)
    {
        QString line = rawLine.trimmed();
        if(!line.startsWith("Row:"))
            continue;

        QString name;
        QString number;

        int dnIdx = line.indexOf("display_name=");
        int nIdx = line.indexOf("name=");
        int d1Idx = line.indexOf("data1=");
        int numIdx = line.indexOf("number=");

        if(dnIdx != -1)
        {
            int start = dnIdx + 13;
            int end = line.indexOf(',', start);
            name = (end == -1) ? line.mid(start).trimmed() : line.mid(start, end - start).trimmed();
        }
        else if(nIdx != -1)
        {
            int start = nIdx + 5;
            int end = line.indexOf(',', start);
            name = (end == -1) ? line.mid(start).trimmed() : line.mid(start, end - start).trimmed();
        }

        if(d1Idx != -1)
        {
            int start = d1Idx + 6;
            int end = line.indexOf(',', start);
            number = (end == -1) ? line.mid(start).trimmed() : line.mid(start, end - start).trimmed();
        }
        else if(numIdx != -1)
        {
            int start = numIdx + 7;
            int end = line.indexOf(',', start);
            number = (end == -1) ? line.mid(start).trimmed() : line.mid(start, end - start).trimmed();
        }

        if(number.isEmpty())
            continue;
        if(name.isEmpty())
            name = "Без имени";

        vCard card;
        card.addProperty(vCardProperty::createName(name.toStdString(), ""));
        card.addProperty({VC_FORMATTED_NAME, name.toStdString()});
        card.addProperty({VC_TELEPHONE, number.toStdString()});
        cards.push_back(card);
    }
    return cards;
}

void ContactFixerWidget::loadFromDevice()
{
    // 1. If device is not connected, try to find an authorized device or request ADB connection
    if(m_device.devId.isEmpty() || !m_fileIO.isConnect())
    {
        QList<AdbDevice> devices = Adb::getDevices();
        AdbDevice authDev;
        for(const auto &dev : devices)
        {
            if(Adb::deviceStatus(dev.devId) == DEVICE)
            {
                authDev = dev;
                break;
            }
        }

        if(!authDev.isEmpty())
        {
            setDevice(authDev);
            if(MainWindow::current)
            {
                MainWindow::current->connectPhone.isAuthed = true;
                MainWindow::current->connectPhone.adbDevice = authDev;
                if(ServiceProvider::currentService())
                    ServiceProvider::currentService()->setArgs(authDev);
            }
        }
        else
        {
            // Launch the service via ADB as user requested!
            requestDeviceConnect();
            return;
        }
    }

    m_statusMsg->setText("Чтение контактов с устройства " + m_device.displayName + "...");
    qApp->processEvents();

    std::vector<vCard> cards;
    QString loadedPath;

    // A. Check remote path specified in field
    QString preferredPath = m_remotePathEdit ? m_remotePathEdit->text().trimmed() : QString {};
    if(!preferredPath.isEmpty())
    {
        QByteArray data = m_fileIO.read(preferredPath);
        if(!data.isEmpty())
        {
            std::string stdData = data.toStdString();
            std::istringstream iss(stdData);
            TextReader reader(iss);
            reader >> cards;
            if(!cards.empty())
                loadedPath = preferredPath;
        }
    }

    // B. Scan standard directories if preferredPath didn't contain cards
    if(cards.empty())
    {
        QStringList candidates = {"/sdcard/Download", "/sdcard/Contacts", "/sdcard/Documents", "/sdcard"};
        for(const auto &dir : candidates)
        {
            auto list = m_fileIO.getFileList(dir);
            for(const auto &f : list)
            {
                if(!f.isDir && (f.name.endsWith(".vcf", Qt::CaseInsensitive) || f.name.endsWith(".vcard", Qt::CaseInsensitive)))
                {
                    QByteArray data = m_fileIO.read(f.fullPath);
                    if(!data.isEmpty())
                    {
                        std::string stdData = data.toStdString();
                        std::istringstream iss(stdData);
                        TextReader reader(iss);
                        reader >> cards;
                        if(!cards.empty())
                        {
                            loadedPath = f.fullPath;
                            if(m_remotePathEdit)
                                m_remotePathEdit->setText(loadedPath);
                            break;
                        }
                    }
                }
            }
            if(!cards.empty())
                break;
        }
    }

    // C. If still empty, query Android contacts directly via ADB shell content query
    if(cards.empty())
    {
        m_statusMsg->setText("Чтение из системной базы контактов Android через ADB...");
        qApp->processEvents();

        AdbShell shell(m_device.devId);
        auto res = shell.commandQueueWait({"content", "query", "--uri", "content://com.android.contacts/data/phones", "--projection", "display_name:data1"});
        if(!res.first || res.second.trimmed().isEmpty() || !res.second.contains("Row:"))
        {
            res = shell.commandQueueWait({"content", "query", "--uri", "content://contacts/phones/", "--projection", "name:number"});
        }

        if(res.first && res.second.contains("Row:"))
        {
            cards = parseContactsFromAdbContent(res.second);
            if(!cards.empty())
            {
                loadedPath = QString("Android: системная книга контактов (%1)").arg(!m_device.displayName.isEmpty() ? m_device.displayName : m_device.devId);
            }
        }
    }

    if(cards.empty())
    {
        m_statusMsg->setText("Контакты на устройстве не найдены.");
        QMessageBox::information(
            this,
            "Импорт с телефона",
            "На устройстве не обнаружено файлов .vcf и нет доступных контактов через систему.\n\n"
            "Вы можете экспортировать контакты в файл .vcf в стандартном приложении «Контакты» на телефоне, "
            "или указать путь к файлу в поле «Путь на телефоне».");
        return;
    }

    m_loadedFilePath = loadedPath;
    if(m_loadedPathEdit)
        m_loadedPathEdit->setText(m_loadedFilePath);

    if(d)
        d->vcards = std::move(cards);

    parseVcards();
    m_statusMsg->setText(QString("Загружено с устройства: %1 (%2 контактов)").arg(loadedPath).arg(d ? d->vcards.size() : 0));
}

void ContactFixerWidget::parseVcards()
{
    m_items.clear();
    if(!d)
        return;

    for(size_t cIdx = 0; cIdx < d->vcards.size(); ++cIdx)
    {
        auto &card = d->vcards[cIdx];
        QString contactName;

        for(const auto &prop : card.properties())
        {
            if(prop.getCName() == VC_FORMATTED_NAME)
            {
                contactName = QString::fromStdString(const_cast<vCardProperty &>(prop).getValue()).trimmed();
                break;
            }
        }

        if(contactName.isEmpty())
        {
            for(const auto &prop : card.properties())
            {
                if(prop.getCName() == VC_NAME)
                {
                    contactName = QString::fromStdString(const_cast<vCardProperty &>(prop).getValue()).trimmed();
                    break;
                }
            }
        }

        if(contactName.isEmpty())
            contactName = "Без имени";

        for(size_t pIdx = 0; pIdx < card.properties().size(); ++pIdx)
        {
            auto &prop = card.properties()[pIdx];
            if(prop.getCName() == VC_TELEPHONE)
            {
                QString rawTel = QString::fromStdString(prop.getValue()).trimmed();
                if(rawTel.isEmpty())
                    continue;

                ContactItem item;
                item.name = contactName;
                item.originalNumber = rawTel;
                item.fixedNumber = computeFixedNumber(rawTel);
                item.phoneType = "CELL";

                NumberPreview np(rawTel.toStdString());
                item.country = QString::fromStdString(np.country());
                item.dialCode = QString::fromStdString(np.dialCode());
                item.countryCode = np.countryCode();
                item.isSelected = true;
                item.vcardIndex = cIdx;
                item.propIndex = pIdx;

                m_items.append(item);
            }
        }
    }

    onSearchFilterChanged(m_searchEdit ? m_searchEdit->text() : QString {});
}

void ContactFixerWidget::populateTable()
{
    m_table->blockSignals(true);
    m_table->setRowCount(0);

    for(int row = 0; row < m_filteredItems.size(); ++row)
    {
        const auto &item = m_filteredItems[row];
        m_table->insertRow(row);

        // Checkbox
        QTableWidgetItem *checkItem = new QTableWidgetItem();
        checkItem->setCheckState(item.isSelected ? Qt::Checked : Qt::Unchecked);
        checkItem->setData(Qt::UserRole, row);
        m_table->setItem(row, 0, checkItem);

        // Name
        QTableWidgetItem *nameItem = new QTableWidgetItem(item.name);
        nameItem->setForeground(QColor("#F8FAFC"));
        nameItem->setFont(QFont("Segoe UI", 9, QFont::DemiBold));
        m_table->setItem(row, 1, nameItem);

        // Original Number
        QTableWidgetItem *origItem = new QTableWidgetItem(item.originalNumber);
        origItem->setForeground(QColor("#94A3B8"));
        origItem->setFont(QFont("monospace", 9));
        m_table->setItem(row, 2, origItem);

        // Country
        QTableWidgetItem *countryItem = new QTableWidgetItem(item.country.isEmpty() ? "—" : item.country);
        countryItem->setForeground(QColor("#E2E8F0"));
        m_table->setItem(row, 3, countryItem);

        // Dial Code
        QTableWidgetItem *codeItem = new QTableWidgetItem(item.dialCode.isEmpty() ? "—" : item.dialCode);
        codeItem->setForeground(QColor("#38BDF8"));
        codeItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 4, codeItem);

        // Fixed Number
        QTableWidgetItem *fixedItem = new QTableWidgetItem(item.fixedNumber);
        bool changed = (item.originalNumber != item.fixedNumber);
        fixedItem->setForeground(changed ? QColor("#10B981") : QColor("#94A3B8"));
        fixedItem->setFont(QFont("monospace", 9, changed ? QFont::Bold : QFont::Normal));
        m_table->setItem(row, 5, fixedItem);

        // Phone Type
        QTableWidgetItem *typeItem = new QTableWidgetItem(item.phoneType);
        typeItem->setForeground(QColor("#64748B"));
        typeItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 6, typeItem);
    }

    m_table->blockSignals(false);
    updateStats();
}

void ContactFixerWidget::updateStats()
{
    int totalContacts = d ? d->vcards.size() : 0;
    int totalNumbers = m_items.size();
    int needsFix = 0;

    for(const auto &item : m_items)
    {
        if(item.originalNumber != item.fixedNumber)
            needsFix++;
    }

    m_statTotalContacts->setText(QString("Всего контактов: %1").arg(totalContacts));
    m_statTotalNumbers->setText(QString("Номеров: %1").arg(totalNumbers));
    m_statNeedsFix->setText(QString("Требуют исправления: %1").arg(needsFix));
}

void ContactFixerWidget::applyFixToAll()
{
    for(auto &item : m_items)
    {
        item.fixedNumber = computeFixedNumber(item.originalNumber);
    }

    onSearchFilterChanged(m_searchEdit ? m_searchEdit->text() : QString {});
    m_statusMsg->setText("Форматы пересчитаны для всех номеров.");
}

void ContactFixerWidget::onFormatRuleChanged(int /*index*/)
{
    applyFixToAll();
}

void ContactFixerWidget::onSearchFilterChanged(const QString &text)
{
    QString query = text.trimmed().toLower();
    m_filteredItems.clear();

    for(const auto &item : m_items)
    {
        if(query.isEmpty() || item.name.toLower().contains(query) || item.originalNumber.contains(query) || item.fixedNumber.contains(query) || item.country.toLower().contains(query))
        {
            m_filteredItems.append(item);
        }
    }

    populateTable();
}

void ContactFixerWidget::onTableItemChanged(QTableWidgetItem *item)
{
    if(item && item->column() == 0)
    {
        int filteredRow = item->row();
        if(filteredRow >= 0 && filteredRow < m_filteredItems.size())
        {
            bool checked = (item->checkState() == Qt::Checked);
            m_filteredItems[filteredRow].isSelected = checked;

            // Update master list
            for(auto &masterItem : m_items)
            {
                if(masterItem.vcardIndex == m_filteredItems[filteredRow].vcardIndex && masterItem.propIndex == m_filteredItems[filteredRow].propIndex)
                {
                    masterItem.isSelected = checked;
                    break;
                }
            }
        }
    }
}

void ContactFixerWidget::onSelectAllClicked()
{
    for(auto &item : m_items)
        item.isSelected = true;
    for(auto &item : m_filteredItems)
        item.isSelected = true;
    populateTable();
}

void ContactFixerWidget::onDeselectAllClicked()
{
    for(auto &item : m_items)
        item.isSelected = false;
    for(auto &item : m_filteredItems)
        item.isSelected = false;
    populateTable();
}

void ContactFixerWidget::applyFixToVcards()
{
    if(!d)
        return;

    for(const auto &item : m_items)
    {
        if(item.isSelected && item.vcardIndex < d->vcards.size())
        {
            auto &card = d->vcards[item.vcardIndex];
            if(item.propIndex < card.properties().size())
            {
                auto &prop = card.properties()[item.propIndex];
                if(prop.getCName() == VC_TELEPHONE && !prop.values().empty())
                {
                    prop.values()[0] = item.fixedNumber.toStdString();
                }
            }
        }
    }
}

void ContactFixerWidget::saveVcfFile()
{
    if(!d || d->vcards.empty())
    {
        QMessageBox::information(this, "Информация", "Нет загруженных контактов для сохранения.");
        return;
    }

    applyFixToVcards();

    QString defaultName = m_loadedFilePath.isEmpty() ? "contacts_fixed.vcf" : (QFileInfo(m_loadedFilePath).completeBaseName() + "_fixed.vcf");
    QString savePath = QFileDialog::getSaveFileName(this, "Сохранить исправленные контакты", defaultName, "vCard Files (*.vcf *.vcard);;All Files (*)");
    if(savePath.isEmpty())
        return;

    std::ofstream ofs(savePath.toStdString(), std::ios::binary);
    if(!ofs.is_open())
    {
        QMessageBox::warning(this, "Ошибка", "Не удалось открыть файл для записи:\n" + savePath);
        return;
    }

    TextWriter writer(ofs);
    writer << d->vcards;
    ofs.close();

    m_statusMsg->setText("Сохранено: " + QFileInfo(savePath).fileName());
    QMessageBox::information(this, "Успех", QString("Файл контактов успешно сохранён:\n%1\n\nВсего контактов: %2\nОбработано номеров: %3").arg(savePath).arg(d->vcards.size()).arg(m_items.size()));
}

void ContactFixerWidget::pushToDevice()
{
    if(!d || d->vcards.empty())
    {
        QMessageBox::information(this, "Информация", "Нет загруженных контактов для отправки на телефон.");
        return;
    }

    if(m_device.devId.isEmpty() || !m_fileIO.isConnect())
    {
        QList<AdbDevice> devices = Adb::getDevices();
        AdbDevice authDev;
        for(const auto &dev : devices)
        {
            if(Adb::deviceStatus(dev.devId) == DEVICE)
            {
                authDev = dev;
                break;
            }
        }

        if(!authDev.isEmpty())
        {
            setDevice(authDev);
            if(MainWindow::current)
            {
                MainWindow::current->connectPhone.isAuthed = true;
                MainWindow::current->connectPhone.adbDevice = authDev;
            }
        }
        else
        {
            requestDeviceConnect();
            return;
        }
    }

    applyFixToVcards();

    std::ostringstream oss;
    TextWriter writer(oss);
    writer << d->vcards;
    std::string str = oss.str();
    QByteArray data(str.data(), static_cast<int>(str.size()));

    QString remotePath = m_remotePathEdit ? m_remotePathEdit->text().trimmed() : QString {};
    if(remotePath.isEmpty())
        remotePath = "/sdcard/Download/contacts_fixed.vcf";
    else if(!remotePath.endsWith(".vcf", Qt::CaseInsensitive) && !remotePath.endsWith(".vcard", Qt::CaseInsensitive))
        remotePath = remotePath + "/contacts_fixed.vcf";

    if(m_fileIO.write(remotePath, data))
    {
        m_statusMsg->setText("Отправлено на устройство: " + remotePath);
        QMessageBox::information(this, "Готово", QString("Файл контактов успешно записан на телефон:\n%1\n\nВы можете открыть его через приложение «Контакты» на телефоне для объединения.").arg(remotePath));
    }
    else
    {
        QMessageBox::warning(this, "Ошибка", "Не удалось записать файл на устройство по пути:\n" + remotePath);
    }
}

void ContactFixerWidget::exportCsv()
{
    if(m_items.isEmpty())
    {
        QMessageBox::information(this, "Информация", "Список контактов пуст.");
        return;
    }

    QString savePath = QFileDialog::getSaveFileName(this, "Экспорт в CSV", "contacts.csv", "CSV Files (*.csv);;All Files (*)");
    if(savePath.isEmpty())
        return;

    QFile file(savePath);
    if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "Ошибка", "Не удалось создать файл:\n" + savePath);
        return;
    }

    QTextStream out(&file);
    out << "Имя;Исходный номер;Исправленный номер;Страна;Код;Тип\n";
    for(const auto &item : m_items)
    {
        out << "\"" << item.name << "\";\"" << item.originalNumber << "\";\"" << item.fixedNumber << "\";\"" << item.country << "\";\"" << item.dialCode << "\";\"" << item.phoneType << "\"\n";
    }
    file.close();

    m_statusMsg->setText("Экспортировано в CSV: " + QFileInfo(savePath).fileName());
    QMessageBox::information(this, "Экспорт завершён", "Контакты успешно сохранены в " + savePath);
}

void ContactFixerWidget::onQuickAddContact()
{
    if(!d)
        return;

    QString name = m_quickNameEdit ? m_quickNameEdit->text().trimmed() : QString {};
    QString number = m_quickNumberEdit ? m_quickNumberEdit->text().trimmed() : QString {};

    if(name.isEmpty())
    {
        bool ok = false;
        name = QInputDialog::getText(this, "Новый контакт", "Введите имя контакта:", QLineEdit::Normal, "", &ok);
        if(!ok || name.trimmed().isEmpty())
            return;
        name = name.trimmed();
    }

    if(number.isEmpty())
    {
        bool ok = false;
        QString defaultNum = m_testNumberInput ? m_testNumberInput->text().trimmed() : QString {};
        number = QInputDialog::getText(this, "Новый контакт", "Введите телефонный номер:", QLineEdit::Normal, defaultNum, &ok);
        if(!ok || number.trimmed().isEmpty())
            return;
        number = number.trimmed();
    }

    vCard card;
    card.addProperty(vCardProperty::createName(name.toStdString(), ""));
    card.addProperty({VC_FORMATTED_NAME, name.toStdString()});
    card.addProperty({VC_TELEPHONE, number.toStdString()});
    d->vcards.push_back(card);

    ContactItem item;
    item.name = name;
    item.originalNumber = number;
    item.fixedNumber = computeFixedNumber(number);
    item.phoneType = "CELL";

    NumberPreview np(number.toStdString());
    item.country = QString::fromStdString(np.country());
    item.dialCode = QString::fromStdString(np.dialCode());
    item.countryCode = np.countryCode();
    item.isSelected = true;
    item.vcardIndex = d->vcards.size() - 1;
    item.propIndex = card.properties().size() - 1;

    m_items.append(item);

    if(m_quickNameEdit)
        m_quickNameEdit->clear();
    if(m_quickNumberEdit)
        m_quickNumberEdit->clear();

    onSearchFilterChanged(m_searchEdit ? m_searchEdit->text() : QString {});
    m_statusMsg->setText("Контакт добавлен: " + name);
}

void ContactFixerWidget::resetSession()
{
    if(d)
        d->vcards.clear();
    m_items.clear();
    m_filteredItems.clear();
    m_loadedFilePath.clear();

    if(m_table)
    {
        m_table->blockSignals(true);
        m_table->setRowCount(0);
        m_table->blockSignals(false);
    }

    if(m_loadedPathEdit)
        m_loadedPathEdit->clear();
    if(m_remotePathEdit)
        m_remotePathEdit->setText("/sdcard/contacts.vcf");
    if(m_searchEdit)
        m_searchEdit->clear();

    if(m_testNumberInput)
        m_testNumberInput->clear();
    if(m_quickNameEdit)
        m_quickNameEdit->clear();
    if(m_quickNumberEdit)
        m_quickNumberEdit->clear();

    if(m_valBeautyGlobal)
        m_valBeautyGlobal->clear();
    if(m_valBeautyLocal)
        m_valBeautyLocal->clear();
    if(m_valCompactGlobal)
        m_valCompactGlobal->clear();
    if(m_valCompactLocal)
        m_valCompactLocal->clear();

    if(m_testCountryLabel)
        m_testCountryLabel->setText(QString::fromUtf8("Страна: —"));
    if(m_testDialCodeLabel)
        m_testDialCodeLabel->setText(QString::fromUtf8("Код: —"));
    if(m_testValidLabel)
        m_testValidLabel->setText(QString::fromUtf8("Корректный: —"));

    if(m_btnSaveVcf)
        m_btnSaveVcf->setEnabled(false);
    if(m_btnPushDevice)
        m_btnPushDevice->setEnabled(false);
    if(m_btnExportCsv)
        m_btnExportCsv->setEnabled(false);

    updateStats();
}

// ==================== ContactFixerService ====================

struct CFSInternalData
{
    bool started = false;
    bool finished = false;
};

ContactFixerService::ContactFixerService(QObject *parent) : Service(DeviceConnectType::None, parent), mInternal(new CFSInternalData)
{
    title = "Исправление контактов";
    active = true;
}

ContactFixerService::~ContactFixerService()
{
    if(mInternal)
    {
        delete mInternal;
        mInternal = nullptr;
    }
}

QString ContactFixerService::uuid() const
{
    return IDServiceContactFixerString;
}

PageIndex ContactFixerService::targetPage()
{
    return ContactFixerPage;
}

QString ContactFixerService::widgetIconName()
{
    return "contact-fixer";
}

bool ContactFixerService::canStart()
{
    return true;
}

bool ContactFixerService::isStarted()
{
    return mInternal && mInternal->started;
}

bool ContactFixerService::isFinish()
{
    return mInternal && mInternal->finished;
}

bool ContactFixerService::start()
{
    sendCheckPull();
    if(mInternal)
    {
        mInternal->started = true;
        mInternal->finished = false;
    }

    if(MainWindow::current)
    {
        auto *widget = static_cast<ContactFixerWidget *>(MainWindow::current->pageWidget(ContactFixerPage));
        if(widget)
        {
            widget->resetSession();
            if(!mAdbDevice.isEmpty())
            {
                widget->setDevice(mAdbDevice);
            }
        }
    }

    return true;
}

void ContactFixerService::stop()
{
    if(mInternal)
    {
        mInternal->started = false;
        mInternal->finished = true;
    }

    if(MainWindow::current)
    {
        auto *widget = static_cast<ContactFixerWidget *>(MainWindow::current->pageWidget(ContactFixerPage));
        if(widget)
            widget->resetSession();
    }
}
