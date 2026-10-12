import QtQuick 2.0
import Sailfish.Silica 1.0
import "../Strings.js" as S

// Spielt eine Runde: 3 Fragen, je 4 gemischte Antworten, Countdown je Frage. Nach dem
// Tippen (oder Timeout) Auflösung: richtige Antwort grün, eigener Fehlgriff rot, Wahl des
// Gegners mit 👤 (falls er die Runde schon gespielt hat). „Weiter"/„Abschicken" steuert
// explizit; Serverfehler werden sichtbar (vorher hing der letzte Zug stumm).
Page {
    id: page
    property var net
    property string roomId
    property int roundIndex: 0
    property var category
    property var room                  // für die Gegner-Antworten dieser Runde
    property int timerSeconds: 20
    property bool selectCategory: true   // false = Kategorie steht fest, nur antworten

    property bool busy: false
    property string error: ""
    property bool copied: false
    property int qIdx: 0
    property var collected: []         // [{answer, questionRevisionId}] (kanonisch, -1 = Timeout)
    property var curOptions: []
    property bool revealed: false
    property int pickedIdx: -2         // -2 keine, -1 Timeout, 0..3
    property int timeLeft: timerSeconds

    property var questions: (category && category.questions) ? category.questions : []
    property var curQ: (page.qIdx < questions.length) ? questions[page.qIdx] : null
    property var curImg: (curQ && curQ.image) ? curQ.image : null
    property string curImgRaw: curImg ? ("" + (curImg.url_xhdpi || curImg.url_hdpi || curImg.url_mdpi || curImg.url_ldpi || "")) : ""
    // Bild-URLs kommen schema-relativ (//host/…) -> https: voranstellen, sonst löst Qt sie
    // gegen die file://-Basis auf und lädt nichts.
    property string curImgUrl: curImgRaw.indexOf("//") === 0 ? ("https:" + curImgRaw) : curImgRaw
    // Bildfragen: der Countdown darf erst laufen, wenn das Bild geladen ist (oder keins da ist,
    // oder es endgültig scheiterte) — sonst tickt die Zeit, während das Bild noch kommt.
    property bool imageReady: page.curImgUrl.length === 0
                              || qImage.status === Image.Ready || qImage.status === Image.Error

    // Gegner-Antworten dieser Runde aus dem commandInvocationStack: {questionRevisionId: answerIdx}
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
    property var oppAns: opponentAnswers()

    function optionsFor(q) {
        var opts = [ { text: q.correct, idx: 0 }, { text: q.wrong1, idx: 1 },
                     { text: q.wrong2, idx: 2 }, { text: q.wrong3, idx: 3 } ]
        for (var i = opts.length - 1; i > 0; i--) {
            var j = Math.floor(Math.random() * (i + 1))
            var t = opts[i]; opts[i] = opts[j]; opts[j] = t
        }
        return opts
    }

    Component.onCompleted: if (questions.length > 0) { curOptions = optionsFor(questions[0]); timeLeft = timerSeconds }

    Timer {
        interval: 1000; repeat: true
        running: !page.revealed && !page.busy && page.error === "" && page.qIdx < page.questions.length
        onTriggered: { page.timeLeft = page.timeLeft - 1; if (page.timeLeft <= 0) page.doReveal(-1) }
    }

    Connections {
        target: net
        onRoomUpdated: { page.busy = false; pageStack.pop() }
        onFailed: {
            page.busy = false
            // NUR der harmlose Fall „Kategorie schon gewählt" (Rennen) -> einmal ohne
            // select_category nachsetzen. Ein echtes „nicht am Zug" (not_in_turn ohne
            // „already selected") NICHT umdeuten (sonst no_category_selected) -> Fehler zeigen.
            if (page.selectCategory && message.indexOf("already selected") >= 0) {
                page.selectCategory = false
                page.retrySubmit()
            } else {
                page.error = message
                page.copied = false
            }
        }
    }

    function doReveal(idx) { if (!revealed) { pickedIdx = idx; revealed = true } }
    function proceed() {
        var a = page.collected
        a.push({ answer: pickedIdx, questionRevisionId: curQ.id })
        page.collected = a
        if (qIdx + 1 < questions.length) {
            qIdx = qIdx + 1
            revealed = false; pickedIdx = -2
            timeLeft = timerSeconds
            curOptions = optionsFor(questions[qIdx])
        } else {
            page.busy = true; page.error = ""
            net.submitRound(roomId, roundIndex, category.id, page.collected, page.selectCategory)
        }
    }
    function retrySubmit() { page.busy = true; page.error = ""; net.submitRound(roomId, roundIndex, category.id, page.collected, page.selectCategory) }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: col.height + Theme.paddingLarge
        VerticalScrollDecorator {}

        Column {
            id: col
            width: page.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: (category ? (category.name ? category.name : ("#" + category.id)) : "")
                       + "   " + Math.min(page.qIdx + 1, page.questions.length) + "/" + page.questions.length
            }

            // Countdown-Leiste
            Item {
                visible: !page.busy && !page.revealed && page.error === ""
                width: parent.width - 2 * Theme.horizontalPageMargin
                anchors.horizontalCenter: parent.horizontalCenter
                height: Theme.paddingMedium
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width; height: Theme.paddingSmall / 2; radius: height / 2
                    color: Theme.rgba(Theme.secondaryColor, 0.3)
                }
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width * Math.max(0, page.timeLeft) / page.timerSeconds
                    height: Theme.paddingSmall / 2; radius: height / 2
                    color: page.timeLeft <= 5 ? Theme.errorColor : Theme.highlightColor
                    Behavior on width { NumberAnimation { duration: 300 } }
                }
            }

            Label {
                visible: !page.busy && !!page.curQ
                x: Theme.horizontalPageMargin; width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap; color: Theme.highlightColor; font.pixelSize: Theme.fontSizeLarge
                text: page.curQ ? page.curQ.question : ""
            }

            Image {
                id: qImage
                visible: !page.busy && page.curImgUrl.length > 0
                source: page.curImgUrl; asynchronous: true; cache: true
                fillMode: Image.PreserveAspectFit
                width: parent.width - 2 * Theme.horizontalPageMargin
                anchors.horizontalCenter: parent.horizontalCenter
                BusyIndicator { anchors.centerIn: parent; size: BusyIndicatorSize.Small; running: parent.status === Image.Loading }
            }
            Label {
                visible: !page.busy && page.curImg && ("" + (page.curImg.credit || "")).length > 0
                x: Theme.horizontalPageMargin; width: parent.width - 2 * Theme.horizontalPageMargin
                horizontalAlignment: Text.AlignRight; wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall; color: Theme.secondaryColor
                text: page.curImg ? ("© " + page.curImg.credit) : ""
            }

            Label {
                visible: page.revealed && page.pickedIdx === -1
                x: Theme.horizontalPageMargin; width: parent.width - 2 * Theme.horizontalPageMargin
                color: Theme.errorColor
                text: S.t(net.locale, "timeUp")
            }

            Repeater {
                model: page.busy ? [] : page.curOptions
                BackgroundItem {
                    width: page.width
                    height: Theme.itemSizeMedium
                    enabled: !page.revealed
                    onClicked: page.doReveal(modelData.idx)
                    Rectangle {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.horizontalPageMargin
                        anchors.rightMargin: Theme.horizontalPageMargin
                        anchors.topMargin: Theme.paddingSmall / 2
                        anchors.bottomMargin: Theme.paddingSmall / 2
                        radius: Theme.paddingSmall
                        color: !page.revealed
                               ? Theme.rgba(Theme.primaryColor, 0.12)
                               : (modelData.idx === 0
                                  ? Qt.rgba(0.18, 0.55, 0.22, 0.55)
                                  : (modelData.idx === page.pickedIdx
                                     ? Qt.rgba(0.70, 0.20, 0.20, 0.55)
                                     : Theme.rgba(Theme.primaryColor, 0.08)))
                        Label {
                            anchors {
                                left: parent.left; right: oppMark.left
                                verticalCenter: parent.verticalCenter
                                leftMargin: Theme.paddingMedium; rightMargin: Theme.paddingSmall
                            }
                            wrapMode: Text.Wrap; color: Theme.primaryColor
                            text: modelData.text
                        }
                        Label {
                            id: oppMark
                            anchors { right: parent.right; verticalCenter: parent.verticalCenter; rightMargin: Theme.paddingMedium }
                            visible: page.revealed && !!page.curQ && ("" + page.oppAns[page.curQ.id]) === ("" + modelData.idx)
                            font.pixelSize: Theme.fontSizeSmall; color: Theme.highlightColor
                            text: "👤"
                        }
                    }
                }
            }

            Button {
                visible: page.revealed && !page.busy && page.error === ""
                anchors.horizontalCenter: parent.horizontalCenter
                text: (page.qIdx + 1 < page.questions.length) ? S.t(net.locale, "next") : S.t(net.locale, "submit")
                onClicked: page.proceed()
            }

            BusyIndicator { running: page.busy; size: BusyIndicatorSize.Medium; anchors.horizontalCenter: parent.horizontalCenter }

            BackgroundItem {
                visible: page.error !== ""
                width: page.width
                height: errCol.height + Theme.paddingMedium
                onClicked: { Clipboard.text = page.error; page.copied = true }
                Column {
                    id: errCol
                    x: Theme.horizontalPageMargin
                    width: page.width - 2 * Theme.horizontalPageMargin
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Theme.paddingSmall
                    Label {
                        width: parent.width; wrapMode: Text.Wrap
                        color: Theme.errorColor; font.pixelSize: Theme.fontSizeSmall
                        // Lesbarer Text; das rohe JSON (page.error) wird beim Antippen kopiert.
                        text: S.errText(net.locale, page.error)
                    }
                    Label {
                        width: parent.width
                        color: Theme.secondaryColor; font.pixelSize: Theme.fontSizeExtraSmall
                        text: page.copied ? "✓" : S.t(net.locale, "copyError")
                    }
                }
            }
            Button {
                visible: page.error !== ""
                anchors.horizontalCenter: parent.horizontalCenter
                text: S.t(net.locale, "retry")
                onClicked: page.retrySubmit()
            }
        }
    }
}
