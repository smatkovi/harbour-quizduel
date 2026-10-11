import QtQuick 2.0
import Sailfish.Silica 1.0

CoverBackground {
    Column {
        anchors.centerIn: parent
        width: parent.width - 2 * Theme.paddingLarge
        spacing: Theme.paddingSmall
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Quiz"
            font.pixelSize: Theme.fontSizeExtraLarge
            color: Theme.highlightColor
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Duel"
            font.pixelSize: Theme.fontSizeExtraLarge
            font.bold: true
            color: Theme.primaryColor
        }
    }
}
