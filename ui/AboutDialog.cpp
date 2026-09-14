#include "AboutDialog.h"
#include "Begin.h"
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
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("О программе AdsKiller"));

    QPixmap iconPix(QStringLiteral(":/resources/banner-low"));
    if(iconPix.isNull())
        iconPix = QPixmap(QStringLiteral(":/resources/banner"));
    if(iconPix.isNull())
        iconPix = QPixmap(QStringLiteral("res/banner-low.png"));
    if(iconPix.isNull())
        iconPix = QPixmap(QStringLiteral(":/resources/app-logo"));
    setWindowIcon(QIcon(iconPix));
    setModal(true);

    resize(580, 520);
    setMinimumSize(520, 460);

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
    // Minimalist Metro UI styling compatible with Obsidian dark theme
    setStyleSheet(QStringLiteral(
        "QDialog {"
        "    font-family: \"Segoe UI Variable\", \"Segoe UI\", -apple-system, BlinkMacSystemFont, Arial, sans-serif;"
        "    background-color: #070A12;"
        "    color: #F8FAFC;"
        "}"
        "QFrame#headerCard {"
        "    background-color: #0F172A;"
        "    border: 1px solid #1E293B;"
        "    border-radius: 0px;"
        "}"
        "QFrame[card=\"true\"] {"
        "    background-color: #0F172A;"
        "    border: 1px solid #1E293B;"
        "    border-radius: 0px;"
        "}"
        "QFrame[card=\"true\"]:hover {"
        "    border: 1px solid #38BDF8;"
        "}"
        "QLabel#cardTitle {"
        "    font-size: 12px;"
        "    font-weight: bold;"
        "    color: #38BDF8;"
        "}"
        "QLabel#aboutBannerIcon {"
        "    background-color: #070A12;"
        "    border: 1px solid #1E293B;"
        "    border-radius: 0px;"
        "}"
        "QLabel#aboutAppIcon {"
        "    background-color: #070A12;"
        "    border: 1px solid #1E293B;"
        "    border-radius: 0px;"
        "    padding: 4px;"
        "}"
        "QLabel#aboutHeroBanner {"
        "    background-color: #070A12;"
        "    border: 1px solid #1E293B;"
        "    border-radius: 0px;"
        "}"
        "QTextEdit#licenseTextEdit {"
        "    font-family: \"Cascadia Code\", \"Consolas\", \"Courier New\", monospace;"
        "    font-size: 11px;"
        "    background-color: #0B0F19;"
        "    color: #CBD5E1;"
        "    border: 1px solid #1E293B;"
        "    border-radius: 0px;"
        "    padding: 8px;"
        "}"
        "QTabWidget::pane {"
        "    border: 1px solid #1E293B;"
        "    background-color: #0B0F19;"
        "    border-radius: 0px;"
        "}"
        "QTabBar::tab {"
        "    background-color: #0F172A;"
        "    color: #94A3B8;"
        "    padding: 6px 14px;"
        "    border: 1px solid #1E293B;"
        "    border-bottom: none;"
        "    border-radius: 0px;"
        "    margin-right: 2px;"
        "}"
        "QTabBar::tab:selected {"
        "    background-color: #0B0F19;"
        "    color: #38BDF8;"
        "    border-top: 2px solid #38BDF8;"
        "    font-weight: bold;"
        "}"
        "QPushButton {"
        "    background-color: #1E293B;"
        "    color: #F8FAFC;"
        "    border: 1px solid #334155;"
        "    border-radius: 0px;"
        "    padding: 6px 14px;"
        "    font-size: 11.5px;"
        "    font-weight: 600;"
        "}"
        "QPushButton:hover {"
        "    background-color: #334155;"
        "    border-color: #38BDF8;"
        "    color: #FFFFFF;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #0F172A;"
        "    border-color: #0284C7;"
        "}"
        "QPushButton#primaryButton {"
        "    background-color: #0284C7;"
        "    border: 1px solid #0284C7;"
        "    color: #FFFFFF;"
        "    font-weight: bold;"
        "}"
        "QPushButton#primaryButton:hover {"
        "    background-color: #0369A1;"
        "    border-color: #38BDF8;"
        "}"));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 14, 16, 14);
    mainLayout->setSpacing(12);

    // 1. Header Banner
    mainLayout->addWidget(createHeaderWidget());

    // 2. Tab Widget
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setObjectName(QStringLiteral("aboutTabWidget"));

    m_tabWidget->addTab(createAboutTab(), QStringLiteral("О программе"));
    m_tabWidget->addTab(createAuthorsTab(), QStringLiteral("Об авторах"));
    m_tabWidget->addTab(createGplTab(), QStringLiteral("GPL v3"));
    m_tabWidget->addTab(createChangelogTab(), QStringLiteral("Что нового"));

    mainLayout->addWidget(m_tabWidget, 1);

    // 3. Footer Bar
    mainLayout->addWidget(createFooterWidget());
}

