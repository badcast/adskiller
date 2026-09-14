#include "RadioPlayerWidget.h"

#include <QInputDialog>
#include <QMessageBox>
#include <QPainter>
#include <QStyle>
#include <QFontMetrics>

RadioPlayerWidget::RadioPlayerWidget(QWidget *parent) : QFrame(parent)
{
    setObjectName("RadioPlayerWidget");

    m_audioOutput = new QAudioOutput(this);
    m_player = new QMediaPlayer(this);
    m_player->setAudioOutput(m_audioOutput);

    connect(m_player, &QMediaPlayer::playbackStateChanged, this, &RadioPlayerWidget::onPlayerPlaybackStateChanged);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &RadioPlayerWidget::onPlayerMediaStatusChanged);
    connect(m_player, &QMediaPlayer::errorOccurred, this, &RadioPlayerWidget::onPlayerErrorOccurred);
    connect(m_player, &QMediaPlayer::metaDataChanged, this, &RadioPlayerWidget::onPlayerMetaDataChanged);

    populateDefaultStations();
    initUi();
    loadSettings();
}

RadioPlayerWidget::~RadioPlayerWidget()
{
    saveSettings();
    if(m_player)
    {
        m_player->stop();
    }
}

void RadioPlayerWidget::populateDefaultStations()
{
    m_stations.clear();
    m_stations.append({QString::fromUtf8("Tengri FM"), QStringLiteral("https://stream.tengrifm.kz/tengrifm.mp3"), QString::fromUtf8("Rock / Modern Pop")});
    m_stations.append({QString::fromUtf8("Жұлдыз FM"), QStringLiteral("https://stream.zhuldyzfm.kz/zhuldyzfm.mp3"), QString::fromUtf8("Kazakh Pop / Folk")});
    m_stations.append({QString::fromUtf8("Radio NS"), QStringLiteral("https://online.ns.kz/live"), QString::fromUtf8("Pop / Hits")});
    m_stations.append({QString::fromUtf8("Gakku FM"), QStringLiteral("https://air.gakku.tv/gakku128.mp3"), QString::fromUtf8("Kazakh Modern Pop")});
    m_stations.append({QString::fromUtf8("Русское Радио Азия"), QStringLiteral("https://stream.rusradio.kz/rusradio_128"), QString::fromUtf8("Pop Hits")});
    m_stations.append({QString::fromUtf8("Любимое Радио"), QStringLiteral("https://stream.lr.kz/live"), QString::fromUtf8("Retro / Pop")});
    m_stations.append({QString::fromUtf8("Монте-Карло"), QStringLiteral("https://montecarlo.hostingradio.ru/montecarlo128.mp3"), QString::fromUtf8("Lounge Pop")});
    m_stations.append({QString::fromUtf8("Lofi Girl"), QStringLiteral("https://play.streamafrica.net/lofigirl"), QString::fromUtf8("Lo-Fi Beats")});
    m_stations.append({QString::fromUtf8("Radio Record"), QStringLiteral("https://radiorecord.hostingradio.ru/rr_96.aacp"), QString::fromUtf8("Dance / Club")});
    m_stations.append({QString::fromUtf8("DFM"), QStringLiteral("https://dfm.hostingradio.ru/dfm96.aacp"), QString::fromUtf8("Dance / Pop")});
    m_stations.append({QString::fromUtf8("Европа Плюс"), QStringLiteral("https://ep.hostingradio.ru/europaplus128.mp3"), QString::fromUtf8("Top 40")});
    m_stations.append({QString::fromUtf8("Ретро FM"), QStringLiteral("https://retro.hostingradio.ru/retro128.mp3"), QString::fromUtf8("Retro 80-90s")});
    m_stations.append({QString::fromUtf8("Relax FM"), QStringLiteral("https://pub0302.101.ru:8443/stream/air/aac/64/200"), QString::fromUtf8("Lounge / Chill")});
    m_stations.append({QString::fromUtf8("Радио Jazz"), QStringLiteral("https://jazz.hostingradio.ru/jazz-128.mp3"), QString::fromUtf8("Jazz & Blues")});
    m_stations.append({QString::fromUtf8("Rock FM"), QStringLiteral("https://nashe1.hostingradio.ru/rock-128.mp3"), QString::fromUtf8("Classic Rock")});
    m_stations.append({QString::fromUtf8("NRJ Energy"), QStringLiteral("https://pub0302.101.ru:8443/stream/air/aac/64/99"), QString::fromUtf8("Modern Hits")});

    // Load custom user stations from QSettings
    QSettings settings("AdsKiller", "RadioStations");
    int customCount = settings.beginReadArray("CustomStations");
    for(int i = 0; i < customCount; ++i)
    {
        settings.setArrayIndex(i);
        QString name = settings.value("name").toString();
        QString url = settings.value("url").toString();
        QString genre = settings.value("genre", QString::fromUtf8("Пользовательская")).toString();
        if(!name.isEmpty() && !url.isEmpty())
        {
            m_stations.append({name, url, genre});
        }
    }
    settings.endArray();
}

