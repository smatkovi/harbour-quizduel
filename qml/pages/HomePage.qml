import QtQuick 2.0
import Sailfish.Silica 1.0
import "../Strings.js" as S

// Startseite = Duell-Übersicht. Status (▶ Dein Zug / ⋯ Wartet) + Gegner kommen als FLACHE
// Felder aus dem NetClient (room.turnStatus / room.opponent, in C++ vorberechnet) — QML
// kaut keine großen Raum-Maps mehr durch (das sprengte vorher den V4-Stack).
Page {
    id: page
    property var net
    property string account: ""
    property var activeItems: []     // [{room, opp, st:"myturn"|"waiting"}]
    property var finishedItems: []   // [{room, opp}]
    property bool busy: false
    property string lastErrorRaw: ""   // rohes Fehler-JSON (zum Antippen/Kopieren)
    property string note: ""           // kurze Rückmeldung (Freund hinzugefügt)

    function rebuild(rooms) {
        var act = [], fin = []
        for (var i = 0; i < rooms.length; i++) {
            var r = rooms[i]
            var sc = r.myScore + ":" + r.oppScore
            if (r.turnStatus === "finished") { fin.push({ room: r, opp: r.opponent, score: sc, outcome: r.outcome, avatar: r.oppAvatar, oppId: r.oppUserId }); continue }
            if (r.status !== "OK") continue
            act.push({ room: r, opp: r.opponent, st: r.turnStatus, score: sc, avatar: r.oppAvatar, oppId: r.oppUserId })
        }
        act.sort(function (a, b) { return (a.st === "myturn" ? 0 : 1) - (b.st === "myturn" ? 0 : 1) })
        page.activeItems = act
        page.finishedItems = fin
    }
    function reload() { if (net) { busy = true; net.refreshGames([]) } }

    Component.onCompleted: reload()

    Connections {
        target: net
        onRoomsUpdated: { page.busy = false; page.rebuild(rooms) }
        onMatchStarted: {
            page.busy = false
            if (match && match.room)
                pageStack.push(Qt.resolvedUrl("DuelPage.qml"), { net: net, room: match.room })
        }
        onRoomUpdated: page.reload()
        onFriendAdded: page.note = S.t(net.locale, "friendAdded")
        onFailed: { page.busy = false; page.lastErrorRaw = message; banner.text = S.errText(net.locale, message) }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge
        VerticalScrollDecorator {}

        PullDownMenu {
            MenuItem { text: S.t(net.locale, "logout"); onClicked: pageStack.animatorReplace(Qt.resolvedUrl("LoginPage.qml"), { net: net }) }
            MenuItem { text: S.t(net.locale, "friends"); onClicked: pageStack.push(Qt.resolvedUrl("FriendsPage.qml"), { net: net }) }
            MenuItem { text: S.t(net.locale, "messages"); onClicked: pageStack.push(Qt.resolvedUrl("MessagesPage.qml"), { net: net }) }
            MenuItem { text: "↻"; onClicked: page.reload() }
            MenuItem { text: S.t(net.locale, "newGame"); onClicked: { page.busy = true; net.startMatchmaking() } }
        }

        Column {
            id: column
            width: page.width
            spacing: Theme.paddingMedium

            PageHeader { title: account.length ? account : "QuizDuel" }

            BusyIndicator { running: page.busy; size: BusyIndicatorSize.Medium; anchors.horizontalCenter: parent.horizontalCenter }
            Label {
                visible: page.busy
                anchors.horizontalCenter: parent.horizontalCenter
                color: Theme.secondaryColor
                text: S.t(net.locale, "matchmaking")
            }

            Label {
                visible: page.note.length > 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                horizontalAlignment: Text.AlignHCenter
                color: Theme.highlightColor
                text: page.note
            }

            SectionHeader { text: S.t(net.locale, "activeGames") }
            Label {
                visible: !page.busy && page.activeItems.length === 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap; color: Theme.secondaryColor
                text: S.t(net.locale, "noActiveGames")
            }
            Repeater {
                model: page.activeItems
                ListItem {
                    width: page.width
                    contentHeight: Theme.itemSizeMedium
                    onClicked: pageStack.push(Qt.resolvedUrl("DuelPage.qml"), { net: net, room: modelData.room })
                    menu: ContextMenu {
                        MenuItem {
                            text: S.t(net.locale, "befriend")
                            enabled: modelData.oppId > 0
                            onClicked: net.addFriend(modelData.oppId)
                        }
                    }
                    Row {
                        anchors.verticalCenter: parent.verticalCenter
                        x: Theme.horizontalPageMargin
                        width: parent.width - 2 * Theme.horizontalPageMargin
                        spacing: Theme.paddingMedium
                        Avatar {
                            size: Theme.itemSizeSmall
                            name: "" + modelData.opp
                            url: "" + (modelData.avatar ? modelData.avatar : "")
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - Theme.itemSizeSmall - Theme.paddingMedium
                            Label { width: parent.width; truncationMode: TruncationMode.Fade; color: Theme.primaryColor; text: modelData.opp }
                            Label {
                                font.pixelSize: Theme.fontSizeExtraSmall
                                color: modelData.st === "myturn" ? Theme.highlightColor : Theme.secondaryColor
                                text: (modelData.st === "myturn" ? ("▶ " + S.t(net.locale, "yourTurn"))
                                                                 : ("⋯ " + S.t(net.locale, "waiting")))
                                      + "    " + modelData.score
                            }
                        }
                    }
                }
            }

            SectionHeader { text: S.t(net.locale, "finished"); visible: page.finishedItems.length > 0 }
            Repeater {
                model: page.finishedItems
                ListItem {
                    width: page.width
                    contentHeight: Theme.itemSizeMedium
                    onClicked: pageStack.push(Qt.resolvedUrl("DuelPage.qml"), { net: net, room: modelData.room })
                    menu: ContextMenu {
                        MenuItem {
                            text: S.t(net.locale, "rematch")
                            enabled: modelData.oppId > 0
                            onClicked: { page.busy = true; net.challengeUser(modelData.oppId) }
                        }
                        MenuItem {
                            text: S.t(net.locale, "befriend")
                            enabled: modelData.oppId > 0
                            onClicked: net.addFriend(modelData.oppId)
                        }
                    }
                    Row {
                        anchors.verticalCenter: parent.verticalCenter
                        x: Theme.horizontalPageMargin
                        width: parent.width - 2 * Theme.horizontalPageMargin
                        spacing: Theme.paddingMedium
                        Avatar {
                            size: Theme.itemSizeSmall
                            name: "" + modelData.opp
                            url: "" + (modelData.avatar ? modelData.avatar : "")
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - Theme.itemSizeSmall - Theme.paddingMedium
                            Label { width: parent.width; truncationMode: TruncationMode.Fade; color: Theme.secondaryColor; text: modelData.opp }
                            Label {
                                font.pixelSize: Theme.fontSizeExtraSmall
                                color: modelData.outcome === "won" ? Theme.highlightColor : Theme.secondaryColor
                                text: (modelData.outcome && ("" + modelData.outcome).length > 0
                                       ? S.t(net.locale, modelData.outcome) : S.t(net.locale, "finished"))
                                      + "    " + modelData.score
                            }
                        }
                    }
                }
            }

            BackgroundItem {
                width: parent.width
                height: banner.height + Theme.paddingSmall
                visible: banner.text.length > 0
                onClicked: if (page.lastErrorRaw.length) Clipboard.text = page.lastErrorRaw
                Label {
                    id: banner
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    wrapMode: Text.Wrap; color: Theme.errorColor
                }
            }
            Label {
                visible: page.lastErrorRaw.length > 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                text: S.t(net.locale, "copyError")
            }
        }
    }
}
