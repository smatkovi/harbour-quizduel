#include "netclient.h"
#include "davinciprotocol.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QUuid>

NetClient::NetClient(QObject *parent)
    : QObject(parent), m_userId(0)
{
    m_deviceId = loadOrCreateDeviceId();
    // Sprache: gespeicherte Wahl, sonst Gerätesprache (Rückfall de_DE).
    QSettings settings(QStringLiteral("harbour-quizduel"), QStringLiteral("harbour-quizduel"));
    m_locale = settings.value(QStringLiteral("locale")).toString();
    if (m_locale.isEmpty()) {
        m_locale = QLocale::system().name();
        if (!m_locale.contains(QLatin1Char('_')))
            m_locale = QStringLiteral("de_DE");
    }
}

void NetClient::setLocale(const QString &locale)
{
    if (locale.isEmpty() || locale == m_locale)
        return;
    m_locale = locale;
    QSettings settings(QStringLiteral("harbour-quizduel"), QStringLiteral("harbour-quizduel"));
    settings.setValue(QStringLiteral("locale"), m_locale);
    emit localeChanged();
}

QString NetClient::loadOrCreateDeviceId()
{
    // Persistent pro Installation. Die QSettings-Kennung MUSS zur .desktop-Kennung
    // passen (Sailjail-Falle: sonst merkt sich die App über das Icon nichts) —
    // darum Organisation und Anwendung beide "harbour-quizduel".
    QSettings settings(QStringLiteral("harbour-quizduel"), QStringLiteral("harbour-quizduel"));
    QString id = settings.value(QStringLiteral("deviceId")).toString();
    if (id.length() != 32) {
        // 16 Zufallsbytes -> 32 Hex klein, wie die deviceId in der Mitschrift.
        id = QString::fromLatin1(QUuid::createUuid().toRfc4122().toHex());
        settings.setValue(QStringLiteral("deviceId"), id);
    }
    return id;
}

void NetClient::send(const QString &method, const QJsonObject &envelope,
                     void (NetClient::*handler)(const QByteArray &))
{
    QByteArray ivHex;
    const QByteArray body = davinci::buildBody(envelope, ivHex);
    if (body.isEmpty()) {
        emit failed(QStringLiteral("Verschlüsselung fehlgeschlagen"));
        return;
    }
    QNetworkRequest req = davinci::requestFor(method, ivHex);
    QNetworkReply *r = m_nam.post(req, body);
    connect(r, &QNetworkReply::finished, this, [this, r, ivHex, handler]() {
        const QByteArray data = r->readAll();
        bool ok = false;
        const QByteArray json = davinci::parseResponse(data, ivHex, &ok);
        if (r->error() != QNetworkReply::NoError) {
            // Auch Fehlerantworten sind AES-verschlüsselt: den entschlüsselten Text
            // melden, sonst den HTTP-Fehler. (So wäre ein wiederkehrender
            // force-upgrade-Riegel endlich im Klartext lesbar.)
            emit failed(ok ? QString::fromUtf8(json) : r->errorString());
        } else if (!ok) {
            emit failed(QStringLiteral("Antwort nicht entschlüsselbar"));
        } else {
            (this->*handler)(json);
        }
        r->deleteLater();
    });
}

