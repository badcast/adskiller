#pragma once

#include <QColor>
#include <QFont>
#include <QIcon>
#include <QObject>

class QWidget;
class QColor;
class QIcon;

/// Applies the application-wide Apple-style theme (palette + QSS from resources).
class ThemeManager : public QObject
{
    Q_OBJECT
public:
    enum class Theme
    {
        Light,
        Dark
    };

    static ThemeManager *instance();

    /// Applies the theme to the whole application. No-op if it is already active.
    void apply(Theme theme);
    Theme theme() const
    {
        return m_theme;
    }
    bool isDark() const
    {
        return m_theme == Theme::Dark;
    }

    /// Theme-aware monochrome icon from a resource SVG (":/svg/<name>"). Colors are
    /// re-mapped to the active theme every time the icon is painted.
    static QIcon icon(const QString &resourcePath);

    /// Maps a legacy dark-design color used in custom painting to the active theme.
    static QColor themed(const QColor &legacy, bool isText = false);

    /// Application UI font (Inter, falling back to the system font) at the given size/weight.
    static QFont uiFont(int pointSize, int weight = -1);

    /// Like themed(), but infers the role from the color itself: dark neutrals are surfaces,
    /// light neutrals are foreground/text. For colors used in custom painting.
    static QColor autoColor(const QColor &legacy);

    /// Adds a soft drop shadow to a static card. The effect is only enabled in the light theme
    /// (it is invisible on dark backgrounds), so it costs nothing there.
    void addCardShadow(QWidget *card);

    /// Recolors a widget-level stylesheet written for the legacy dark design to the given theme's tokens.
    static QString remapStyleSheet(const QString &css, Theme theme, bool roundable = false);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void themeChanged(ThemeManager::Theme theme);

private:
    explicit ThemeManager(QObject *parent = nullptr);

    static void loadFonts();
    static QString loadStyleSheet(Theme theme);
    static QPalette buildPalette(Theme theme);

    void restyleWidget(QWidget *w);
    void restyleLabel(class QLabel *label);
    void restyleAllWidgets();

    Theme m_theme = Theme::Dark;
    bool m_applied = false;
};
