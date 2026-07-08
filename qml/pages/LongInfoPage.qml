import QtQuick
import QtQuick.Controls.Material
import QtQuick.Controls
import QtQuick.Layouts
import Adskiller 1.0

Item {
    id: root

    property var service: AppController.activeService
    property bool hasSysInfo: service && typeof service.sysOsVersion !== "undefined" && service.sysOsVersion !== ""

    RowLayout {
        anchors.fill: parent
        anchors.margins: 40
        spacing: 40

        // Left side: Main progress and logs
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 30

            Label {
                text: service ? service.deviceName : ""
                font.pixelSize: 22
                font.bold: true
                color: "#FFFFFF"
                Layout.alignment: Qt.AlignHCenter
            }

            // Circular Progress
            CircularProgress {
                id: progressCircle
                width: 240
                height: 240
                Layout.alignment: Qt.AlignHCenter
                progress: service ? (service.progress / 100.0) : 0.0
                text: service ? service.progress + "%" : "0%"
                
                glowColor: {
                    if (!service) return "#3b82f6";
                    if (service.isRunning) return "#00F2FE";
                    return service.successState ? "#4CAF50" : "#F44336";
                }
            }

            Label {
                text: service ? service.statusText : ""
                font.pixelSize: 18
                color: "#AAAAAA"
                Layout.alignment: Qt.AlignHCenter
                Layout.maximumWidth: 500
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }

            // Marquee Log Banner
            GlassCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                visible: service && typeof service.logs !== "undefined"
                
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 12
                    
                    Text { text: "📝"; font.pixelSize: 20 }
                    
                    // Marquee Area
                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        
                        Text {
                            id: marqueeText
                            text: service && service.logs && service.logs.length > 0 ? service.logs[service.logs.length - 1] : "..."
                            color: "#00F2FE"
                            font.pixelSize: 15
                            font.family: "monospace"
                            anchors.verticalCenter: parent.verticalCenter
                            x: parent.width
                            
                            NumberAnimation on x {
                                id: marqueeAnim
                                from: marqueeText.parent.width
                                to: -marqueeText.width
                                duration: 8000
                                loops: Animation.Infinite
                                running: service && service.isRunning
                            }
                        }
                    }
                    
                    Button {
                        text: "Развернуть"
                        Layout.preferredHeight: 36
                        background: Rectangle {
                            color: "transparent"
                            border.color: "#33FFFFFF"
                            radius: 8
                        }
                        contentItem: Label {
                            text: parent.text
                            color: "white"
                            font.pixelSize: 13
                        }
                        onClicked: logPopup.open()
                    }
                }
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 20

                Button {
                    text: "ПОВТОРИТЬ"
                    Layout.preferredWidth: 200
                    Layout.preferredHeight: 56
                    visible: service ? !service.isRunning : false
                    background: Rectangle {
                        color: "#FF416C"
                        radius: 28
                    }
                    contentItem: Label {
                        text: parent.text
                        color: "white"
                        font.bold: true
                        font.pixelSize: 16
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        if (service && service.canStart()) {
                            service.start();
                        }
                    }
                }
                
                Button {
                    text: "НАЗАД"
                    Layout.preferredWidth: 200
                    Layout.preferredHeight: 56
                    enabled: service ? !service.isRunning : true
                    background: Rectangle {
                        color: "transparent"
                        border.color: parent.enabled ? "#555555" : "#222222"
                        border.width: 2
                        radius: 28
                    }
                    contentItem: Label {
                        text: parent.text
                        color: parent.enabled ? "white" : "#777777"
                        font.bold: true
                        font.pixelSize: 16
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        AppController.closeService()
                        if (stackView.depth > 1) stackView.pop()
                    }
                }
            }
        }

        // Right side: Split box for device specs
        GlassCard {
            Layout.preferredWidth: 350
            Layout.fillHeight: true
            visible: root.hasSysInfo
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 24
                spacing: 24

                Label {
                    text: "Информация об устройстве"
                    font.pixelSize: 18
                    font.bold: true
                    color: "#FFFFFF"
                    Layout.alignment: Qt.AlignHCenter
                }

                // Big Phone Icon
                Rectangle {
                    width: 120
                    height: 120
                    radius: 60
                    color: "#11FFFFFF"
                    Layout.alignment: Qt.AlignHCenter
                    border.color: "#3300F2FE"
                    border.width: 2
                    
                    Text {
                        anchors.centerIn: parent
                        text: "📱"
                        font.pixelSize: 64
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: "#33FFFFFF" }

                // Device Specs
                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    rowSpacing: 16
                    columnSpacing: 16

                    Text { text: "⚙️ ОС:"; color: "#AAAAAA"; font.pixelSize: 15 }
                    Label { text: service ? service.sysOsVersion : ""; color: "#00F2FE"; font.pixelSize: 15; font.bold: true; Layout.fillWidth: true; wrapMode: Text.Wrap }

                    Text { text: "📱 Модель:"; color: "#AAAAAA"; font.pixelSize: 15 }
                    Label { text: service ? service.sysModel : ""; color: "#FFFFFF"; font.pixelSize: 15; font.bold: true; Layout.fillWidth: true; wrapMode: Text.Wrap }

                    Text { text: "🏢 Производитель:"; color: "#AAAAAA"; font.pixelSize: 15 }
                    Label { text: service ? service.sysVendor : ""; color: "#FFFFFF"; font.pixelSize: 15; font.bold: true; Layout.fillWidth: true; wrapMode: Text.Wrap }

                    Text { text: "💾 Хранилище:"; color: "#AAAAAA"; font.pixelSize: 15 }
                    Label { text: service ? service.sysStorage : ""; color: "#FFFFFF"; font.pixelSize: 15; font.bold: true; Layout.fillWidth: true; wrapMode: Text.Wrap }

                    Text { text: "🧠 ОЗУ:"; color: "#AAAAAA"; font.pixelSize: 15 }
                    Label { text: service ? service.sysRam : ""; color: "#FFFFFF"; font.pixelSize: 15; font.bold: true; Layout.fillWidth: true; wrapMode: Text.Wrap }

                    Text { text: "🔧 Ядро:"; color: "#AAAAAA"; font.pixelSize: 15 }
                    Label { text: service ? service.sysKernel : ""; color: "#FFFFFF"; font.pixelSize: 15; font.bold: true; Layout.fillWidth: true; wrapMode: Text.Wrap }
                }

                Item { Layout.fillHeight: true }
            }
        }
    }

    // Full logs popup
    Popup {
        id: logPopup
        width: parent.width * 0.8
        height: parent.height * 0.8
        anchors.centerIn: parent
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        
        background: Rectangle { 
            color: "#E6121212"
            radius: 16
            border.color: "#33FFFFFF"
            border.width: 1
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 16

            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: "Полный журнал (adskiller logs)"
                    font.pixelSize: 20
                    font.bold: true
                    color: "#FFFFFF"
                    Layout.fillWidth: true
                }
                Button {
                    text: "✕"
                    background: Rectangle { color: "transparent" }
                    contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 20 }
                    onClicked: logPopup.close()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#11000000"
                border.color: "#33FFFFFF"
                radius: 8
                
                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 12
                    clip: true
                    
                    TextArea {
                        text: service && typeof service.logs !== "undefined" ? service.logs.join("\n") : ""
                        color: "#00F2FE"
                        font.family: "monospace"
                        font.pixelSize: 14
                        readOnly: true
                        background: null
                        wrapMode: TextArea.Wrap
                    }
                }
            }
        }
    }
}