void NetClient::onUsers(const QByteArray &json)
{
    // Die Response<T>-Nutzlast ist literal "null", wenn kein Konto zum Gerät gehört
    // (bestätigt am Testvektor recon/krypto-testvektor.json).
    if (json.trimmed() == "null") {
        emit usersFound(QVariantList());
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    // OFFEN: die genaue GetUsersByDeviceResponse-Form ist nicht mitgeschnitten (PII).
    // Darum defensiv: Top-Array, Feld "users", oder notfalls das Objekt selbst.
    QVariantList users;
    if (doc.isArray()) {
        users = doc.array().toVariantList();
    } else if (doc.isObject()) {
        const QJsonObject o = doc.object();
        if (o.value("users").isArray())
            users = o.value("users").toArray().toVariantList();
        else
            users.append(o.toVariantMap());
    }
    emit usersFound(users);
}

void NetClient::onLogin(const QByteArray &json)
{
    const QJsonObject o = QJsonDocument::fromJson(json).object();
    // Session/userId defensiv übernehmen — die Response<T>-Verschachtelung ist nicht
    // mitgeschnitten (OFFEN). Beide üblichen Stellen (top-level oder "response") prüfen.
    if (o.contains("session"))
        m_session = o.value("session").toString();
    else if (o.value("response").isObject())
        m_session = o.value("response").toObject().value("session").toString();
    if (o.contains("userId"))
        m_userId = o.value("userId").toVariant().toLongLong();
    emit loggedIn(o.toVariantMap());
}

void NetClient::onCreated(const QByteArray &json)
{
    const QJsonObject o = QJsonDocument::fromJson(json).object();
    // Nach create ist i.d.R. ebenfalls Session/Benutzer-ID dabei; Form OFFEN.
    if (o.contains("session"))
        m_session = o.value("session").toString();
    if (o.contains("userId"))
        m_userId = o.value("userId").toVariant().toLongLong();
    emit created(o.toVariantMap());
}

void NetClient::getUsersByDevice()
{
    // method genau EINMAL als lokale Konstante — dieselbe Zeichenkette fließt in die
    // ticket-Signatur (via baseEnvelope) UND in die URL (via send/requestFor); kein
    // zweites Literal, sonst drifteten Signatur und Endpunkt auseinander.
    const QString method = QStringLiteral("getUsersByDevice");
    QJsonObject env = davinci::baseEnvelope(m_deviceId, method, m_session, m_userId, m_locale);
    // getUsersByDevice hat keine Zusatzfelder (bestätigt an der Mitschrift).
    send(method, env, &NetClient::onUsers);
}

void NetClient::login(const QString &username, const QString &password)
{
    const QString method = QStringLiteral("login");   // ein Literal für Signatur + URL
    QJsonObject env = davinci::baseEnvelope(m_deviceId, method, m_session, m_userId, m_locale);
    env.insert("username", username);
    // Passwort im Klartext: an 6 Login-Flows bestätigt (7-11 ASCII-Zeichen, kein
    // Hash) — die AES-Schicht ist der Transportschutz, nicht ein Passwort-Digest.
    env.insert("password", password);
    // quizDuelLocale == locale in der Mitschrift (z.B. "de_DE").
    env.insert("quizDuelLocale", env.value("locale"));
    send(method, env, &NetClient::onLogin);
}

void NetClient::createAccount(const QString &username, const QString &password,
                              const QString &email)
{
    const QString method = QStringLiteral("create");   // ein Literal für Signatur + URL
    QJsonObject env = davinci::baseEnvelope(m_deviceId, method, m_session, m_userId, m_locale);
    env.insert("username", username);
    env.insert("password", password);
    // E-Mail ist OPTIONAL (nur für Passwort-Reset). Leer -> null schicken, NICHT "" — sonst
    // nimmt der Server die (ggf. schon vergebene) Adresse an und lehnt mit EMAIL_TAKEN ab.
    if (email.trimmed().isEmpty())
        env.insert("email", QJsonValue(QJsonValue::Null));
    else
        env.insert("email", email.trimmed());
    env.insert("quizDuelLocale", env.value("locale"));
    env.insert("avatarCode", QJsonValue(QJsonValue::Null));        // vorhanden (null) in der Mitschrift
    // create IST mitgeschnitten (1 Flow; Server-Antwort 500 EMAIL_TAKEN belegt, dass der
    // Body geparst wurde): Feldset username/password/email/quizDuelLocale/avatarCode. Weitere
    // optionale Pflichtfelder tauchen erst bei anderen Flows auf.
    send(method, env, &NetClient::onCreated);
}

// --- v1: Messaging + Freunde (Formen aus recon/spiel-protokoll.md) ---

void NetClient::getFriends()
{
    const QString method = QStringLiteral("getFriends");   // user/quizduel/getFriends
    QJsonObject env = davinci::baseEnvelope(m_deviceId, method, m_session, m_userId, m_locale);
    env.insert("quizDuelLocale", m_locale);
    send(method, env, &NetClient::onFriends);
}

void NetClient::findUser(const QString &username)
{
    const QString method = QStringLiteral("findUser");   // user/quizduel/findUser (exakter Name)
    QJsonObject env = davinci::baseEnvelope(m_deviceId, method, m_session, m_userId, m_locale);
    env.insert("username", username);
    env.insert("quizDuelLocale", m_locale);
    send(method, env, &NetClient::onUserFound);
}

void NetClient::addFriend(qlonglong friendUserId)
{
    const QString method = QStringLiteral("addFriend");   // user/quizduel/addFriend
    QJsonObject env = davinci::baseEnvelope(m_deviceId, method, m_session, m_userId, m_locale);
    env.insert("friendIdToAdd", QJsonValue(friendUserId));
    env.insert("quizDuelLocale", m_locale);
    send(method, env, &NetClient::onFriendAdded);
}

void NetClient::challengeUser(qlonglong opponentUserId)
{
    // Eigenes Duell gegen einen bestimmten Gegner: createRoom mit invitee + join_game.
    // Spiegelt startMatchmaking (gleiche clientParameters), nur mit festem Gegner statt
    // Matchmaker. Deckt auch Rematch ab (opponentUserId = oppUserId eines Spiels).
    const QString method = QStringLiteral("multiplayer/createRoom");
    QJsonObject env = davinci::baseEnvelope(m_deviceId, method, m_session, m_userId, m_locale);
    env.insert("participantId", QJsonValue(m_userId));
    env.insert("roomTemplateName", QStringLiteral("qd_classic_game_template"));
    QJsonArray invitees; invitees.append(QJsonValue(opponentUserId));
    env.insert("invitees", invitees);
    auto kv = [](const QString &k, const QString &v) {
        QJsonObject o; o.insert("key", k); o.insert("value", v); return o;
    };
    QJsonArray cp;
    cp.append(kv(QStringLiteral("difficultyMin"), QStringLiteral("0")));
    cp.append(kv(QStringLiteral("difficultyMax"), QStringLiteral("100")));
    cp.append(kv(QStringLiteral("locale"), m_locale));
    cp.append(kv(QStringLiteral("chanceToGetImageQuestion"), QStringLiteral("20")));
    cp.append(kv(QStringLiteral("rankingNamespace"), m_locale));
    cp.append(kv(QStringLiteral("poolId"), m_locale));
    cp.append(kv(QStringLiteral("rankingTemplateName"), QStringLiteral("qd_classic_game_elo")));
    QJsonObject iep; iep.insert("clientParameters", cp);
    env.insert("initEngineParameters", iep);
    QJsonObject joinCmd;
    joinCmd.insert("dictionaryPayload", QJsonValue(QJsonValue::Null));
    joinCmd.insert("bytePayload", QJsonValue(QJsonValue::Null));
    joinCmd.insert("command", QStringLiteral("join_game"));
    QJsonArray cmds; cmds.append(joinCmd);
    env.insert("commands", cmds);
    env.insert("matchAlreadyInvited", false);
    send(method, env, &NetClient::onRoomCreated);
}

void NetClient::getChatThreads()
{
    const QString method = QStringLiteral("chatthread/getAllMyChatThreads");
    QJsonObject env = davinci::baseEnvelope(m_deviceId, method, m_session, m_userId, m_locale);
    send(method, env, &NetClient::onThreads);
}

void NetClient::getChatThread(qlonglong threadId)
{
    const QString method = QStringLiteral("chatthread/getChatThread");
    QJsonObject env = davinci::baseEnvelope(m_deviceId, method, m_session, m_userId, m_locale);
    env.insert("chatThreadId", QJsonValue(threadId));
    send(method, env, &NetClient::onThread);
}

void NetClient::sendMessageToThread(qlonglong threadId, const QString &message)
{
    const QString method = QStringLiteral("chatthread/sendMessageToThread");
    QJsonObject env = davinci::baseEnvelope(m_deviceId, method, m_session, m_userId, m_locale);
    env.insert("chatThreadId", QJsonValue(threadId));
    env.insert("message", message);
    env.insert("system", false);
    send(method, env, &NetClient::onThread);   // Antwort = aktualisierter Thread
}

void NetClient::startChatThread(qlonglong recipientUserId, const QString &message)
{
    const QString method = QStringLiteral("chatthread/startNewChatThread");
    QJsonObject env = davinci::baseEnvelope(m_deviceId, method, m_session, m_userId, m_locale);
    QJsonArray recips;
    recips.append(QJsonValue(recipientUserId));
    env.insert("message", message);
    env.insert("recipientUserIds", recips);
    env.insert("tryFindExistingThread", true);   // vorhandene 1:1-Unterhaltung wiederverwenden
    env.insert("system", false);
    send(method, env, &NetClient::onThread);
}

void NetClient::inboxList()
{
    const QString method = QStringLiteral("inbox/list");
    QJsonObject env = davinci::baseEnvelope(m_deviceId, method, m_session, m_userId, m_locale);
    send(method, env, &NetClient::onInbox);
}

// --- v1-Spiel: echtes rooms/matchmaker/engine-Modell (recon/spiel-protokoll-v2.md) ---

QString NetClient::nowClientTime()
{
    // C#-Round-trip-ISO "yyyy-MM-ddTHH:mm:ss.fffffffZ" (7 Nachkommastellen). Qt liefert
    // nur ms (3) -> auf 7 auffüllen (Rest Nullen; die Sub-ms-Präzision ist unkritisch).
    const QDateTime now = QDateTime::currentDateTimeUtc();
    return now.toString(QStringLiteral("yyyy-MM-ddTHH:mm:ss.zzz")) + QStringLiteral("0000Z");
}

void NetClient::loginByDevice()
{
    const QString method = QStringLiteral("loginByDevice");   // user/quizduel/loginByDevice
    QJsonObject env = davinci::baseEnvelope(m_deviceId, method, m_session, m_userId, m_locale);
    env.insert("quizDuelLocale", m_locale);    // in 00_enc vorhanden
    send(method, env, &NetClient::onLogin);     // setzt session/userId + emit loggedIn
}

void NetClient::getQuizzes()
{
    const QString method = QStringLiteral("getQuizzes");
    QJsonObject env = davinci::baseEnvelope(m_deviceId, method, m_session, m_userId, m_locale);
    env.insert("quizDuelLocale", m_locale);
    env.insert("onlyActiveQuizzes", true);
    env.insert("clientTime", nowClientTime());
    send(method, env, &NetClient::onQuizzes);
}

void NetClient::getQuiz(const QString &quizId)
{
    const QString method = QStringLiteral("getQuiz");
    QJsonObject env = davinci::baseEnvelope(m_deviceId, method, m_session, m_userId, m_locale);
    env.insert("quizDuelLocale", m_locale);
    env.insert("quizId", quizId);
    send(method, env, &NetClient::onQuiz);
}

void NetClient::settings()
{
    const QString method = QStringLiteral("game/common/settings");
    QJsonObject env = davinci::baseEnvelope(m_deviceId, method, m_session, m_userId, m_locale);
    env.insert("adConfigId", QJsonValue(QJsonValue::Null));
    env.insert("user", QJsonObject());    // leeres QuizDuelUser-DTO (Newtonsoft füllt Defaults)
    send(method, env, &NetClient::onSettings);
}

void NetClient::startMatchmaking()
{
    const QString method = QStringLiteral("multiplayer/startMatchmaking");
    QJsonObject env = davinci::baseEnvelope(m_deviceId, method, m_session, m_userId, m_locale);
    env.insert("participantId", QJsonValue(m_userId));   // == userId, JSON-Zahl
    env.insert("matchmakerTemplateName", QStringLiteral("qd_classic_game_matchmaking_template"));
    // clientParameters: ALLE value sind STRINGS (auch Zahlen!).
    auto kv = [](const QString &k, const QString &v) {
        QJsonObject o; o.insert("key", k); o.insert("value", v); return o;
    };
    QJsonArray cp;
    cp.append(kv(QStringLiteral("difficultyMin"), QStringLiteral("0")));
    cp.append(kv(QStringLiteral("difficultyMax"), QStringLiteral("100")));
    cp.append(kv(QStringLiteral("locale"), m_locale));
    cp.append(kv(QStringLiteral("chanceToGetImageQuestion"), QStringLiteral("20")));
    cp.append(kv(QStringLiteral("rankingNamespace"), m_locale));
    cp.append(kv(QStringLiteral("poolId"), m_locale));
    cp.append(kv(QStringLiteral("rankingTemplateName"), QStringLiteral("qd_classic_game_elo")));
    QJsonObject joinCmd;
    joinCmd.insert("dictionaryPayload", QJsonValue(QJsonValue::Null));
    joinCmd.insert("bytePayload", QJsonValue(QJsonValue::Null));
    joinCmd.insert("command", QStringLiteral("join_game"));
    QJsonArray cmds; cmds.append(joinCmd);
    QJsonObject mp;
    mp.insert("clientParameters", cp);
    mp.insert("commands", cmds);
    env.insert("matchmakerParameters", mp);
    send(method, env, &NetClient::onMatch);
}

void NetClient::refreshGames(const QVariantList &roomCacheEntries)
{
    const QString method = QStringLiteral("multiplayer/refresh");
    QJsonObject env = davinci::baseEnvelope(m_deviceId, method, m_session, m_userId, m_locale);
    // CacheableRequest-Umschlag: alle null mangels Cache/Clan.
    env.insert("status", QJsonValue(QJsonValue::Null));
    env.insert("cacheKey", QJsonValue(QJsonValue::Null));
    env.insert("cacheTimestamp", QJsonValue(QJsonValue::Null));
    env.insert("clanIds", QJsonValue(QJsonValue::Null));
    // Erstlauf: leere Liste -> Server liefert alle Räume voll.
    env.insert("roomCacheEntries", QJsonArray::fromVariantList(roomCacheEntries));
    send(method, env, &NetClient::onRooms);
}

void NetClient::invokeEngineCommands(const QString &roomId, const QVariantList &commands,
                                     const QVariant &roomCacheEntry)
{
    const QString method = QStringLiteral("multiplayer/invokeEngineCommands");
    QJsonObject env = davinci::baseEnvelope(m_deviceId, method, m_session, m_userId, m_locale);
    // RAUM-LOKALE participantId (nicht m_userId!). Fehlt sie, laut abbrechen statt den
    // "not present in room"-Serverfehler zu reproduzieren — meist hilft Spieleliste neu laden.
    const qint64 pid = m_roomParticipant.value(roomId, 0);
    if (pid == 0) {
        emit failed(QStringLiteral("participantId für Raum %1 nicht gefunden (userId %2). "
                                   "Bitte Spieleliste neu laden.").arg(roomId).arg(m_userId));
        return;
    }
    env.insert("participantId", QJsonValue((qlonglong)pid));
    env.insert("roomId", roomId);
    // dictionaryPayload MUSS ein JSON-STRING sein (doppelt kodiert). Aus Map serialisieren,
    // falls nicht schon String.
    QJsonArray outCmds;
    foreach (const QVariant &cv, commands) {
        const QVariantMap cm = cv.toMap();
        QJsonObject c;
        c.insert("command", cm.value(QStringLiteral("command")).toString());
        const QVariant dp = cm.value(QStringLiteral("dictionaryPayload"));
        if (!dp.isValid() || dp.isNull()) {
            c.insert("dictionaryPayload", QJsonValue(QJsonValue::Null));
        } else if (dp.type() == QVariant::String) {
            c.insert("dictionaryPayload", dp.toString());
        } else {
            const QJsonDocument d = QJsonDocument::fromVariant(dp);
            c.insert("dictionaryPayload", QString::fromUtf8(d.toJson(QJsonDocument::Compact)));
        }
        c.insert("bytePayload", QJsonValue(QJsonValue::Null));
        outCmds.append(c);
    }
    env.insert("commands", outCmds);
    if (!roomCacheEntry.isValid() || roomCacheEntry.isNull())
        env.insert("roomCacheEntry", QJsonValue(QJsonValue::Null));
    else
        env.insert("roomCacheEntry", QJsonValue::fromVariant(roomCacheEntry));
    send(method, env, &NetClient::onRoom);
}

void NetClient::submitRound(const QString &roomId, int roundIndex, int categoryId,
                            const QVariantList &answers, bool selectCategory)
{
    QVariantList cmds;
    // Kategorie wählen nur, wenn sie in dieser Runde noch nicht gewählt ist
    // (Reihenfolge laut 48_enc: zuerst select_category, dann submit_answers).
    if (selectCategory) {
        QVariantMap sel;
        sel.insert(QStringLiteral("command"), QStringLiteral("select_category"));
        QVariantMap selP;
        selP.insert(QStringLiteral("roundIndex"), roundIndex);
        selP.insert(QStringLiteral("categoryId"), categoryId);
        sel.insert(QStringLiteral("dictionaryPayload"), selP);   // wird in invokeEngineCommands stringifiziert
        cmds << sel;
    }
    QVariantMap sub;
    sub.insert(QStringLiteral("command"), QStringLiteral("submit_answers"));
    QVariantMap subP;
    subP.insert(QStringLiteral("roundIndex"), roundIndex);
    subP.insert(QStringLiteral("answers"), answers);   // [{answer,questionRevisionId}]
    sub.insert(QStringLiteral("dictionaryPayload"), subP);
    cmds << sub;

    invokeEngineCommands(roomId, cmds, QVariant());
}

// --- Antwort-Parser ---

// --- Zuglogik in C++ (Portierung von qml/Game.js). Modell: 6 Runden, alternierender
//     Wähler (hat der Gegner gewählt, wähle ich die nächste Runde), aktuelle Runde = die
//     niedrigste, die NICHT beide beantwortet haben (kein Vorgreifen). ---
static QVariantList dvInvocations(const QVariantMap &room)
{
    return room.value(QStringLiteral("gameState")).toMap()
               .value(QStringLiteral("json")).toMap()
               .value(QStringLiteral("commandInvocationStack")).toMap()
               .value(QStringLiteral("invocations")).toList();
}
static qint64 dvWhoSelected(const QVariantList &inv, int r)
{
    foreach (const QVariant &v, inv) {
        const QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("command")).toString() != QLatin1String("select_category")) continue;
        if (m.value(QStringLiteral("dictionaryPayload")).toMap().value(QStringLiteral("roundIndex")).toInt() == r)
            return m.value(QStringLiteral("participantUserId")).toLongLong();
    }
    return 0;
}
static int dvCatSelected(const QVariantList &inv, int r)
{
    foreach (const QVariant &v, inv) {
        const QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("command")).toString() != QLatin1String("select_category")) continue;
        const QVariantMap dp = m.value(QStringLiteral("dictionaryPayload")).toMap();
        if (dp.value(QStringLiteral("roundIndex")).toInt() == r) return dp.value(QStringLiteral("categoryId")).toInt();
    }
    return -1;
}
// equalMe: true -> prüft Einreichung VON mir, false -> von irgendwem ANDEREN
static bool dvSubmitted(const QVariantList &inv, int r, qint64 myId, bool equalMe)
{
    foreach (const QVariant &v, inv) {
        const QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("command")).toString() != QLatin1String("submit_answers")) continue;
        const bool isMe = (m.value(QStringLiteral("participantUserId")).toLongLong() == myId);
        if (equalMe ? !isMe : isMe) continue;
        if (m.value(QStringLiteral("dictionaryPayload")).toMap().value(QStringLiteral("roundIndex")).toInt() == r)
            return true;
    }
    return false;
}
// Wähler von Runde 0 = der ERSTE Beitritt (join_game mit kleinstem Zeitstempel), also der
// Spiel-/Matchmaking-Initiator. Beim de-Konto war das stets ich (darum lief es); beim sv-Spiel
// kann es der Gegner sein → dann warte ich auf DESSEN Runde-0-Wahl, statt selbst zu wählen
// (sonst not_in_turn_to_select_category → Retry ohne Kategorie → no_category_selected).
static qint64 dvFirstChooser(const QVariantList &inv)
{
    qint64 best = 0; qint64 bestT = -1;
    foreach (const QVariant &v, inv) {
        const QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("command")).toString() != QLatin1String("join_game")) continue;
        const qint64 t = m.value(QStringLiteral("utcTimeInMilliSeconds")).toLongLong();
        if (bestT < 0 || t < bestT) { bestT = t; best = m.value(QStringLiteral("participantUserId")).toLongLong(); }
    }
    return best;
}
static bool dvMineToSelect(const QVariantList &inv, int r, qint64 myId)
{
    int r0 = -1; bool c0me = false;
    for (int k = 0; k < 6; ++k) {
        const qint64 w = dvWhoSelected(inv, k);
        if (w != 0) { r0 = k; c0me = (w == myId); break; }
    }
    if (r0 < 0) {
        // Noch keine Kategorie gewählt → Runde 0 wählt der Initiator, danach abwechselnd.
        const qint64 fc = dvFirstChooser(inv);
        const bool meFirst = (fc != 0) ? (fc == myId) : true;   // unbekannt -> wie bisher ich
        return ((r % 2) == 0) ? meFirst : !meFirst;
    }
    return (((r - r0) % 2) == 0) ? c0me : !c0me;
}
static int dvNextPlayableRound(const QVariantList &inv, qint64 myId)
{
    int cur = 6;
    for (int r = 0; r < 6; ++r)
        if (!(dvSubmitted(inv, r, myId, true) && dvSubmitted(inv, r, myId, false))) { cur = r; break; }
    if (cur >= 6) return -1;
    if (dvSubmitted(inv, cur, myId, true)) return -1;      // meine Runde erledigt -> warten
    if (dvCatSelected(inv, cur) >= 0) return cur;          // Kategorie steht -> antworten
    if (dvMineToSelect(inv, cur, myId)) return cur;        // ich wähle + antworte
    return -1;
}
static QVariantList dvParticipants(const QVariantMap &room)
{
    const QVariant p = room.value(QStringLiteral("participants"));
    QVariantList ps;
    if (p.type() == QVariant::List) ps = p.toList();
    else if (p.type() == QVariant::Map) ps.append(p);
    return ps;
}
// MEINE raum-lokale participantId: Teilnehmer mit userId==myId -> dessen participantId.
// Rückfall: Einreichungsstapel (participantUserId==myId -> participantId). 0 = nicht gefunden
// (Aufrufer behandelt das laut als Fehler, statt still m_userId zu schicken und den
// "not present in room"-Fehler zu reproduzieren).
static qint64 dvMyParticipantId(const QVariantMap &room, qint64 myId)
{
    foreach (const QVariant &v, dvParticipants(room)) {
        const QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("userId")).toLongLong() == myId) {
            const qint64 pid = m.value(QStringLiteral("participantId")).toLongLong();
            if (pid) return pid;
        }
    }
    foreach (const QVariant &v, dvInvocations(room)) {
        const QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("participantUserId")).toLongLong() == myId) {
            const qint64 pid = m.value(QStringLiteral("participantId")).toLongLong();
            if (pid) return pid;
        }
    }
    return 0;
}
// Gegner-Teilnehmer (erster mit userId!=myId) als ganze Map (für Name + avatarUrl + userId).
static QVariantMap dvOpponentEntry(const QVariantMap &room, qint64 myId)
{
    foreach (const QVariant &v, dvParticipants(room)) {
        const QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("userId")).toLongLong() != myId) return m;
    }
    return QVariantMap();
}
static QString dvOpponentName(const QVariantMap &opp)
{
    const QString u = opp.value(QStringLiteral("username")).toString();
    if (!u.isEmpty()) return u;
    const QString t = opp.value(QStringLiteral("usernameForTag")).toString();
    return t.isEmpty() ? QStringLiteral("…") : t;
}

