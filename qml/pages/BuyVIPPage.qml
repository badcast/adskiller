import QtQuick
import QtQuick.Controls.Material
import QtQuick.Controls
import QtQuick.Layouts
import Adskiller 1.0

Item {
    id: root

    property var service: AppController.activeService

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 30

        Label {
            text: "Покупка VIP Статуса"
            font.pixelSize: 24
            font.bold: true
            color: AppController.textColor
            Layout.alignment: Qt.AlignHCenter
        }

        Rectangle {
            Layout.preferredWidth: 400
            Layout.preferredHeight: 300
            color: AppController.cardColor
            radius: 12
            border.color: "#333333"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 24
                spacing: 20

                Label {
                    text: service ? service.balanceText : "Ваш баланс: 0"
                    font.pixelSize: 18
                    color: "#4CAF50"
                    font.bold: true
                    Layout.alignment: Qt.AlignHCenter
                }

                ComboBox {
                    id: variantCombo
                    Layout.fillWidth: true
                    model: service ? service.variants : []
                    font.pixelSize: 16
                    onCurrentIndexChanged: {
                        if (service) {
                            service.selectVariant(currentIndex)
                        }
                    }
                }

                Label {
                    text: service ? service.infoText : "Выберите доступный вариант."
                    font.pixelSize: 14
                    color: AppController.textSecondary
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }

                Item { Layout.fillHeight: true }

                Button {
                    text: "ОПЛАТИТЬ"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 46
                    Material.background: Material.accent
                    Material.foreground: "white"
                    enabled: variantCombo.currentIndex > 0
                    onClicked: {
                        if (service) {
                            service.buyVip(variantCombo.currentIndex)
                        }
                    }
                }
            }
        }

        Button {
            text: "НАЗАД"
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 200
            Material.background: "#333333"
            Material.foreground: "white"
            onClicked: {
                AppController.closeService()
                if (stackView.depth > 1) stackView.pop()
            }
        }
    }
}
