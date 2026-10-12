#include <QPainter>
#include <QDir>
#include <QPainterPath>

#include "Services.h"

const std::array<const char *, 5> ConstDataSizes = {"байт(ов)", "КБ", "МБ", "ГБ", "ТБ"};

namespace Generic
{
    // Beautify number with spaces every three digits
    // example: 1234567890 -> 1 234 567 890
    // example: 1000 -> 1 000
    QString beautifyNumber(std::uint64_t number, const QString &symbol)
    {
        QString str = QString::number(number);
        int len = str.length();
        for(int i = len - 3; i > 0; i -= 3)
            str.insert(i, symbol);
        return str;
    }

    // Format bytes to design size, e.g. 1234567890 -> 1.23 GB
    std::pair<int, QString> formatDesignSize(std::uint64_t bytes)
    {
        int x = 1, power = 0;
        while(bytes >= 1000 && power < ConstDataSizes.size())
        {
            bytes /= 1000;
            ++power;
        }
        while(x < bytes)
        {
            x *= 2;
        }
        return {x, ConstDataSizes[power]};
    }

    // Format bytes to human-readable size, e.g. 1234567890 -> 1.23 GB
    QString formatSizes(std::uint64_t bytes)
    {
        if(bytes >= 1024ULL * 1024ULL * 1024ULL)
            return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " " + QString::fromUtf8(ConstDataSizes[3]);
        if(bytes >= 1024ULL * 1024ULL)
            return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " " + QString::fromUtf8(ConstDataSizes[2]);
        if(bytes >= 1024ULL)
            return QString::number(bytes / 1024.0, 'f', 1) + " " + QString::fromUtf8(ConstDataSizes[1]);
        if(bytes > 0)
            return QString::number(bytes) + " " + QString::fromUtf8(ConstDataSizes[0]);
        return "0 " + QString::fromUtf8(ConstDataSizes[0]);
    }

    // Generate a fallback icon for an app based on its name and package name
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