QVariantMap NetClient::parseRoom(const QJsonObject &roomObj)
{
    QVariantMap m = roomObj.toVariantMap();
    // gameState.json ist ein JSON-STRING -> einmal extra parsen, den String ersetzen,
    // damit QML direkt room.gameState.json.dictionaryState.rounds[...] sieht.
    const QJsonObject gs = roomObj.value("gameState").toObject();
    const QString gsJson = gs.value("json").toString();
    if (!gsJson.isEmpty()) {
        const QJsonDocument gd = QJsonDocument::fromJson(gsJson.toUtf8());
        if (gd.isObject()) {
            QVariantMap gsm = gs.toVariantMap();
            gsm.insert("json", gd.object().toVariantMap());
            m.insert("gameState", gsm);
        }
    }
    // Zug-Infos FLACH vorberechnen (QML liest nur diese Felder, kein tiefes JS je Raum).
    const QVariantList inv = dvInvocations(m);
    const bool finished = (m.value("roomStatus").toString() == QLatin1String("finished"));
    const int tr = finished ? -2 : dvNextPlayableRound(inv, m_userId);
    m.insert("turnRound", tr);
    m.insert("turnStatus", finished ? QStringLiteral("finished")
                                    : (tr >= 0 ? QStringLiteral("myturn") : QStringLiteral("waiting")));
    m.insert("selCatId", tr >= 0 ? dvCatSelected(inv, tr) : -1);
    // Meine raum-lokale participantId merken (roomId -> pid); invokeEngineCommands liest sie.
    const QString roomId = m.value(QStringLiteral("id")).toString();
    const qint64 myPid = dvMyParticipantId(m, m_userId);
    m.insert("myParticipantId", (qlonglong)myPid);
    if (myPid && !roomId.isEmpty()) m_roomParticipant.insert(roomId, myPid);
    // Gegner: Name + Profilbild (avatarUrl, vom Server gerendert; leer = Standard-Avatar).
    const QVariantMap opp = dvOpponentEntry(m, m_userId);
    m.insert("opponent", dvOpponentName(opp));
    m.insert("oppUserId", opp.value(QStringLiteral("userId")));
    m.insert("oppAvatar", opp.value(QStringLiteral("avatarUrl")).toString());
    // Punktestand: richtige Antworten (answer==0) je Spieler aus den Einreichungen ableiten
    // (der Server echot keine Fragen-Punktzahl). Ergebnis bei beendetem Spiel.
    int myScore = 0, oppScore = 0;
    foreach (const QVariant &v, inv) {
        const QVariantMap im = v.toMap();
        if (im.value(QStringLiteral("command")).toString() != QLatin1String("submit_answers")) continue;
        const bool isMe = (im.value(QStringLiteral("participantUserId")).toLongLong() == m_userId);
        const QVariantList answers = im.value(QStringLiteral("dictionaryPayload")).toMap()
                                       .value(QStringLiteral("answers")).toList();
        foreach (const QVariant &a, answers)
            if (a.toMap().value(QStringLiteral("answer")).toInt() == 0) { if (isMe) ++myScore; else ++oppScore; }
    }
    m.insert("myScore", myScore);
    m.insert("oppScore", oppScore);
    m.insert("outcome", finished ? (myScore > oppScore ? QStringLiteral("won")
                                    : (myScore < oppScore ? QStringLiteral("lost") : QStringLiteral("draw")))
                                 : QString());
    return m;
}

