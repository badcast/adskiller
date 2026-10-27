#pragma once

#include <QObject>
#include <QSoundEffect>
#include <QHash>
#include <QVector>

#include "PixelBlastGame.h"

class PB_EXPORT SoundManager : public QObject
{
    Q_OBJECT
public:
    explicit SoundManager(QObject *parent = nullptr);
    void registerSound(const QString &name, const QUrl &url, bool asPool = true);
    void playSound(const QString &name, qreal volume = 1.0);

    void setPoolSize(int size);

private:
    // Each sound owns its own preloaded effects: the source is set once and never changed,
    // because QSoundEffect::setSource() reloads/decodes the sample (very slow on Windows).
    int m_poolSize = 3;
    QHash<QString, QVector<QSoundEffect *>> m_registry;
    QHash<QString, int> m_nextIndex;
};