void RadioPlayerWidget::initUi()
{
    setFixedHeight(34);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Sleek Cyber/Obsidian styling for the top radio player dock
    setStyleSheet(
        "QFrame#RadioPlayerWidget {"
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #0F172A, stop:1 #070B14);"
        "   border-bottom: 1px solid #1E293B;"
        "   border-top: 1px solid #1E293B;"
        "}"
        "QLabel {"
        "   color: #94A3B8;"
        "   font-size: 10.5px;"
        "}"
        "QComboBox {"
        "   background-color: #0F172A;"
        "   color: #F8FAFC;"
        "   border: 1px solid #334155;"
        "   border-radius: 0px;"
        "   padding: 1px 8px;"
        "   font-size: 11px;"
        "   font-weight: 600;"
        "   min-width: 140px;"
        "   max-width: 200px;"
        "   height: 24px;"
        "}"
        "QComboBox:hover {"
        "   border-color: #38BDF8;"
        "   background-color: #131E35;"
        "}"
        "QComboBox::drop-down {"
        "   subcontrol-origin: padding;"
        "   subcontrol-position: top right;"
        "   width: 18px;"
        "   border-left: 1px solid #334155;"
        "   border-radius: 0px;"
        "}"
        "QComboBox QAbstractItemView {"
        "   background-color: #0F172A;"
        "   color: #F8FAFC;"
        "   selection-background-color: #0284C7;"
        "   selection-color: #FFFFFF;"
        "   border: 1px solid #38BDF8;"
        "   padding: 4px;"
        "}"
        "QPushButton#btnAddStation {"
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #132238, stop:1 #0B1728);"
        "   color: #38BDF8;"
        "   border: 1px solid #1E3A5F;"
        "   border-radius: 0px;"
        "   font-size: 12px;"
        "   font-weight: bold;"
        "   min-width: 26px;"
        "   max-width: 26px;"
        "   min-height: 24px;"
        "   max-height: 24px;"
        "   padding: 0px;"
        "}"
        "QPushButton#btnAddStation:hover {"
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #1E3B63, stop:1 #132845);"
        "   border-color: #38BDF8;"
        "   color: #FFFFFF;"
        "}"
        "QPushButton#btnAddStation:pressed {"
        "   background-color: #08111D;"
        "   border-color: #0284C7;"
        "}"
        "QPushButton#btnPrev, QPushButton#btnNext {"
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #1E293B, stop:1 #0F172A);"
        "   color: #E2E8F0;"
        "   border: 1px solid #334155;"
        "   border-radius: 0px;"
        "   font-size: 11px;"
        "   font-weight: bold;"
        "   min-width: 28px;"
        "   max-width: 28px;"
        "   min-height: 24px;"
        "   max-height: 24px;"
        "   padding: 0px;"
        "}"
        "QPushButton#btnPrev:hover, QPushButton#btnNext:hover {"
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #2C3E55, stop:1 #1A283C);"
        "   border-color: #38BDF8;"
        "   color: #38BDF8;"
        "}"
        "QPushButton#btnPrev:pressed, QPushButton#btnNext:pressed {"
        "   background-color: #0A1120;"
        "   border-color: #0284C7;"
        "   color: #0284C7;"
        "}"
        "QPushButton#btnPlay {"
        "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0284C7, stop:1 #0EA5E9);"
        "   border: 1px solid #38BDF8;"
        "   color: #FFFFFF;"
        "   font-size: 12px;"
        "   font-weight: 800;"
        "   border-radius: 0px;"
        "   min-width: 38px;"
        "   max-width: 38px;"
        "   min-height: 24px;"
        "   max-height: 24px;"
        "   padding: 0px;"
        "}"
        "QPushButton#btnPlay:hover {"
        "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0369A1, stop:1 #38BDF8);"
        "   border-color: #BAE6FD;"
        "   color: #FFFFFF;"
        "}"
        "QPushButton#btnPlay:pressed {"
        "   background-color: #034E7B;"
        "   border-color: #0284C7;"
        "}"
        "QPushButton#btnMute {"
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #1E293B, stop:1 #0F172A);"
        "   color: #38BDF8;"
        "   border: 1px solid #334155;"
        "   border-radius: 0px;"
        "   font-size: 11px;"
        "   min-width: 28px;"
        "   max-width: 28px;"
        "   min-height: 24px;"
        "   max-height: 24px;"
        "   padding: 0px;"
        "}"
        "QPushButton#btnMute:hover {"
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #2C3E55, stop:1 #1A283C);"
        "   border-color: #38BDF8;"
        "   color: #7DD3FC;"
        "}"
        "QPushButton#btnMute:pressed {"
        "   background-color: #0A1120;"
        "   border-color: #0284C7;"
        "}"
        "QPushButton#btnClose {"
        "   background: transparent;"
        "   color: #64748B;"
        "   border: 1px solid transparent;"
        "   border-radius: 0px;"
        "   font-size: 11px;"
        "   font-weight: bold;"
        "   min-width: 22px;"
        "   max-width: 22px;"
        "   min-height: 24px;"
        "   max-height: 24px;"
        "   padding: 0px;"
        "}"
        "QPushButton#btnClose:hover {"
        "   background-color: #2E1218;"
        "   border-color: #991B1B;"
        "   color: #EF4444;"
        "}"
        "QPushButton#btnClose:pressed {"
        "   background-color: #4C1D24;"
        "   border-color: #DC2626;"
        "}"
        "QSlider::groove:horizontal {"
        "   height: 4px;"
        "   background: #111827;"
        "   border: 1px solid #1E293B;"
        "   border-radius: 0px;"
        "}"
        "QSlider::sub-page:horizontal {"
        "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0284C7, stop:1 #38BDF8);"
        "   border-radius: 0px;"
        "}"
        "QSlider::handle:horizontal {"
        "   background: #F8FAFC;"
        "   border: 1px solid #0284C7;"
        "   width: 8px;"
        "   margin-top: -5px;"
        "   margin-bottom: -5px;"
        "   border-radius: 0px;"
        "}"
        "QSlider::handle:horizontal:hover {"
        "   background: #38BDF8;"
        "   border-color: #BAE6FD;"
        "}");

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 2, 8, 2);
    layout->setSpacing(6);

    // 1. Logo / Radio badge
    m_lblLogo = new QLabel(QString::fromUtf8("<img src=\":/svg/radio\" width=\"13\" height=\"13\" style=\"vertical-align: middle;\"/> <b>FM LIVE</b>"), this);
    m_lblLogo->setObjectName("lblRadioLogo");
    m_lblLogo->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0B1E36, stop:1 #0F2744); color: #38BDF8; border: 1px solid #0284C7; border-radius: 0px; font-size: 10px; font-weight: 800; letter-spacing: 0.8px; padding: 3px 8px;");
    layout->addWidget(m_lblLogo);

    // 2. Station Selector Combo
    m_comboStations = new QComboBox(this);
    m_comboStations->setFixedHeight(24);
    for(const auto &station : m_stations)
    {
        m_comboStations->addItem(QStringLiteral("%1 (%2)").arg(station.name, station.genre));
    }
    connect(m_comboStations, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RadioPlayerWidget::onStationComboChanged);
    layout->addWidget(m_comboStations);

    // 3. Add Custom Station Button
    m_btnAddStation = new QPushButton(this);
    m_btnAddStation->setObjectName("btnAddStation");
    m_btnAddStation->setIcon(QIcon(":/svg/plus"));
    m_btnAddStation->setIconSize(QSize(13, 13));
    m_btnAddStation->setToolTip(QString::fromUtf8("Добавить свой поток радио"));
    m_btnAddStation->setFixedSize(26, 24);
    m_btnAddStation->setCursor(Qt::PointingHandCursor);
    connect(m_btnAddStation, &QPushButton::clicked, this, &RadioPlayerWidget::showAddStationDialog);
    layout->addWidget(m_btnAddStation);

    // 4. Playback Controls (Prev, Play/Pause, Next)
    m_btnPrev = new QPushButton(this);
    m_btnPrev->setObjectName("btnPrev");
    m_btnPrev->setIcon(QIcon(":/svg/skip-back"));
    m_btnPrev->setIconSize(QSize(13, 13));
    m_btnPrev->setToolTip(QString::fromUtf8("Предыдущая станция"));
    m_btnPrev->setFixedSize(28, 24);
    m_btnPrev->setCursor(Qt::PointingHandCursor);
    connect(m_btnPrev, &QPushButton::clicked, this, &RadioPlayerWidget::previousStation);
    layout->addWidget(m_btnPrev);

    m_btnPlay = new QPushButton(this);
    m_btnPlay->setObjectName("btnPlay");
    m_btnPlay->setIcon(QIcon(":/svg/play"));
    m_btnPlay->setIconSize(QSize(13, 13));
    m_btnPlay->setToolTip(QString::fromUtf8("Включить радио (Пробел)"));
    m_btnPlay->setFixedSize(38, 24);
    m_btnPlay->setCursor(Qt::PointingHandCursor);
    connect(m_btnPlay, &QPushButton::clicked, this, &RadioPlayerWidget::togglePlay);
    layout->addWidget(m_btnPlay);

    m_btnNext = new QPushButton(this);
    m_btnNext->setObjectName("btnNext");
    m_btnNext->setIcon(QIcon(":/svg/skip-forward"));
    m_btnNext->setIconSize(QSize(13, 13));
    m_btnNext->setToolTip(QString::fromUtf8("Следующая станция"));
    m_btnNext->setFixedSize(28, 24);
    m_btnNext->setCursor(Qt::PointingHandCursor);
    connect(m_btnNext, &QPushButton::clicked, this, &RadioPlayerWidget::nextStation);
    layout->addWidget(m_btnNext);

    // 5. Status indicator badge
    m_lblStatus = new QLabel(QString::fromUtf8("СТОП"), this);
    m_lblStatus->setObjectName("lblRadioStatus");
    m_lblStatus->setStyleSheet("background-color: #111827; border: 1px solid #334155; color: #94A3B8; font-size: 10px; font-weight: 600; padding: 2px 7px; border-radius: 0px;");
    m_lblStatus->setMinimumWidth(72);
    m_lblStatus->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_lblStatus);

    // 6. Track / Stream info label (expands)
    m_lblTrackInfo = new QLabel(QString::fromUtf8("Выберите станцию и нажмите «Включить радио»"), this);
    m_lblTrackInfo->setStyleSheet("color: #E2E8F0; font-size: 11px; font-weight: 500; padding-left: 4px;");
    m_lblTrackInfo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_lblTrackInfo->setTextInteractionFlags(Qt::NoTextInteraction);
    layout->addWidget(m_lblTrackInfo, 1);

    // 7. Volume controls
    m_btnMute = new QPushButton(this);
    m_btnMute->setObjectName("btnMute");
    m_btnMute->setIcon(QIcon(":/svg/volume"));
    m_btnMute->setIconSize(QSize(14, 14));
    m_btnMute->setToolTip(QString::fromUtf8("Выключить звук"));
    m_btnMute->setFixedSize(28, 24);
    m_btnMute->setCursor(Qt::PointingHandCursor);
    connect(m_btnMute, &QPushButton::clicked, this, &RadioPlayerWidget::toggleMute);
    layout->addWidget(m_btnMute);

    m_sliderVolume = new QSlider(Qt::Horizontal, this);
    m_sliderVolume->setRange(0, 100);
    m_sliderVolume->setValue(70);
    m_sliderVolume->setFixedSize(70, 18);
    m_sliderVolume->setCursor(Qt::PointingHandCursor);
    m_sliderVolume->setToolTip(QString::fromUtf8("Громкость радио"));
    connect(m_sliderVolume, &QSlider::valueChanged, this, &RadioPlayerWidget::onVolumeSliderChanged);
    layout->addWidget(m_sliderVolume);

    m_lblVolumePercent = new QLabel(QStringLiteral("70%"), this);
    m_lblVolumePercent->setFixedWidth(30);
    m_lblVolumePercent->setStyleSheet("color: #38BDF8; font-size: 10px; font-weight: bold; padding-left: 2px;");
    layout->addWidget(m_lblVolumePercent);

    // 8. Close / Hide button
    m_btnClose = new QPushButton(this);
    m_btnClose->setObjectName("btnClose");
    m_btnClose->setIcon(QIcon(":/svg/close"));
    m_btnClose->setIconSize(QSize(11, 11));
    m_btnClose->setToolTip(QString::fromUtf8("Скрыть панель радио"));
    m_btnClose->setFixedSize(22, 24);
    m_btnClose->setCursor(Qt::PointingHandCursor);
    connect(m_btnClose, &QPushButton::clicked, this, &RadioPlayerWidget::requestClose);
    layout->addWidget(m_btnClose);
}