void NetClient::onQuizzes(const QByteArray &json)
{
    const QJsonObject o = QJsonDocument::fromJson(json).object();
    const QJsonValue aq = o.value("apiQuizzes");
    QVariantList out;
    if (aq.isArray())
        out = aq.toArray().toVariantList();
    else if (aq.isObject())
        out.append(aq.toObject().toVariantMap());   // Einzelobjekt -> 1-Element-Liste (§1.5)
    emit quizzesLoaded(out);
}

void NetClient::onQuiz(const QByteArray &json)
{
    const QJsonObject o = QJsonDocument::fromJson(json).object();
    const QJsonValue aq = o.value("apiQuizzes");
    if (aq.isObject())
        emit quizLoaded(aq.toObject().toVariantMap());
    else if (aq.isArray() && !aq.toArray().isEmpty())
        emit quizLoaded(aq.toArray().first().toObject().toVariantMap());
    else
        emit quizLoaded(QVariantMap());
}

void NetClient::onSettings(const QByteArray &json)
{
    // Roh durchreichen; adConfig/products/properties sind JSON-in-String (QML/Helfer parst
    // bei Bedarf). minimalVersionAndroid / *RefreshTime liegen top-level.
    emit settingsLoaded(QJsonDocument::fromJson(json).object().toVariantMap());
}

