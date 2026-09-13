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
    m_stations.append({QString::fromUtf8("Radio Record"), QStringLiteral("http://air.radiorecord.ru:805/rr_320"), QString::fromUtf8("Dance / Club")});
    m_stations.append({QString::fromUtf8("DFM"), QStringLiteral("http://icecast.radiodfm.hostingradio.ru/dfm.mp3"), QString::fromUtf8("Dance / Pop")});
    m_stations.append({QString::fromUtf8("Европа Плюс"), QStringLiteral("http://ep256.hostingradio.ru:8052/europaplus256.mp3"), QString::fromUtf8("Top 40")});
    m_stations.append({QString::fromUtf8("Ретро FM"), QStringLiteral("http://retro256.hostingradio.ru:8043/retro256.mp3"), QString::fromUtf8("Retro 80-90s")});
    m_stations.append({QString::fromUtf8("Relax FM"), QStringLiteral("http://ic7.101.ru:8000/a200"), QString::fromUtf8("Lounge / Chill")});
    m_stations.append({QString::fromUtf8("Радио Jazz"), QStringLiteral("http://jazz.streamr.ru/jazz-64.mp3"), QString::fromUtf8("Jazz & Blues")});
    m_stations.append({QString::fromUtf8("Rock FM"), QStringLiteral("http://nashe1.hostingradio.ru/rock-128.mp3"), QString::fromUtf8("Classic Rock")});
    m_stations.append({QString::fromUtf8("NRJ Energy"), QStringLiteral("http://ic7.101.ru:8000/a1"), QString::fromUtf8("Modern Hits")});
    m_stations.append({QString::fromUtf8("Монте-Карло"), QStringLiteral("http://montecarlo.hostingradio.ru/montecarlo128.mp3"), QString::fromUtf8("Lounge Pop")});
    m_stations.append({QString::fromUtf8("Lofi Chillhop"), QStringLiteral("http://stream.zeno.fm/f3wvbbqmdg8uv"), QString::fromUtf8("Lo-Fi Beats")});

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
    setFixedHeight(30);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Midnight Obsidian theme styling for the compact top radio player bar
    setStyleSheet(
        "QFrame#RadioPlayerWidget {"
        "   background-color: #0B1120;"
        "   border-bottom: 1px solid #1E293B;"
        "   border-top: 1px solid #1E293B;"
        "}"
        "QLabel {"
        "   color: #94A3B8;"
        "   font-size: 10px;"
        "}"
        "QComboBox {"
        "   background-color: #0F172A;"
        "   color: #F8FAFC;"
        "   border: 1px solid #334155;"
        "   border-radius: 4px;"
        "   padding: 1px 6px;"
        "   font-size: 10.5px;"
        "   font-weight: 500;"
        "   min-width: 140px;"
        "   max-width: 200px;"
        "   height: 20px;"
        "}"
        "QComboBox:hover {"
        "   border-color: #38BDF8;"
        "}"
        "QComboBox::drop-down {"
        "   subcontrol-origin: padding;"
        "   subcontrol-position: top right;"
        "   width: 16px;"
        "   border-left: 1px solid #334155;"
        "   border-top-right-radius: 4px;"
        "   border-bottom-right-radius: 4px;"
        "}"
        "QComboBox QAbstractItemView {"
        "   background-color: #0F172A;"
        "   color: #F8FAFC;"
        "   selection-background-color: #0284C7;"
        "   selection-color: #FFFFFF;"
        "   border: 1px solid #334155;"
        "   padding: 2px;"
        "}"
        "QPushButton {"
        "   background-color: #1E293B;"
        "   color: #F8FAFC;"
        "   border: 1px solid #334155;"
        "   border-radius: 4px;"
        "   font-size: 10px;"
        "   font-weight: bold;"
        "   padding: 0px 4px;"
        "   height: 20px;"
        "   min-height: 20px;"
        "   max-height: 20px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #334155;"
        "   border-color: #38BDF8;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #0F172A;"
        "}"
        "QPushButton#btnPlay {"
        "   background-color: #10B981;"
        "   border-color: #059669;"
        "   color: #FFFFFF;"
        "   font-size: 10px;"
        "   font-weight: bold;"
        "}"
        "QPushButton#btnPlay:hover {"
        "   background-color: #34D399;"
        "   border-color: #10B981;"
        "}"
        "QPushButton#btnPlay:pressed {"
        "   background-color: #047857;"
        "}"
        "QPushButton#btnClose {"
        "   background-color: transparent;"
        "   color: #64748B;"
        "   border: none;"
        "   font-size: 11px;"
        "   font-weight: bold;"
        "   padding: 0px 2px;"
        "}"
        "QPushButton#btnClose:hover {"
        "   color: #EF4444;"
        "}"
        "QSlider::groove:horizontal {"
        "   height: 3px;"
        "   background: #1E293B;"
        "   border-radius: 1px;"
        "}"
        "QSlider::sub-page:horizontal {"
        "   background: #38BDF8;"
        "   border-radius: 1px;"
        "}"
        "QSlider::handle:horizontal {"
        "   background: #F8FAFC;"
        "   border: 1px solid #0284C7;"
        "   width: 10px;"
        "   margin-top: -3px;"
        "   margin-bottom: -3px;"
        "   border-radius: 5px;"
        "}"
        "QSlider::handle:horizontal:hover {"
        "   background: #38BDF8;"
        "   border-color: #BAE6FD;"
        "}");

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 1, 6, 1);
    layout->setSpacing(5);

    // 1. Logo / Radio badge
    m_lblLogo = new QLabel(QString::fromUtf8("📻 <b>РАДИО</b>"), this);
    m_lblLogo->setStyleSheet("color: #38BDF8; font-size: 11px; font-weight: bold; padding-right: 2px;");
    layout->addWidget(m_lblLogo);

    // 2. Station Selector Combo
    m_comboStations = new QComboBox(this);
    m_comboStations->setFixedHeight(20);
    for(const auto &station : m_stations)
    {
        m_comboStations->addItem(QStringLiteral("%1 (%2)").arg(station.name, station.genre));
    }
    connect(m_comboStations, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RadioPlayerWidget::onStationComboChanged);
    layout->addWidget(m_comboStations);

    // 3. Add Custom Station Button
    m_btnAddStation = new QPushButton(QString::fromUtf8("➕"), this);
    m_btnAddStation->setToolTip(QString::fromUtf8("Добавить свой поток радио"));
    m_btnAddStation->setFixedSize(20, 20);
    connect(m_btnAddStation, &QPushButton::clicked, this, &RadioPlayerWidget::showAddStationDialog);
    layout->addWidget(m_btnAddStation);

    // 4. Playback Controls (Prev, Play/Pause, Next)
    m_btnPrev = new QPushButton(QString::fromUtf8("⏮"), this);
    m_btnPrev->setToolTip(QString::fromUtf8("Предыдущая станция"));
    m_btnPrev->setFixedSize(22, 20);
    connect(m_btnPrev, &QPushButton::clicked, this, &RadioPlayerWidget::previousStation);
    layout->addWidget(m_btnPrev);

    m_btnPlay = new QPushButton(QString::fromUtf8("▶"), this);
    m_btnPlay->setObjectName("btnPlay");
    m_btnPlay->setToolTip(QString::fromUtf8("Слушать / Пауза"));
    m_btnPlay->setFixedSize(24, 20);
    connect(m_btnPlay, &QPushButton::clicked, this, &RadioPlayerWidget::togglePlay);
    layout->addWidget(m_btnPlay);

    m_btnNext = new QPushButton(QString::fromUtf8("⏭"), this);
    m_btnNext->setToolTip(QString::fromUtf8("Следующая станция"));
    m_btnNext->setFixedSize(22, 20);
    connect(m_btnNext, &QPushButton::clicked, this, &RadioPlayerWidget::nextStation);
    layout->addWidget(m_btnNext);

    // 5. Status indicator badge
    m_lblStatus = new QLabel(QString::fromUtf8("⚪ Готов"), this);
    m_lblStatus->setStyleSheet("color: #64748B; font-size: 10px;");
    m_lblStatus->setMinimumWidth(72);
    layout->addWidget(m_lblStatus);

    // 6. Track / Stream info label (expands)
    m_lblTrackInfo = new QLabel(QString::fromUtf8("Выберите станцию и нажмите «Слушать»"), this);
    m_lblTrackInfo->setStyleSheet("color: #E2E8F0; font-size: 10.5px; font-weight: 500;");
    m_lblTrackInfo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_lblTrackInfo->setTextInteractionFlags(Qt::NoTextInteraction);
    layout->addWidget(m_lblTrackInfo, 1);

    // 7. Volume controls
    m_btnMute = new QPushButton(QString::fromUtf8("🔊"), this);
    m_btnMute->setObjectName("btnMute");
    m_btnMute->setToolTip(QString::fromUtf8("Без звука"));
    m_btnMute->setFixedSize(20, 20);
    connect(m_btnMute, &QPushButton::clicked, this, &RadioPlayerWidget::toggleMute);
    layout->addWidget(m_btnMute);

    m_sliderVolume = new QSlider(Qt::Horizontal, this);
    m_sliderVolume->setRange(0, 100);
    m_sliderVolume->setValue(70);
    m_sliderVolume->setFixedSize(60, 16);
    m_sliderVolume->setToolTip(QString::fromUtf8("Громкость радио"));
    connect(m_sliderVolume, &QSlider::valueChanged, this, &RadioPlayerWidget::onVolumeSliderChanged);
    layout->addWidget(m_sliderVolume);

    m_lblVolumePercent = new QLabel(QStringLiteral("70%"), this);
    m_lblVolumePercent->setFixedWidth(26);
    m_lblVolumePercent->setStyleSheet("color: #94A3B8; font-size: 9.5px; font-weight: bold;");
    layout->addWidget(m_lblVolumePercent);

    // 8. Close / Hide button
    m_btnClose = new QPushButton(QString::fromUtf8("✕"), this);
    m_btnClose->setObjectName("btnClose");
    m_btnClose->setToolTip(QString::fromUtf8("Скрыть панель радио"));
    m_btnClose->setFixedSize(18, 18);
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
    m_lblStatus->setText(QString::fromUtf8("🟡 Соединение..."));
    m_lblStatus->setStyleSheet("color: #F59E0B; font-size: 11px;");
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
    m_lblStatus->setText(QString::fromUtf8("⏸ Пауза"));
    m_lblStatus->setStyleSheet("color: #94A3B8; font-size: 11px;");
    emit playbackStateChanged(false);
}