void RadioPlayerWidget::loadSettings()
{
    QSettings settings("AdsKiller", "RadioPlayer");
    int vol = settings.value("Volume", 70).toInt();
    bool muted = settings.value("Muted", false).toBool();
    int stationIdx = settings.value("LastStationIndex", 0).toInt();

    m_sliderVolume->setValue(vol);
    m_audioOutput->setVolume(static_cast<float>(vol) / 100.0f);
    m_audioOutput->setMuted(muted);
    updateVolumeUi(vol, muted);

    if(stationIdx >= 0 && stationIdx < m_comboStations->count())
    {
        m_comboStations->setCurrentIndex(stationIdx);
    }
}

void RadioPlayerWidget::saveSettings()
{
    QSettings settings("AdsKiller", "RadioPlayer");
    settings.setValue("Volume", m_sliderVolume->value());
    settings.setValue("Muted", m_audioOutput->isMuted());
    settings.setValue("LastStationIndex", m_comboStations->currentIndex());
}

bool RadioPlayerWidget::isPlaying() const
{
    return m_player && (m_player->playbackState() == QMediaPlayer::PlayingState);
}

int RadioPlayerWidget::volume() const
{
    return m_sliderVolume ? m_sliderVolume->value() : 70;
}

QString RadioPlayerWidget::currentStationName() const
{
    int idx = currentStationIndex();
    if(idx >= 0 && idx < m_stations.size())
    {
        return m_stations[idx].name;
    }
    return QString();
}

