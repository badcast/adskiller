#pragma once

#include <QDialog>

class QTabWidget;
class QTextEdit;
class QPushButton;

class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    enum TabIndex
    {
        TabAbout = 0,
        TabAuthors = 1,
        TabGplV3 = 2
    };

    explicit AboutDialog(QWidget *parent = nullptr);
    ~AboutDialog() override = default;

    void setCurrentTab(TabIndex tab);

private slots:
    void copyLicenseToClipboard();
    void openProjectWebsite();
    void openGplWebsite();
    void openSupportWhatsApp();

private:
    void setupUi();
    QWidget *createHeaderWidget();
    QWidget *createAboutTab();
    QWidget *createAuthorsTab();
    QWidget *createGplTab();
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

    QTabWidget *m_tabWidget { nullptr };
    QTextEdit *m_licenseEdit { nullptr };
    QPushButton *m_copyLicenseBtn { nullptr };
};
