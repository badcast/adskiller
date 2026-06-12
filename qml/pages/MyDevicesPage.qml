import QtQuick
import QtQuick.Controls.Material
import QtQuick.Controls
import QtQuick.Layouts
import Adskiller 1.0

Item {
    id: root

    property var service: AppController.activeService

    onServiceChanged: {
        if (service && service.devices.length === 0) {
            fetchData()
        }
    }

    Component.onCompleted: {
        if (service && service.devices.length === 0) {
            fetchData()
        }
    }

    function formatToISO(dateStr, isEnd) {
        if (!dateStr.match(/^\d{4}-\d{2}-\d{2}$/)) return "";
        return dateStr + (isEnd ? "T23:59:59" : "T00:00:00");
    }

    function fetchData() {
        if (service) {
            let start = formatToISO(startDateField.text, false);
            let end = formatToISO(endDateField.text, true);
            if (start === "" || end === "") return;
            service.refreshData(start, end, guaranteeSwitch.checked)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 24

        Label {
            text: "Мои устройства"
            font.pixelSize: 24
            font.weight: Font.Bold
            color: "#FFFFFF"
            Layout.fillWidth: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 16

            TextField {
                id: startDateField
                placeholderText: "Начало (ГГГГ-ММ-ДД)"
                text: "2024-01-01"
                Layout.preferredWidth: 150
                color: "#FFFFFF"
                validator: RegularExpressionValidator { regularExpression: /^\d{4}-\d{2}-\d{2}$/ }
                onAccepted: fetchData()
                background: Rectangle { color: "transparent"; border.color: "#33FFFFFF"; radius: 8 }
            }

            TextField {
                id: endDateField
                placeholderText: "Конец (ГГГГ-ММ-ДД)"
                text: Qt.formatDateTime(new Date(), "yyyy-MM-dd")
                Layout.preferredWidth: 150
                color: "#FFFFFF"
                validator: RegularExpressionValidator { regularExpression: /^\d{4}-\d{2}-\d{2}$/ }
                onAccepted: fetchData()
                background: Rectangle { color: "transparent"; border.color: "#33FFFFFF"; radius: 8 }
            }

            Switch {
                id: guaranteeSwitch
                text: "Гарантия"
                checked: true
                contentItem: Text {
                    text: guaranteeSwitch.text
                    color: "#FFFFFF"
                    font.pixelSize: 14
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: guaranteeSwitch.indicator.width + guaranteeSwitch.spacing
                }
                onCheckedChanged: {
                    if (service) service.filterData(checked)
                }
            }

            Item { Layout.fillWidth: true }

            Button {
                id: refreshBtn
                text: "ОБНОВИТЬ"
                enabled: service ? !service.isRefreshing : false
                Layout.preferredHeight: 36
                
                contentItem: Label {
                    text: refreshBtn.text
                    font.weight: Font.Bold
                    font.pixelSize: 13
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: refreshBtn.down ? "#99e11d48" : (refreshBtn.hovered ? "#e11d48" : "transparent")
                    radius: 18
                    border.color: "#e11d48"
                    border.width: 1
                }
                onClicked: fetchData()
            }

            Button {
                id: backBtn
                text: "НАЗАД"
                Layout.preferredHeight: 36
                contentItem: Label {
                    text: backBtn.text
                    font.weight: Font.Bold
                    font.pixelSize: 13
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: backBtn.down ? "#44FFFFFF" : (backBtn.hovered ? "#33FFFFFF" : "transparent")
                    radius: 18
                    border.color: "#888888"
                    border.width: 1
                }
                onClicked: {
                    AppController.closeService()
                    if (stackView.depth > 1) stackView.pop()
                }
            }
        }

        ProgressBar {
            Layout.fillWidth: true
            indeterminate: true
            visible: service ? service.isRefreshing : false
            Material.accent: "#FF416C"
        }

        ListView {
            id: listView
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: service ? service.devices : []
            clip: true
            spacing: 12

            delegate: Rectangle {
                width: listView.width
                height: 80
                color: Qt.rgba(20/255, 25/255, 40/255, 0.6)
                radius: 16
                border.color: "#22FFFFFF"
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 24

                    // Left Column (Icon + Name)
                    RowLayout {
                        Layout.preferredWidth: parent.width * 0.3
                        spacing: 16

                        Rectangle {
                            width: 48
                            height: 48
                            radius: 12
                            color: modelData.vendor.toLowerCase().includes("xiaomi") ? "#FF7A00" : "#3b82f6"
                            Text {
                                anchors.centerIn: parent
                                text: modelData.vendor.charAt(0).toUpperCase()
                                color: "#FFFFFF"
                                font.pixelSize: 24
                                font.weight: Font.Bold
                            }
                            // Small green circle indicator
                            Rectangle {
                                width: 12
                                height: 12
                                radius: 6
                                color: "#4CAF50"
                                border.color: "#1a1f2e"
                                border.width: 2
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                visible: true
                            }
                        }

                        ColumnLayout {
                            spacing: 4
                            Label {
                                text: modelData.vendor + " " + modelData.model
                                font.pixelSize: 16
                                font.weight: Font.Bold
                                color: "#FFFFFF"
                            }
                            Label {
                                text: "ID: " + modelData.id
                                font.pixelSize: 12
                                color: "#888888"
                            }
                        }
                    }

                    // Middle Column 1
                    ColumnLayout {
                        Layout.preferredWidth: parent.width * 0.2
                        spacing: 4
                        Label {
                            text: "Последняя акт."
                            font.pixelSize: 12
                            color: "#888888"
                        }
                        Label {
                            text: modelData.lastConnection
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            color: "#FFFFFF"
                        }
                    }

                    // Middle Column 2
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Label {
                            text: "Регистрация"
                            font.pixelSize: 12
                            color: "#888888"
                        }
                        Label {
                            text: modelData.registrationDate
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            color: "#FFFFFF"
                        }
                    }

                    // Middle Column 3
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Label {
                            text: "Пакетов / Подкл."
                            font.pixelSize: 12
                            color: "#888888"
                        }
                        Label {
                            text: modelData.packages + " / " + modelData.connections
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            color: "#FFFFFF"
                        }
                    }

                    // Right Column (Button / Expiration)
                    Button {
                        Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                        Layout.preferredWidth: 160
                        Layout.preferredHeight: 40
                        
                        contentItem: RowLayout {
                            anchors.centerIn: parent
                            spacing: 4
                            Label {
                                text: modelData.expiration
                                color: modelData.expiration.includes("1970") ? "#FF416C" : "#4CAF50"
                                font.pixelSize: 12
                                font.weight: Font.Bold
                            }
                            Text {
                                text: ">"
                                color: modelData.expiration.includes("1970") ? "#FF416C" : "#4CAF50"
                                font.pixelSize: 14
                                font.weight: Font.Bold
                            }
                        }
                        
                        background: Rectangle {
                            color: "transparent"
                            radius: 20
                            border.color: modelData.expiration.includes("1970") ? "#FF416C" : "#4CAF50"
                            border.width: 1
                            Rectangle {
                                anchors.fill: parent
                                radius: 20
                                color: modelData.expiration.includes("1970") ? "#FF416C" : "#4CAF50"
                                opacity: 0.1
                            }
                        }
                    }
                }
            }
        }
        
        Label {
            text: "Нет данных для отображения"
            visible: service && !service.isRefreshing && service.devices.length === 0
            font.pixelSize: 16
            color: "#888888"
            Layout.alignment: Qt.AlignCenter
        }
    }
}
