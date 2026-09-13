#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QSplitter>
#include <QIcon>
#include <vector>

#include "adbfront.h"

struct ContactFixerPrivate;

struct ContactItem
{
    QString name;
    QString originalNumber;
    QString fixedNumber;
    QString country;
    QString dialCode;
    int countryCode = 0;
    QString phoneType;
    bool isSelected = true;
    size_t vcardIndex = 0;
    size_t propIndex = 0;
};

class ContactFixerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ContactFixerWidget(QWidget *parent = nullptr);
    ~ContactFixerWidget() override;

    void setDevice(const AdbDevice &device);

private slots:
    void openVcfFile();
    void loadFromDevice();
    void saveVcfFile();
    void pushToDevice();
    void exportCsv();
    void applyFixToAll();
    void onFormatRuleChanged(int index);
    void onSearchFilterChanged(const QString &text);
    void onTableItemChanged(QTableWidgetItem *item);
    void onSelectAllClicked();
    void onDeselectAllClicked();
    void onTestNumberInputChanged(const QString &text);
    void onQuickAddContact();
    void requestDeviceConnect();

private:
    void setupUi();
    void updateDeviceUi();
    void populateTable();
    void updateStats();
    void parseVcards();
    QString computeFixedNumber(const QString &rawNumber) const;
    void applyFixToVcards();

    AdbDevice m_device;
    AdbFileIO m_fileIO;
    ContactFixerPrivate *d = nullptr;
    QList<ContactItem> m_items;
    QList<ContactItem> m_filteredItems;
    QString m_loadedFilePath;
    bool m_autoLoadOnConnect = false;

    // Device & Source Controls
    QLabel *m_lblDeviceStatus = nullptr;
    QPushButton *m_btnConnectDevice = nullptr;
    QLineEdit *m_loadedPathEdit = nullptr;
    QLineEdit *m_remotePathEdit = nullptr;

    // Action Buttons
    QPushButton *m_btnOpenVcf = nullptr;
    QPushButton *m_btnLoadDevice = nullptr;
    QPushButton *m_btnSaveVcf = nullptr;
    QPushButton *m_btnPushDevice = nullptr;
    QPushButton *m_btnExportCsv = nullptr;
    QPushButton *m_btnApplyFix = nullptr;

    // Format & Filter
    QComboBox *m_comboFormatRule = nullptr;
    QCheckBox *m_chkFixLeading8 = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QPushButton *m_btnSelectAll = nullptr;
    QPushButton *m_btnDeselectAll = nullptr;

    QTableWidget *m_table = nullptr;

    // Right Side: Live Tester & Quick Add
    QLineEdit *m_testNumberInput = nullptr;
    QLabel *m_testCountryLabel = nullptr;
    QLabel *m_testDialCodeLabel = nullptr;
    QLabel *m_testValidLabel = nullptr;
    QLabel *m_valBeautyGlobal = nullptr;
    QLabel *m_valBeautyLocal = nullptr;
    QLabel *m_valCompactGlobal = nullptr;
    QLabel *m_valCompactLocal = nullptr;

    QLineEdit *m_quickNameEdit = nullptr;
    QLineEdit *m_quickNumberEdit = nullptr;
    QPushButton *m_btnQuickAdd = nullptr;

    // Stats
    QLabel *m_statTotalContacts = nullptr;
    QLabel *m_statTotalNumbers = nullptr;
    QLabel *m_statNeedsFix = nullptr;
    QLabel *m_statusMsg = nullptr;
};