QWidget *AboutDialog::createHeaderWidget()
{
    auto *headerFrame = new QFrame(this);
    headerFrame->setObjectName(QStringLiteral("headerCard"));

    auto *headerLayout = new QHBoxLayout(headerFrame);
    headerLayout->setContentsMargins(14, 10, 14, 10);
    headerLayout->setSpacing(14);

    // App Logo / Banner Icon - ADSKILLER icon
    auto *logoLabel = new QLabel(headerFrame);
    logoLabel->setObjectName(QStringLiteral("aboutAppIcon"));
    logoLabel->setFixedSize(80, 80);
    logoLabel->setAlignment(Qt::AlignCenter);

    QPixmap appIconPix(QStringLiteral(":/resources/icon-hello"));
    if(appIconPix.isNull())
        appIconPix = QPixmap(QStringLiteral(":/resources/app-logo"));
    if(appIconPix.isNull())
        appIconPix = QPixmap(QStringLiteral("res/icon-hello.png"));

    if(!appIconPix.isNull())
    {
        logoLabel->setPixmap(appIconPix.scaled(76, 76, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    headerLayout->addWidget(logoLabel);

    // App Info Layout
    auto *infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(3);

    // Row 1: Title + Version Pill + Tag
    auto *titleRow = new QHBoxLayout();
    titleRow->setSpacing(8);

    auto *titleLabel = new QLabel(QStringLiteral("AdsKiller"), headerFrame);
    titleLabel->setStyleSheet(QStringLiteral("font-size: 20px; font-weight: 800; letter-spacing: 0.5px; color: #F8FAFC;"));
    titleRow->addWidget(titleLabel);

    const QString verStr = QStringLiteral("v%1.%2.%3").arg(AppVerMajor).arg(AppVerMinor).arg(AppVerPatch);
    auto *versionBadge = new QLabel(verStr, headerFrame);
    versionBadge->setStyleSheet(QStringLiteral(
        "background-color: #0284C7;"
        "color: #FFFFFF;"
        "border-radius: 0px;"
        "padding: 2px 8px;"
        "font-size: 11px;"
        "font-weight: bold;"));
    titleRow->addWidget(versionBadge);

    auto *channelBadge = new QLabel(QStringLiteral("Stable Release"), headerFrame);
    channelBadge->setStyleSheet(QStringLiteral(
        "background-color: rgba(16, 185, 129, 0.20);"
        "color: #10B981;"
        "border: 1px solid rgba(16, 185, 129, 0.40);"
        "border-radius: 0px;"
        "padding: 2px 8px;"
        "font-size: 10px;"
        "font-weight: 600;"));
    titleRow->addWidget(channelBadge);
    titleRow->addStretch();
    infoLayout->addLayout(titleRow);

    // Row 2: Subtitle
    auto *subLabel = new QLabel(QStringLiteral("Очистка, ускорение и деблоатинг Android без Root-прав"), headerFrame);
    subLabel->setStyleSheet(QStringLiteral("font-size: 12px; color: #94A3B8;"));
    infoLayout->addWidget(subLabel);

    // Row 3: Meta info
    auto *metaLabel = new QLabel(QStringLiteral("GNU GPL v3 • Авторские права © 2026 imister.tech • C++17 / Qt 6"), headerFrame);
    metaLabel->setStyleSheet(QStringLiteral("font-size: 11px; color: #64748B;"));
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
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    // 1. Hero Card with Minimalist Description & Banner Icon
    auto *heroCard = new QFrame(container);
    heroCard->setProperty("card", true);
    auto *heroLayout = new QHBoxLayout(heroCard);
    heroLayout->setContentsMargins(12, 10, 12, 10);
    heroLayout->setSpacing(14);

    auto *heroBannerLabel = new QLabel(heroCard);
    heroBannerLabel->setObjectName(QStringLiteral("aboutHeroBanner"));
    heroBannerLabel->setFixedSize(70, 100);
    heroBannerLabel->setAlignment(Qt::AlignCenter);

    QPixmap bannerPix(QStringLiteral(":/resources/banner-low"));
    if(bannerPix.isNull())
        bannerPix = QPixmap(QStringLiteral(":/resources/banner"));
    if(bannerPix.isNull())
        bannerPix = QPixmap(QStringLiteral("res/banner-low.png"));
    if(!bannerPix.isNull())
    {
        heroBannerLabel->setPixmap(bannerPix.scaled(66, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    heroLayout->addWidget(heroBannerLabel);

    auto *heroTextLayout = new QVBoxLayout();
    heroTextLayout->setSpacing(4);

    auto *heroTitle = new QLabel(QStringLiteral("<b>AdsKiller</b> — легковесный инструмент деблоатинга"), heroCard);
    heroTitle->setStyleSheet(QStringLiteral("font-size: 12.5px; color: #F8FAFC; font-weight: bold;"));
    heroTextLayout->addWidget(heroTitle);

    auto *heroDesc = new QLabel(
        QStringLiteral("Безопасная очистка Android от встроенной рекламы, системного мусора и ускорение оперативной памяти через изолированный ADB-режим без Root-прав."),
        heroCard);
    heroDesc->setWordWrap(true);
    heroDesc->setStyleSheet(QStringLiteral("font-size: 11.5px; color: #94A3B8;"));
    heroTextLayout->addWidget(heroDesc);

    heroLayout->addLayout(heroTextLayout, 1);
    layout->addWidget(heroCard);

    // 2. Minimalist Feature Tiles Grid (2x2)
    auto *gridCard = new QFrame(container);
    gridCard->setProperty("card", true);
    auto *gridLayout = new QGridLayout(gridCard);
    gridLayout->setContentsMargins(12, 10, 12, 10);
    gridLayout->setHorizontalSpacing(12);
    gridLayout->setVerticalSpacing(8);

    auto createMiniTile = [gridCard](const QString &icon, const QString &title, const QString &subtitle) -> QWidget *
    {
        auto *tile = new QFrame(gridCard);
        tile->setStyleSheet(QStringLiteral(
            "QFrame {"
            "    background-color: #0B0F19;"
            "    border: 1px solid #1E293B;"
            "    border-radius: 0px;"
            "}"
            "QFrame:hover {"
            "    border-color: #38BDF8;"
            "}"));

        auto *tl = new QHBoxLayout(tile);
        tl->setContentsMargins(8, 6, 8, 6);
        tl->setSpacing(8);

        auto *iconLbl = new QLabel(tile);
        iconLbl->setPixmap(QPixmap(icon).scaled(18, 18, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        iconLbl->setFixedWidth(22);
        iconLbl->setAlignment(Qt::AlignCenter);
        tl->addWidget(iconLbl);

        auto *textCol = new QVBoxLayout();
        textCol->setSpacing(1);

        auto *headLbl = new QLabel(title, tile);
        headLbl->setStyleSheet(QStringLiteral("font-size: 11px; font-weight: bold; color: #F8FAFC; background: transparent; border: none;"));
        textCol->addWidget(headLbl);

        auto *subLbl = new QLabel(subtitle, tile);
        subLbl->setStyleSheet(QStringLiteral("font-size: 10px; color: #94A3B8; background: transparent; border: none;"));
        textCol->addWidget(subLbl);

        tl->addLayout(textCol, 1);
        return tile;
    };

    gridLayout->addWidget(createMiniTile(QStringLiteral(":/svg/shield"), QStringLiteral("Блокировка рекламы"), QStringLiteral("Нейтрализация баннеров и трекеров (No Root)")), 0, 0);
    gridLayout->addWidget(createMiniTile(QStringLiteral(":/svg/zap"), QStringLiteral("Boost RAM & Очистка"), QStringLiteral("Освобождение памяти и удаление кэша")), 0, 1);
    gridLayout->addWidget(createMiniTile(QStringLiteral(":/svg/users"), QStringLiteral("Contact Fixer"), QStringLiteral("Исправление телефонных книг VCF")), 1, 0);
    gridLayout->addWidget(createMiniTile(QStringLiteral(":/svg/bot"), QStringLiteral("AI-Ассистент"), QStringLiteral("Перевод документов и умный чат")), 1, 1);

    layout->addWidget(gridCard);

    // 3. Minimalist Tech Spec Strip
    auto *specCard = new QFrame(container);
    specCard->setProperty("card", true);
    auto *specLayout = new QHBoxLayout(specCard);
    specLayout->setContentsMargins(12, 6, 12, 6);
    specLayout->setSpacing(8);

    const QStringList techBadges = {QStringLiteral("C++17"), QStringLiteral("Qt 6"), QStringLiteral("ADB Safe Engine"), QStringLiteral("Safe User Mode"), QStringLiteral("GNU GPL v3")};
    for(const QString &b : techBadges)
    {
        auto *lbl = new QLabel(b, specCard);
        lbl->setStyleSheet(QStringLiteral(
            "background-color: #070A12;"
            "color: #38BDF8;"
            "border: 1px solid #1E293B;"
            "border-radius: 0px;"
            "padding: 2px 7px;"
            "font-size: 10.5px;"
            "font-weight: 600;"));
        specLayout->addWidget(lbl);
    }
    specLayout->addStretch();
    layout->addWidget(specCard);

    // 4. Compact Links Row
    auto *linksCard = new QFrame(container);
    linksCard->setProperty("card", true);
    auto *linksLayout = new QHBoxLayout(linksCard);
    linksLayout->setContentsMargins(12, 6, 12, 6);
    linksLayout->setSpacing(10);

    auto *webBtn = new QPushButton(QStringLiteral("adskiller.imister.tech"), linksCard);
    webBtn->setIcon(QIcon(":/svg/globe"));
    webBtn->setIconSize(QSize(15, 15));
    connect(webBtn, &QPushButton::clicked, this, &AboutDialog::openProjectWebsite);
    linksLayout->addWidget(webBtn);

    auto *supportBtn = new QPushButton(QStringLiteral("WhatsApp Поддержка"), linksCard);
    supportBtn->setIcon(QIcon(":/svg/message-circle"));
    supportBtn->setIconSize(QSize(15, 15));
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
        QStringLiteral(
            "<b>Команда авторов и создатели проекта AdsKiller</b><br>"
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
        QStringLiteral("badcast &lt;anon&gt; • Разработка ядра и сервисов"));
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
        QStringLiteral("LeoJames &lt;anon&gt; • Дизайн и графическое оформление"));
    layout->addWidget(designerCard);

    // Author 3: imister.tech (Project Lead & Infrastructure)
    QWidget *leadProjectCard = createAuthorCard(
        QStringLiteral("IM"),
        QColor(QStringLiteral("#059669")),
        QColor(QStringLiteral("#047857")),
        QStringLiteral("Команда imister.tech"),
        QStringLiteral("Издатель & Инфраструктура / Project Lead"),
        QStringLiteral("#047857"),
        QStringLiteral(
            "• Концепция, развитие и выпуск официальных релизов программы AdsKiller.<br>"
            "• Облачная инфраструктура, серверная база сигнатур рекламных модулей и вредоносного ПО.<br>"
            "• Поддержка серверов автоматического обновления и клиентской базы данных.<br>"
            "• Веб-ресурсы: <a href=\"https://imister.tech\" style=\"color: #0078D4;\">imister.tech</a> "
            "и <a href=\"https://adskiller.imister.tech\" style=\"color: #0078D4;\">adskiller.imister.tech</a>."),
        QStringLiteral("imister.tech • Казахстан • Издатель и инфраструктура"));
    layout->addWidget(leadProjectCard);

    // Card 4: Acknowledgements
    auto *thanksCard = new QFrame(container);
    thanksCard->setProperty("card", true);
    auto *thanksLayout = new QVBoxLayout(thanksCard);
    thanksLayout->setContentsMargins(14, 12, 14, 12);
    thanksLayout->setSpacing(8);

    auto *thanksTitle = new QLabel(QStringLiteral("Благодарности сообществу"), thanksCard);
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

QWidget *AboutDialog::createAuthorCard(const QString &initials, const QColor &gradStart, const QColor &gradEnd, const QString &name, const QString &role, const QString &roleColor, const QString &description, const QString &contact)
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
                                 "border-radius: 0px;"
                                 "padding: 2px 7px;"
                                 "font-size: 10px;"
                                 "font-weight: 600;")
                                 .arg(roleColor));
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

    auto *summaryTitle = new QLabel(QStringLiteral("GNU General Public License, Version 3 (GPL v3)"), summaryCard);
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

    const struct
    {
        QString text;
        QString color;
    } chips[] = {
        {QStringLiteral("Свободное использование"), QStringLiteral("#10B981")},
        {QStringLiteral("Доступ к исходному коду"), QStringLiteral("#10B981")},
        {QStringLiteral("Модификация"), QStringLiteral("#10B981")},
        {QStringLiteral("Распространение"), QStringLiteral("#10B981")},
        {QStringLiteral("Copyleft (GPL v3)"), QStringLiteral("#0078D4")},
        {QStringLiteral("Без гарантий (AS IS)"), QStringLiteral("#F59E0B")}};

    for(const auto &chip : chips)
    {
        auto *chipLabel = new QLabel(chip.text, summaryCard);
        chipLabel->setStyleSheet(QStringLiteral(
                                     "background-color: rgba(127, 127, 127, 0.12);"
                                     "color: %1;"
                                     "border: 1px solid rgba(127, 127, 127, 0.25);"
                                     "border-radius: 0px;"
                                     "padding: 2px 7px;"
                                     "font-size: 10px;"
                                     "font-weight: 600;")
                                     .arg(chip.color));
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

    m_copyLicenseBtn = new QPushButton(QStringLiteral("Копировать текст лицензии"), widget);
    m_copyLicenseBtn->setIcon(QIcon(":/svg/copy"));
    m_copyLicenseBtn->setIconSize(QSize(15, 15));
    connect(m_copyLicenseBtn, &QPushButton::clicked, this, &AboutDialog::copyLicenseToClipboard);
    btnRow->addWidget(m_copyLicenseBtn);

    auto *fsfBtn = new QPushButton(QStringLiteral("Официальная страница gnu.org/licenses"), widget);
    fsfBtn->setIcon(QIcon(":/svg/globe"));
    fsfBtn->setIconSize(QSize(15, 15));
    connect(fsfBtn, &QPushButton::clicked, this, &AboutDialog::openGplWebsite);
    btnRow->addWidget(fsfBtn);

    btnRow->addStretch();
    layout->addLayout(btnRow);

    return widget;
}

QWidget *AboutDialog::createChangelogTab()
{
    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *container = new QWidget(scrollArea);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    // Top Card with Title, Source & Buttons
    auto *topCard = new QFrame(container);
    topCard->setProperty("card", true);
    auto *topLayout = new QHBoxLayout(topCard);
    topLayout->setContentsMargins(14, 12, 14, 12);
    topLayout->setSpacing(12);

    auto *topInfoLayout = new QVBoxLayout();
    topInfoLayout->setSpacing(3);

    auto *topTitle = new QLabel(QStringLiteral("Журнал версий и обновлений"), topCard);
    topTitle->setObjectName(QStringLiteral("cardTitle"));
    topInfoLayout->addWidget(topTitle);

    auto *topSub = new QLabel(QStringLiteral("История выпусков • Сервер: <a href=\"https://adskiller.imister.tech/changelog\" style=\"color:#0078D4; text-decoration:none;\">adskiller.imister.tech/changelog</a>"), topCard);
    topSub->setTextFormat(Qt::RichText);
    topSub->setOpenExternalLinks(true);
    topSub->setStyleSheet(QStringLiteral("color: rgba(127,127,127,0.9); font-size: 11px;"));
    topInfoLayout->addWidget(topSub);

    topLayout->addLayout(topInfoLayout, 1);

    m_changelogRefreshBtn = new QPushButton(QStringLiteral("Обновить"), topCard);
    m_changelogRefreshBtn->setIcon(QIcon(":/svg/refresh-cw"));
    m_changelogRefreshBtn->setIconSize(QSize(14, 14));
    m_changelogRefreshBtn->setCursor(Qt::PointingHandCursor);
    m_changelogRefreshBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "    background-color: rgba(127, 127, 127, 0.12);"
        "    border: 1px solid rgba(127, 127, 127, 0.25);"
        "    border-radius: 0px;"
        "    padding: 5px 12px;"
        "    font-size: 11.5px;"
        "    font-weight: 500;"
        "}"
        "QPushButton:hover {"
        "    background-color: rgba(0, 120, 212, 0.2);"
        "    border-color: #0078D4;"
        "    color: #0078D4;"
        "}"
        "QPushButton:pressed {"
        "    background-color: rgba(0, 120, 212, 0.35);"
        "}"));
    connect(m_changelogRefreshBtn, &QPushButton::clicked, this, &AboutDialog::fetchChangelog);
    topLayout->addWidget(m_changelogRefreshBtn);

    auto *webBtn = new QPushButton(QStringLiteral("В браузере"), topCard);
    webBtn->setIcon(QIcon(":/svg/globe"));
    webBtn->setIconSize(QSize(14, 14));
    webBtn->setCursor(Qt::PointingHandCursor);
    webBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "    background-color: rgba(127, 127, 127, 0.12);"
        "    border: 1px solid rgba(127, 127, 127, 0.25);"
        "    border-radius: 0px;"
        "    padding: 5px 12px;"
        "    font-size: 11.5px;"
        "    font-weight: 500;"
        "}"
        "QPushButton:hover {"
        "    background-color: rgba(0, 120, 212, 0.2);"
        "    border-color: #0078D4;"
        "    color: #0078D4;"
        "}"
        "QPushButton:pressed {"
        "    background-color: rgba(0, 120, 212, 0.35);"
        "}"));
    connect(webBtn, &QPushButton::clicked, this, &AboutDialog::openChangelogWebsite);
    topLayout->addWidget(webBtn);

    layout->addWidget(topCard);

    // Status Banner Widget (for Loading / Error notifications)
    m_changelogStatusWidget = new QFrame(container);
    m_changelogStatusWidget->setProperty("card", true);
    auto *statusLayout = new QHBoxLayout(m_changelogStatusWidget);
    statusLayout->setContentsMargins(14, 10, 14, 10);
    statusLayout->setSpacing(10);

    m_changelogStatusLabel = new QLabel(m_changelogStatusWidget);
    m_changelogStatusLabel->setTextFormat(Qt::RichText);
    m_changelogStatusLabel->setWordWrap(true);
    statusLayout->addWidget(m_changelogStatusLabel, 1);

    m_changelogRetryBtn = new QPushButton(QStringLiteral("Повторить"), m_changelogStatusWidget);
    m_changelogRetryBtn->setCursor(Qt::PointingHandCursor);
    m_changelogRetryBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "    background-color: #0078D4;"
        "    color: #FFFFFF;"
        "    border: none;"
        "    border-radius: 0px;"
        "    padding: 4px 12px;"
        "    font-size: 11px;"
        "    font-weight: 600;"
        "}"
        "QPushButton:hover { background-color: #1084E3; }"));
    connect(m_changelogRetryBtn, &QPushButton::clicked, this, &AboutDialog::fetchChangelog);
    statusLayout->addWidget(m_changelogRetryBtn);

    m_changelogStatusWidget->hide();
    layout->addWidget(m_changelogStatusWidget);

    // Container for list of version cards
    auto *listContainer = new QWidget(container);
    listContainer->setStyleSheet(QStringLiteral("background: transparent;"));
    m_changelogListLayout = new QVBoxLayout(listContainer);
    m_changelogListLayout->setContentsMargins(0, 0, 0, 0);
    m_changelogListLayout->setSpacing(10);

    layout->addWidget(listContainer);
    layout->addStretch();

    scrollArea->setWidget(container);

    // Trigger asynchronous fetch
    QTimer::singleShot(50, this, &AboutDialog::fetchChangelog);

    return scrollArea;
}

