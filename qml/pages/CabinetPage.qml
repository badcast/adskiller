import QtQuick
import QtQuick.Controls.Material
import QtQuick.Controls
import QtQuick.Layouts
import Adskiller 1.0

Item {
    id: root
    
    property var activeServices: []
    property var inactiveServices: []

    function updateServiceLists() {
        let active = [];
        let inactive = [];
        let list = AppController.serviceList;
        for (let i = 0; i < list.length; i++) {
            let s = list[i];
            let wrapper = { "originalIndex": i, "icon": s.icon, "title": s.title, "active": s.active };
            if (s.active) {
                active.push(wrapper);
            } else {
                inactive.push(wrapper);
            }
        }
        activeServices = active;
        inactiveServices = inactive;
    }

    Connections {
        target: AppController
        function onServicesChanged() {
            root.updateServiceLists()
        }
    }

    Component.onCompleted: {
        updateServiceLists()
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 32

        // Glassmorphism Sidebar
        GlassCard {
            id: sidebar
            Layout.preferredWidth: 320
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 24

                // User profile section
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16
                    
                    Rectangle {
                        width: 64
                        height: 64
                        radius: 32
                        color: "#3b82f6" // Fluent Blue
                        Label {
                            anchors.centerIn: parent
                            text: AppController.loginName.length > 0 ? AppController.loginName.charAt(0).toUpperCase() : "?"
                            font.pixelSize: 28
                            font.weight: Font.Bold
                            color: "white"
                        }

                        // Green online badge
                        Rectangle {
                            width: 14
                            height: 14
                            radius: 7
                            color: AppController.blocked ? "#FF416C" : "#4CAF50"
                            border.color: "#1a1f2e"
                            border.width: 2
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.margins: 4
                        }
                    }
                    
                    ColumnLayout {
                        spacing: 4
                        Layout.fillWidth: true
                        Label {
                            text: AppController.loginName
                            font.pixelSize: 20
                            font.weight: Font.Bold
                            color: "#FFFFFF"
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Label {
                            text: AppController.vipDays > 0 ? "Premium User" : "User"
                            font.pixelSize: 12
                            color: AppController.vipDays > 0 ? "#FFC107" : "#888888"
                        }
                    }
                }

                // Statistics grid
                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    rowSpacing: 12
                    columnSpacing: 12

                    Label { text: "Кредиты:"; color: "#AAAAAA"; font.pixelSize: 13; Layout.alignment: Qt.AlignLeft }
                    Label { text: AppController.credits; font.weight: Font.Bold; color: "#00F2FE"; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.Wrap }

                    Label { text: "VIP (Дней):"; color: "#AAAAAA"; font.pixelSize: 13; Layout.alignment: Qt.AlignLeft }
                    Label { text: AppController.vipDays; font.weight: Font.Bold; color: "#FFC107"; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.Wrap }
                    
                    Label { text: "Устройств:"; color: "#AAAAAA"; font.pixelSize: 13; Layout.alignment: Qt.AlignLeft }
                    Label { text: AppController.connectedDevices; font.weight: Font.Bold; color: "#FFFFFF"; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.Wrap }

                    Label { text: "Локация:"; color: "#AAAAAA"; font.pixelSize: 13; Layout.alignment: Qt.AlignLeft }
                    Label { text: AppController.location; font.weight: Font.Bold; color: "#FFFFFF"; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.Wrap }
                    
                    Label { text: "Баз. цена:"; color: "#AAAAAA"; font.pixelSize: 13; Layout.alignment: Qt.AlignLeft }
                    Label { text: AppController.basePrice + " " + AppController.currencyType; font.weight: Font.Bold; color: "#FFFFFF"; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.Wrap }

                    Label { text: "Посл. вход:"; color: "#AAAAAA"; font.pixelSize: 13; Layout.alignment: Qt.AlignLeft }
                    Label { text: Qt.formatDateTime(AppController.lastLogin, "dd.MM.yyyy hh:mm"); font.weight: Font.Bold; color: "#FFFFFF"; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.Wrap }
                }

                // Green refresh button
                Button {
                    id: refreshBtn
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    
                    contentItem: RowLayout {
                        anchors.centerIn: parent
                        spacing: 8
                        Text { text: "🔄"; color: "#4CAF50"; font.pixelSize: 16 }
                        Label {
                            text: "Обновить сервисы"
                            color: "#4CAF50"
                            font.pixelSize: 14
                            font.bold: true
                        }
                    }
                    
                    background: Rectangle {
                        color: "transparent"
                        radius: 8
                        border.color: "#1a4CAF50"
                        border.width: 1
                        Rectangle {
                            anchors.fill: parent
                            radius: 8
                            color: "#4CAF50"
                            opacity: refreshBtn.down ? 0.2 : 0.1
                        }
                    }
                    onClicked: {
                        if (typeof AppController !== "undefined" && AppController) {
                            AppController.refreshServices();
                        }
                    }
                }

                // Spacer
                Item { height: 16 }

                // Navigation menu (Functional and minimal, no garbage)
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    
                    // Serivces Tab (Active)
                    Rectangle {
                        Layout.fillWidth: true
                        height: 44
                        radius: 8
                        color: "#2a3556"
                        
                        Rectangle {
                            width: 4
                            height: 24
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: -4
                            radius: 2
                            color: "#3b82f6"
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 12
                            Text { text: "🏠"; color: "#FFFFFF"; font.pixelSize: 16 }
                            Label {
                                text: "Сервисы"
                                color: "#FFFFFF"
                                font.pixelSize: 15
                                Layout.fillWidth: true
                            }
                        }
                    }

                    // Logout Tab
                    Rectangle {
                        Layout.fillWidth: true
                        height: 44
                        radius: 8
                        color: "transparent"
                        
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 12
                            Text { text: "⚙️"; color: "#FFFFFF"; font.pixelSize: 16 }
                            Label {
                                text: "Выход"
                                color: "#AAAAAA"
                                font.pixelSize: 15
                                Layout.fillWidth: true
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                AppController.explicitLogout = true;
                                AppController.logoutSystem();
                            }
                        }
                    }
                }

                Item { Layout.fillHeight: true } // Spacer pushes everything up
            }
        }

        // Main Content Area
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "transparent"

            ColumnLayout {
                anchors.fill: parent
                spacing: 24

                RowLayout {
                    Layout.fillWidth: true
                    
                    Label {
                        text: "Сервисы"
                        font.pixelSize: 32
                        font.weight: Font.Bold
                        color: "#FFFFFF"
                        Layout.fillWidth: true
                    }

                    ToolButton {
                        text: "⋮"
                        font.pixelSize: 32
                        onClicked: quickSettingsMenu.open()
                        
                        Menu {
                            id: quickSettingsMenu
                            y: parent.height
                            
                            MenuItem {
                                text: "Поддержка"
                                onClicked: AppController.openSupport()
                            }
                            MenuItem {
                                text: "О программе"
                                onClicked: AppController.openAbout()
                            }
                        }
                    }
                }

                ScrollView {
                    id: scrollView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    Column {
                        width: scrollView.availableWidth
                        spacing: 24

                        // Available Services Section
                        Column {
                            width: parent.width
                            spacing: 16
                            visible: root.activeServices.length > 0

                            Label {
                                text: "Доступные сервисы"
                                font.pixelSize: 18
                                font.weight: Font.Bold
                                color: "#FFFFFF"
                            }

                            Flow {
                                width: parent.width
                                spacing: 16

                                Repeater {
                                    model: root.activeServices

                                    delegate: Item {
                                        width: 160
                                        height: 160

                                        Rectangle {
                                            id: cardRect
                                            anchors.fill: parent
                                            color: Qt.rgba(20/255, 25/255, 40/255, 0.6) // Glass look
                                            radius: 12
                                            
                                            border.color: mouseAreaActive.containsMouse ? "#00F2FE" : "#33FFFFFF"
                                            border.width: mouseAreaActive.containsMouse ? 2 : 1
                                            
                                            // Glow effect for active/hovered
                                            Rectangle {
                                                anchors.fill: parent
                                                radius: 12
                                                color: "transparent"
                                                border.color: mouseAreaActive.containsMouse ? "#3300F2FE" : "transparent"
                                                border.width: 4
                                                z: -1
                                            }

                                            ColumnLayout {
                                                anchors.fill: parent
                                                anchors.margins: 12
                                                spacing: 12

                                                Item { Layout.fillHeight: true }

                                                Image {
                                                    Layout.alignment: Qt.AlignHCenter
                                                    source: modelData.icon ? ("qrc:/service-icons/" + modelData.icon) : ""
                                                    width: 64
                                                    height: 64
                                                    sourceSize: Qt.size(64, 64)
                                                    fillMode: Image.PreserveAspectFit
                                                }

                                                Label {
                                                    text: modelData.title
                                                    font.pixelSize: 14
                                                    font.weight: Font.DemiBold
                                                    color: "#FFFFFF"
                                                    Layout.fillWidth: true
                                                    horizontalAlignment: Text.AlignHCenter
                                                    wrapMode: Text.WordWrap
                                                    maximumLineCount: 2
                                                    elide: Text.ElideRight
                                                }
                                                
                                                Item { Layout.fillHeight: true }
                                            }

                                            MouseArea {
                                                id: mouseAreaActive
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                onClicked: {
                                                    AppController.runServiceQml(modelData.originalIndex)
                                                }
                                                onEntered: cardRect.color = Qt.rgba(20/255, 25/255, 40/255, 0.8)
                                                onExited: cardRect.color = Qt.rgba(20/255, 25/255, 40/255, 0.6)
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // Unavailable Services Section
                        Column {
                            width: parent.width
                            spacing: 16
                            visible: root.inactiveServices.length > 0

                            Label {
                                text: "Недоступные сервисы"
                                font.pixelSize: 18
                                font.weight: Font.Bold
                                color: "#888888"
                            }

                            Flow {
                                width: parent.width
                                spacing: 16

                                Repeater {
                                    model: root.inactiveServices

                                    delegate: Item {
                                        width: 160
                                        height: 160
                                        opacity: 0.5

                                        Rectangle {
                                            anchors.fill: parent
                                            color: Qt.rgba(15/255, 20/255, 30/255, 0.4)
                                            radius: 12
                                            border.color: "#22FFFFFF"
                                            border.width: 1

                                            ColumnLayout {
                                                anchors.fill: parent
                                                anchors.margins: 12
                                                spacing: 12

                                                Item { Layout.fillHeight: true }

                                                Image {
                                                    Layout.alignment: Qt.AlignHCenter
                                                    source: modelData.icon ? ("qrc:/service-icons/" + modelData.icon) : ""
                                                    width: 64
                                                    height: 64
                                                    sourceSize: Qt.size(64, 64)
                                                    fillMode: Image.PreserveAspectFit
                                                }

                                                Label {
                                                    text: modelData.title
                                                    font.pixelSize: 14
                                                    font.weight: Font.DemiBold
                                                    color: "#888888"
                                                    Layout.fillWidth: true
                                                    horizontalAlignment: Text.AlignHCenter
                                                    wrapMode: Text.WordWrap
                                                }
                                                
                                                Item { Layout.fillHeight: true }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
