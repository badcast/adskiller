import QtQuick
import QtQuick.Controls.Material
import QtQuick.Controls
import QtQuick.Layouts
import Adskiller 1.0

Item {
    id: root

    property var service: AppController.activeService
    property var adbDevices: AppController.getAdbDevices()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            spacing: 16

            Label {
                text: "Выберите устройство"
                font.pixelSize: 24
                font.bold: true
                color: AppController.textColor
                Layout.fillWidth: true
            }

            Button {
                id: refreshBtn
                text: "ОБНОВИТЬ СПИСОК"
                Layout.preferredHeight: 40
                
                contentItem: Label {
                    text: refreshBtn.text
                    font.weight: Font.Bold
                    font.pixelSize: 14
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
                background: Rectangle {
                    color: refreshBtn.down ? "#44FFFFFF" : (refreshBtn.hovered ? "#33FFFFFF" : "#1AFFFFFF")
                    radius: 8
                    border.color: refreshBtn.hovered ? "#00F2FE" : "#33FFFFFF"
                    border.width: 1
                    
                    Behavior on color { ColorAnimation { duration: 150 } }
                    Behavior on border.color { ColorAnimation { duration: 150 } }
                }

                Timer {
                    interval: 1000
                    running: root.visible
                    repeat: true
                    onTriggered: {
                        var newDevices = AppController.getAdbDevices();
                        if (root.adbDevices.length !== newDevices.length) {
                            root.adbDevices = newDevices;
                        }
                    }
                }

                onVisibleChanged: {
                    if (visible) {
                        root.adbDevices = AppController.getAdbDevices();
                    }
                }

                onClicked: {
                    root.adbDevices = AppController.getAdbDevices();
                }
            }

            Button {
                id: backBtn
                text: "НАЗАД"
                Layout.preferredHeight: 40
                
                contentItem: Label {
                    text: backBtn.text
                    font.weight: Font.Bold
                    font.pixelSize: 14
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
                background: Rectangle {
                    color: backBtn.down ? "#44FFFFFF" : (backBtn.hovered ? "#33FFFFFF" : "#11FFFFFF")
                    radius: 8
                    border.color: backBtn.hovered ? "#FF416C" : "#33FFFFFF"
                    border.width: 1
                    
                    Behavior on color { ColorAnimation { duration: 150 } }
                    Behavior on border.color { ColorAnimation { duration: 150 } }
                }

                onClicked: {
                    AppController.closeService()
                    if (stackView.depth > 1) stackView.pop()
                }
            }
        }

        ListView {
            id: listView
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: root.adbDevices
            clip: true
            spacing: 16

            delegate: Rectangle {
                width: listView.width
                height: 100
                color: AppController.cardColor
                radius: 12
                border.color: hoverArea.containsMouse ? Material.accent : "#333333"
                border.width: hoverArea.containsMouse ? 2 : 1

                MouseArea {
                    id: hoverArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        AppController.selectAdbDevice(modelData.devId)
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 20

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Label {
                            text: modelData.displayName !== "" ? modelData.displayName : (modelData.vendor + " " + modelData.model)
                            font.pixelSize: 18
                            font.bold: true
                            color: AppController.textColor
                        }
                        Label {
                            text: "ID: " + modelData.devId
                            font.pixelSize: 14
                            color: AppController.textSecondary
                        }
                    }

                    Button {
                        id: selectBtn
                        text: "ВЫБРАТЬ"
                        Layout.alignment: Qt.AlignVCenter
                        Layout.preferredHeight: 40
                        
                        contentItem: Label {
                            text: selectBtn.text
                            font.weight: Font.Bold
                            font.pixelSize: 14
                            color: "white"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        
                        background: Rectangle {
                            color: selectBtn.down ? "#44FFFFFF" : (selectBtn.hovered ? "#33FFFFFF" : "#1AFFFFFF")
                            radius: 8
                            border.color: selectBtn.hovered ? "#00F2FE" : "#33FFFFFF"
                            border.width: 1
                            
                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on border.color { ColorAnimation { duration: 150 } }
                        }

                        onClicked: {
                            AppController.selectAdbDevice(modelData.devId)
                        }
                    }
                }
            }
        }

        ColumnLayout {
            Layout.alignment: Qt.AlignCenter
            visible: root.adbDevices.length === 0
            spacing: 20

            BusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                running: root.adbDevices.length === 0
                width: 64
                height: 64
                Material.accent: Material.accent
            }

            Label {
                text: "Ожидание подключения..."
                font.pixelSize: 20
                font.bold: true
                color: "white"
                Layout.alignment: Qt.AlignHCenter

                SequentialAnimation on opacity {
                    loops: Animation.Infinite
                    running: root.adbDevices.length === 0
                    NumberAnimation { from: 1.0; to: 0.4; duration: 1000; easing.type: Easing.InOutSine }
                    NumberAnimation { from: 0.4; to: 1.0; duration: 1000; easing.type: Easing.InOutSine }
                }
            }

            Label {
                text: "Пожалуйста, подключите устройство по USB\nи убедитесь, что отладка по USB включена."
                font.pixelSize: 16
                color: AppController.textSecondary
                Layout.alignment: Qt.AlignHCenter
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