int RadioPlayerWidget::currentStationIndex() const
{
    return m_comboStations ? m_comboStations->currentIndex() : -1;
}

void RadioPlayerWidget::play()
{
    int idx = currentStationIndex();
    if(idx < 0 || idx >= m_stations.size())
        return;

    const auto &station = m_stations[idx];
    QUrl url(station.url);
    if(m_player->source() != url)
    {
        m_player->setSource(url);
    }
    m_player->play();
    m_lblStatus->setText(QString::fromUtf8("СОЕДИНЕНИЕ..."));
    m_lblStatus->setStyleSheet("background-color: #2D2006; border: 1px solid #D97706; color: #FCD34D; font-size: 10px; font-weight: bold; padding: 1px 6px; border-radius: 0px;");
    m_lblTrackInfo->setText(QStringLiteral("%1 • Подключение к эфиру...").arg(station.name));
    updatePlayButtonState(true);
    emit playbackStateChanged(true);
}

void RadioPlayerWidget::pause()
{
    if(m_player)
    {
        m_player->pause();
    }
    updatePlayButtonState(false);
    m_lblStatus->setText(QString::fromUtf8("ПАУЗА"));
    m_lblStatus->setStyleSheet("background-color: #1E293B; border: 1px solid #475569; color: #94A3B8; font-size: 10px; font-weight: bold; padding: 1px 6px; border-radius: 0px;");
    emit playbackStateChanged(false);
}

