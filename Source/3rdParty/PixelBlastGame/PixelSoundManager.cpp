#include <QAudioDevice>

#include "PixelSoundManager.h"

SoundManager::SoundManager(QObject *parent) : QObject(parent)
{
}

void SoundManager::setPoolSize(int size)
{
    if(size <= 0)
        return;
    m_poolSize = size;
}

void SoundManager::registerSound(const QString &name, const QUrl &url, bool asPool)
{
    QVector<QSoundEffect *> &effects = m_registry[name];
    qDeleteAll(effects);
    effects.clear();

    const int count = asPool ? m_poolSize : 1;
    effects.reserve(count);
    for(int i = 0; i < count; ++i)
    {
        QSoundEffect *se = new QSoundEffect(this);
        se->setLoopCount(1);
        se->setVolume(1.0);
        se->setSource(url);
        effects.append(se);
    }
    m_nextIndex[name] = 0;
}

void SoundManager::playSound(const QString &name, qreal volume)
{
    auto it = m_registry.constFind(name);
    if(it == m_registry.constEnd() || it->isEmpty())
        return;

    const QVector<QSoundEffect *> &effects = *it;
    QSoundEffect *effect = nullptr;
    for(QSoundEffect *se : effects)
    {
        if(!se->isPlaying())
        {
            effect = se;
            break;
        }
    }
    if(effect == nullptr)
    {
        int &next = m_nextIndex[name];
        effect = effects[next % effects.size()];
        next = (next + 1) % effects.size();
    }

    effect->setVolume(qBound<qreal>(0.0, volume, 1.0));
    effect->play();
}
