import QtQuick 1.1
import com.nokia.meego 1.0
import "../Strings.js" as S

// Spielt eine Runde: 3 Fragen, je 4 gemischte Antworten, Countdown. Bildfragen: das Bild
// holt der Holer (rustls/TLS 1.2) in eine LOKALE Datei — Harmattans Qt 4 kann die CDN-
// https-Bilder nicht —, und der Timer startet ERST, wenn das Bild lokal da ist (oder keins
// da ist / es scheiterte). Antworten gehen als JSON-String + room.myParticipantId zum Zug.
Page {
    id: page
    orientationLock: PageOrientation.LockPortrait
    property variant room: 0
    property string roomId: ""
    property int roundIndex: 0
    property variant category: 0
    property bool selectCategory: true
    property real participantId: 0     // room-lokal (32 Bit reicht nicht -> real/double)

    property bool busy: false
    property string errRaw: ""
    property int qIdx: 0
    property variant collected: []
    property variant curOptions: []
    property bool revealed: false
    property int pickedIdx: -2
    property int timerSeconds: 20
    property int timeLeft: 20

    property variant questions: (category && category.questions) ? category.questions : []
    property variant curQ: (page.qIdx < questions.length) ? questions[page.qIdx] : 0
    property variant curImg: (curQ && curQ.image) ? curQ.image : 0
    property string curImgRaw: curImg ? ("" + (curImg.url_xhdpi || curImg.url_hdpi || curImg.url_mdpi || curImg.url_ldpi || "")) : ""
    // Bild-URLs kommen schema-relativ (//host/…) -> https: voranstellen (sonst lädt nichts).
    property string curImgUrl: curImgRaw.indexOf("//") === 0 ? ("https:" + curImgRaw) : curImgRaw

    property string localImg: ""       // file://… sobald der Holer das Bild geladen hat
    property bool imgDone: false
    property bool imageReady: curImgUrl.length === 0 || imgDone

    function myPid() { return (room && room.myParticipantId) ? room.myParticipantId : page.participantId }

    // Gegner-Antworten dieser Runde: {questionRevisionId: answerIdx}
    function opponentAnswers() {
        var map = {}
        if (!room || !room.gameState || !room.gameState.json) return map
        var cis = room.gameState.json.commandInvocationStack
        var inv = (cis && cis.invocations) ? cis.invocations : []
        var mine = "" + net.userId()
        for (var i = 0; i < inv.length; i++) {
            var it = inv[i]
            if (it.command !== "submit_answers" || ("" + it.participantUserId) === mine) continue
            var dp = it.dictionaryPayload
            if (!dp || ("" + dp.roundIndex) !== ("" + roundIndex)) continue
            var ans = dp.answers || []
            for (var j = 0; j < ans.length; j++) map["" + ans[j].questionRevisionId] = ans[j].answer
        }
        return map
    }
    property variant oppAns: opponentAnswers()

    function optionsFor(q) {
        var opts = [ { text: q.correct, idx: 0 }, { text: q.wrong1, idx: 1 },
                     { text: q.wrong2, idx: 2 }, { text: q.wrong3, idx: 3 } ]
        for (var i = opts.length - 1; i > 0; i--) {
            var j = Math.floor(Math.random() * (i + 1))
            var t = opts[i]; opts[i] = opts[j]; opts[j] = t
        }
        return opts
    }

    function startQuestion() {
        page.revealed = false
        page.pickedIdx = -2
        page.timeLeft = page.timerSeconds
        page.localImg = ""
        page.imgDone = false
        if (page.qIdx < questions.length) page.curOptions = optionsFor(questions[page.qIdx])
        if (page.curImgUrl.length > 0) net.fetchImage(page.curImgUrl)   // Timer wartet auf imageReady
    }
    Component.onCompleted: if (questions.length > 0) startQuestion()
    onQIdxChanged: startQuestion()

    Connections {
        target: net
        onImageFetched: {
            if (url === page.curImgUrl) {
                page.localImg = path.length ? ("file://" + path) : ""
                page.imgDone = true
            }
        }
        onRoomUpdated: { page.busy = false; pageStack.pop() }
        onFailed: {
            page.busy = false
            // Nur der harmlose Fall „Kategorie schon gewählt" wird ohne select_category
            // nachgesetzt; echtes „nicht am Zug" zeigt den Fehler (kein no_category_selected).
            if (page.selectCategory && message.indexOf("already selected") >= 0) {
                page.selectCategory = false
                page.retrySubmit()
            } else {
                page.errRaw = message
            }
        }
    }

    Timer {
        interval: 1000; repeat: true
        running: page.imageReady && !page.revealed && !page.busy && page.errRaw === "" && page.qIdx < page.questions.length
        onTriggered: { page.timeLeft = page.timeLeft - 1; if (page.timeLeft <= 0) page.doReveal(-1) }
    }

    function doReveal(idx) { if (!revealed) { pickedIdx = idx; revealed = true } }
    function proceed() {
        var a = page.collected
        a.push({ answer: pickedIdx, questionRevisionId: curQ.id })
        page.collected = a
        if (qIdx + 1 < questions.length) {
            qIdx = qIdx + 1
        } else {
            page.busy = true; page.errRaw = ""
            net.submitRound(roomId, myPid(), roundIndex, category.id, JSON.stringify(page.collected), page.selectCategory)
        }
    }
    function retrySubmit() {
        page.busy = true; page.errRaw = ""
        net.submitRound(roomId, myPid(), roundIndex, category.id, JSON.stringify(page.collected), page.selectCategory)
    }

    tools: ToolBarLayout {
        ToolIcon { platformIconId: "toolbar-back"; onClicked: pageStack.pop() }
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
            Label {
                text: (category ? (category.name ? category.name : ("#" + category.id)) : "")
                      + "   " + Math.min(page.qIdx + 1, page.questions.length) + "/" + page.questions.length
                font.pixelSize: 28; font.bold: true; color: "white"
            }

            // Countdown-Leiste (oder „Bild lädt…", solange der Holer holt)
            Rectangle {
                visible: !page.busy && !page.revealed && page.errRaw === "" && page.imageReady
                width: parent.width; height: 8; radius: 4; color: "#404040"
                Rectangle {
                    width: parent.width * Math.max(0, page.timeLeft) / page.timerSeconds
                    height: parent.height; radius: 4
                    color: page.timeLeft <= 5 ? "#ff6b6b" : "#7eb3ff"
                }
            }
            Row {
                visible: !page.busy && !page.revealed && page.errRaw === "" && !page.imageReady
                spacing: 10
                BusyIndicator { running: true; width: 32; height: 32 }
                Label { text: S.t(net.locale, "pleaseWait"); color: "#a0a0a0"; font.pixelSize: 20; anchors.verticalCenter: parent.verticalCenter }
            }

            Label {
                visible: !page.busy && !!page.curQ
                width: parent.width; wrapMode: Text.Wrap
                color: "#e6e6e6"; font.pixelSize: 26
                text: page.curQ ? page.curQ.question : ""
            }

            Image {
                visible: !page.busy && page.localImg.length > 0
                source: page.localImg
                cache: false          // Datei kann je Frage neu geschrieben werden -> nicht cachen
                asynchronous: true
                fillMode: Image.PreserveAspectFit
                width: parent.width
                sourceSize.width: parent.width
            }
            Label {
                visible: page.revealed && page.pickedIdx === -1
                color: "#ff6b6b"; font.pixelSize: 22
                text: S.t(net.locale, "timeUp")
            }

            Repeater {
                model: page.busy ? [] : page.curOptions
                Rectangle {
                    width: col.width
                    height: 72
                    radius: 8
                    color: !page.revealed
                           ? "#223040"
                           : (modelData.idx === 0
                              ? Qt.rgba(0.18, 0.55, 0.22, 0.6)
                              : (modelData.idx === page.pickedIdx
                                 ? Qt.rgba(0.70, 0.20, 0.20, 0.6)
                                 : "#1c2630"))
                    Label {
                        anchors.left: parent.left; anchors.right: oppMark.left
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 14; anchors.rightMargin: 6
                        wrapMode: Text.Wrap; color: "white"; font.pixelSize: 22
                        text: modelData.text
                    }
                    Label {
                        id: oppMark
                        anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                        anchors.rightMargin: 14
                        visible: page.revealed && !!page.curQ && ("" + page.oppAns["" + page.curQ.id]) === ("" + modelData.idx)
                        text: "👤"; font.pixelSize: 22; color: "#7eb3ff"
                    }
                    MouseArea { anchors.fill: parent; enabled: !page.revealed; onClicked: page.doReveal(modelData.idx) }
                }
            }

            Button {
                visible: page.revealed && !page.busy && page.errRaw === ""
                width: parent.width
                text: (page.qIdx + 1 < page.questions.length) ? S.t(net.locale, "next") : S.t(net.locale, "submit")
                onClicked: page.proceed()
            }
            BusyIndicator { anchors.horizontalCenter: parent.horizontalCenter; running: page.busy; visible: page.busy }

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
            Button {
                visible: page.errRaw.length > 0
                width: parent.width
                text: S.t(net.locale, "retry")
                onClicked: page.retrySubmit()
            }
        }
    }
}
