import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Adskiller 1.0

ApplicationWindow {
    id: window
    width: 900
    height: 650
    minimumWidth: 800
    minimumHeight: 515
    visible: true
    title: qsTr("Ads Mobile Killer")

    Material.theme: Material.Dark
    Material.accent: "#FF416C" // Vibrant Red/Pink
    Material.primary: "#0B192C" // Dark Blue
    Material.background: "#050B14" // Fallback dark

    // Custom properties for easy access
    property color cardColor: "#40000000" // Translucent dark glass
    property color textColor: "#FFFFFF"
    property color textSecondary: "#B0BEC5"
    property color cardBorderColor: "#33FFFFFF"

    Component.onCompleted: {
        AppController.startVersionCheck()
    }
    
    onClosing: {
        Qt.quit()
    }
    
    function showToast(message) {
        toastPopup.text = message;
        toastPopup.open();
    }
    
    Popup {
        id: toastPopup
        property string text: ""
        y: window.height - height - 40
        x: (window.width - width) / 2
        
        background: Rectangle {
            color: "#333333"
            radius: 8
            border.color: "#444444"
        }
        
        contentItem: Label {
            text: toastPopup.text
            color: "white"
            font.pixelSize: 14
            padding: 10
        }
        
        enter: Transition { NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 200 } }
        exit: Transition { NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 200 } }
        
        Timer {
            id: toastTimer
            interval: 3000
            running: toastPopup.opened
            onTriggered: toastPopup.close()
        }
    }
    

    // Dynamic Vibrant Background
    Rectangle {
        anchors.fill: parent
        color: "#0f172a" // Deep slate base
        
        // Abstract colorful accents
        Rectangle {
            width: parent.width * 0.8
            height: parent.width * 0.8
            radius: width / 2
            x: -width * 0.2
            y: -height * 0.2
            color: "#3b82f6" // Bright Blue
            opacity: 0.15
            rotation: 45
        }
        
        Rectangle {
            width: parent.width * 0.6
            height: parent.width * 0.6
            radius: width / 2
            x: parent.width * 0.5
            y: parent.height * 0.4
            color: "#ef4444" // Bright Red
            opacity: 0.15
        }

        gradient: Gradient {
            GradientStop { position: 0.0; color: "#000B192C" }
            GradientStop { position: 1.0; color: "#440B192C" }
        }
        z: -1
    }
    
    // Main Navigation Stack
    StackView {
        id: stackView
        anchors.fill: parent
        initialItem: "pages/AuthPage.qml"
        
        pushEnter: Transition {
            PropertyAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: 400
                easing.type: Easing.OutCubic
            }
            PropertyAnimation {
                property: "scale"
                from: 0.95
                to: 1
                duration: 400
                easing.type: Easing.OutBack
            }
        }
        pushExit: Transition {
            PropertyAnimation {
                property: "opacity"
                from: 1
                to: 0
                duration: 400
                easing.type: Easing.InCubic
            }
        }
        popEnter: Transition {
            PropertyAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: 400
            }
        }
        popExit: Transition {
            PropertyAnimation {
                property: "opacity"
                from: 1
                to: 0
                duration: 400
            }
        }
    }

    Connections {
        target: AppController
        function onPageChangeRequested(pageIndex) {
            // Page indices from C++: AuthPage=0, CabinetPage=1
            if (pageIndex === 0) {
                stackView.replace("pages/AuthPage.qml")
            } else if (pageIndex === 1) {
                stackView.replace("pages/CabinetPage.qml")
            }
        }
        function onOpenServicePage(pageName) {
            stackView.push("pages/" + pageName + ".qml")
        }
        function onUpdateAvailable(version, url) {
            updateAvailableDialog.newVersion = version
            updateAvailableDialog.downloadUrl = url
            updateAvailableDialog.countdown = 5
            updateAvailableDialog.open()
        }
        function onNetworkWarning(attemptsLeft) {
            networkWarningDialog.attempts = attemptsLeft
            networkWarningDialog.open()
        }
        function onForceCloseApp() {
            Qt.quit()
        }
    }

    // Update check dialog (spinner + status text)
    Popup {
        id: updateCheckPopup
        anchors.centerIn: parent
        modal: true
        closePolicy: Popup.NoAutoClose
        padding: 0

        Connections {
            target: AppController
            function onUpdateCheckActiveChanged() {
                if (AppController.updateCheckActive)
                    updateCheckPopup.open()
                else
                    updateCheckPopup.close()
            }
        }

        enter: Transition { NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 200 } }
        exit: Transition { NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 200 } }

        background: Rectangle {
            color: "#1a1a2e"
            radius: 16
            border.color: "#33FFFFFF"
            border.width: 1
        }

        contentItem: ColumnLayout {
            spacing: 20
            implicitWidth: 300

            Item { Layout.preferredHeight: 10 }

            BusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                running: updateCheckPopup.opened
                Material.accent: "#FF416C"
                implicitWidth: 56
                implicitHeight: 56
            }

            Label {
                text: AppController.updateCheckStatus
                Layout.alignment: Qt.AlignHCenter
                Layout.leftMargin: 30
                Layout.rightMargin: 30
                Layout.fillWidth: true
                font.pixelSize: 16
                font.weight: Font.Medium
                color: "#FFFFFF"
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Item { Layout.preferredHeight: 10 }
        }
    }

    // Update available dialog
    Dialog {
        id: updateAvailableDialog
        anchors.centerIn: parent
        modal: true
        closePolicy: Popup.NoAutoClose
        title: "Обнаружена новая версия"

        property string newVersion: ""
        property string downloadUrl: ""
        property int countdown: 5

        Material.background: "#1a1a2e"

        background: Rectangle {
            color: "#1a1a2e"
            radius: 16
            border.color: "#33FFFFFF"
            border.width: 1
        }

        Timer {
            id: updateTimer
            interval: 1000
            repeat: true
            running: updateAvailableDialog.opened
            onTriggered: {
                if (updateAvailableDialog.countdown > 0) {
                    updateAvailableDialog.countdown--
                }
                if (updateAvailableDialog.countdown === 0) {
                    updateTimer.stop()
                    updateAvailableDialog.accept()
                }
            }
        }

        contentItem: ColumnLayout {
            spacing: 16
            implicitWidth: 400

            Label {
                text: {
                    var msg = "Обнаружена новая версия программного обеспечения.\n\n"
                    msg += "Версия на сервере: v" + updateAvailableDialog.newVersion + "\n\n"
                    msg += Qt.platform.os === "windows"
                        ? "После нажатия кнопки \"Обновить\" будет запущено обновление ПО."
                        : "После нажатия кнопки \"Обновить\" откроется ссылка в вашем браузере.\nПожалуйста, скачайте обновление по прямой ссылке."
                    msg += "\n\nС уважением ваша команда Adskiller Team."
                    return msg
                }
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                font.pixelSize: 14
                color: "#FFFFFF"
                wrapMode: Text.WordWrap
            }

            Label {
                text: "Приложение будет обновлено автоматически через " + updateAvailableDialog.countdown + " сек..."
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                font.pixelSize: 14
                font.bold: true
                color: "#FF416C"
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
        }

        footer: DialogButtonBox {
            Button {
                text: "Обновить"
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                Material.background: "#FF416C"
                Material.foreground: "#FFFFFF"
            }
        }

        onAccepted: {
            updateTimer.stop()
            AppController.startUpdate(downloadUrl)
        }
    }

    // Network Warning Dialog
    Dialog {
        id: networkWarningDialog
        anchors.centerIn: parent
        modal: true
        title: "Отсутствует соединение с интернетом"

        property int attempts: 3

        Material.background: "#1a1a2e"

        background: Rectangle {
            color: "#1a1a2e"
            radius: 16
            border.color: "#33FFFFFF"
            border.width: 1
        }

        contentItem: Label {
            text: "У вас осталось попыток (" + networkWarningDialog.attempts + "), срочно восстановите связь, иначе приложение аварийно завершится."
            font.pixelSize: 14
            color: "#FFFFFF"
            wrapMode: Text.WordWrap
            width: 300
        }

        footer: DialogButtonBox {
            Button {
                text: "ОК"
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                Material.background: "#FF416C"
                Material.foreground: "#FFFFFF"
            }
        }
    }
}
