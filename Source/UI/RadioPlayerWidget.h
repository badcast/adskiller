#pragma once

#include <QFrame>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QMediaMetaData>
#include <QComboBox>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QHBoxLayout>
#include <QList>
#include <QString>
#include <QUrl>
#include <QSettings>

struct RadioStation
{
    QString name;
    QString url;
    QString genre;
};

class RadioPlayerWidget : public QFrame
{
    Q_OBJECT

public:
    explicit RadioPlayerWidget(QWidget *parent = nullptr);
    ~RadioPlayerWidget() override;

    bool isPlaying() const;
    int volume() const;
    QString currentStationName() const;
    const QList<RadioStation> &stations() const
    {
        return m_stations;
    }
    int currentStationIndex() const;

public slots:
    void play();
    void pause();
    void stop();
    void togglePlay();
    void nextStation();
    void previousStation();
    void setStationIndex(int index);
    void setVolume(int volume);
    void toggleMute();
    void showAddStationDialog();

signals:
    void playbackStateChanged(bool playing);
    void stationChanged(const QString &stationName, int index);
    void requestClose();

private slots:
    void onPlayerPlaybackStateChanged(QMediaPlayer::PlaybackState state);
    void onPlayerMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onPlayerErrorOccurred(QMediaPlayer::Error error, const QString &errorString);
    void onPlayerMetaDataChanged();
    void onStationComboChanged(int index);
    void onVolumeSliderChanged(int value);

private:
    void initUi();
    void populateDefaultStations();
    void loadSettings();
    void saveSettings();
    void updatePlayButtonState(bool isPlaying);
    void updateVolumeUi(int value, bool muted);

    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_audioOutput = nullptr;

    QList<RadioStation> m_stations;

    QLabel *m_lblLogo = nullptr;
    QComboBox *m_comboStations = nullptr;
    QPushButton *m_btnPrev = nullptr;
    QPushButton *m_btnPlay = nullptr;
    QPushButton *m_btnNext = nullptr;
    QLabel *m_lblStatus = nullptr;
    QLabel *m_lblTrackInfo = nullptr;
    QPushButton *m_btnMute = nullptr;
    QSlider *m_sliderVolume = nullptr;
    QLabel *m_lblVolumePercent = nullptr;
    QPushButton *m_btnAddStation = nullptr;
    QPushButton *m_btnClose = nullptr;

    int m_lastNonZeroVolume = 70;
};
