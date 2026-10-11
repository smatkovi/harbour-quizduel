import QtQuick 2.0
import Sailfish.Silica 1.0
import "../Strings.js" as S

// Ein Duell. Zug-Infos (welche Runde ich spielen darf, ob Kategorie schon gewählt) kommen
// FLACH aus dem NetClient (room.turnRound / room.selCatId, in C++ vorberechnet).
// turnRound: 0..5 spielbar, -1 = Gegner am Zug, -2 = Spiel beendet.
Page {
    id: page
    property var net
    property var room
    property int roundIdx: room && (room.turnRound !== undefined) ? room.turnRound : -2
    property int selCatId: room && (room.selCatId !== undefined) ? room.selCatId : -1
    property bool finished: roundIdx === -2 || (room && room.roomStatus === "finished")
    property bool waiting: !finished && roundIdx === -1

    property var dictState: (room && room.gameState && room.gameState.json)
                            ? room.gameState.json.dictionaryState : null
    property var roundObj: (dictState && dictState.rounds && roundIdx >= 0 && roundIdx < dictState.rounds.length)
                           ? dictState.rounds[roundIdx] : null
    property var playCats: {
        if (!roundObj) return []
        if (selCatId < 0) return roundObj.categories
        for (var i = 0; i < roundObj.categories.length; i++)
            if (("" + roundObj.categories[i].id) === ("" + selCatId))
                return [roundObj.categories[i]]
        return []
    }

    property string note: ""   // kurze Rückmeldung (Freund hinzugefügt)

    Connections {
        target: net
        onRoomUpdated: page.room = room
        onRoomsUpdated: {
            var id = room ? ("" + room.id) : ""
            for (var i = 0; i < rooms.length; i++)
                if (("" + rooms[i].id) === id) { page.room = rooms[i]; break }
        }
        onFriendAdded: page.note = S.t(net.locale, "friendAdded")
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: col.height + Theme.paddingLarge
        VerticalScrollDecorator {}

        PullDownMenu {
            MenuItem {
                visible: room && room.oppUserId > 0
                text: S.t(net.locale, "befriend") + (room && room.opponent ? (": " + room.opponent) : "")
                onClicked: if (room) net.addFriend(room.oppUserId)
            }
            MenuItem { text: "↻"; onClicked: net.refreshGames([]) }
        }

        Column {
            id: col
            width: page.width
            spacing: Theme.paddingLarge

            PageHeader { title: S.t(net.locale, "round") + " " + (page.roundIdx >= 0 ? (page.roundIdx + 1) : "–") }

            // Gegner-Profilbild (avatarUrl vom Server; leer -> Initiale).
            Avatar {
                anchors.horizontalCenter: parent.horizontalCenter
                size: Theme.itemSizeLarge
                name: room ? ("" + room.opponent) : ""
                url: room ? ("" + (room.oppAvatar ? room.oppAvatar : "")) : ""
            }

            Label {
                visible: page.note.length > 0
                x: Theme.horizontalPageMargin; width: parent.width - 2 * Theme.horizontalPageMargin
                horizontalAlignment: Text.AlignHCenter
                color: Theme.highlightColor; font.pixelSize: Theme.fontSizeSmall
                text: page.note
            }

            // Punktestand: Du X : Y Gegner
            Label {
                x: Theme.horizontalPageMargin; width: parent.width - 2 * Theme.horizontalPageMargin
                horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap
                color: Theme.highlightColor; font.pixelSize: Theme.fontSizeLarge
                text: room ? (S.t(net.locale, "you") + " " + room.myScore + " : "
                              + room.oppScore + " " + room.opponent) : ""
            }
            Label {
                visible: page.finished && room && ("" + room.outcome).length > 0
                x: Theme.horizontalPageMargin; width: parent.width - 2 * Theme.horizontalPageMargin
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: Theme.fontSizeExtraLarge
                color: (room && room.outcome === "won") ? Theme.highlightColor : Theme.secondaryHighlightColor
                text: room ? S.t(net.locale, room.outcome) : ""
            }

            Label {
                visible: page.finished
                x: Theme.horizontalPageMargin; width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap; color: Theme.highlightColor; font.pixelSize: Theme.fontSizeLarge
                text: S.t(net.locale, "finished")
            }
            // Nochmal spielen (Rematch gegen denselben Gegner) — die HomePage öffnet das neue Duell.
            Button {
                visible: page.finished && room && room.oppUserId > 0
                anchors.horizontalCenter: parent.horizontalCenter
                text: S.t(net.locale, "rematch")
                onClicked: { net.challengeUser(room.oppUserId); pageStack.pop() }
            }
            Label {
                visible: page.waiting
                x: Theme.horizontalPageMargin; width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap; color: Theme.secondaryColor; font.pixelSize: Theme.fontSizeLarge
                text: S.t(net.locale, "waiting")
            }
            Label {
                visible: !page.finished && !page.waiting && !!page.roundObj
                x: Theme.horizontalPageMargin; width: parent.width - 2 * Theme.horizontalPageMargin
                color: Theme.secondaryHighlightColor
                text: S.t(net.locale, page.selCatId < 0 ? "chooseCategory" : "yourTurn")
            }
            Label {
                visible: !page.finished && !page.waiting && page.roundIdx >= 0 && !page.roundObj
                x: Theme.horizontalPageMargin; width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap; color: Theme.secondaryColor
                text: S.t(net.locale, "pleaseWait")
            }

            Repeater {
                model: page.playCats
                BackgroundItem {
                    width: page.width
                    height: Theme.itemSizeLarge
                    onClicked: pageStack.push(Qt.resolvedUrl("RoundPage.qml"),
                        { net: net, roomId: ("" + page.room.id), roundIndex: page.roundIdx,
                          category: modelData, room: page.room, selectCategory: (page.selCatId < 0) })
                    Rectangle {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.horizontalPageMargin
                        anchors.rightMargin: Theme.horizontalPageMargin
                        anchors.topMargin: Theme.paddingSmall
                        anchors.bottomMargin: Theme.paddingSmall
                        radius: Theme.paddingSmall
                        color: (modelData.color && ("" + modelData.color).length > 0)
                               ? modelData.color : Theme.rgba(Theme.highlightColor, 0.25)
                        Label {
                            anchors.centerIn: parent
                            color: "white"; font.pixelSize: Theme.fontSizeLarge
                            text: modelData.name ? modelData.name : ("#" + modelData.id)
                        }
                    }
                }
            }
        }
    }
}