void AboutDialog::openChangelogWebsite()
{
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://adskiller.imister.tech/changelog")));
}

void AboutDialog::fetchChangelog()
{
    if(!m_netManager)
    {
        m_netManager = new QNetworkAccessManager(this);
    }

    showChangelogLoading();

    QNetworkRequest request(QUrl(QStringLiteral("https://adskiller.imister.tech/changelog")));
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("AdsKiller-Desktop/%1.%2.%3").arg(AppVerMajor).arg(AppVerMinor).arg(AppVerPatch));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(6000);

    QNetworkReply *reply = m_netManager->get(request);
    connect(
        reply,
        &QNetworkReply::finished,
        this,
        [this, reply]()
        {
            reply->deleteLater();
            if(m_changelogRefreshBtn)
                m_changelogRefreshBtn->setEnabled(true);

            if(reply->error() == QNetworkReply::NoError)
            {
                QByteArray data = reply->readAll();
                QJsonParseError parseErr;
                QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
                if(parseErr.error == QJsonParseError::NoError)
                {
                    QJsonArray arr;
                    if(doc.isArray())
                    {
                        arr = doc.array();
                    }
                    else if(doc.isObject())
                    {
                        QJsonObject root = doc.object();
                        if(root.contains(QStringLiteral("changelog")) && root[QStringLiteral("changelog")].isArray())
                            arr = root[QStringLiteral("changelog")].toArray();
                        else if(root.contains(QStringLiteral("data")) && root[QStringLiteral("data")].isArray())
                            arr = root[QStringLiteral("data")].toArray();
                        else if(root.contains(QStringLiteral("releases")) && root[QStringLiteral("releases")].isArray())
                            arr = root[QStringLiteral("releases")].toArray();
                    }

                    if(!arr.isEmpty())
                    {
                        if(m_changelogStatusWidget)
                            m_changelogStatusWidget->hide();
                        renderChangelog(arr);
                        return;
                    }
                }
                showChangelogError(QStringLiteral("Сервер вернул пустой или некорректный формат списка изменений."));
            }
            else
            {
                showChangelogError(QStringLiteral("Не удалось связаться с сервером обновлений (%1).").arg(reply->errorString()));
            }

            // Render offline fallback releases so tab is never empty
            renderChangelog(getFallbackChangelog());
        });
}

