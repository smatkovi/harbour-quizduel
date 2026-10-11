import QtQuick 2.0
import Sailfish.Silica 1.0
import "../Strings.js" as S

// Eine Unterhaltung: Nachrichten + Eingabezeile. threadId 0 = neue Unterhaltung
// (dann wird beim ersten Senden startNewChatThread mit recipientUserId benutzt;
// die Antwort liefert die neue chatThreadId).
Page {
    id: page
    property var net
    property var threadId: 0
    property var recipientUserId: 0
    property var messages: []
    property bool busy: false

    Component.onCompleted: if (threadId > 0) { busy = true; net.getChatThread(threadId) }

    Connections {
        target: net
        onThreadLoaded: {
            page.threadId = thread.chatThreadId
            page.messages = thread.messages
            page.busy = false
        }
        onFailed: page.busy = false
    }

    function send() {
        if (!net || input.text.length === 0) return
        page.busy = true
        if (threadId > 0) net.sendMessageToThread(threadId, input.text)
        else net.startChatThread(recipientUserId, input.text)
        input.text = ""
    }

    SilicaFlickable {
        id: flick
        anchors { top: parent.top; left: parent.left; right: parent.right; bottom: inputRow.top }
        contentHeight: col.height + Theme.paddingLarge
        clip: true
        VerticalScrollDecorator {}
        Column {
            id: col
            width: page.width
            spacing: Theme.paddingSmall
            PageHeader { title: S.t(net.locale, "conversations") + (page.threadId > 0 ? " #" + page.threadId : "") }
            Label {
                visible: page.messages.length === 0 && !page.busy
                x: Theme.horizontalPageMargin; width: parent.width - 2 * Theme.horizontalPageMargin
                color: Theme.secondaryColor; wrapMode: Text.Wrap
                text: S.t(net.locale, "noMessages")
            }
            Repeater {
                model: page.messages
                Label {
                    x: Theme.horizontalPageMargin
                    width: page.width - 2 * Theme.horizontalPageMargin
                    wrapMode: Text.Wrap
                    text: modelData.message
                    // eigene Nachrichten rechts + hervorgehoben
                    color: ("" + modelData.senderUserId === "" + net.userId()) ? Theme.highlightColor : Theme.primaryColor
                    horizontalAlignment: ("" + modelData.senderUserId === "" + net.userId()) ? Text.AlignRight : Text.AlignLeft
                }
            }
        }
    }

    Row {
        id: inputRow
        anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
        height: Math.max(input.height, Theme.itemSizeMedium)
        TextField {
            id: input
            width: parent.width - sendBtn.width
            anchors.verticalCenter: parent.verticalCenter
            placeholderText: S.t(net.locale, "messageHint")
            EnterKey.iconSource: "image://theme/icon-m-enter-accept"
            EnterKey.onClicked: send()
        }
        IconButton {
            id: sendBtn
            anchors.verticalCenter: parent.verticalCenter
            icon.source: "image://theme/icon-m-send"
            enabled: input.text.length > 0 && !page.busy
            onClicked: send()
        }
    }
}
