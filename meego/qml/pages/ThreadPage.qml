import QtQuick 1.1
import com.nokia.meego 1.0
import "../Strings.js" as S

// Eine Unterhaltung + Eingabezeile. threadId 0 = neu (erstes Senden ruft
// startNewChatThread mit recipientUserId; die Antwort trägt die neue chatThreadId).
Page {
    id: page
    orientationLock: PageOrientation.LockPortrait
    property int threadId: 0
    property int recipientUserId: 0
    property variant messages: []
    property bool busy: false

    tools: ToolBarLayout { ToolIcon { platformIconId: "toolbar-back"; onClicked: pageStack.pop() } }

    Component.onCompleted: if (threadId > 0) { busy = true; net.getChatThread(threadId) }

    Connections {
        target: net
        onThreadLoaded: {
            busy = false
            try {
                var r = JSON.parse(payload)
                if (r.payload) { page.threadId = r.payload.chatThreadId; page.messages = r.payload.messages ? r.payload.messages : [] }
            } catch (e) {}
        }
        onFailed: busy = false
    }

    function send() {
        if (input.text.length === 0) return
        busy = true
        if (threadId > 0) net.sendMessageToThread(threadId, input.text)
        else net.startChatThread(recipientUserId, input.text)
        input.text = ""
    }

    Flickable {
        anchors { top: parent.top; left: parent.left; right: parent.right; bottom: inputRow.top }
        contentHeight: col.height + 20; clip: true
        Column {
            id: col
            x: 16; width: page.width - 32; spacing: 10
            Item { width: 1; height: 8 }
            Label { text: S.t(net.locale, "conversations") + (page.threadId > 0 ? " #" + page.threadId : ""); font.pixelSize: 28; color: "white" }
            Label { visible: page.messages.length === 0 && !page.busy; width: parent.width; wrapMode: Text.Wrap
                text: S.t(net.locale, "noMessages"); color: "#909090"; font.pixelSize: 20 }
            Repeater {
                model: page.messages
                Label {
                    width: col.width; wrapMode: Text.Wrap; font.pixelSize: 22
                    text: modelData.message
                    color: ("" + modelData.senderUserId === "" + net.userId()) ? "#7eb3ff" : "white"
                    horizontalAlignment: ("" + modelData.senderUserId === "" + net.userId()) ? Text.AlignRight : Text.AlignLeft
                }
            }
        }
    }

    Item {
        id: inputRow
        anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
        height: 72
        TextField {
            id: input
            anchors { left: parent.left; right: sendBtn.left; verticalCenter: parent.verticalCenter; margins: 8 }
            placeholderText: S.t(net.locale, "messageHint")
        }
        Button {
            id: sendBtn
            anchors { right: parent.right; verticalCenter: parent.verticalCenter; rightMargin: 8 }
            width: 130
            text: S.t(net.locale, "send")
            enabled: input.text.length > 0 && !page.busy
            onClicked: send()
        }
    }
}
