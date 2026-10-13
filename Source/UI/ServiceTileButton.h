#ifndef SERVICETILEBUTTON_H
#define SERVICETILEBUTTON_H

#include <QColor>
#include <QIcon>
#include <QPoint>
#include <QPushButton>
#include <QString>
#include <QVariantAnimation>

#include "Services.h"

class QEnterEvent;
class QEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;
class QShowEvent;

class ServiceTileButton : public QPushButton
{
    Q_OBJECT

public:
    enum class Tier
    {
        Vip,
        Free,
        Credit,
        Dynamic,
        Disabled
    };

    struct Details
    {
        QString title;
        QString description;
        QString badgeText;
        Tier tier = Tier::Free;
        bool isActive = true;
        bool isUnderDev = false;
        bool needVip = false;
        uint32_t price = 0;
        QString currency = QString::fromUtf8("кредитов");
        QString connectTypeName = QString::fromUtf8("Не требуется");
        QIcon icon;
        int sortScore = 100;
        ServiceFlag flag = ServiceFlag::None;
    };

    explicit ServiceTileButton(const QIcon &icon, const QString &title, const QString &badgeText, Tier tier, QWidget *parent = nullptr);
    explicit ServiceTileButton(const QIcon &icon, const QString &title, const QString &badgeText, Tier tier, ServiceFlag flag, QWidget *parent = nullptr);
    explicit ServiceTileButton(const QIcon &icon, const QString &title, const QString &badgeText, Tier tier, bool showRibbon, const QString &ribbonText = QString::fromUtf8("NEW"), QWidget *parent = nullptr);

    int sort_score() const
    {
        return m_sortScore;
    }
    void setSortScore(int score)
    {
        m_sortScore = score;
    }

    void setTier(Tier tier);
    Tier tier() const
    {
        return m_tier;
    }

    void setTitle(const QString &title);
    QString title() const
    {
        return m_title;
    }

    void setBadgeText(const QString &text);
    QString badgeText() const
    {
        return m_badgeText;
    }

    void setFlag(ServiceFlag flag);
    ServiceFlag flag() const
    {
        return m_flag;
    }

    void setShowRibbon(bool show);
    bool showRibbon() const
    {
        return m_showRibbon;
    }

    void setRibbonText(const QString &text);
    QString ribbonText() const
    {
        return m_ribbonText;
    }

    bool isLaunching() const
    {
        return m_isLaunching;
    }
    void setLaunching(bool launching)
    {
        m_isLaunching = launching;
    }
    static bool isAnyLaunching()
    {
        return s_isAnyLaunching;
    }
    static void setAnyLaunching(bool launching)
    {
        s_isAnyLaunching = launching;
    }
    QColor accentColor() const;

    void triggerClickAnimation();

    bool isServiceActive() const
    {
        return m_serviceActive;
    }
    void setServiceActive(bool active);

    void setServiceDetails(const Details &details);
    const Details &serviceDetails() const
    {
        return m_details;
    }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void showInfoDialog();

protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent *event) override;
#else
    void enterEvent(QEvent *event) override;
#endif
    void leaveEvent(QEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private:
    QString m_title;
    QString m_badgeText;
    Tier m_tier;
    bool m_showRibbon;
    QString m_ribbonText;
    ServiceFlag m_flag = ServiceFlag::None;
    bool m_isLaunching = false;
    static bool s_isAnyLaunching;

    bool m_serviceActive = true;
    Details m_details;
    QPushButton *m_btnHelp = nullptr;

    QPoint m_mousePos;
    bool m_isHovered = false;
    QPoint m_pressPos;
    qreal m_rippleProgress = 0.0;
    qreal m_rippleRadius = 0.0;
    qreal m_rippleOpacity = 0.0;
    QVariantAnimation *m_rippleAnim = nullptr;

    qreal m_clickProgress = 0.0;
    QVariantAnimation *m_clickAnim = nullptr;

    int m_sortScore = 100;
};

#endif