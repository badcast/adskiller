import QtQuick
import QtQuick.Controls.Material
import QtQuick.Controls
import QtQuick.Layouts
import Adskiller 1.0
import "../components"

Item {
    id: root

    Component.onCompleted: {
        if (typeof AppController !== "undefined" && AppController) {
            loginField.text = AppController.savedLogin || "";
            passwordField.text = AppController.savedPassword || "";
            if (loginField.text !== "" && passwordField.text !== "") {
                if (!window.explicitLogout) {
                    loginButton.clicked();
                } else {
                    window.explicitLogout = false;
                }
            }
        }
    }

    GlassCard {
        id: authCard
        width: 440
        height: 520
        anchors.centerIn: parent

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 40
            spacing: 24

            Image {
                id: logo
                source: "qrc:/resources/app-logo"
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 120
                Layout.preferredHeight: 120
                fillMode: Image.PreserveAspectFit
            }

            Label {
                text: "АВТОРИЗАЦИЯ"
                font.pixelSize: 22
                font.bold: true
                color: "#FFFFFF"
                Layout.alignment: Qt.AlignHCenter
                font.letterSpacing: 2
            }

            TextField {
                id: loginField
                placeholderText: "логин"
                Layout.fillWidth: true
                Layout.preferredHeight: 46
                font.pixelSize: 16
                color: "#FFFFFF"
                leftPadding: 40
                
                background: Rectangle {
                    color: "transparent"
                    border.color: loginField.activeFocus ? "#FF416C" : "#555555"
                    border.width: 1
                    radius: 8
                    
                    Text {
                        text: "👤"
                        anchors.left: parent.left
                        anchors.leftMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        font.pixelSize: 16
                        color: "#888888"
                    }
                }
            }

            TextField {
                id: passwordField
                placeholderText: "пароль"
                echoMode: TextInput.Password
                Layout.fillWidth: true
                Layout.preferredHeight: 46
                font.pixelSize: 16
                color: "#FFFFFF"
                leftPadding: 40
                rightPadding: 40
                
                background: Rectangle {
                    color: "transparent"
                    border.color: passwordField.activeFocus ? "#FF416C" : "#555555"
                    border.width: 1
                    radius: 8
                    
                    Text {
                        text: "🔒"
                        anchors.left: parent.left
                        anchors.leftMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        font.pixelSize: 16
                        color: "#888888"
                    }

                    Text {
                        text: "👁"
                        anchors.right: parent.right
                        anchors.rightMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        font.pixelSize: 18
                        color: "#888888"
                        
                        MouseArea {
                            anchors.fill: parent
                            onClicked: passwordField.echoMode = passwordField.echoMode === TextInput.Password ? TextInput.Normal : TextInput.Password
                        }
                    }
                }
                
                onAccepted: loginButton.clicked()
            }

            Label {
                text: typeof AppController !== "undefined" && AppController ? AppController.statusAuthText : ""
                color: "#FF416C"
                font.pixelSize: 13
                Layout.alignment: Qt.AlignHCenter
                visible: text !== ""
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 20

                CheckBox {
                    id: autoLoginCheck
                    text: "Войти при запуске"
                    checked: true
                    Layout.fillWidth: true
                    contentItem: Text {
                        text: autoLoginCheck.text
                        font: autoLoginCheck.font
                        color: "#FFFFFF"
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: autoLoginCheck.indicator.width + autoLoginCheck.spacing
                    }
                    indicator: Rectangle {
                        implicitWidth: 20
                        implicitHeight: 20
                        x: autoLoginCheck.leftPadding
                        y: parent.height / 2 - height / 2
                        radius: 4
                        color: autoLoginCheck.checked ? "#FF416C" : "transparent"
                        border.color: autoLoginCheck.checked ? "#FF416C" : "#888888"
                        
                        Text {
                            anchors.centerIn: parent
                            text: "✓"
                            color: "white"
                            font.bold: true
                            visible: autoLoginCheck.checked
                        }
                    }
                }

                Button {
                    id: loginButton
                    Layout.preferredWidth: 140
                    Layout.preferredHeight: 46
                    enabled: typeof AppController !== "undefined" && AppController ? !AppController.isNetworkPending : true
                    
                    contentItem: Item {
                        anchors.fill: parent
                        Label {
                            anchors.centerIn: parent
                            text: "ВОЙТИ"
                            color: "white"
                            font.bold: true
                            font.letterSpacing: 1
                            visible: typeof AppController !== "undefined" && AppController ? !AppController.isNetworkPending : true
                        }
                        BusyIndicator {
                            anchors.centerIn: parent
                            width: 24
                            height: 24
                            running: typeof AppController !== "undefined" && AppController ? AppController.isNetworkPending : false
                            visible: running
                            Material.accent: "white"
                        }
                    }
                    
                    background: Rectangle {
                        radius: 23
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: loginButton.down ? "#d81b60" : "#FF416C" }
                            GradientStop { position: 1.0; color: loginButton.down ? "#c2185b" : "#FF4B2B" }
                        }
                        
                        Rectangle {
                            anchors.fill: parent
                            radius: parent.radius
                            color: "transparent"
                            border.color: "#FF416C"
                            border.width: 1
                            opacity: 0.5
                        }
                    }
                    
                    onClicked: {
                        if (loginField.text.trim() === "" || passwordField.text.trim() === "") {
                            if (typeof AppController !== "undefined" && AppController) {
                                AppController.statusAuthText = "Введите логин и пароль";
                            }
                            return;
                        }
                        if (typeof AppController !== "undefined" && AppController) {
                            AppController.login(loginField.text, passwordField.text);
                        }
                    }
                }
            }

            Item { Layout.fillHeight: true } // Spacer
        }
    }
}
