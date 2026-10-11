import QtQuick 1.1
import com.nokia.meego 1.0
import "../Strings.js" as S

// Startseite = Duell-Übersicht. Die Raumliste kommt SCHLANK vom Holer (flache Zug-Felder,
// kein gameState) — QtQuick-1.1-JS darf keine großen Raum-Maps kauen. Tippen öffnet das
// Duell (openRoom liest den vollen Raum aus der Cache-Datei).
Page {
    id: page
    orientationLock: PageOrientation.LockPortrait
    property string account: ""
    property variant activeItems: []
    property variant finishedItems: []
    property bool busy: false
    property string note: ""
    property string errRaw: ""

    function rebuild(payload) {
        var d
        try { d = JSON.parse(payload) } catch (e) { return }
        var rooms = (d && d.payload && d.payload.rooms) ? d.payload.rooms : []
        var act = [], fin = []
        for (var i = 0; i < rooms.length; i++) {
            var r = rooms[i]
            var it = { id: "" + r.id, opp: "" + r.opponent, score: (r.myScore + ":" + r.oppScore),
                       st: "" + r.turnStatus, outcome: "" + (r.outcome ? r.outcome : "") }
            if (r.turnStatus === "finished") fin.push(it)
            else act.push(it)
        }
        act.sort(function (a, b) { return (a.st === "myturn" ? 0 : 1) - (b.st === "myturn" ? 0 : 1) })
        page.activeItems = act
        page.finishedItems = fin
    }
    function reload() { page.busy = true; page.errRaw = ""; net.refreshGames() }

    // Beim ersten Zeigen UND nach Rückkehr aus einem Duell die Liste auffrischen. Der
    // !busy-Riegel verhindert, dass Component.onCompleted + Active doppelt laden.
    Component.onCompleted: reload()
    onStatusChanged: if (status === PageStatus.Active && !page.busy) reload()

    Connections {
        target: net
        onRoomsUpdated: { page.busy = false; page.rebuild(payload) }
        onMatchStarted: { page.busy = false; pageStack.push(Qt.resolvedUrl("DuelPage.qml"), { roomJson: payload }) }
        onRoomOpened:   pageStack.push(Qt.resolvedUrl("DuelPage.qml"), { roomJson: payload })
        onFriendAdded:  page.note = S.t(net.locale, "friendAdded")
        onFailed:       { page.busy = false; page.errRaw = message }
    }

    tools: ToolBarLayout {
        ToolIcon { platformIconId: "toolbar-back"; onClicked: pageStack.pop() }
        ToolIcon { platformIconId: "toolbar-refresh"; onClicked: page.reload() }
        ToolIcon { platformIconId: "toolbar-add"; onClicked: { page.busy = true; net.startMatchmaking() } }
    }

    Flickable {
        anchors.fill: parent
        contentHeight: col.height + 40
        Column {
            id: col
            x: 16
            width: page.width - 32
            spacing: 14

            Item { width: 1; height: 8 }
            Label { text: page.account.length ? page.account : "QuizDuel"; font.pixelSize: 38; font.bold: true; color: "white" }

            Row {
                width: parent.width
                spacing: 10
                Button { width: (parent.width - 20) / 3; text: S.t(net.locale, "newGame"); onClicked: { page.busy = true; net.startMatchmaking() } }
                Button { width: (parent.width - 20) / 3; text: S.t(net.locale, "friends"); onClicked: pageStack.push(Qt.resolvedUrl("FriendsPage.qml")) }
                Button { width: (parent.width - 20) / 3; text: S.t(net.locale, "messages"); onClicked: pageStack.push(Qt.resolvedUrl("MessagesPage.qml")) }
            }

            BusyIndicator { anchors.horizontalCenter: parent.horizontalCenter; running: page.busy; visible: page.busy }
            Label { visible: page.busy; text: S.t(net.locale, "matchmaking"); color: "#a0a0a0"; font.pixelSize: 20 }
            Label { visible: page.note.length > 0; text: page.note; color: "#7eb3ff"; font.pixelSize: 20 }

            Label { text: S.t(net.locale, "activeGames"); font.pixelSize: 24; color: "#d0d0d0" }
            Label { visible: !page.busy && page.activeItems.length === 0; text: S.t(net.locale, "noActiveGames"); color: "#808080"; font.pixelSize: 20 }

            Repeater {
                model: page.activeItems
                Item {
                    width: col.width
                    height: 84
                    Row {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 12
                        Avatar { size: 56; name: modelData.opp; anchors.verticalCenter: parent.verticalCenter }
                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            Label { text: modelData.opp; color: "white"; font.pixelSize: 24 }
                            Label {
                                text: (modelData.st === "myturn" ? ("▶ " + S.t(net.locale, "yourTurn"))
                                                                 : ("… " + S.t(net.locale, "waiting"))) + "   " + modelData.score
                                color: modelData.st === "myturn" ? "#7eb3ff" : "#909090"
                                font.pixelSize: 18
                            }
                        }
                    }
                    MouseArea { anchors.fill: parent; onClicked: net.openRoom(modelData.id) }
                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#333333" }
                }
            }

            Label { visible: page.finishedItems.length > 0; text: S.t(net.locale, "finished"); font.pixelSize: 24; color: "#d0d0d0" }
            Repeater {
                model: page.finishedItems
                Item {
                    width: col.width
                    height: 84
                    Row {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 12
                        Avatar { size: 56; name: modelData.opp; anchors.verticalCenter: parent.verticalCenter }
                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            Label { text: modelData.opp; color: "#c0c0c0"; font.pixelSize: 24 }
                            Label {
                                text: (modelData.outcome.length > 0 ? S.t(net.locale, modelData.outcome) : S.t(net.locale, "finished"))
                                      + "   " + modelData.score
                                color: modelData.outcome === "won" ? "#7eb3ff" : "#909090"
                                font.pixelSize: 18
                            }
                        }
                    }
                    MouseArea { anchors.fill: parent; onClicked: net.openRoom(modelData.id) }
                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#333333" }
                }
            }

            Label {
                visible: page.errRaw.length > 0
                width: parent.width; wrapMode: Text.Wrap
                color: "#ff6b6b"; font.pixelSize: 20
                text: S.errText(net.locale, page.errRaw)
            }
            Label {
                visible: page.errRaw.length > 0
                width: parent.width; wrapMode: Text.Wrap
                color: "#707070"; font.pixelSize: 14
                text: page.errRaw
            }
        }
    }
}