void AboutDialog::showChangelogLoading()
{
    if(m_changelogRefreshBtn)
        m_changelogRefreshBtn->setEnabled(false);

    if(m_changelogStatusWidget && m_changelogStatusLabel)
    {
        m_changelogStatusLabel->setText(QStringLiteral(
            "<img src=\":/svg/clock\" width=\"13\" height=\"13\" style=\"vertical-align: middle;\"/> "
            "<span style=\"color:#0078D4; font-weight:600;\">Загрузка...</span> "
            "Получение актуального списка изменений с adskiller.imister.tech/changelog"));
        if(m_changelogRetryBtn)
            m_changelogRetryBtn->hide();
        m_changelogStatusWidget->setStyleSheet(QStringLiteral("QFrame { background-color: rgba(0, 120, 212, 0.08); border: 1px solid rgba(0, 120, 212, 0.25); border-radius: 0px; }"));
        m_changelogStatusWidget->show();
    }
}

void AboutDialog::showChangelogError(const QString &errorMsg)
{
    if(m_changelogStatusWidget && m_changelogStatusLabel)
    {
        m_changelogStatusLabel->setText(QStringLiteral(
                                            "<img src=\":/svg/alert-triangle\" width=\"13\" height=\"13\" style=\"vertical-align: middle;\"/> "
                                            "<span style=\"color:#EF4444; font-weight:bold;\">Ошибка соединения:</span> %1<br>"
                                            "<span style=\"color:rgba(127,127,127,0.9); font-size:11px;\">Ниже показаны встроенные сведения о релизе (автономный режим).</span>")
                                            .arg(errorMsg));
        if(m_changelogRetryBtn)
            m_changelogRetryBtn->show();
        m_changelogStatusWidget->setStyleSheet(QStringLiteral("QFrame { background-color: rgba(239, 68, 68, 0.08); border: 1px solid rgba(239, 68, 68, 0.25); border-radius: 0px; }"));
        m_changelogStatusWidget->show();
    }
}

