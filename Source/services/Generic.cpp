#include <QPainter>
#include <QDir>
#include <QPainterPath>

#include "Services.h"

namespace Generic
{
    QString formatSizes(qint64 bytes)
    {
        if(bytes >= 1024ULL * 1024ULL * 1024ULL)
            return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " ГБ";
        if(bytes >= 1024ULL * 1024ULL)
            return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " МБ";
        if(bytes >= 1024ULL)
            return QString::number(bytes / 1024.0, 'f', 1) + " КБ";
        if(bytes > 0)
            return QString::number(bytes) + " Б";
        return "0 Б";
    }

    QPixmap generateFallbackIcon(const QString &appName, const QString &pkgName, bool isSystem)
    {
        const int size = 64;
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);

        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);

        QRectF rect(1, 1, size - 2, size - 2);
        QPainterPath path;
        path.addRoundedRect(rect, 15, 15);

        QLinearGradient grad(0, 0, 0, size);
        if(isSystem)
        {
            grad.setColorAt(0.0, QColor(51, 65, 85));
            grad.setColorAt(1.0, QColor(30, 41, 59));
        }
        else
        {
            uint h = qHash(pkgName);
            int hue = h % 360;
            grad.setColorAt(0.0, QColor::fromHsv(hue, 180, 220));
            grad.setColorAt(1.0, QColor::fromHsv((hue + 45) % 360, 210, 150));
        }

        p.fillPath(path, grad);

        QPen borderPen(QColor(255, 255, 255, 45), 1.5);
        p.setPen(borderPen);
        p.drawPath(path);

        QString monogram = "?";
        if(!appName.trimmed().isEmpty())
        {
            QString clean = appName.trimmed();
            if(clean.size() >= 2 && clean[0].isLetter() && clean[1].isLetter())
                monogram = clean.left(2).toUpper();
            else
                monogram = clean.left(1).toUpper();
        }

        QFont font("Segoe UI", 16, QFont::Bold);
        p.setFont(font);
        p.setPen(Qt::white);
        p.drawText(rect, Qt::AlignCenter, monogram);

        p.end();
        return pixmap;
    }
} // namespace Generic