void NetClient::onMatch(const QByteArray &json)
{
    const QJsonObject o = QJsonDocument::fromJson(json).object();
    QVariantMap m;
    m.insert("matchId", o.value("matchId").toString());
    m.insert("roomId", o.value("roomId").toString());
    m.insert("status", o.value("status").toString());
    const QJsonValue ur = o.value("updatedRooms");   // EIN Objekt trotz Plural (§1.5)
    if (ur.isObject())
        m.insert("room", parseRoom(ur.toObject()));
    emit matchStarted(m);
}

void NetClient::onRooms(const QByteArray &json)
{
    const QJsonObject o = QJsonDocument::fromJson(json).object();
    QVariantList out;
    foreach (const QJsonValue &v, o.value("rooms").toArray())
        out.append(parseRoom(v.toObject()));
    emit roomsUpdated(out);
}

void NetClient::onRoom(const QByteArray &json)
{
    const QJsonObject o = QJsonDocument::fromJson(json).object();
    emit roomUpdated(parseRoom(o.value("room").toObject()));
}

void NetClient::onFriends(const QByteArray &json)
{
    const QJsonObject o = QJsonDocument::fromJson(json).object();
    QVariantList out;
    foreach (const QJsonValue &v, o.value("apiUserList").toArray()) {
        const QJsonObject u = v.toObject();
        QVariantMap m;
        m.insert("userId", u.value("userId").toVariant());
        m.insert("username", u.value("username").toString());
        m.insert("avatarCode", u.value("avatarCode").toString());
        m.insert("rating", u.value("rating").toInt());
        out.append(m);
    }
    emit friendsLoaded(out);
}