void RadioPlayerWidget::stop()
{
    if(m_player)
    {
        m_player->stop();
    }
    updatePlayButtonState(false);
    m_lblStatus->setText(QString::fromUtf8("СТОП"));
    m_lblStatus->setStyleSheet("background-color: #0F172A; border: 1px solid #334155; color: #64748B; font-size: 10px; font-weight: bold; padding: 1px 6px; border-radius: 0px;");
    emit playbackStateChanged(false);
}

void RadioPlayerWidget::togglePlay()
{
    if(isPlaying())
    {
        pause();
    }
    else
    {
        play();
    }
}

void RadioPlayerWidget::nextStation()
{
    int nextIdx = (currentStationIndex() + 1) % m_stations.size();
    m_comboStations->setCurrentIndex(nextIdx);
}

void RadioPlayerWidget::previousStation()
{
    int prevIdx = currentStationIndex() - 1;
    if(prevIdx < 0)
        prevIdx = m_stations.size() - 1;
    m_comboStations->setCurrentIndex(prevIdx);
}

void RadioPlayerWidget::setStationIndex(int index)
{
    if(index >= 0 && index < m_stations.size() && m_comboStations)
    {
        m_comboStations->setCurrentIndex(index);
    }
}

void RadioPlayerWidget::setVolume(int volume)
{
    if(m_sliderVolume)
    {
        m_sliderVolume->setValue(volume);
    }
}

