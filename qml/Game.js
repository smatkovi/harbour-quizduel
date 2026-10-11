.pragma library
// Gemeinsame Zuglogik für Übersicht (HomePage) und Duell (DuelPage), damit beide
// dasselbe Modell nutzen. Zugmodell (aus den Mitschnitten abgeleitet): 6 Runden,
// 3 pro Zug in Blöcken — der Starter wählt+beantwortet Runden 0-2 und wartet dann,
// der Gegner beantwortet 0-2 und macht 3-5, dann beantwortet der Starter 3-5.
// Der aktuelle Zustand wird aus room.gameState.json.commandInvocationStack abgeleitet
// (es gibt kein explizites "currentRound"-/"turn"-Feld).

function invocations(room) {
    if (!room || !room.gameState || !room.gameState.json) return []
    var cis = room.gameState.json.commandInvocationStack
    return (cis && cis.invocations) ? cis.invocations : []
}

function catSelectedForRound(room, r) {
    var inv = invocations(room)
    for (var i = 0; i < inv.length; i++) {
        if (inv[i].command !== "select_category") continue
        var dp = inv[i].dictionaryPayload
        if (dp && ("" + dp.roundIndex) === ("" + r)) return dp.categoryId
    }
    return -1
}

function whoSelectedRound(room, r) {
    var inv = invocations(room)
    for (var i = 0; i < inv.length; i++) {
        if (inv[i].command !== "select_category") continue
        var dp = inv[i].dictionaryPayload
        if (dp && ("" + dp.roundIndex) === ("" + r)) return "" + inv[i].participantUserId
    }
    return ""
}

function mySubmitted(room, r, myId) {
    var inv = invocations(room); var mine = "" + myId
    for (var i = 0; i < inv.length; i++) {
        if (inv[i].command !== "submit_answers" || ("" + inv[i].participantUserId) !== mine) continue
        var dp = inv[i].dictionaryPayload
        if (dp && ("" + dp.roundIndex) === ("" + r)) return true
    }
    return false
}

// Darf ICH die Kategorie für Runde r wählen? Der Wähler WECHSELT je Runde (alternierend):
// hat der Gegner eine Runde gewählt, wähle ich die nächste. Anker ist die erste bereits
// gewählte Runde; über die Parität des Abstands ergibt sich der Wähler jeder Runde.
function mineToSelect(room, r, myId) {
    var r0 = -1, c0me = false
    for (var k = 0; k < 6; k++) {
        var w = whoSelectedRound(room, k)
        if (w !== "") { r0 = k; c0me = (w === ("" + myId)); break }
    }
    if (r0 < 0) return (r === 0)                  // frisches Spiel: ich beginne mit Runde 0
    var same = (((r - r0) % 2) === 0)
    return same ? c0me : !c0me
}

function submittedByOther(room, r, myId) {
    var inv = invocations(room); var mine = "" + myId
    for (var i = 0; i < inv.length; i++) {
        if (inv[i].command !== "submit_answers" || ("" + inv[i].participantUserId) === mine) continue
        var dp = inv[i].dictionaryPayload
        if (dp && ("" + dp.roundIndex) === ("" + r)) return true
    }
    return false
}

// Die aktuelle Runde des Spiels = die niedrigste, die NICHT von BEIDEN beantwortet wurde.
// Man darf nicht vorgreifen: die nächste Runde öffnet erst, wenn die aktuelle für beide
// durch ist (sonst lehnt der Server mit wrong_round "game is on round N" ab).
function currentRound(room, myId) {
    for (var r = 0; r < 6; r++)
        if (!(mySubmitted(room, r, myId) && submittedByOther(room, r, myId))) return r
    return 6
}

// Nächste Runde, die ICH spielen darf (0..5), oder -1, wenn der Gegner dran ist.
function nextPlayableRound(room, myId) {
    if (!room || !room.gameState || !room.gameState.json) return 0
    var r = currentRound(room, myId)
    if (r >= 6) return -1                                 // alle Runden durch
    if (mySubmitted(room, r, myId)) return -1             // ich bin fertig -> warte auf Gegner
    if (catSelectedForRound(room, r) >= 0) return r       // Kategorie steht -> antworten
    if (mineToSelect(room, r, myId)) return r             // ich wähle + antworte
    return -1                                              // Gegner muss erst wählen -> warten
}

// "finished" | "myturn" | "waiting"
function status(room, myId) {
    if (room && room.roomStatus === "finished") return "finished"
    return nextPlayableRound(room, myId) >= 0 ? "myturn" : "waiting"
}

// Gegnername aus den participants (der mit anderer userId als ich).
function opponentName(room, myId) {
    var ps = (room && room.participants) ? room.participants : null
    if (!ps) return "…"
    if (!Array.isArray(ps)) ps = [ps]
    var mine = "" + myId
    for (var i = 0; i < ps.length; i++)
        if (("" + ps[i].userId) !== mine)
            return ps[i].username ? ps[i].username : (ps[i].usernameForTag ? ps[i].usernameForTag : "?")
    return "…"
}
