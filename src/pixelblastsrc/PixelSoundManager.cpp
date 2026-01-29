#include <QAudioDevice>

#include "PixelSoundManager.h"

SoundManager::SoundManager(QObject *parent) : QObject(parent), m_minIndex(0)
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

void SoundManager::registerSound(const QString &name, const QUrl &url, bool asPool)
{
    m_registry.insert(name, std::make_tuple((asPool ? (-1) : (m_minIndex++)), url));
    if(m_minIndex >= m_poolSize)
        m_poolSize = m_poolSize * 2;
    ensurePool();
    for(int i = m_minIndex; i < m_pool.size(); ++i)
    {
        if(m_pool[i]->source().isEmpty())
        {
            m_pool[i]->setSource(url);
            break;
        }
    }
}

void SoundManager::playSound(const QString &name, qreal volume)
{
    int x, y;
    if(!m_registry.contains(name))
        return;
    ensurePool();
    const std::tuple<int, QUrl> &val = m_registry.value(name);
    int start = m_nextIndex;
    int idx = std::get<0>(val);
    if(idx == -1)
    {
        for(x = m_minIndex; x < m_pool.size(); ++x)
        {
            y = (start + x) % m_pool.size();
            QSoundEffect *se = m_pool[y];
            if(!se->isPlaying())
            {
                idx = y;
                break;
            }
        }
        if(idx == -1)
        {
            idx = (m_nextIndex + m_minIndex) % m_pool.size() + m_minIndex;
            m_nextIndex = (m_nextIndex + 1 + m_minIndex) % m_pool.size() + m_minIndex;
        }
    }

    QSoundEffect *effect = m_pool[idx];
    if(effect->source() != std::get<1>(val))
    {
        effect->setSource(std::get<1>(val));
    }
    effect->setVolume(qBound<qreal>(0.0, volume, 1.0));
    effect->play();
}
