import QtQuick 1.1
import com.nokia.meego 1.0
import "../Strings.js" as S

// Spiel-Liste (boards/by-collection) + Fragen-Probe (questions/difficulty).
// Parameter Best-Effort (B bestätigt sie).
Page {
    id: page
    orientationLock: PageOrientation.LockPortrait
    property variant games: []
    property variant questions: []
    property bool busy: false

    tools: ToolBarLayout {
        ToolIcon { platformIconId: "toolbar-back"; onClicked: pageStack.pop() }
        ToolIcon { platformIconId: "toolbar-refresh"; onClicked: { busy = true; net.gameList() } }
    }

    Component.onCompleted: { busy = true; net.gameList() }

    Connections {
        target: net
        onGamesLoaded: {
            busy = false
            try { var r = JSON.parse(payload); page.games = (r.payload && r.payload.GameBoards) ? r.payload.GameBoards : [] }
            catch (e) { page.games = [] }
        }
        onQuestionsLoaded: {
            busy = false
            try {
                var r = JSON.parse(payload)
                var cs = (r.payload && r.payload.categories) ? r.payload.categories : []
                var out = []
                for (var i = 0; i < cs.length; i++) {
                    var qs = cs[i].questions || cs[i].questionList || []
                    for (var j = 0; j < qs.length; j++) out.push(qs[j])
                }
                page.questions = out
            } catch (e) { page.questions = [] }
        }
        onFailed: busy = false
    }

    Flickable {
        anchors.fill: parent
        contentHeight: col.height + 40
        Column {
            id: col
            x: 16; width: page.width - 32; spacing: 12
            Item { width: 1; height: 8 }
            Label { text: S.t(net.locale, "games"); font.pixelSize: 36; font.bold: true; color: "white" }
            Button { width: parent.width; text: S.t(net.locale, "loadQuestions"); onClicked: { busy = true; net.fetchQuestions(0, 5) } }

            Label { visible: page.games.length === 0 && !busy; width: parent.width; wrapMode: Text.Wrap
                text: S.t(net.locale, "noGames"); color: "#909090"; font.pixelSize: 20 }
            Repeater {
                model: page.games
                Label { width: col.width; color: "white"; font.pixelSize: 22
                    text: "#" + modelData.id + (modelData.sortIndex !== undefined ? ("  (" + modelData.sortIndex + ")") : "") }
            }

            Label { visible: page.questions.length > 0; text: S.t(net.locale, "questions"); font.pixelSize: 24; color: "#7eb3ff" }
            Repeater {
                model: page.questions
                Column {
                    width: col.width; spacing: 2
                    Label { width: parent.width; wrapMode: Text.Wrap; color: "#7eb3ff"; font.pixelSize: 20; text: modelData.question }
                    Label { width: parent.width; wrapMode: Text.Wrap; color: "#b0b0b0"; font.pixelSize: 16
                        text: "✓ " + modelData.correct + "   ·   " + ((modelData.wrong1 || "") + "   ·   " + (modelData.wrong2 || "") + "   ·   " + (modelData.wrong3 || "")) }
                }
            }
        }
    }
}
