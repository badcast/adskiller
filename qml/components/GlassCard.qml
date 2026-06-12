import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    color: Qt.rgba(20/255, 25/255, 40/255, 0.4) // Semi-transparent dark blue/slate
    radius: 16
    border.color: "#33FFFFFF"
    border.width: 1

    property alias content: container.children

    Item {
        id: container
        anchors.fill: parent
        anchors.margins: 16
    }
}