QVariantMap NetClient::quizUser(const QJsonObject &u)
{
    QVariantMap m;
    m.insert("userId", u.value("userId").toVariant());
    m.insert("username", u.value("username").toString());
    m.insert("avatarCode", u.value("avatarCode").toString());
    m.insert("rating", u.value("rating").toInt());
    return m;
}

void NetClient::onUserFound(const QByteArray &json)
{
    // QuizDuelUserResponse { QuizDuelUser User }. Groß-/Kleinschreibung des Felds
    // defensiv prüfen (User/user/apiUser), sonst das Objekt selbst. Nichts -> leere Map.
    if (json.trimmed() == "null") { emit userFound(QVariantMap()); return; }
    const QJsonObject o = QJsonDocument::fromJson(json).object();
    QJsonObject u;
    if (o.value("User").isObject())       u = o.value("User").toObject();
    else if (o.value("user").isObject())  u = o.value("user").toObject();
    else if (o.value("apiUser").isObject()) u = o.value("apiUser").toObject();
    else if (o.contains("userId"))        u = o;
    if (u.isEmpty() || u.value("userId").toVariant().toLongLong() == 0)
        emit userFound(QVariantMap());
    else
        emit userFound(quizUser(u));
}

void NetClient::onFriendAdded(const QByteArray &json)
{
    const QJsonObject o = QJsonDocument::fromJson(json).object();
    QJsonObject u;
    if (o.value("User").isObject())      u = o.value("User").toObject();
    else if (o.value("user").isObject()) u = o.value("user").toObject();
    else                                 u = o;
    emit friendAdded(quizUser(u));
}

