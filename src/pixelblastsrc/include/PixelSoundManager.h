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
    void registerSound(const QString &name, const QUrl &url);
    void playSound(const QString &name, qreal volume = 1.0);

    void setPoolSize(int size);

private:
    void ensurePool();

    int m_poolSize = 16;
    QVector<QSoundEffect *> m_pool;
    QHash<QString, QUrl> m_registry;
    int m_nextIndex = 0;
};
