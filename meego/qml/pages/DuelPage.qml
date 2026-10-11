import QtQuick 1.1
import com.nokia.meego 1.0
import "../Strings.js" as S

// Ein Duell. Der volle (angereicherte) Raum kommt als JSON-String (roomJson) herein; die
// flachen Zug-Felder (turnRound/selCatId/opponent/scores) hat der Holer vorberechnet.
Page {
    id: page
    orientationLock: PageOrientation.LockPortrait
    property string roomJson: ""
    property variant room: 0
    property int roundIdx: -2
    property int selCatId: -1
    property string note: ""
    property string errRaw: ""

    function setRoom(payload) {
        var d
        try { d = JSON.parse(payload) } catch (e) { return }
        var r = (d && d.payload) ? (d.payload.room ? d.payload.room : d.payload.updatedRooms) : 0
        if (!r) return
        page.room = r
        page.roundIdx = (r.turnRound !== undefined) ? r.turnRound : -2
        page.selCatId = (r.selCatId !== undefined) ? r.selCatId : -1
    }
    Component.onCompleted: setRoom(roomJson)

    Connections {
        target: net
        onRoomUpdated: page.setRoom(payload)   // nach einem Zug: voller aktualisierter Raum
        onFriendAdded: page.note = S.t(net.locale, "friendAdded")
        onFailed: page.errRaw = message
    }

    property bool finished: roundIdx === -2 || (room && room.roomStatus === "finished")
    property bool waiting: !finished && roundIdx === -1

    function playCats() {
        if (!room || !room.gameState || !room.gameState.json) return []
        var ds = room.gameState.json.dictionaryState
        if (!ds || !ds.rounds || roundIdx < 0 || roundIdx >= ds.rounds.length) return []
        var cats = ds.rounds[roundIdx].categories || []
        if (selCatId < 0) return cats
        for (var i = 0; i < cats.length; i++)
            if (("" + cats[i].id) === ("" + selCatId)) return [cats[i]]
        return []
    }
    property variant cats: playCats()

    tools: ToolBarLayout {
        ToolIcon { platformIconId: "toolbar-back"; onClicked: pageStack.pop() }
        ToolIcon {
            platformIconId: "toolbar-add"
            onClicked: if (page.room && page.room.oppUserId > 0) net.addFriend(page.room.oppUserId)
        }
        ToolIcon { platformIconId: "toolbar-refresh"; onClicked: net.refreshGames() }
    }

    Flickable {
        anchors.fill: parent
        contentHeight: col.height + 40
        Column {
            id: col
            x: 16
            width: page.width - 32
            spacing: 16

            Item { width: 1; height: 8 }
            Label {
                text: S.t(net.locale, "round") + " " + (page.roundIdx >= 0 ? (page.roundIdx + 1) : "–")
                font.pixelSize: 32; font.bold: true; color: "white"
            }

            Avatar { anchors.horizontalCenter: parent.horizontalCenter; size: 96
                     name: page.room ? ("" + page.room.opponent) : "" }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: page.room ? (S.t(net.locale, "you") + " " + page.room.myScore + " : "
                                   + page.room.oppScore + " " + page.room.opponent) : ""
                font.pixelSize: 28; color: "#7eb3ff"
            }
            Label {
                visible: page.finished && page.room && ("" + page.room.outcome).length > 0
                anchors.horizontalCenter: parent.horizontalCenter
                text: page.room ? S.t(net.locale, "" + page.room.outcome) : ""
                font.pixelSize: 34; font.bold: true
                color: (page.room && page.room.outcome === "won") ? "#7eb3ff" : "#c0c0c0"
            }
            Label { visible: page.note.length > 0; anchors.horizontalCenter: parent.horizontalCenter
                    text: page.note; color: "#7eb3ff"; font.pixelSize: 18 }

            Label { visible: page.waiting; text: S.t(net.locale, "waiting"); color: "#c0c0c0"; font.pixelSize: 24 }
            Label {
                visible: !page.finished && !page.waiting && page.cats.length > 0
                text: S.t(net.locale, page.selCatId < 0 ? "chooseCategory" : "yourTurn")
                color: "#d0d0d0"; font.pixelSize: 24
            }

            Repeater {
                model: (page.finished || page.waiting) ? [] : page.cats
                Rectangle {
                    width: col.width
                    height: 78
                    radius: 8
                    color: (modelData.color && ("" + modelData.color).length > 0) ? modelData.color : "#35507a"
                    Label {
                        anchors.centerIn: parent
                        text: modelData.name ? modelData.name : ("#" + modelData.id)
                        color: "white"; font.pixelSize: 26
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: pageStack.push(Qt.resolvedUrl("RoundPage.qml"),
                            { room: page.room, roomId: ("" + page.room.id), roundIndex: page.roundIdx,
                              category: modelData, selectCategory: (page.selCatId < 0),
                              participantId: page.room.myParticipantId })
                    }
                }
            }

            Button {
                visible: page.finished && page.room && page.room.oppUserId > 0
                width: parent.width
                text: S.t(net.locale, "rematch")
                onClicked: { net.challengeUser(page.room.oppUserId); pageStack.pop() }
            }

            Label {
                visible: page.errRaw.length > 0
                width: parent.width; wrapMode: Text.Wrap
                color: "#ff6b6b"; font.pixelSize: 20
                text: S.errText(net.locale, page.errRaw)
            }
        }
    }
}