QString AboutDialog::formatChangelogText(const QString &raw)
{
    if(raw.isEmpty())
        return QStringLiteral("<i>Нет описания изменений.</i>");

    if(raw.contains(QStringLiteral("<p>")) || raw.contains(QStringLiteral("<br>")) || raw.contains(QStringLiteral("<div>")))
        return raw;

    QString s = raw.toHtmlEscaped();
    s.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));

    // Bold **text**
    static QRegularExpression boldRe(QStringLiteral(R"(\*\*(.+?)\*\*)"));
    s.replace(boldRe, QStringLiteral("<b style=\"color:#FFFFFF;\">\\1</b>"));

    // Code `code`
    static QRegularExpression codeRe(QStringLiteral(R"(`([^`]+)`)"));
    s.replace(codeRe, QStringLiteral("<code style=\"background:rgba(127,127,127,0.15); padding:1px 5px; border-radius: 0px; font-family:monospace;\">\\1</code>"));

    // Tag pills
    static QRegularExpression tagNew(QStringLiteral(R"(\[(New|Новое|Добавлено)\])"), QRegularExpression::CaseInsensitiveOption);
    s.replace(tagNew, QStringLiteral("<span style=\"background:rgba(16,185,129,0.2); color:#10B981; border:1px solid rgba(16,185,129,0.4); border-radius: 0px; padding:1px 6px; font-size:10px; font-weight:bold;\">НОВОЕ</span>"));

    static QRegularExpression tagFix(QStringLiteral(R"(\[(Fix|Исправлено|Исправление)\])"), QRegularExpression::CaseInsensitiveOption);
    s.replace(tagFix, QStringLiteral("<span style=\"background:rgba(59,130,246,0.2); color:#3B82F6; border:1px solid rgba(59,130,246,0.4); border-radius: 0px; padding:1px 6px; font-size:10px; font-weight:bold;\">ИСПРАВЛЕНО</span>"));

    static QRegularExpression tagOpt(QStringLiteral(R"(\[(Opt|Optimized|Improvement|Улучшено|Улучшение)\])"), QRegularExpression::CaseInsensitiveOption);
    s.replace(tagOpt, QStringLiteral("<span style=\"background:rgba(245,158,11,0.2); color:#F59E0B; border:1px solid rgba(245,158,11,0.4); border-radius: 0px; padding:1px 6px; font-size:10px; font-weight:bold;\">УЛУЧШЕНИЕ</span>"));

    static QRegularExpression tagUi(QStringLiteral(R"(\[(UI|Дизайн|Интерфейс)\])"), QRegularExpression::CaseInsensitiveOption);
    s.replace(tagUi, QStringLiteral("<span style=\"background:rgba(168,85,247,0.2); color:#A855F7; border:1px solid rgba(168,85,247,0.4); border-radius: 0px; padding:1px 6px; font-size:10px; font-weight:bold;\">ДИЗАЙН</span>"));

    static QRegularExpression tagSec(QStringLiteral(R"(\[(Sec|Security|Безопасность)\])"), QRegularExpression::CaseInsensitiveOption);
    s.replace(tagSec, QStringLiteral("<span style=\"background:rgba(239,68,68,0.2); color:#EF4444; border:1px solid rgba(239,68,68,0.4); border-radius: 0px; padding:1px 6px; font-size:10px; font-weight:bold;\">БЕЗОПАСНОСТЬ</span>"));

    // Bullets (- or * at start of line)
    static QRegularExpression bulletRe(QStringLiteral(R"((?:^|\n)[-*•]\s+(.+))"));
    s.replace(bulletRe, QStringLiteral("<div style=\"margin: 2px 0 2px 10px;\"><span style=\"color:#0078D4;\">•</span> &nbsp;\\1</div>"));

    s.replace(QStringLiteral("\n\n"), QStringLiteral("<div style=\"height:6px;\"></div>"));
    s.replace(QStringLiteral("\n"), QStringLiteral("<br/>"));

    return QStringLiteral("<div style=\"line-height: 1.5; font-size: 11.5px;\">%1</div>").arg(s);
}

