#include "about_dialog.h"
#include "begin.h"
#include "Strings.h"

#include <QApplication>
#include <QCoreApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QFile>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("О программе AdsKiller"));
    setWindowIcon(QIcon(QStringLiteral(":/resources/app-logo")));
    setModal(true);

    resize(680, 640);
    setMinimumSize(600, 520);

    setupUi();
}

void AboutDialog::setCurrentTab(TabIndex tab)
{
    if(m_tabWidget)
    {
        m_tabWidget->setCurrentIndex(static_cast<int>(tab));
    }
}

void AboutDialog::setupUi()
{
    // Modern dialog styling compatible with both light and dark themes
    setStyleSheet(QStringLiteral(
        "QDialog {"
        "    font-family: \"Segoe UI Variable\", \"Segoe UI\", -apple-system, BlinkMacSystemFont, Arial, sans-serif;"
        "}"
        "QFrame#headerCard {"
        "    background-color: rgba(127, 127, 127, 0.08);"
        "    border: 1px solid rgba(127, 127, 127, 0.20);"
        "    border-radius: 12px;"
        "}"
        "QFrame[card=\"true\"] {"
        "    background-color: rgba(127, 127, 127, 0.08);"
        "    border: 1px solid rgba(127, 127, 127, 0.18);"
        "    border-radius: 10px;"
        "}"
        "QFrame[card=\"true\"]:hover {"
        "    border: 1px solid rgba(127, 127, 127, 0.32);"
        "}"
        "QLabel#cardTitle {"
        "    font-size: 13px;"
        "    font-weight: bold;"
        "}"
        "QTextEdit#licenseTextEdit {"
        "    font-family: \"Cascadia Code\", \"Consolas\", \"Courier New\", monospace;"
        "    font-size: 11px;"
        "    border: 1px solid rgba(127, 127, 127, 0.22);"
        "    border-radius: 8px;"
        "    padding: 8px;"
        "}"
    ));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 18, 18, 18);
    mainLayout->setSpacing(14);

    // 1. Header Banner
    mainLayout->addWidget(createHeaderWidget());

    // 2. Tab Widget
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setObjectName(QStringLiteral("aboutTabWidget"));

    m_tabWidget->addTab(createAboutTab(), QStringLiteral("О программе"));
    m_tabWidget->addTab(createAuthorsTab(), QStringLiteral("Об авторах"));
    m_tabWidget->addTab(createGplTab(), QStringLiteral("GPL v3"));

    mainLayout->addWidget(m_tabWidget, 1);

    // 3. Footer Bar
    mainLayout->addWidget(createFooterWidget());
}

