import QtQuick 2.0
import Sailfish.Silica 1.0
import "../Strings.js" as S

// Freunde: Liste (user/quizduel/getFriends), Suche nach exaktem Namen (user/quizduel/findUser),
// Hinzufügen (addFriend) und Herausfordern (createRoom). Die Navigation ins neue Duell macht
// die HomePage (sie hört ebenfalls auf matchStarted) — hier wird nur ausgelöst.
// Profilbilder: Freunde/Suche liefern nur avatarCode (keine URL) -> Initiale. Echte Bilder
// erscheinen bei den Gegnern in der Spieleliste (avatarUrl).
Page {
    id: page
    property var net
    property var friends: []
    property var found: null        // Suchergebnis (ein Benutzer) oder null
    property bool searched: false
    property bool busy: false
    property string info: ""        // kurze Rückmeldung (hinzugefügt / herausgefordert)

    function doSearch() {
        if (!net || query.text.trim().length === 0) return
        page.found = null; page.searched = false; page.info = ""
        page.busy = true
        net.findUser(query.text.trim())
    }

    Component.onCompleted: if (net) { busy = true; net.getFriends() }

    Connections {
        target: net
        onFriendsLoaded: { page.friends = friends; page.busy = false }
        onUserFound: { page.busy = false; page.found = (user && user.userId) ? user : null; page.searched = true }
        onFriendAdded: {
            page.info = S.t(net.locale, "friendAdded")
            if (net) net.getFriends()
        }
        onFailed: { page.busy = false }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: col.height + Theme.paddingLarge
        VerticalScrollDecorator {}

        Column {
            id: col
            width: page.width
            spacing: Theme.paddingMedium

            PageHeader { title: S.t(net.locale, "friends") }

            // --- Suche ---
            SearchField {
                id: query
                width: parent.width
                placeholderText: S.t(net.locale, "searchHint")
                label: S.t(net.locale, "searchUser")
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                EnterKey.iconSource: "image://theme/icon-m-enter-accept"
                EnterKey.onClicked: page.doSearch()
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                text: S.t(net.locale, "searchExact")
            }

            BusyIndicator { running: page.busy; size: BusyIndicatorSize.Medium
                            anchors.horizontalCenter: parent.horizontalCenter }

            // Suchergebnis
            Label {
                visible: page.searched && !page.found && !page.busy
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                color: Theme.secondaryColor
                text: S.t(net.locale, "userNotFound")
            }
            ListItem {
                visible: page.found !== null
                width: page.width
                contentHeight: Theme.itemSizeMedium
                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    spacing: Theme.paddingMedium
                    Avatar {
                        size: Theme.itemSizeSmall
                        name: page.found ? ("" + page.found.username) : ""
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - Theme.itemSizeSmall - addBtn.width - chalBtn.width - 3 * Theme.paddingMedium
                        truncationMode: TruncationMode.Fade
                        color: Theme.primaryColor
                        text: page.found ? (page.found.username
                              + (page.found.rating ? ("  (" + page.found.rating + ")") : "")) : ""
                    }
                    IconButton {
                        id: addBtn
                        anchors.verticalCenter: parent.verticalCenter
                        icon.source: "image://theme/icon-m-add"
                        onClicked: if (page.found) net.addFriend(page.found.userId)
                    }
                    IconButton {
                        id: chalBtn
                        anchors.verticalCenter: parent.verticalCenter
                        icon.source: "image://theme/icon-m-play"
                        onClicked: if (page.found) { page.busy = true; net.challengeUser(page.found.userId) }
                    }
                }
            }

            Label {
                visible: page.info.length > 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                horizontalAlignment: Text.AlignHCenter
                color: Theme.highlightColor
                text: page.info
            }

            // --- Freundesliste ---
            SectionHeader { text: S.t(net.locale, "friends") }
            Label {
                visible: page.friends.length === 0 && !page.busy
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                color: Theme.secondaryColor
                text: S.t(net.locale, "noFriends")
            }
            Repeater {
                model: page.friends
                ListItem {
                    width: page.width
                    contentHeight: Theme.itemSizeMedium
                    menu: ContextMenu {
                        MenuItem {
                            text: S.t(net.locale, "challenge")
                            onClicked: { page.busy = true; net.challengeUser(modelData.userId) }
                        }
                        MenuItem {
                            text: S.t(net.locale, "messages")
                            onClicked: pageStack.push(Qt.resolvedUrl("ThreadPage.qml"),
                                           { net: net, threadId: 0, recipientUserId: modelData.userId })
                        }
                    }
                    onClicked: pageStack.push(Qt.resolvedUrl("ThreadPage.qml"),
                                   { net: net, threadId: 0, recipientUserId: modelData.userId })
                    Row {
                        anchors.verticalCenter: parent.verticalCenter
                        x: Theme.horizontalPageMargin
                        width: parent.width - 2 * Theme.horizontalPageMargin
                        spacing: Theme.paddingMedium
                        Avatar {
                            size: Theme.itemSizeSmall
                            name: "" + (modelData.username ? modelData.username : "?")
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - Theme.itemSizeSmall - Theme.paddingMedium
                            truncationMode: TruncationMode.Fade
                            color: Theme.primaryColor
                            text: (modelData.username ? modelData.username : ("#" + modelData.userId))
                                  + (modelData.rating ? ("  (" + modelData.rating + ")") : "")
                        }
                    }
                }
            }
        }
    }
}
