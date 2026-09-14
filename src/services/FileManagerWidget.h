#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QSplitter>
#include <QIcon>
#include <QMenu>
#include <QPoint>
#include "AdbFront.h"

class FileManagerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FileManagerWidget(QWidget *parent = nullptr);
    ~FileManagerWidget() override = default;

    void setDevice(const AdbDevice &device);
    void refreshList();
    void navigateTo(const QString &path);

    static QString formatBytes(qint64 bytes);

private slots:
    void navigateUp();
    void navigateBack();
    void navigateForward();
    void createDirectory();
    void onItemDoubleClicked(int row, int column);
    void onSelectionChanged();
    void downloadSelected();
    void uploadFile();
    void previewSelected();
    void deleteSelected();
    void renameSelected();
    void onSearchFilterChanged(const QString &filter);
    void onQuickAccessClicked(QListWidgetItem *item);
    void showContextMenu(const QPoint &pos);

private:
    void setupUi();
    void populateTable();
    void updateInspector();
    QIcon getFileIcon(bool isDir, const QString &fileName);
    QString getFileType(bool isDir, const QString &fileName);

    AdbDevice m_device;
    AdbFileIO m_fileIO;
    QString m_currentPath = "/sdcard";
    QList<AdbFileInfo> m_currentItems;
    QList<AdbFileInfo> m_filteredItems;

    QStringList m_historyBack;
    QStringList m_historyForward;

    // Top Navigation UI
    QPushButton *m_btnBack = nullptr;
    QPushButton *m_btnForward = nullptr;
    QPushButton *m_btnUp = nullptr;
    QLineEdit *m_pathEdit = nullptr;
    QPushButton *m_btnCopyPath = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QPushButton *m_btnNewDir = nullptr;
    QPushButton *m_btnUpload = nullptr;
    QPushButton *m_btnRefresh = nullptr;

    // Center layout
    QListWidget *m_quickAccessList = nullptr;
    QTableWidget *m_table = nullptr;

    // Right inspector panel
    QWidget *m_inspectorPane = nullptr;
    QLabel *m_inspIcon = nullptr;
    QLabel *m_inspName = nullptr;
    QLabel *m_inspTypeBadge = nullptr;
    QLabel *m_inspSizeBadge = nullptr;
    QLabel *m_inspPathLabel = nullptr;
    QLabel *m_inspDateVal = nullptr;
    QLabel *m_inspPermsVal = nullptr;

    QPushButton *m_btnDownload = nullptr;
    QPushButton *m_btnPreview = nullptr;
    QPushButton *m_btnRename = nullptr;
    QPushButton *m_btnDelete = nullptr;

    // Status bar
    QLabel *m_statusSummary = nullptr;
    QLabel *m_statusSelected = nullptr;
    QLabel *m_statusMsg = nullptr;
};