QWidget *AboutDialog::createHeaderWidget()
{
    auto *headerFrame = new QFrame(this);
    headerFrame->setObjectName(QStringLiteral("headerCard"));

    auto *headerLayout = new QHBoxLayout(headerFrame);
    headerLayout->setContentsMargins(16, 14, 16, 14);
    headerLayout->setSpacing(16);

    // App Logo
    auto *logoLabel = new QLabel(headerFrame);
    logoLabel->setFixedSize(60, 60);
    QPixmap logoPix(QStringLiteral(":/resources/app-logo"));
    if(!logoPix.isNull())
    {
        logoLabel->setPixmap(logoPix.scaled(60, 60, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    logoLabel->setAlignment(Qt::AlignCenter);
    headerLayout->addWidget(logoLabel);

    // App Info Layout
    auto *infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(4);

    // Row 1: Title + Version Pill + Tag
    auto *titleRow = new QHBoxLayout();
    titleRow->setSpacing(8);

    auto *titleLabel = new QLabel(QStringLiteral("AdsKiller"), headerFrame);
    titleLabel->setStyleSheet(QStringLiteral("font-size: 22px; font-weight: 800; letter-spacing: 0.5px;"));
    titleRow->addWidget(titleLabel);

    const QString verStr = QStringLiteral("v%1.%2.%3").arg(AppVerMajor).arg(AppVerMinor).arg(AppVerPatch);
    auto *versionBadge = new QLabel(verStr, headerFrame);
    versionBadge->setStyleSheet(QStringLiteral(
        "background-color: #005FB8;"
        "color: #FFFFFF;"
        "border-radius: 10px;"
        "padding: 2px 10px;"
        "font-size: 11px;"
        "font-weight: bold;"
    ));
    titleRow->addWidget(versionBadge);

    auto *channelBadge = new QLabel(QStringLiteral("Stable Release"), headerFrame);
    channelBadge->setStyleSheet(QStringLiteral(
        "background-color: rgba(16, 185, 129, 0.20);"
        "color: #10B981;"
        "border: 1px solid rgba(16, 185, 129, 0.40);"
        "border-radius: 10px;"
        "padding: 2px 8px;"
        "font-size: 10px;"
        "font-weight: 600;"
    ));
    titleRow->addWidget(channelBadge);
    titleRow->addStretch();
    infoLayout->addLayout(titleRow);

    // Row 2: Subtitle
    auto *subLabel = new QLabel(
        QStringLiteral("Десктопный комплекс для очистки, оптимизации и безопасного деблоатинга Android"),
        headerFrame);
    subLabel->setStyleSheet(QStringLiteral("font-size: 12px; color: rgba(127, 127, 127, 0.95);"));
    infoLayout->addWidget(subLabel);

    // Row 3: Meta info
    auto *metaLabel = new QLabel(
        QStringLiteral("Лицензия: GNU GPL v3 • Авторские права © 2026 imister.kz"),
        headerFrame);
    metaLabel->setStyleSheet(QStringLiteral("font-size: 11px; color: rgba(127, 127, 127, 0.85);"));
    infoLayout->addWidget(metaLabel);

    headerLayout->addLayout(infoLayout);
    return headerFrame;
}

QWidget *AboutDialog::createAboutTab()
{
    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setStyleSheet(QStringLiteral("background: transparent; border: none;"));

    auto *container = new QWidget(scrollArea);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(8, 12, 8, 12);
    layout->setSpacing(12);

    // Card 1: Description
    auto *descCard = new QFrame(container);
    descCard->setProperty("card", true);
    auto *descLayout = new QVBoxLayout(descCard);
    descLayout->setContentsMargins(14, 12, 14, 12);
    descLayout->setSpacing(8);

    auto *descTitle = new QLabel(QStringLiteral("✦ О проекте"), descCard);
    descTitle->setObjectName(QStringLiteral("cardTitle"));
    descLayout->addWidget(descTitle);

    auto *descText = new QLabel(
        QStringLiteral(
            "<b>AdsKiller</b> — высокопроизводительное десктопное приложение на C++, предназначенное "
            "для сервисных центров, системных инженеров и пользователей Android.<br><br>"
            "Программа позволяет в один клик избавить подключенное устройство от навязчивой встроенной "
            "рекламы, неиспользуемых системных служб (<b>bloatware</b>), трекеров и мусора "
            "<b>без необходимости получения Root-прав</b>.<br><br>"
            "Вся работа строится на базе протокола <b>Android Debug Bridge (ADB)</b> в изолированном "
            "режиме пользователя (<code>--user 0</code>), что гарантирует стабильность операционной "
            "системы и исключает риск повреждения устройства («окирпичивания»)."),
        descCard);
    descText->setWordWrap(true);
    descText->setTextFormat(Qt::RichText);
    descLayout->addWidget(descText);
    layout->addWidget(descCard);

    // Card 2: Key Features
    auto *featCard = new QFrame(container);
    featCard->setProperty("card", true);
    auto *featLayout = new QVBoxLayout(featCard);
    featLayout->setContentsMargins(14, 12, 14, 12);
    featLayout->setSpacing(8);

    auto *featTitle = new QLabel(QStringLiteral("✦ Ключевые возможности"), featCard);
    featTitle->setObjectName(QStringLiteral("cardTitle"));
    featLayout->addWidget(featTitle);

    auto *featText = new QLabel(
        QStringLiteral(
            "• 🛡️ <b>Удаление рекламы и трекеров:</b> пакетное сканирование и нейтрализация рекламных SDK "
            "(AdMob, UnityAds, IronSource, встроенные пуши MIUI/HyperOS) и шпионских служб.<br>"
            "• ⚡ <b>Ускорение и оптимизация RAM (Boost RAM):</b> мониторинг процессов, принудительная "
            "выгрузка фоновых демонов и автозапуска, освобождающая от 500 МБ до 2+ ГБ оперативной памяти.<br>"
            "• 🧹 <b>Глубокая очистка диска:</b> безопасное удаление временных файлов, логов сбоев, "
            "миниатюр галереи и остаточного кэша приложений без затрагивания личных данных.<br>"
            "• 👥 <b>Восстановление контактов (Contact Fixer):</b> обработка и исправление телефонных книг "
            "в формате VCard (VCF), исправление масок телефонных номеров и удаление дубликатов.<br>"
            "• 🔓 <b>Сервисные модули для Xiaomi:</b> считывание параметров загрузчика (Bootloader), "
            "проверка статуса привязки Mi-аккаунта.<br>"
            "• 📱 <b>База устройств и гарантийный учет:</b> распознавание марок и моделей смартфонов, "
            "ведение подробной истории сервисных сессий."),
        featCard);
    featText->setWordWrap(true);
    featText->setTextFormat(Qt::RichText);
    featLayout->addWidget(featText);
    layout->addWidget(featCard);

    // Card 3: Tech Stack & Architecture
    auto *techCard = new QFrame(container);
    techCard->setProperty("card", true);
    auto *techLayout = new QVBoxLayout(techCard);
    techLayout->setContentsMargins(14, 12, 14, 12);
    techLayout->setSpacing(8);

    auto *techTitle = new QLabel(QStringLiteral("✦ Архитектура и технологии"), techCard);
    techTitle->setObjectName(QStringLiteral("cardTitle"));
    techLayout->addWidget(techTitle);

    auto *techChipsLayout = new QHBoxLayout();
    techChipsLayout->setSpacing(8);

    const QStringList techBadges = {
        QStringLiteral("C++17 Core"),
        QStringLiteral("Qt 6 Widgets"),
        QStringLiteral("ADB Front Engine"),
        QStringLiteral("Safe User Mode"),
        QStringLiteral("GNU GPL v3")
    };

    for(const QString &badgeText : techBadges)
    {
        auto *badge = new QLabel(badgeText, techCard);
        badge->setStyleSheet(QStringLiteral(
            "background-color: rgba(0, 95, 184, 0.15);"
            "color: #0078D4;"
            "border: 1px solid rgba(0, 95, 184, 0.30);"
            "border-radius: 6px;"
            "padding: 3px 8px;"
            "font-size: 11px;"
            "font-weight: 600;"
        ));
        techChipsLayout->addWidget(badge);
    }
    techChipsLayout->addStretch();
    techLayout->addLayout(techChipsLayout);
    layout->addWidget(techCard);

    // Card 4: Quick Links
    auto *linksCard = new QFrame(container);
    linksCard->setProperty("card", true);
    auto *linksLayout = new QHBoxLayout(linksCard);
    linksLayout->setContentsMargins(14, 10, 14, 10);
    linksLayout->setSpacing(12);

    auto *webBtn = new QPushButton(QStringLiteral("🌐 Официальный сайт (adskiller.imister.tech)"), linksCard);
    connect(webBtn, &QPushButton::clicked, this, &AboutDialog::openProjectWebsite);
    linksLayout->addWidget(webBtn);

    auto *supportBtn = new QPushButton(QStringLiteral("💬 Техническая поддержка (WhatsApp)"), linksCard);
    connect(supportBtn, &QPushButton::clicked, this, &AboutDialog::openSupportWhatsApp);
    linksLayout->addWidget(supportBtn);

    linksLayout->addStretch();
    layout->addWidget(linksCard);

    layout->addStretch();
    scrollArea->setWidget(container);
    return scrollArea;
}

QWidget *AboutDialog::createAuthorsTab()
{
    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setStyleSheet(QStringLiteral("background: transparent; border: none;"));

    auto *container = new QWidget(scrollArea);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(8, 12, 8, 12);
    layout->setSpacing(12);

    auto *introLabel = new QLabel(
        QStringLiteral("<b>Команда авторов и создатели проекта AdsKiller</b><br>"
                       "<span style=\"color: rgba(127,127,127,0.9); font-size: 11px;\">"
                       "Люди, благодаря которым разрабатывается и развивается проект.</span>"),
        container);
    introLabel->setTextFormat(Qt::RichText);
    layout->addWidget(introLabel);

    // Author 1: badcast (Lead Developer)
    QWidget *leadDevCard = createAuthorCard(
        QStringLiteral("NK"),
        QColor(QStringLiteral("#2563EB")),
        QColor(QStringLiteral("#1D4ED8")),
        QStringLiteral("Нурсеит К. (badcast)"),
        QStringLiteral("Ведущий разработчик / Lead Developer"),
        QStringLiteral("#005FB8"),
        QStringLiteral(
            "• Архитектура клиентского ядра приложения на C++17 и модульная система сервисов.<br>"
            "• Разработка высокопроизводительного слоя интеграции с Android Debug Bridge (ADB).<br>"
            "• Модули оптимизации RAM (Boost RAM), сканирования и удаления рекламных пакетов.<br>"
            "• Сетевой клиент, система шифрования и пользовательский интерфейс Qt."),
        QStringLiteral("badcast &lt;anon&gt; • Разработка ядра и сервисов")
    );
    layout->addWidget(leadDevCard);

    // Author 2: LeoJames (Icon & Visual Designer)
    QWidget *designerCard = createAuthorCard(
        QStringLiteral("LJ"),
        QColor(QStringLiteral("#DB2777")),
        QColor(QStringLiteral("#9D174D")),
        QStringLiteral("Владимир (LeoJames)"),
        QStringLiteral("UI/UX Дизайнер / Visual Artist"),
        QStringLiteral("#9D174D"),
        QStringLiteral(
            "• Создание фирменного визуального стиля и дизайн-системы приложения.<br>"
            "• Авторский набор иконок для сервисов (Ads Remove, Boost RAM, Storage Cleaner, Mi Unlock и др.).<br>"
            "• Графические ресурсы, оптимизация пиктограмм под светлую и тёмную темы интерфейса."),
        QStringLiteral("LeoJames &lt;anon&gt; • Дизайн и графическое оформление")
    );
    layout->addWidget(designerCard);

    // Author 3: imister.kz (Project Lead & Infrastructure)
    QWidget *leadProjectCard = createAuthorCard(
        QStringLiteral("IM"),
        QColor(QStringLiteral("#059669")),
        QColor(QStringLiteral("#047857")),
        QStringLiteral("Команда imister.kz"),
        QStringLiteral("Издатель & Инфраструктура / Project Lead"),
        QStringLiteral("#047857"),
        QStringLiteral(
            "• Концепция, развитие и выпуск официальных релизов программы AdsKiller.<br>"
            "• Облачная инфраструктура, серверная база сигнатур рекламных модулей и вредоносного ПО.<br>"
            "• Поддержка серверов автоматического обновления и клиентской базы данных.<br>"
            "• Веб-ресурсы: <a href=\"https://imister.kz\" style=\"color: #0078D4;\">imister.kz</a> "
            "и <a href=\"https://adskiller.imister.tech\" style=\"color: #0078D4;\">adskiller.imister.tech</a>."),
        QStringLiteral("imister.kz • Казахстан • Издатель и инфраструктура")
    );
    layout->addWidget(leadProjectCard);

    // Card 4: Acknowledgements
    auto *thanksCard = new QFrame(container);
    thanksCard->setProperty("card", true);
    auto *thanksLayout = new QVBoxLayout(thanksCard);
    thanksLayout->setContentsMargins(14, 12, 14, 12);
    thanksLayout->setSpacing(8);

    auto *thanksTitle = new QLabel(QStringLiteral("✦ Благодарности сообществу"), thanksCard);
    thanksTitle->setObjectName(QStringLiteral("cardTitle"));
    thanksLayout->addWidget(thanksTitle);

    auto *thanksText = new QLabel(
        QStringLiteral(
            "• <b>Free Software Foundation (FSF)</b> — за принципы свободного программного обеспечения и лицензию GNU GPL v3.<br>"
            "• <b>Проекту Qt Project</b> — за великолепный кроссплатформенный графический инструментарий.<br>"
            "• <b>Android Open Source Project (AOSP)</b> — за инструменты платформы Android и протокол ADB.<br>"
            "• <b>Всем пользователям и сообществу</b> — за полезные отзывы, тестирование и поддержку проекта!"),
        thanksCard);
    thanksText->setWordWrap(true);
    thanksText->setTextFormat(Qt::RichText);
    thanksLayout->addWidget(thanksText);
    layout->addWidget(thanksCard);

    layout->addStretch();
    scrollArea->setWidget(container);
    return scrollArea;
}

QWidget *AboutDialog::createAuthorCard(const QString &initials,
                                       const QColor &gradStart,
                                       const QColor &gradEnd,
                                       const QString &name,
                                       const QString &role,
                                       const QString &roleColor,
                                       const QString &description,
                                       const QString &contact)
{
    auto *cardFrame = new QFrame(this);
    cardFrame->setProperty("card", true);

    auto *cardLayout = new QHBoxLayout(cardFrame);
    cardLayout->setContentsMargins(14, 12, 14, 12);
    cardLayout->setSpacing(14);

    // Avatar
    auto *avatarLabel = new QLabel(cardFrame);
    avatarLabel->setFixedSize(48, 48);
    avatarLabel->setPixmap(createAvatarPixmap(initials, gradStart, gradEnd, 48));
    avatarLabel->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(avatarLabel, 0, Qt::AlignTop);

    // Info Layout
    auto *infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(5);

    // Header: Name + Role Badge
    auto *nameRow = new QHBoxLayout();
    nameRow->setSpacing(8);

    auto *nameLabel = new QLabel(name, cardFrame);
    nameLabel->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: bold;"));
    nameRow->addWidget(nameLabel);

    auto *roleBadge = new QLabel(role, cardFrame);
    roleBadge->setStyleSheet(QStringLiteral(
        "background-color: %1;"
        "color: #FFFFFF;"
        "border-radius: 5px;"
        "padding: 2px 7px;"
        "font-size: 10px;"
        "font-weight: 600;"
    ).arg(roleColor));
    nameRow->addWidget(roleBadge);
    nameRow->addStretch();
    infoLayout->addLayout(nameRow);

    // Description
    auto *descLabel = new QLabel(description, cardFrame);
    descLabel->setWordWrap(true);
    descLabel->setTextFormat(Qt::RichText);
    descLabel->setOpenExternalLinks(true);
    descLabel->setStyleSheet(QStringLiteral("font-size: 11px; line-height: 1.4;"));
    infoLayout->addWidget(descLabel);

    // Contact info
    if(!contact.isEmpty())
    {
        auto *contactLabel = new QLabel(contact, cardFrame);
        contactLabel->setTextFormat(Qt::RichText);
        contactLabel->setOpenExternalLinks(true);
        contactLabel->setStyleSheet(QStringLiteral("font-size: 10px; color: rgba(127, 127, 127, 0.9);"));
        infoLayout->addWidget(contactLabel);
    }

    cardLayout->addLayout(infoLayout, 1);
    return cardFrame;
}

QWidget *AboutDialog::createGplTab()
{
    auto *widget = new QWidget(this);
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(8, 12, 8, 12);
    layout->setSpacing(10);

    // Top Summary Card
    auto *summaryCard = new QFrame(widget);
    summaryCard->setProperty("card", true);
    auto *summaryLayout = new QVBoxLayout(summaryCard);
    summaryLayout->setContentsMargins(14, 12, 14, 12);
    summaryLayout->setSpacing(8);

    auto *summaryTitle = new QLabel(QStringLiteral("✦ GNU General Public License, Version 3 (GPL v3)"), summaryCard);
    summaryTitle->setObjectName(QStringLiteral("cardTitle"));
    summaryLayout->addWidget(summaryTitle);

    auto *summaryDesc = new QLabel(
        QStringLiteral(
            "Программа <b>AdsKiller</b> является свободным программным обеспечением. "
            "Вы можете свободно распространять и/или модифицировать её на условиях "
            "<b>Стандартной Общественной Лицензии GNU (GPLv3)</b>, опубликованной Фондом Свободного ПО (FSF).<br>"
            "Исходный код открыт и доступен для изучения, улучшения и адаптации."),
        summaryCard);
    summaryDesc->setWordWrap(true);
    summaryDesc->setTextFormat(Qt::RichText);
    summaryLayout->addWidget(summaryDesc);

    // Badges / Permissions Row
    auto *chipsLayout = new QHBoxLayout();
    chipsLayout->setSpacing(6);

    const struct { QString text; QString color; } chips[] = {
        { QStringLiteral("✓ Свободное использование"), QStringLiteral("#10B981") },
        { QStringLiteral("✓ Доступ к исходному коду"), QStringLiteral("#10B981") },
        { QStringLiteral("✓ Модификация"), QStringLiteral("#10B981") },
        { QStringLiteral("✓ Распространение"), QStringLiteral("#10B981") },
        { QStringLiteral("ℹ Copyleft (GPL v3)"), QStringLiteral("#0078D4") },
        { QStringLiteral("⚠️ Без гарантий (AS IS)"), QStringLiteral("#F59E0B") }
    };

    for(const auto &chip : chips)
    {
        auto *chipLabel = new QLabel(chip.text, summaryCard);
        chipLabel->setStyleSheet(QStringLiteral(
            "background-color: rgba(127, 127, 127, 0.12);"
            "color: %1;"
            "border: 1px solid rgba(127, 127, 127, 0.25);"
            "border-radius: 5px;"
            "padding: 2px 7px;"
            "font-size: 10px;"
            "font-weight: 600;"
        ).arg(chip.color));
        chipsLayout->addWidget(chipLabel);
    }
    chipsLayout->addStretch();
    summaryLayout->addLayout(chipsLayout);

    layout->addWidget(summaryCard);

    // Full Text License Viewer
    m_licenseEdit = new QTextEdit(widget);
    m_licenseEdit->setObjectName(QStringLiteral("licenseTextEdit"));
    m_licenseEdit->setReadOnly(true);
    m_licenseEdit->setLineWrapMode(QTextEdit::WidgetWidth);
    m_licenseEdit->setPlainText(loadLicenseText());
    layout->addWidget(m_licenseEdit, 1);

    // License Action Buttons Bar
    auto *btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);

    m_copyLicenseBtn = new QPushButton(QStringLiteral("📋 Копировать текст лицензии"), widget);
    connect(m_copyLicenseBtn, &QPushButton::clicked, this, &AboutDialog::copyLicenseToClipboard);
    btnRow->addWidget(m_copyLicenseBtn);

    auto *fsfBtn = new QPushButton(QStringLiteral("🌐 Официальная страница gnu.org/licenses"), widget);
    connect(fsfBtn, &QPushButton::clicked, this, &AboutDialog::openGplWebsite);
    btnRow->addWidget(fsfBtn);

    btnRow->addStretch();
    layout->addLayout(btnRow);

    return widget;
}

QWidget *AboutDialog::createFooterWidget()
{
    auto *footerWidget = new QWidget(this);
    auto *footerLayout = new QHBoxLayout(footerWidget);
    footerLayout->setContentsMargins(0, 0, 0, 0);

    auto *siteLabel = new QLabel(
        QStringLiteral("<a href=\"https://adskiller.imister.tech\" style=\"text-decoration: none; color: #0078D4; font-weight: 600;\">"
                       "🌐 adskiller.imister.tech</a>"),
        footerWidget);
    siteLabel->setOpenExternalLinks(true);
    footerLayout->addWidget(siteLabel);

    footerLayout->addStretch();

    auto *closeBtn = new QPushButton(QStringLiteral("Закрыть"), footerWidget);
    closeBtn->setObjectName(QStringLiteral("primaryButton"));
    closeBtn->setFixedSize(110, 32);
    closeBtn->setDefault(true);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    footerLayout->addWidget(closeBtn);

    return footerWidget;
}

QPixmap AboutDialog::createAvatarPixmap(const QString &initials,
                                         const QColor &startColor,
                                         const QColor &endColor,
                                         int size)
{
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);

    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing);

    QLinearGradient grad(0, 0, size, size);
    grad.setColorAt(0, startColor);
    grad.setColorAt(1, endColor);

    painter.setBrush(grad);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(0, 0, size, size);

    painter.setPen(Qt::white);
    QFont font = painter.font();
    font.setBold(true);
    font.setPixelSize(static_cast<int>(size * 0.38));
    painter.setFont(font);

    painter.drawText(QRect(0, 0, size, size), Qt::AlignCenter, initials);
    painter.end();

    return pix;
}