void RadioPlayerWidget::toggleMute()
{
    if(!m_audioOutput)
        return;

    bool willBeMuted = !m_audioOutput->isMuted();
    m_audioOutput->setMuted(willBeMuted);
    updateVolumeUi(m_sliderVolume->value(), willBeMuted);
}

void RadioPlayerWidget::onPlayerPlaybackStateChanged(QMediaPlayer::PlaybackState state)
{
    bool playing = (state == QMediaPlayer::PlayingState);
    updatePlayButtonState(playing);
    if(!playing)
    {
        if(state == QMediaPlayer::PausedState)
        {
            m_lblStatus->setText(QString::fromUtf8("ПАУЗА"));
            m_lblStatus->setStyleSheet("background-color: #1E293B; border: 1px solid #475569; color: #94A3B8; font-size: 10px; font-weight: bold; padding: 1px 6px; border-radius: 0px;");
        }
        else
        {
            m_lblStatus->setText(QString::fromUtf8("СТОП"));
            m_lblStatus->setStyleSheet("background-color: #0F172A; border: 1px solid #334155; color: #64748B; font-size: 10px; font-weight: bold; padding: 1px 6px; border-radius: 0px;");
        }
    }
    emit playbackStateChanged(playing);
}

void RadioPlayerWidget::onPlayerMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    int idx = currentStationIndex();
    QString stationName = (idx >= 0 && idx < m_stations.size()) ? m_stations[idx].name : QString();

    switch(status)
    {
        case QMediaPlayer::LoadingMedia:
        case QMediaPlayer::BufferingMedia:
            m_lblStatus->setText(QString::fromUtf8("БУФЕР..."));
            m_lblStatus->setStyleSheet("background-color: #2D2006; border: 1px solid #D97706; color: #FCD34D; font-size: 10px; font-weight: bold; padding: 1px 6px; border-radius: 0px;");
            break;
        case QMediaPlayer::BufferedMedia:
        case QMediaPlayer::LoadedMedia:
            if(isPlaying())
            {
                m_lblStatus->setText(QString::fromUtf8("В ЭФИРЕ"));
                m_lblStatus->setStyleSheet("background-color: #062B1D; border: 1px solid #059669; color: #34D399; font-size: 10px; font-weight: bold; padding: 1px 6px; border-radius: 0px;");
            }
            break;
        case QMediaPlayer::StalledMedia:
            m_lblStatus->setText(QString::fromUtf8("СЕТЬ..."));
            m_lblStatus->setStyleSheet("background-color: #331A0B; border: 1px solid #EA580C; color: #FDBA74; font-size: 10px; font-weight: bold; padding: 1px 6px; border-radius: 0px;");
            break;
        case QMediaPlayer::EndOfMedia:
            m_lblStatus->setText(QString::fromUtf8("КОНЕЦ"));
            m_lblStatus->setStyleSheet("background-color: #0F172A; border: 1px solid #334155; color: #64748B; font-size: 10px; font-weight: bold; padding: 1px 6px; border-radius: 0px;");
            break;
        case QMediaPlayer::InvalidMedia:
            m_lblStatus->setText(QString::fromUtf8("ОШИБКА"));
            m_lblStatus->setStyleSheet("background-color: #2E1119; border: 1px solid #DC2626; color: #FCA5A5; font-size: 10px; font-weight: bold; padding: 1px 6px; border-radius: 0px;");
            m_lblTrackInfo->setText(QStringLiteral("%1: Поток недоступен").arg(stationName));
            break;
        default:
            break;
    }
}

