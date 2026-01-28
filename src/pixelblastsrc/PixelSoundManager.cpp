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
    qDeleteAll(m_pool);
    m_pool.clear();
    m_nextIndex = 0;
}

void SoundManager::ensurePool()
{
    if(!m_pool.isEmpty())
        return;
    m_pool.reserve(m_poolSize);
    for(int i = 0; i < m_poolSize; ++i)
    {
        QSoundEffect *se = new QSoundEffect(this);
        se->setLoopCount(1);
        se->setVolume(1.0);
        m_pool.append(se);
    }
}

void SoundManager::registerSound(const QString &name, const QUrl &url)
{
    m_registry.insert(name, url);
    ensurePool();
    for(QSoundEffect *se : std::as_const(m_pool))
    {
        if(se->source().isEmpty())
        {
            se->setSource(url);
            break;
        }
    }
}

void SoundManager::playSound(const QString &name, qreal volume)
{
    if(!m_registry.contains(name))
        return;
    ensurePool();
    QUrl url = m_registry.value(name);
    int start = m_nextIndex;
    int idx = -1;
    for(int i = 0; i < m_pool.size(); ++i)
    {
        int j = (start + i) % m_pool.size();
        QSoundEffect *se = m_pool[j];
        if(!se->isPlaying())
        {
            idx = j;
            break;
        }
    }
    if(idx == -1)
    {
        idx = m_nextIndex % m_pool.size();
        m_nextIndex = (m_nextIndex + 1) % m_pool.size();
    }

    QSoundEffect *effect = m_pool[idx];
    if(effect->source() != url)
    {
        effect->setSource(url);
    }
    effect->setVolume(qBound<qreal>(0.0, volume, 1.0));
    effect->play();
}
