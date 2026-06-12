import QtQuick
import QtQuick.Controls.Material
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Adskiller 1.0
import "../components"
import QtCore

Item {
    id: root

    property var service: AppController.activeService
    property var currentFiles: []
    
    function refresh() {
        if (service) {
            currentFiles = service.listFiles(service.currentPath);
        }
    }

    Component.onCompleted: {
        refresh();
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16

        // Header / Path bar
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Button {
                text: "НАЗАД"
                Layout.preferredHeight: 46
                background: Rectangle {
                    color: "transparent"
                    border.color: "#555555"
                    border.width: 2
                    radius: 8
                }
                contentItem: Label {
                    text: parent.text
                    color: "white"
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    AppController.closeService()
                    if (stackView.depth > 1) stackView.pop()
                }
            }

            Button {
                text: "ВВЕРХ ⬆"
                Layout.preferredHeight: 46
                background: Rectangle {
                    color: "#33FFFFFF"
                    radius: 8
                }
                contentItem: Label {
                    text: parent.text
                    color: "white"
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    if (service && service.currentPath !== "/" && service.currentPath !== "") {
                        var p = service.currentPath;
                        if (p.endsWith("/")) p = p.substring(0, p.length - 1);
                        var idx = p.lastIndexOf("/");
                        if (idx >= 0) {
                            service.currentPath = p.substring(0, idx + 1);
                            refresh();
                        }
                    }
                }
            }

            TextField {
                Layout.fillWidth: true
                Layout.preferredHeight: 46
                text: service ? service.currentPath : ""
                color: "white"
                font.pixelSize: 16
                background: Rectangle {
                    color: "#11FFFFFF"
                    radius: 8
                    border.color: "#33FFFFFF"
                    border.width: 1
                }
                onAccepted: {
                    if (service) {
                        var p = text;
                        if (!p.endsWith("/")) p += "/";
                        service.currentPath = p;
                        refresh();
                    }
                }
            }
            
            Button {
                text: "ОБНОВИТЬ"
                Layout.preferredHeight: 46
                background: Rectangle {
                    color: "#00F2FE"
                    radius: 8
                }
                contentItem: Label {
                    text: parent.text
                    color: "black"
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: refresh()
            }
        }

        // List View for Files
        GlassCard {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: fileList
                anchors.fill: parent
                anchors.margins: 12
                model: currentFiles
                clip: true
                spacing: 4

                delegate: Rectangle {
                    width: fileList.width
                    height: 50
                    color: hoverArea.containsMouse ? "#22FFFFFF" : "transparent"
                    radius: 8
                    
                    MouseArea {
                        id: hoverArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onDoubleClicked: {
                            if (modelData.isDir) {
                                var p = service.currentPath;
                                if (!p.endsWith("/")) p += "/";
                                service.currentPath = p + modelData.name + "/";
                                refresh();
                            }
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 12

                        Text {
                            text: modelData.isDir ? "📁" : "📄"
                            font.pixelSize: 24
                        }

                        Label {
                            text: modelData.name
                            color: "white"
                            font.pixelSize: 16
                            font.bold: modelData.isDir
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Label {
                            text: modelData.size >= 0 ? (modelData.size / 1024).toFixed(1) + " KB" : ""
                            color: "#AAAAAA"
                            font.pixelSize: 14
                            Layout.preferredWidth: 80
                            horizontalAlignment: Text.AlignRight
                        }

                        Label {
                            text: modelData.date
                            color: "#AAAAAA"
                            font.pixelSize: 14
                            Layout.preferredWidth: 120
                            horizontalAlignment: Text.AlignRight
                        }
                        
                        Item {
                            Layout.fillWidth: true
                        }
                        
                        Button {
                            text: "Скачать"
                            visible: !modelData.isDir
                            background: Rectangle { color: "#3300FF00"; radius: 4 }
                            contentItem: Label { text: parent.text; color: "#00FF00"; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            onClicked: {
                                pullDialog.sourceRemotePath = service.currentPath + modelData.name;
                                pullDialog.open();
                            }
                        }
                        
                        Button {
                            text: "Удалить"
                            background: Rectangle { color: "#33FF0000"; radius: 4 }
                            contentItem: Label { text: parent.text; color: "#FF5555"; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            onClicked: {
                                let fullPath = service.currentPath + modelData.name;
                                if(service.deleteFile(fullPath)) {
                                    console.log("Deleted", fullPath);
                                    refresh();
                                } else {
                                    console.log("Failed to delete", fullPath);
                                }
                            }
                        }
                    }
                }
            }
            
            Label {
                anchors.centerIn: parent
                visible: currentFiles.length === 0
                text: "Папка пуста"
                color: "#555555"
                font.pixelSize: 18
            }
        }
        
        RowLayout {
            Layout.fillWidth: true
            spacing: 16
            
            Button {
                text: "ЗАГРУЗИТЬ ФАЙЛ СЮДА (PUSH)"
                Layout.preferredHeight: 46
                Layout.fillWidth: true
                background: Rectangle { color: "#33FFFFFF"; radius: 8 }
                contentItem: Label { text: parent.text; color: "white"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                onClicked: {
                    pushDialog.open();
                }
            }
        }
    }
    
    // Dialogs for file operations
    FileDialog {
        id: pushDialog
        title: "Выберите файл для загрузки на устройство"
        fileMode: FileDialog.OpenFile
        onAccepted: {
            // Qt.resolvedUrl might have file:// prefix, we need to strip it for local path
            let localPath = currentFile.toString().replace("file://", "");
            let remotePath = service.currentPath + localPath.substring(localPath.lastIndexOf("/") + 1);
            if (service.pushFile(localPath, remotePath)) {
                console.log("File pushed successfully");
                refresh(); // Refresh
            } else {
                console.error("Failed to push file");
            }
        }
    }
    
    FolderDialog {
        id: pullDialog
        title: "Выберите папку для сохранения файла"
        property string sourceRemotePath: ""
        onAccepted: {
            let localDir = currentFolder.toString().replace("file://", "");
            let fileName = sourceRemotePath.substring(sourceRemotePath.lastIndexOf("/") + 1);
            let localPath = localDir + "/" + fileName;
            
            if (service.pullFile(sourceRemotePath, localPath)) {
                console.log("File pulled successfully to " + localPath);
            } else {
                console.error("Failed to pull file");
            }
        }
    }
}