void RadioPlayerWidget::onPlayerErrorOccurred(QMediaPlayer::Error error, const QString &errorString)
{
    Q_UNUSED(error);
    m_lblStatus->setText(QString::fromUtf8("ОШИБКА"));
    m_lblStatus->setStyleSheet("background-color: #2E1119; border: 1px solid #DC2626; color: #FCA5A5; font-size: 10px; font-weight: bold; padding: 1px 6px; border-radius: 0px;");
    m_lblStatus->setToolTip(errorString);
    m_lblTrackInfo->setText(QStringLiteral("Ошибка: %1").arg(errorString));
    updatePlayButtonState(false);
}

void RadioPlayerWidget::onPlayerMetaDataChanged()
{
    if(!m_player)
        return;

    QMediaMetaData meta = m_player->metaData();
    QString title = meta.value(QMediaMetaData::Title).toString();
    QString artist = meta.value(QMediaMetaData::ContributingArtist).toString();
    if(artist.isEmpty())
        artist = meta.value(QMediaMetaData::Author).toString();

    int idx = currentStationIndex();
    QString stationName = (idx >= 0 && idx < m_stations.size()) ? m_stations[idx].name : QString();

    if(!title.isEmpty() && !artist.isEmpty())
    {
        m_lblTrackInfo->setText(QStringLiteral("%1 • %2 — %3").arg(stationName, artist, title));
    }
    else if(!title.isEmpty())
    {
        m_lblTrackInfo->setText(QStringLiteral("%1 • %2").arg(stationName, title));
    }
    else
    {
        m_lblTrackInfo->setText(QStringLiteral("%1 • Прямой эфир").arg(stationName));
    }
}

void RadioPlayerWidget::onStationComboChanged(int index)
{
    if(index < 0 || index >= m_stations.size())
        return;

    bool wasPlaying = isPlaying();
    const auto &station = m_stations[index];
    m_player->setSource(QUrl(station.url));

    emit stationChanged(station.name, index);

    if(wasPlaying)
    {
        play();
    }
    else
    {
        m_lblTrackInfo->setText(QStringLiteral("%1 (%2)").arg(station.name, station.genre));
    }
}

void RadioPlayerWidget::onVolumeSliderChanged(int value)
{
    float normVol = static_cast<float>(value) / 100.0f;
    if(m_audioOutput)
    {
        m_audioOutput->setVolume(normVol);
        if(m_audioOutput->isMuted() && value > 0)
        {
            m_audioOutput->setMuted(false);
        }
    }
    updateVolumeUi(value, m_audioOutput ? m_audioOutput->isMuted() : false);
}

void RadioPlayerWidget::updatePlayButtonState(bool isPlaying)
{
    if(!m_btnPlay)
        return;

    if(isPlaying)
    {
        m_btnPlay->setText(QString());
        m_btnPlay->setIcon(QIcon(":/svg/pause"));
        m_btnPlay->setIconSize(QSize(13, 13));
        m_btnPlay->setToolTip(QString::fromUtf8("Пауза (Пробел)"));
        m_btnPlay->setStyleSheet(
            "QPushButton#btnPlay {"
            "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #059669, stop:1 #10B981);"
            "   color: #FFFFFF;"
            "   border: 1px solid #34D399;"
            "   border-radius: 0px;"
            "   font-size: 11px;"
            "   font-weight: bold;"
            "   min-width: 38px; max-width: 38px;"
            "   min-height: 24px; max-height: 24px;"
            "   padding: 0px;"
            "}"
            "QPushButton#btnPlay:hover {"
            "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #047857, stop:1 #34D399);"
            "   border: 1px solid #6EE7B7;"
            "}"
            "QPushButton#btnPlay:pressed {"
            "   background: #065F46;"
            "   border: 1px solid #047857;"
            "}");
    }
    else
    {
        m_btnPlay->setText(QString());
        m_btnPlay->setIcon(QIcon(":/svg/play"));
        m_btnPlay->setIconSize(QSize(13, 13));
        m_btnPlay->setToolTip(QString::fromUtf8("Слушать (Пробел)"));
        m_btnPlay->setStyleSheet(
            "QPushButton#btnPlay {"
            "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #0284C7, stop:1 #0EA5E9);"
            "   color: #FFFFFF;"
            "   border: 1px solid #38BDF8;"
            "   border-radius: 0px;"
            "   font-size: 11px;"
            "   font-weight: bold;"
            "   min-width: 38px; max-width: 38px;"
            "   min-height: 24px; max-height: 24px;"
            "   padding: 0px;"
            "}"
            "QPushButton#btnPlay:hover {"
            "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #0369A1, stop:1 #38BDF8);"
            "   border: 1px solid #7DD3FC;"
            "}"
            "QPushButton#btnPlay:pressed {"
            "   background: #0284C7;"
            "   border: 1px solid #0369A1;"
            "}");
    }
}

