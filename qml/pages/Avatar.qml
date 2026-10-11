import QtQuick 2.0
import Sailfish.Silica 1.0
import QtGraphicalEffects 1.0

// Rundes Profilbild. url kommt aus der vom Server gerenderten avatarUrl (Raum-Teilnehmer);
// ist sie leer (Standard-Avatar) oder lädt noch, zeigt der Platzhalter die Initiale.
// Freunde/Suche liefern nur avatarCode (keine URL) -> dort bleibt es bei der Initiale.
Item {
    id: av
    property string url: ""
    property string name: ""
    property int size: Theme.itemSizeSmall
    width: size
    height: size

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        visible: img.status !== Image.Ready
        color: Theme.rgba(Theme.highlightColor, 0.3)
        Label {
            anchors.centerIn: parent
            text: av.name.length ? av.name.charAt(0).toUpperCase() : "?"
            color: Theme.highlightColor
            font.pixelSize: parent.width * 0.45
        }
    }
    Image {
        id: img
        anchors.fill: parent
        source: av.url
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        visible: false
    }
    Rectangle { id: mask; anchors.fill: parent; radius: width / 2; visible: false }
    OpacityMask {
        anchors.fill: parent
        source: img
        maskSource: mask
        visible: img.status === Image.Ready
    }
}
