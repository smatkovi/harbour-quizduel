import QtQuick 2.0
import Sailfish.Silica 1.0
import "../Strings.js" as S

// Aktive Duelle (multiplayer/refresh) + neues Spiel (multiplayer/startMatchmaking).
// Ersetzt das alte boards-Modell (recon/spiel-protokoll-v2.md): rooms + matchmaker.
Page {
    id: page
    property var net
    property var rooms: []
    property bool busy: false

    function loadRooms() { if (net) { busy = true; net.refreshGames([]) } }   // leerer Cache -> alle Räume

    Component.onCompleted: loadRooms()

    Connections {
        target: net
        onRoomsUpdated: { page.rooms = rooms; page.busy = false }
        onMatchStarted: {
            page.busy = false
            if (match && match.room)   // frisches Spiel -> direkt ins Duell
                pageStack.push(Qt.resolvedUrl("DuelPage.qml"), { net: net, room: match.room })
        }
        onFailed: page.busy = false
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: col.height + Theme.paddingLarge
        VerticalScrollDecorator {}

        PullDownMenu {
            MenuItem { text: S.t(net.locale, "newGame"); onClicked: { page.busy = true; net.startMatchmaking() } }
            MenuItem { text: "↻"; onClicked: page.loadRooms() }
        }

        Column {
            id: col
            width: page.width
            spacing: Theme.paddingMedium

            PageHeader { title: S.t(net.locale, "activeGames") }

            BusyIndicator {
                running: page.busy
                size: BusyIndicatorSize.Medium
                anchors.horizontalCenter: parent.horizontalCenter
            }
            Label {
                visible: page.busy
                anchors.horizontalCenter: parent.horizontalCenter
                color: Theme.secondaryColor
                text: S.t(net.locale, "matchmaking")
            }

            Label {
                visible: !page.busy && page.rooms.length === 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.secondaryColor
                text: S.t(net.locale, "noActiveGames")
            }

            Repeater {
                model: page.rooms
                ListItem {
                    width: page.width
                    enabled: modelData.status === "OK"   // CACHE_OK-Stubs tragen keinen gameState
                    onClicked: pageStack.push(Qt.resolvedUrl("DuelPage.qml"), { net: net, room: modelData })
                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        x: Theme.horizontalPageMargin
                        width: parent.width - 2 * Theme.horizontalPageMargin
                        Label {
                            color: Theme.primaryColor
                            text: (modelData.roomStatus === "finished")
                                  ? S.t(net.locale, "finished")
                                  : S.t(net.locale, "yourTurn")
                        }
                        Label {
                            color: Theme.secondaryColor
                            font.pixelSize: Theme.fontSizeExtraSmall
                            text: ("" + (modelData.id || "")).substring(0, 8)
                        }
                    }
                }
            }
        }
    }
}
