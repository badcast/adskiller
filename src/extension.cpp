#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QString>

#include "Services.h"
#include "extension.h"
#include "mainwindow.h"

QByteArray CipherAlgoCrypto::RandomKey()
{
    int x;
    QByteArray key;
    key.resize(8);
    for(x = 0; x < key.length(); ++x)
    {
        key[x] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    }
    return key;
}

QByteArray CipherAlgoCrypto::ConvBytes(const QByteArray &bytes, const QByteArray &key)
{
    int x;
    QByteArray retval;
    retval.resize(bytes.length());
    for(x = 0; x < bytes.length() && !key.isEmpty(); ++x)
        retval[x] = bytes[x] ^ key[x % key.length()];
    return retval;
}

QString CipherAlgoCrypto::PackDC(const QByteArray &dataInit, const QByteArray &key)
{
    QByteArray retval {};
    QByteArray data = ConvBytes(dataInit, key);
    int keylen = key.toHex().length();
    int hashlen = QCryptographicHash::hashLength(QCryptographicHash::Sha256);
    retval.resize(hashlen + data.length() + keylen + 1);
    retval[hashlen] = static_cast<char>(keylen);
    retval.replace(hashlen + 1, keylen, key.toHex());
    retval.replace(hashlen + 1 + keylen, data.length(), data);
    retval.replace(0, hashlen, QCryptographicHash::hash(retval.mid(hashlen), QCryptographicHash::Sha256));
    return QLatin1String(retval.toBase64());
}

QByteArray CipherAlgoCrypto::UnpackDC(const QString &packed)
{
    QByteArray key {}, data {};
    int keylen;
    int hashlen = QCryptographicHash::hashLength(QCryptographicHash::Sha256);
    data = QByteArray::fromBase64(packed.toLatin1());
    if(!data.isEmpty() && data.mid(0, hashlen) == QCryptographicHash::hash(data.mid(hashlen), QCryptographicHash::Sha256))
    {
        keylen = data[hashlen];
        key = QByteArray::fromHex(data.mid(hashlen + 1, keylen));
        data = ConvBytes(data.mid(hashlen + 1 + keylen), key);
    }
    else
    {
        data.clear();
    }
    return data;
}