void AboutDialog::renderChangelog(const QJsonArray &entries)
{
    if(!m_changelogListLayout)
        return;

    // Clear old items
    QLayoutItem *item;
    while((item = m_changelogListLayout->takeAt(0)) != nullptr)
    {
        if(item->widget())
            delete item->widget();
        delete item;
    }

    const QString currentVerStr = QStringLiteral("%1.%2.%3").arg(AppVerMajor).arg(AppVerMinor).arg(AppVerPatch);

    for(int i = 0; i < entries.size(); ++i)
    {
        QJsonValue val = entries[i];
        if(!val.isObject())
            continue;

        QJsonObject obj = val.toObject();

        QString version = obj.value(QStringLiteral("version")).toString();
        if(version.isEmpty())
            version = obj.value(QStringLiteral("ver")).toString();
        if(version.isEmpty())
            version = obj.value(QStringLiteral("tag")).toString();
        if(version.isEmpty())
            version = QStringLiteral("1.0.0");

        QString changelog = obj.value(QStringLiteral("changelog")).toString();
        if(changelog.isEmpty())
            changelog = obj.value(QStringLiteral("changes")).toString();
        if(changelog.isEmpty())
            changelog = obj.value(QStringLiteral("notes")).toString();
        if(changelog.isEmpty())
            changelog = obj.value(QStringLiteral("description")).toString();
        if(changelog.isEmpty())
            changelog = obj.value(QStringLiteral("body")).toString();

        QString date = obj.value(QStringLiteral("date")).toString();
        if(date.isEmpty())
            date = obj.value(QStringLiteral("released_at")).toString();
        if(date.isEmpty())
            date = obj.value(QStringLiteral("created_at")).toString();

        auto *card = new QFrame();
        card->setProperty("card", true);
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(14, 12, 14, 12);
        cardLayout->setSpacing(8);

        // Header row
        auto *headerLayout = new QHBoxLayout();
        headerLayout->setSpacing(8);

        QString verDisplay = version.startsWith(QLatin1Char('v')) ? version : QStringLiteral("v%1").arg(version);
        auto *verBadge = new QLabel(verDisplay, card);
        verBadge->setStyleSheet(QStringLiteral(
            "background-color: #0284C7;"
            "color: #FFFFFF;"
            "font-weight: bold;"
            "font-size: 11.5px;"
            "border-radius: 0px;"
            "padding: 3px 9px;"));
        headerLayout->addWidget(verBadge);

        QString cleanVer = version;
        if(cleanVer.startsWith(QLatin1Char('v')))
            cleanVer = cleanVer.mid(1);

        if(cleanVer == currentVerStr)
        {
            auto *curBadge = new QLabel(QStringLiteral("ТЕКУЩАЯ ВЕРСИЯ"), card);
            curBadge->setStyleSheet(QStringLiteral(
                "background-color: rgba(16, 185, 129, 0.18);"
                "color: #10B981;"
                "border: 1px solid rgba(16, 185, 129, 0.40);"
                "border-radius: 0px;"
                "padding: 2px 8px;"
                "font-size: 10px;"
                "font-weight: 600;"));
            headerLayout->addWidget(curBadge);
        }

        if(!date.isEmpty())
        {
            auto *dateLabel = new QLabel(card);
            dateLabel->setTextFormat(Qt::RichText);
            dateLabel->setText(QStringLiteral("<img src=\":/svg/calendar\" width=\"12\" height=\"12\" style=\"vertical-align: middle;\"/> %1").arg(date));
            dateLabel->setStyleSheet(QStringLiteral("color: rgba(127, 127, 127, 0.85); font-size: 11px;"));
            headerLayout->addWidget(dateLabel);
        }

        headerLayout->addStretch();
        cardLayout->addLayout(headerLayout);

        // Subtle separator
        auto *sep = new QFrame(card);
        sep->setFrameShape(QFrame::HLine);
        sep->setStyleSheet(QStringLiteral("border: none; border-top: 1px solid rgba(127, 127, 127, 0.15); margin: 2px 0;"));
        cardLayout->addWidget(sep);

        // Body content
        auto *contentLabel = new QLabel(card);
        contentLabel->setTextFormat(Qt::RichText);
        contentLabel->setText(formatChangelogText(changelog));
        contentLabel->setWordWrap(true);
        contentLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
        contentLabel->setOpenExternalLinks(true);
        cardLayout->addWidget(contentLabel);

        m_changelogListLayout->addWidget(card);
    }
}