void RadioPlayerWidget::stop()
{
    if(m_player)
    {
        m_player->stop();
    }
    updatePlayButtonState(false);
    m_lblStatus->setText(QString::fromUtf8("⚪ Остановлено"));
    m_lblStatus->setStyleSheet("color: #64748B; font-size: 11px;");
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
            m_lblStatus->setText(QString::fromUtf8("⏸ Пауза"));
            m_lblStatus->setStyleSheet("color: #94A3B8; font-size: 11px;");
        }
        else
        {
            m_lblStatus->setText(QString::fromUtf8("⚪ Стоп"));
            m_lblStatus->setStyleSheet("color: #64748B; font-size: 11px;");
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
            m_lblStatus->setText(QString::fromUtf8("🟡 Буферизация"));
            m_lblStatus->setStyleSheet("color: #F59E0B; font-size: 11px; font-weight: bold;");
            break;
        case QMediaPlayer::BufferedMedia:
        case QMediaPlayer::LoadedMedia:
            if(isPlaying())
            {
                m_lblStatus->setText(QString::fromUtf8("🟢 В эфире"));
                m_lblStatus->setStyleSheet("color: #10B981; font-size: 11px; font-weight: bold;");
            }
            break;
        case QMediaPlayer::StalledMedia:
            m_lblStatus->setText(QString::fromUtf8("🟠 Медл. сеть"));
            m_lblStatus->setStyleSheet("color: #F97316; font-size: 11px;");
            break;
        case QMediaPlayer::EndOfMedia:
            m_lblStatus->setText(QString::fromUtf8("⚪ Завершено"));
            m_lblStatus->setStyleSheet("color: #64748B; font-size: 11px;");
            break;
        case QMediaPlayer::InvalidMedia:
            m_lblStatus->setText(QString::fromUtf8("🔴 Ошибка"));
            m_lblStatus->setStyleSheet("color: #EF4444; font-size: 11px; font-weight: bold;");
            m_lblTrackInfo->setText(QStringLiteral("%1: Поток недоступен").arg(stationName));
            break;
        default:
            break;
    }
}

