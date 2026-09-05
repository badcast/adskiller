#pragma once

#include <QDialog>

class QTabWidget;
class QTextEdit;
class QPushButton;
class QLabel;
class QVBoxLayout;
class QNetworkAccessManager;
class QJsonArray;

class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    enum TabIndex
    {
        TabAbout = 0,
        TabAuthors = 1,
        TabGplV3 = 2,
        TabChangelog = 3
    };

    explicit AboutDialog(QWidget *parent = nullptr);
    ~AboutDialog() override = default;

    void setCurrentTab(TabIndex tab);

private slots:
    void copyLicenseToClipboard();
    void openProjectWebsite();
    void openGplWebsite();
    void openSupportWhatsApp();
    void fetchChangelog();
    void openChangelogWebsite();

private:
    void setupUi();
    QWidget *createHeaderWidget();
    QWidget *createAboutTab();
    QWidget *createAuthorsTab();
    QWidget *createGplTab();
    QWidget *createChangelogTab();
    QWidget *createFooterWidget();

    QWidget *createAuthorCard(const QString &initials,
                             const QColor &gradStart,
                             const QColor &gradEnd,
                             const QString &name,
                             const QString &role,
                             const QString &roleColor,
                             const QString &description,
                             const QString &contact = QString());

    static QPixmap createAvatarPixmap(const QString &initials,
                                      const QColor &startColor,
                                      const QColor &endColor,
                                      int size = 48);

    static QString loadLicenseText();

    void renderChangelog(const QJsonArray &entries);
    void showChangelogLoading();
    void showChangelogError(const QString &errorMsg);
    static QString formatChangelogText(const QString &raw);
    static QJsonArray getFallbackChangelog();

    QTabWidget *m_tabWidget { nullptr };
    QTextEdit *m_licenseEdit { nullptr };
    QPushButton *m_copyLicenseBtn { nullptr };

    // Changelog tab elements
    QNetworkAccessManager *m_netManager { nullptr };
    QVBoxLayout *m_changelogListLayout { nullptr };
    QWidget *m_changelogStatusWidget { nullptr };
    QLabel *m_changelogStatusLabel { nullptr };
    QPushButton *m_changelogRetryBtn { nullptr };
    QPushButton *m_changelogRefreshBtn { nullptr };
};
