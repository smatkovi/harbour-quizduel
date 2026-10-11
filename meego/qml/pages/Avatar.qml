import QtQuick 1.1

// Profilbild für MeeGo = Initiale im Kreis. Die vom Server gerenderten avatarUrl-Bilder
// liegen auf einem https-CDN; Harmattans Qt 4 / OpenSSL 0.9.8 kann TLS 1.2 nicht, also
// keine Fernbilder in QML. url bleibt nur der API-Gleichheit mit SFOS halber da.
Item {
    id: av
    property string url: ""
    property string name: ""
    property int size: 64
    width: size
    height: size
    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: "#2a4a7a"
        Text {
            anchors.centerIn: parent
            text: av.name.length ? av.name.charAt(0).toUpperCase() : "?"
            color: "#cfe0ff"
            font.pixelSize: parent.width * 0.45
        }
    }
}