void RadioPlayerWidget::updateVolumeUi(int value, bool muted)
{
    if(m_lblVolumePercent)
    {
        m_lblVolumePercent->setText(QStringLiteral("%1%").arg(muted ? 0 : value));
        m_lblVolumePercent->setStyleSheet(
            muted
                ? "color: #EF4444; font-size: 10px; font-weight: bold; min-width: 28px; max-width: 28px;"
                : "color: #38BDF8; font-size: 10px; font-weight: bold; min-width: 28px; max-width: 28px;"
        );
    }
    if(m_btnMute)
    {
        if(muted || value == 0)
        {
            m_btnMute->setText(QString());
            m_btnMute->setIcon(QIcon(":/svg/volume-x"));
            m_btnMute->setIconSize(QSize(14, 14));
            m_btnMute->setToolTip(QString::fromUtf8("Включить звук"));
            m_btnMute->setStyleSheet(
                "QPushButton#btnMute {"
                "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #2E1119, stop:1 #1A0A0F);"
                "   color: #F87171;"
                "   border: 1px solid #991B1B;"
                "   border-radius: 0px;"
                "   font-size: 11px;"
                "   min-width: 28px; max-width: 28px;"
                "   min-height: 24px; max-height: 24px;"
                "   padding: 0px;"
                "}"
                "QPushButton#btnMute:hover {"
                "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #45121E, stop:1 #2E1119);"
                "   border: 1px solid #EF4444;"
                "   color: #FFFFFF;"
                "}"
                "QPushButton#btnMute:pressed {"
                "   background: #1A0A0F;"
                "   border: 1px solid #7F1D1D;"
                "}");
        }
        else
        {
            m_btnMute->setText(QString());
            m_btnMute->setIcon(QIcon(":/svg/volume"));
            m_btnMute->setIconSize(QSize(14, 14));
            m_btnMute->setToolTip(QString::fromUtf8("Отключить звук"));
            m_btnMute->setStyleSheet(
                "QPushButton#btnMute {"
                "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #1E293B, stop:1 #0F172A);"
                "   color: #38BDF8;"
                "   border: 1px solid #334155;"
                "   border-radius: 0px;"
                "   font-size: 11px;"
                "   min-width: 28px; max-width: 28px;"
                "   min-height: 24px; max-height: 24px;"
                "   padding: 0px;"
                "}"
                "QPushButton#btnMute:hover {"
                "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #334155, stop:1 #1E293B);"
                "   border: 1px solid #38BDF8;"
                "   color: #FFFFFF;"
                "}"
                "QPushButton#btnMute:pressed {"
                "   background: #0B1120;"
                "   border: 1px solid #0284C7;"
                "}");
        }
    }
}

void RadioPlayerWidget::showAddStationDialog()
{
    bool okName = false;
    QString name = QInputDialog::getText(this, QString::fromUtf8("Новая радиостанция"), QString::fromUtf8("Введите название станции:"), QLineEdit::Normal, QString(), &okName);
    if(!okName || name.trimmed().isEmpty())
        return;

    bool okUrl = false;
    QString url = QInputDialog::getText(this, QString::fromUtf8("Адрес потока"), QString::fromUtf8("Введите URL интернет-потока (http/https):"), QLineEdit::Normal, QStringLiteral("http://"), &okUrl);
    if(!okUrl || url.trimmed().isEmpty())
        return;

    name = name.trimmed();
    url = url.trimmed();

    RadioStation customStation {name, url, QString::fromUtf8("Моё радио")};
    m_stations.append(customStation);
    m_comboStations->addItem(QStringLiteral("%1 (%2)").arg(customStation.name, customStation.genre));

    // Persist custom stations to QSettings
    QSettings settings("AdsKiller", "RadioStations");
    int customCount = settings.beginReadArray("CustomStations");
    settings.endArray();

    settings.beginWriteArray("CustomStations");
    settings.setArrayIndex(customCount);
    settings.setValue("name", name);
    settings.setValue("url", url);
    settings.setValue("genre", customStation.genre);
    settings.endArray();

    // Select the newly added station
    m_comboStations->setCurrentIndex(m_stations.size() - 1);
}