void RadioPlayerWidget::onPlayerErrorOccurred(QMediaPlayer::Error error, const QString &errorString)
{
    Q_UNUSED(error);
    m_lblStatus->setText(QString::fromUtf8("🔴 Ошибка"));
    m_lblStatus->setStyleSheet("color: #EF4444; font-size: 11px; font-weight: bold;");
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
        m_lblTrackInfo->setText(QStringLiteral("%1 • 🎵 %2 — %3").arg(stationName, artist, title));
    }
    else if(!title.isEmpty())
    {
        m_lblTrackInfo->setText(QStringLiteral("%1 • 🎵 %2").arg(stationName, title));
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
        m_btnPlay->setText(QString::fromUtf8("⏸"));
        m_btnPlay->setToolTip(QString::fromUtf8("Пауза"));
        m_btnPlay->setStyleSheet(
            "QPushButton#btnPlay {"
            "   background-color: #0284C7;"
            "   border-color: #0369A1;"
            "   color: #FFFFFF;"
            "   font-size: 10px;"
            "   font-weight: bold;"
            "   height: 20px;"
            "   border-radius: 4px;"
            "   padding: 0px;"
            "}"
            "QPushButton#btnPlay:hover { background-color: #38BDF8; }");
    }
    else
    {
        m_btnPlay->setText(QString::fromUtf8("▶"));
        m_btnPlay->setToolTip(QString::fromUtf8("Слушать"));
        m_btnPlay->setStyleSheet(
            "QPushButton#btnPlay {"
            "   background-color: #10B981;"
            "   border-color: #059669;"
            "   color: #FFFFFF;"
            "   font-size: 10px;"
            "   font-weight: bold;"
            "   height: 20px;"
            "   border-radius: 4px;"
            "   padding: 0px;"
            "}"
            "QPushButton#btnPlay:hover { background-color: #34D399; }");
    }
}

void RadioPlayerWidget::updateVolumeUi(int value, bool muted)
{
    if(m_lblVolumePercent)
    {
        m_lblVolumePercent->setText(QStringLiteral("%1%").arg(muted ? 0 : value));
    }
    if(m_btnMute)
    {
        if(muted || value == 0)
        {
            m_btnMute->setText(QString::fromUtf8("🔇"));
            m_btnMute->setStyleSheet(
                "QPushButton#btnMute {"
                "   background-color: #1E293B;"
                "   color: #EF4444;"
                "   border: 1px solid #334155;"
                "   border-radius: 4px;"
                "   font-size: 10px;"
                "   padding: 0px;"
                "}"
                "QPushButton#btnMute:hover { background-color: #334155; }");
        }
        else
        {
            m_btnMute->setText(QString::fromUtf8("🔊"));
            m_btnMute->setStyleSheet(
                "QPushButton#btnMute {"
                "   background-color: #1E293B;"
                "   color: #F8FAFC;"
                "   border: 1px solid #334155;"
                "   border-radius: 4px;"
                "   font-size: 10px;"
                "   padding: 0px;"
                "}"
                "QPushButton#btnMute:hover { background-color: #334155; }");
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
