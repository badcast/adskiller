#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QProgressBar>
#include <QTableWidget>
#include <QStackedWidget>
#include <QTimer>
#include <QList>
#include <QPair>

class AITranslaterWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AITranslaterWidget(QWidget *parent = nullptr);
    ~AITranslaterWidget() override;

    // Called by service to pass network token / auth
    void setNetwork(class Network *net);

private:
    void setupUi();
    void setupHeaderFrame();
    void setupTextTranslatePanel();
    void setupDocumentPanel();
    void setupQueuePanel();
    void applyTheme();

    // ── Network helpers ──
    void sendTextTranslate();
    void sendDocumentTranslate(bool async);
    void pollAsyncTask();
    void requestLanguages();

    // ── UI state ──
    void setUiBusy(bool busy);
    void showStatus(const QString &msg, bool error = false);
    void clearTextResults();

    // ── Helpers ──
    QString selectedSourceLang() const;
    QString selectedTargetLang() const;
    void fillLanguageCombo(QComboBox *cb, const QString &selectCode = QString());

    // ── Private data ──
    Network *m_net = nullptr;
    QString m_asyncTaskId;
    QTimer *m_pollTimer = nullptr;

    // Header
    QLabel *m_lblStatus = nullptr;

    // Text panel
    QComboBox *m_cbSrcLang = nullptr;
    QComboBox *m_cbDstLang = nullptr;
    QTextEdit *m_editInput = nullptr;
    QTextEdit *m_editOutput = nullptr;
    QPushButton *m_btnSwapLang = nullptr;
    QPushButton *m_btnTranslateText = nullptr;
    QLabel *m_lblTextInfo = nullptr;

    // Document panel
    QLineEdit *m_editDocPath = nullptr;
    QComboBox *m_cbDocSrcLang = nullptr;
    QComboBox *m_cbDocDstLang = nullptr;
    QComboBox *m_cbTargetFmt = nullptr;
    QPushButton *m_btnChooseDoc = nullptr;
    QPushButton *m_btnTranslateDoc = nullptr;
    QPushButton *m_btnTranslateDocAsync = nullptr;
    QLabel *m_lblDocInfo = nullptr;
    QProgressBar *m_progressDoc = nullptr;

    // Async queue panel
    QTableWidget *m_queueTable = nullptr;
    QPushButton *m_btnRefreshQueue = nullptr;

    // State
    QByteArray m_pendingDocBytes;
    QString m_pendingDocName;
    QList<QPair<QString, QString>> m_languages; // code, name
};
