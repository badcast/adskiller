import QtQuick
import QtQuick.Controls.Material
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Adskiller 1.0

Item {
    id: root

    property var service: AppController.activeService
    
    property var loadedContacts: []

    FileDialog {
        id: fileOpenDialog
        title: "Выберите VCF файл"
        nameFilters: ["VCard files (*.vcf)"]
        onAccepted: {
            if (service) {
                loadedContacts = service.loadVcf(selectedFile)
            }
        }
    }

    FileDialog {
        id: fileSaveDialog
        title: "Сохранить исправленный VCF"
        nameFilters: ["VCard files (*.vcf)"]
        fileMode: FileDialog.SaveFile
        onAccepted: {
            if (service) {
                let success = service.exportVcf(loadedContacts, selectedFile)
                if (success) {
                    // Show success
                    resultDialog.text = "Файл успешно экспортирован!"
                } else {
                    resultDialog.text = "Ошибка при сохранении файла!"
                }
                resultDialog.open()
            }
        }
    }

    Dialog {
        id: resultDialog
        property string text: ""
        title: "Результат"
        standardButtons: Dialog.Ok
        Label { text: resultDialog.text }
        anchors.centerIn: parent
    }

    SplitView {
        anchors.fill: parent
        anchors.margins: 20
        orientation: Qt.Horizontal

        // Left Panel: Batch Fixer
        ColumnLayout {
            SplitView.preferredWidth: parent.width * 0.6
            SplitView.minimumWidth: 300
            spacing: 16

            Label {
                text: "Массовое исправление VCard"
                font.pixelSize: 24
                font.bold: true
                color: AppController.textColor
            }

            RowLayout {
                spacing: 16
                Button {
                    text: "ОТКРЫТЬ ФАЙЛ"
                    Material.background: Material.accent
                    Material.foreground: "white"
                    onClicked: fileOpenDialog.open()
                }

                Button {
                    text: "ЭКСПОРТ (ИСПРАВИТЬ)"
                    Material.background: "#4CAF50"
                    Material.foreground: "white"
                    enabled: loadedContacts.length > 0
                    onClicked: fileSaveDialog.open()
                }
                
                Item { Layout.fillWidth: true }
            }

            Label {
                text: "Загружено контактов: " + loadedContacts.length
                color: AppController.textSecondary
                visible: loadedContacts.length > 0
            }

            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: loadedContacts

                delegate: ItemDelegate {
                    width: ListView.view.width
                    text: modelData.name + "\n" + (modelData.numbers ? modelData.numbers.join(", ") : "")
                }

                ScrollBar.vertical: ScrollBar {}
            }
            
            Button {
                text: "НАЗАД"
                Material.background: "#333333"
                Material.foreground: "white"
                onClicked: {
                    AppController.closeService()
                    if (stackView.depth > 1) stackView.pop()
                }
            }
        }

        // Right Panel: Single Number Analyzer
        ColumnLayout {
            SplitView.minimumWidth: 250
            Layout.fillHeight: true
            spacing: 16

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#1e1e1e"
                radius: 8
                border.color: "#333333"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 16

                    Label {
                        text: "Анализатор номера"
                        font.pixelSize: 20
                        font.bold: true
                        color: AppController.textColor
                    }

                    TextField {
                        id: numberInput
                        placeholderText: "Введите номер (например, +123456789)"
                        Layout.fillWidth: true
                        
                        onTextChanged: {
                            if (service && text.length > 3) {
                                let res = service.parseNumber(text)
                                resultView.updateInfo(res)
                            } else {
                                resultView.clear()
                            }
                        }
                    }

                    ColumnLayout {
                        id: resultView
                        Layout.fillWidth: true
                        spacing: 8
                        
                        property var currentData: null
                        
                        function clear() { currentData = null; }
                        function updateInfo(data) { currentData = data; }

                        Label {
                            text: "Результат:"
                            font.bold: true
                            color: AppController.textSecondary
                            visible: resultView.currentData !== null
                        }

                        Label {
                            text: resultView.currentData ? "Страна: " + resultView.currentData.country + " (+" + resultView.currentData.dialCode + ")" : ""
                            color: AppController.textColor
                            visible: resultView.currentData !== null && !resultView.currentData.isEmpty
                        }

                        Label {
                            text: resultView.currentData ? "Международный: " + resultView.currentData.beautyGlobal : ""
                            color: AppController.textColor
                            visible: resultView.currentData !== null && !resultView.currentData.isEmpty
                        }

                        Label {
                            text: resultView.currentData ? "Локальный: " + resultView.currentData.beautyLocal : ""
                            color: AppController.textColor
                            visible: resultView.currentData !== null && !resultView.currentData.isEmpty
                        }
                        
                        Label {
                            text: resultView.currentData ? "Компактный: " + resultView.currentData.compactGlobal : ""
                            color: AppController.textColor
                            visible: resultView.currentData !== null && !resultView.currentData.isEmpty
                        }

                        Label {
                            text: resultView.currentData && !resultView.currentData.isGeneric ? "⚠️ Номер не определен или слишком короткий" : "✅ Номер распознан"
                            color: resultView.currentData && !resultView.currentData.isGeneric ? "#F44336" : "#4CAF50"
                            visible: resultView.currentData !== null && !resultView.currentData.isEmpty
                        }
                    }

                    Item { Layout.fillHeight: true }
                }
            }
        }
    }
}