void NetClient::onRoomCreated(const QByteArray &json)
{
    // MultiplayerRoomResponse { MultiplayerRoom room }. Wie ein neues Match behandeln:
    // Raum parsen, matchStarted auslösen -> die Oberfläche öffnet das Duell.
    const QJsonObject o = QJsonDocument::fromJson(json).object();
    QJsonObject roomObj;
    if (o.value("room").isObject())       roomObj = o.value("room").toObject();
    else if (o.value("updatedRooms").isObject()) roomObj = o.value("updatedRooms").toObject();
    QVariantMap m;
    m.insert("roomId", roomObj.value("id").toString());
    if (!roomObj.isEmpty())
        m.insert("room", parseRoom(roomObj));
    emit matchStarted(m);
}

void NetClient::onThreads(const QByteArray &json)
{
    const QJsonObject o = QJsonDocument::fromJson(json).object();
    QVariantList out;
    foreach (const QJsonValue &v, o.value("chatThreadsStripped").toArray()) {
        const QJsonObject t = v.toObject();
        QVariantMap m;
        m.insert("chatThreadId", t.value("chatThreadId").toVariant());
        m.insert("members", t.value("threadMemberUserIds").toArray().toVariantList());
        m.insert("lastUpdated", t.value("lastUpdated").toVariant());
        out.append(m);
    }
    emit threadsLoaded(out);
}

