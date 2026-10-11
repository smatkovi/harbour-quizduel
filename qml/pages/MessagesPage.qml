import QtQuick 2.0
import Sailfish.Silica 1.0
import "../Strings.js" as S

// Unterhaltungen (chatthread/*) + Posteingang (inbox/list). net ist die geteilte
// NetClient-Instanz. Beide Listen werden beim Öffnen geladen.
Page {
    id: page
    property var net
    property var threads: []
    property var inbox: []
    property bool busy: false

    function reload() { if (net) { busy = true; net.getChatThreads(); net.inboxList() } }
    Component.onCompleted: reload()

    Connections {
        target: net
        onThreadsLoaded: { page.threads = threads; page.busy = false }
        onInboxLoaded: page.inbox = messages
        onFailed: page.busy = false
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: col.height + Theme.paddingLarge
        VerticalScrollDecorator {}

        PullDownMenu {
            MenuItem { text: S.t(net.locale, "friends"); onClicked: pageStack.push(Qt.resolvedUrl("FriendsPage.qml"), { net: net }) }
            MenuItem { text: "↻"; onClicked: reload() }
        }

        Column {
            id: col
            width: page.width
            spacing: Theme.paddingSmall

            PageHeader { title: S.t(net.locale, "messages") }

            SectionHeader { text: S.t(net.locale, "conversations") }
            Label {
                visible: page.threads.length === 0
                x: Theme.horizontalPageMargin; width: parent.width - 2 * Theme.horizontalPageMargin
                color: Theme.secondaryColor; wrapMode: Text.Wrap
                text: S.t(net.locale, "noThreads")
            }
            Repeater {
                model: page.threads
                ListItem {
                    width: page.width
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        x: Theme.horizontalPageMargin
                        text: S.t(net.locale, "conversations") + " #" + modelData.chatThreadId
                              + "  (" + (modelData.members ? modelData.members.length : 0) + ")"
                        color: Theme.primaryColor
                    }
                    onClicked: pageStack.push(Qt.resolvedUrl("ThreadPage.qml"), { net: net, threadId: modelData.chatThreadId })
                }
            }

            SectionHeader { text: S.t(net.locale, "inbox") }
            Label {
                visible: page.inbox.length === 0
                x: Theme.horizontalPageMargin; width: parent.width - 2 * Theme.horizontalPageMargin
                color: Theme.secondaryColor; wrapMode: Text.Wrap
                text: S.t(net.locale, "noMessages")
            }
            Repeater {
                model: page.inbox
                ListItem {
                    width: page.width
                    contentHeight: Theme.itemSizeMedium
                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        x: Theme.horizontalPageMargin
                        width: page.width - 2 * Theme.horizontalPageMargin
                        Label {
                            width: parent.width; truncationMode: TruncationMode.Fade
                            text: modelData.subject && modelData.subject.length ? modelData.subject : ("#" + modelData.messageId)
                            color: modelData.read ? Theme.secondaryColor : Theme.highlightColor
                            font.pixelSize: Theme.fontSizeSmall
                        }
                        Label {
                            width: parent.width; wrapMode: Text.Wrap; maximumLineCount: 2; truncationMode: TruncationMode.Fade
                            text: modelData.message ? modelData.message : ""
                            color: Theme.secondaryColor; font.pixelSize: Theme.fontSizeExtraSmall
                        }
                    }
                }
            }
        }
    }
}