QString AboutDialog::loadLicenseText()
{
    // 1. Try reading from compiled Qt resource
    QFile resFile(QStringLiteral(":/resources/license-gplv3"));
    if(resFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QString content = QString::fromUtf8(resFile.readAll());
        resFile.close();
        if(!content.trimmed().isEmpty())
        {
            return content;
        }
    }

    // 2. Try reading LICENSE from current directory or application directory
    const QStringList candidatePaths = {
        QStringLiteral("LICENSE"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/LICENSE"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../LICENSE"),
        QStringLiteral("/media/dev/adskiller/LICENSE")
    };

    for(const QString &path : candidatePaths)
    {
        QFile file(path);
        if(file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QString content = QString::fromUtf8(file.readAll());
            file.close();
            if(!content.trimmed().isEmpty())
            {
                return content;
            }
        }
    }

    // 3. Fallback header text
    return QStringLiteral(
        "GNU GENERAL PUBLIC LICENSE\n"
        "Version 3, 29 June 2007\n\n"
        "Copyright (C) 2007 Free Software Foundation, Inc. <https://fsf.org/>\n"
        "Everyone is permitted to copy and distribute verbatim copies\n"
        "of this license document, but changing it is not allowed.\n\n"
        "Preamble\n\n"
        "The GNU General Public License is a free, copyleft license for\n"
        "software and other kinds of works.\n\n"
        "The licenses for most software and other practical works are designed\n"
        "to take away your freedom to share and change the works.  By contrast,\n"
        "the GNU General Public License is intended to guarantee your freedom to\n"
        "share and change all versions of a program--to make sure it remains free\n"
        "software for all its users.  We, the Free Software Foundation, use the\n"
        "GNU General Public License for most of our software; it applies also to\n"
        "any other work released this way by its authors.  You can apply it to\n"
        "your programs, too.\n\n"
        "When we speak of free software, we are referring to freedom, not\n"
        "price.  Our General Public Licenses are designed to make sure that you\n"
        "have the freedom to distribute copies of free software (and charge for\n"
        "them if you wish), that you receive source code or can get it if you\n"
        "want it, that you can change the software or use pieces of it in new\n"
        "free programs, and that you know you can do these things.\n\n"
        "To protect your rights, we need to prevent others from denying you\n"
        "these rights or asking you to surrender the rights.  Therefore, you have\n"
        "certain responsibilities if you distribute copies of the software, or if\n"
        "you modify it: responsibilities to respect the freedom of others.\n\n"
        "Подробная информация о лицензии доступна по адресу:\n"
        "https://www.gnu.org/licenses/gpl-3.0.html\n"
    );
}

void AboutDialog::copyLicenseToClipboard()
{
    if(!m_licenseEdit)
        return;

    QClipboard *clipboard = QGuiApplication::clipboard();
    if(clipboard)
    {
        clipboard->setText(m_licenseEdit->toPlainText());
    }

    if(m_copyLicenseBtn)
    {
        const QString origText = m_copyLicenseBtn->text();
        m_copyLicenseBtn->setText(QStringLiteral("✓ Скопировано в буфер обмена!"));
        m_copyLicenseBtn->setEnabled(false);

        QTimer::singleShot(2500, this, [this, origText]() {
            if(m_copyLicenseBtn)
            {
                m_copyLicenseBtn->setText(origText);
                m_copyLicenseBtn->setEnabled(true);
            }
        });
    }
}

void AboutDialog::openProjectWebsite()
{
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://adskiller.imister.tech")));
}

void AboutDialog::openGplWebsite()
{
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://www.gnu.org/licenses/gpl-3.0.html")));
}

void AboutDialog::openSupportWhatsApp()
{
    QString dec = acceptLinkWaMe;
    dec = QByteArray::fromBase64(dec.toUtf8());
    QDesktopServices::openUrl(QUrl(dec));
}