void NetClient::onThread(const QByteArray &json)
{
    const QJsonObject o = QJsonDocument::fromJson(json).object();
    QVariantMap thread;
    thread.insert("chatThreadId", o.value("chatThreadId").toVariant());
    thread.insert("members", o.value("members").toArray().toVariantList());
    QVariantList msgs;
    foreach (const QJsonValue &v, o.value("messages").toArray()) {
        const QJsonObject mm = v.toObject();
        QVariantMap m;
        m.insert("message", mm.value("message").toString());
        m.insert("senderUserId", mm.value("senderUserId").toVariant());
        m.insert("timestamp", mm.value("timestamp").toVariant());
        msgs.append(m);
    }
    thread.insert("messages", msgs);
    emit threadLoaded(thread);
}

void NetClient::onInbox(const QByteArray &json)
{
    const QJsonObject o = QJsonDocument::fromJson(json).object();
    QVariantList out;
    foreach (const QJsonValue &v, o.value("messages").toArray()) {
        const QJsonObject im = v.toObject();
        const QJsonObject body = im.value("body").toObject();
        QVariantMap m;
        m.insert("messageId", im.value("messageId").toString());
        m.insert("subject", body.value("subject").toString());
        m.insert("message", body.value("message").toString());
        m.insert("senderId", im.value("sender").toObject().value("id").toVariant());
        m.insert("sentAtUtcMs", im.value("sentAtUtcMs").toVariant());
        m.insert("read", im.value("readAtUtcMs").toVariant().toLongLong() > 0);
        out.append(m);
    }
    emit inboxLoaded(out);
}
