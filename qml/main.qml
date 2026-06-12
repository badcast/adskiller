import QtQuick
import QtQuick.Controls.Material
import QtQuick.Controls
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
    property bool explicitLogout: false
    
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
    }
}
