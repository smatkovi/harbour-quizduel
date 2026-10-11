import QtQuick 1.1
import com.nokia.meego 1.0
import "../Strings.js" as S

// Freunde: Liste (getFriends), Suche nach EXAKTEM Namen (findUser), Hinzufügen (addFriend),
// Herausfordern (createRoom). Die Navigation ins neue Duell macht die HomePage (sie hört
// ebenfalls auf matchStarted). Profilbilder: nur Initiale (Harmattan lädt keine CDN-Bilder).
Page {
    id: page
    orientationLock: PageOrientation.LockPortrait
    property variant friends: []
    property variant found: 0
    property bool searched: false
    property bool busy: false
    property string note: ""

    function doSearch() {
        if (!net || query.text.length === 0) return
        page.found = 0; page.searched = false; page.note = ""; page.busy = true
        net.findUser(query.text)
    }

    tools: ToolBarLayout { ToolIcon { platformIconId: "toolbar-back"; onClicked: pageStack.pop() } }

    Component.onCompleted: { busy = true; net.getFriends() }

    Connections {
        target: net
        onFriendsLoaded: {
            busy = false
            try { var r = JSON.parse(payload); page.friends = (r.payload && r.payload.apiUserList) ? r.payload.apiUserList : [] }
            catch (e) { page.friends = [] }
        }
        onUserFound: {
            busy = false; page.searched = true
            try {
                var r = JSON.parse(payload)
                var u = (r.payload && (r.payload.User || r.payload.user)) ? (r.payload.User || r.payload.user) : 0
                page.found = (u && u.userId) ? u : 0
            } catch (e) { page.found = 0 }
        }
        onFriendAdded: { page.note = S.t(net.locale, "friendAdded"); net.getFriends() }
        onFailed: busy = false
    }

    Flickable {
        anchors.fill: parent
        contentHeight: col.height + 40
        Column {
            id: col
            x: 16; width: page.width - 32; spacing: 12
            Item { width: 1; height: 8 }
            Label { text: S.t(net.locale, "friends"); font.pixelSize: 36; font.bold: true; color: "white" }

            // Suche
            Row {
                width: parent.width; spacing: 10
                TextField {
                    id: query
                    width: parent.width - searchBtn.width - 10
                    placeholderText: S.t(net.locale, "searchHint")
                    inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                    Keys.onReturnPressed: page.doSearch()
                }
                Button { id: searchBtn; width: 120; text: S.t(net.locale, "search"); onClicked: page.doSearch() }
            }
            Label { width: parent.width; wrapMode: Text.Wrap; text: S.t(net.locale, "searchExact"); color: "#808080"; font.pixelSize: 15 }

            BusyIndicator { anchors.horizontalCenter: parent.horizontalCenter; running: page.busy; visible: page.busy }
            Label { visible: page.searched && !page.found && !page.busy; text: S.t(net.locale, "userNotFound"); color: "#909090"; font.pixelSize: 20 }
            Label { visible: page.note.length > 0; text: page.note; color: "#7eb3ff"; font.pixelSize: 20 }

            // Suchergebnis
            Item {
                visible: page.found !== 0
                width: col.width; height: 72
                Row {
                    anchors.verticalCenter: parent.verticalCenter; spacing: 12; width: parent.width
                    Avatar { size: 56; name: page.found ? ("" + page.found.username) : ""; anchors.verticalCenter: parent.verticalCenter }
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 56 - addBtn.width - chalBtn.width - 36
                        elide: Text.ElideRight; color: "white"; font.pixelSize: 22
                        text: page.found ? (page.found.username + (page.found.rating ? ("  (" + page.found.rating + ")") : "")) : ""
                    }
                    Button { id: addBtn; width: 120; text: S.t(net.locale, "addFriend"); anchors.verticalCenter: parent.verticalCenter
                             onClicked: if (page.found) net.addFriend(page.found.userId) }
                    Button { id: chalBtn; width: 120; text: S.t(net.locale, "challenge"); anchors.verticalCenter: parent.verticalCenter
                             onClicked: if (page.found) net.challengeUser(page.found.userId) }
                }
            }

            Rectangle { width: parent.width; height: 1; color: "#333333" }

            // Freundesliste
            Label { visible: page.friends.length === 0 && !page.busy; width: parent.width; wrapMode: Text.Wrap
                text: S.t(net.locale, "noFriends"); color: "#909090"; font.pixelSize: 20 }
            Repeater {
                model: page.friends
                Item {
                    width: col.width; height: 72
                    Row {
                        anchors.verticalCenter: parent.verticalCenter; spacing: 12; width: parent.width
                        Avatar { size: 56; name: "" + (modelData.username ? modelData.username : "?"); anchors.verticalCenter: parent.verticalCenter }
                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - 56 - chalBtn2.width - 24
                            Label { color: "white"; font.pixelSize: 22; elide: Text.ElideRight; width: parent.width
                                text: (modelData.username ? modelData.username : ("#" + modelData.userId))
                                      + (modelData.rating ? ("  (" + modelData.rating + ")") : "") }
                            Label { color: "#808080"; font.pixelSize: 15; text: S.t(net.locale, "messages") }
                        }
                        Button { id: chalBtn2; width: 120; text: S.t(net.locale, "challenge"); anchors.verticalCenter: parent.verticalCenter
                                 onClicked: net.challengeUser(modelData.userId) }
                    }
                    MouseArea {
                        anchors.fill: parent; anchors.rightMargin: 132
                        onClicked: pageStack.push(Qt.resolvedUrl("ThreadPage.qml"), { threadId: 0, recipientUserId: modelData.userId })
                    }
                }
            }
        }
    }
}
