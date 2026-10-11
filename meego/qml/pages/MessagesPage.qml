import QtQuick 1.1
import com.nokia.meego 1.0
import "../Strings.js" as S

// Unterhaltungen (chatthread/*) + Posteingang (inbox/list). net.*Loaded liefert den
// JSON-String der Holer-Antwort; hier mit JSON.parse ausgewertet (.payload).
Page {
    id: page
    orientationLock: PageOrientation.LockPortrait
    property variant threads: []
    property variant inbox: []
    property bool busy: false

    tools: ToolBarLayout {
        ToolIcon { platformIconId: "toolbar-back"; onClicked: pageStack.pop() }
        ToolIcon { platformIconId: "toolbar-refresh"; onClicked: reload() }
    }

    function reload() { busy = true; net.getChatThreads(); net.inboxList() }
    Component.onCompleted: reload()

    Connections {
        target: net
        onThreadsLoaded: {
            busy = false
            try { var r = JSON.parse(payload); page.threads = (r.payload && r.payload.chatThreadsStripped) ? r.payload.chatThreadsStripped : [] }
            catch (e) { page.threads = [] }
        }
        onInboxLoaded: {
            try { var r = JSON.parse(payload); page.inbox = (r.payload && r.payload.messages) ? r.payload.messages : [] }
            catch (e) { page.inbox = [] }
        }
        onFailed: busy = false
    }

    Flickable {
        anchors.fill: parent
        contentHeight: col.height + 40
        Column {
            id: col
            x: 16; width: page.width - 32; spacing: 14
            Item { width: 1; height: 8 }
            Label { text: S.t(net.locale, "messages"); font.pixelSize: 36; font.bold: true; color: "white" }

            Label { text: S.t(net.locale, "conversations"); font.pixelSize: 24; color: "#7eb3ff" }
            Label { visible: page.threads.length === 0; width: parent.width; wrapMode: Text.Wrap
                text: S.t(net.locale, "noThreads"); color: "#909090"; font.pixelSize: 20 }
            Repeater {
                model: page.threads
                Item {
                    width: col.width; height: 64
                    Label {
                        anchors.verticalCenter: parent.verticalCenter; color: "white"; font.pixelSize: 22
                        text: S.t(net.locale, "conversations") + " #" + modelData.chatThreadId
                              + "  (" + (modelData.threadMemberUserIds ? modelData.threadMemberUserIds.length : 0) + ")"
                    }
                    MouseArea { anchors.fill: parent; onClicked: pageStack.push(Qt.resolvedUrl("ThreadPage.qml"), { threadId: modelData.chatThreadId }) }
                }
            }

            Label { text: S.t(net.locale, "inbox"); font.pixelSize: 24; color: "#7eb3ff" }
            Label { visible: page.inbox.length === 0; width: parent.width; wrapMode: Text.Wrap
                text: S.t(net.locale, "noMessages"); color: "#909090"; font.pixelSize: 20 }
            Repeater {
                model: page.inbox
                Column {
                    width: col.width; spacing: 2
                    Label {
                        width: parent.width; elide: Text.ElideRight; font.pixelSize: 20
                        color: (modelData.readAtUtcMs > 0) ? "#909090" : "white"
                        text: (modelData.body && modelData.body.subject) ? modelData.body.subject : ("#" + modelData.messageId)
                    }
                    Label {
                        width: parent.width; wrapMode: Text.Wrap; color: "#808080"; font.pixelSize: 16
                        text: (modelData.body && modelData.body.message) ? modelData.body.message : ""
                    }
                }
            }
        }
    }
}