QJsonArray AboutDialog::getFallbackChangelog()
{
    QJsonArray arr;
    QJsonObject v1;
    v1[QStringLiteral("version")] = QStringLiteral("%1.%2.%3").arg(AppVerMajor).arg(AppVerMinor).arg(AppVerPatch);
    v1[QStringLiteral("date")] = QStringLiteral("2026-09-01");
    v1[QStringLiteral("changelog")] = QStringLiteral("[New] Официальный стабильный релиз утилиты AdsKiller для десктопа.");
    arr.append(v1);
    return arr;
}

QWidget *AboutDialog::createFooterWidget()
{
    auto *footerWidget = new QWidget(this);
    auto *footerLayout = new QHBoxLayout(footerWidget);
    footerLayout->setContentsMargins(0, 0, 0, 0);

    auto *siteLabel = new QLabel(
        QStringLiteral(
            "<a href=\"https://adskiller.imister.tech\" style=\"text-decoration: none; color: #38BDF8; font-weight: 600;\">"
            "<img src=\":/svg/globe\" width=\"12\" height=\"12\" style=\"vertical-align: middle;\"/> adskiller.imister.tech</a>"),
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

QPixmap AboutDialog::createAvatarPixmap(const QString &initials, const QColor &startColor, const QColor &endColor, int size)
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
    const QStringList candidatePaths = {QStringLiteral("LICENSE"), QCoreApplication::applicationDirPath() + QStringLiteral("/LICENSE"), QCoreApplication::applicationDirPath() + QStringLiteral("/../LICENSE"), QStringLiteral("/media/dev/adskiller/LICENSE")};

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
        "https://www.gnu.org/licenses/gpl-3.0.html\n");
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
        m_copyLicenseBtn->setIcon(QIcon(":/svg/check"));
        m_copyLicenseBtn->setText(QStringLiteral(" Скопировано в буфер обмена!"));
        m_copyLicenseBtn->setEnabled(false);

        QTimer::singleShot(
            2500,
            this,
            [this, origText]()
            {
                if(m_copyLicenseBtn)
                {
                    m_copyLicenseBtn->setIcon(QIcon(":/svg/copy"));
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
