#include "netbridge.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QSettings>
#include <QStringList>
#include <QUuid>

// Minimale Feldextraktion aus der Holer-Nutzlast (Qt 4 hat kein QJsonDocument; die
// QML-Seiten parsen den Rest mit JSON.parse — der C++-Teil braucht nur session/userId
// nach dem Login, um Folge-Requests zu authentifizieren).
static QString jsonStr(const QString &s, const QString &key)
{
    const QString pat = QLatin1String("\"") + key + QLatin1String("\":\"");
    const int i = s.indexOf(pat);
    if (i < 0) return QString();
    const int a = i + pat.length();
    const int b = s.indexOf(QLatin1Char('"'), a);
    return (b > a) ? s.mid(a, b - a) : QString();
}
static qlonglong jsonNum(const QString &s, const QString &key)
{
    const QString pat = QLatin1String("\"") + key + QLatin1String("\":");
    const int i = s.indexOf(pat);
    if (i < 0) return 0;
    QString num;
    for (int j = i + pat.length(); j < s.length(); ++j) {
        const QChar c = s.at(j);
        if ((c >= QLatin1Char('0') && c <= QLatin1Char('9')) || c == QLatin1Char('-')) num += c;
        else if (num.length()) break;
        // Zahl kann als STRING kommen ("userId":"123") -> führende Leerzeichen UND
        // Anführungszeichen überspringen, sonst bleibt userId 0 -> Server: "invalid ticket".
        else if (c == QLatin1Char(' ') || c == QLatin1Char('"')) continue;
        else break;
    }
    return num.toLongLong();
}

NetBridge::NetBridge(QObject *parent)
    : QObject(parent), m_userId(0), m_proc(0), m_which(0)
{
    QSettings s(QLatin1String("harbour-quizduel"), QLatin1String("harbour-quizduel"));
    m_deviceId = s.value(QLatin1String("deviceId")).toString();
    if (m_deviceId.length() != 32) {
        // Qt 4.7 hat kein QUuid::toRfc4122(); 32 Hex aus toString() ("{8-4-4-4-12}").
        QString u = QUuid::createUuid().toString();
        u.remove(QLatin1Char('{'));
        u.remove(QLatin1Char('}'));
        u.remove(QLatin1Char('-'));
        m_deviceId = u;
        s.setValue(QLatin1String("deviceId"), m_deviceId);
    }
    m_locale = s.value(QLatin1String("locale")).toString();
    if (m_locale.isEmpty()) {
        m_locale = QLocale::system().name();
        if (!m_locale.contains(QLatin1Char('_')))
            m_locale = QLatin1String("de_DE");
    }
}

void NetBridge::setLocale(const QString &locale)
{
    if (locale.isEmpty() || locale == m_locale)
        return;
    m_locale = locale;
    QSettings s(QLatin1String("harbour-quizduel"), QLatin1String("harbour-quizduel"));
    s.setValue(QLatin1String("locale"), m_locale);
    emit localeChanged();
}

QString NetBridge::holerPath() const
{
    const QString installed = QLatin1String("/opt/harbour-quizduel/bin/quizduel-holer");
    if (QFileInfo(installed).exists())
        return installed;
    return QCoreApplication::applicationDirPath() + QLatin1String("/quizduel-holer");
}

QString NetBridge::cacheDir() const
{
    // Beschreibbarer Ordner für die vom Holer angereicherten Raumdateien + geholte Bilder.
    const QString d = QDir::tempPath() + QLatin1String("/harbour-quizduel-rooms");
    QDir().mkpath(d);
    return d;
}

QString NetBridge::jsonEscape(const QString &s)
{
    QString o;
    o.reserve(s.size() + 8);
    for (int i = 0; i < s.size(); ++i) {
        const QChar c = s.at(i);
        const ushort u = c.unicode();
        if (c == QLatin1Char('\\') || c == QLatin1Char('"')) {
            o += QLatin1Char('\\');
            o += c;
        } else if (u < 0x20) {
            o += QString::fromLatin1("\\u%1").arg(u, 4, 16, QLatin1Char('0'));
        } else {
            o += c;
        }
    }
    return o;
}

void NetBridge::run(const QString &method, const QString &fieldsJson, int which)
{
    const bool busy = (m_proc && m_proc->state() != QProcess::NotRunning);
    if (busy) {
        emit failed(QString::fromUtf8("Bitte warten…"));
        return;
    }
    m_which = which;
    // Kleine Request-Beschreibung; den vollen Umschlag + Krypto baut der Holer. cacheDir
    // (top-level, NICHT in fields -> fließt nicht in den Server-Umschlag) sagt dem Holer,
    // wohin er die angereicherten Raumdateien schreibt.
    QString req = QLatin1String("{\"method\":\"") + method
        + QLatin1String("\",\"deviceId\":\"") + m_deviceId
        + QLatin1String("\",\"session\":\"") + jsonEscape(m_session)
        + QLatin1String("\",\"userId\":") + QString::number(m_userId)
        + QLatin1String(",\"locale\":\"") + m_locale
        + QLatin1String("\",\"cacheDir\":\"") + jsonEscape(cacheDir())
        + QLatin1String("\",\"fields\":") + fieldsJson + QLatin1String("}");

    // IMMER ein FRISCHER QProcess. Sonst würde z.B. die Auto-Aktualisierung der HomePage,
    // die SYNCHRON im finished-Slot des Login-Prozesses läuft (loggedIn -> push -> onCompleted
    // -> refreshGames), denselben m_proc aus seinem eigenen finished-Signal neu starten —
    // Reentranz, die die Antwort verschluckt (-> „keine aktiven Spiele").
    QProcess *p = new QProcess(this);
    m_proc = p;
    connect(p, SIGNAL(finished(int, QProcess::ExitStatus)),
            this, SLOT(onFinished(int, QProcess::ExitStatus)));
    connect(p, SIGNAL(error(QProcess::ProcessError)),
            this, SLOT(onError(QProcess::ProcessError)));
    p->start(holerPath());
    if (!p->waitForStarted(5000)) {
        emit failed(QString::fromUtf8("Holer startet nicht: ") + holerPath());
        if (p == m_proc) m_proc = 0;
        p->deleteLater();
        return;
    }
    p->write(req.toUtf8());
    p->closeWriteChannel();
}

void NetBridge::onError(QProcess::ProcessError)
{
    QProcess *p = qobject_cast<QProcess *>(sender());
    emit failed(QString::fromUtf8("Holer-Fehler: ") + (p ? p->errorString() : QString()));
}

void NetBridge::onFinished(int, QProcess::ExitStatus)
{
    QProcess *p = qobject_cast<QProcess *>(sender());
    if (!p) return;
    const QString s = QString::fromUtf8(p->readAllStandardOutput()).trimmed();
    // Prozess freigeben, BEVOR wir Signale auslösen (die synchron neue Requests starten).
    if (p == m_proc) m_proc = 0;
    p->deleteLater();
    if (s.isEmpty()) {
        emit failed(QString::fromUtf8("Keine Antwort vom Holer"));
        return;
    }
    // Der Holer liefert entweder {"error":"..."} oder {"status":N,"payload":...}.
    if (!s.contains(QLatin1String("\"status\"")) && s.contains(QLatin1String("\"error\""))) {
        QString msg = s;
        const int i = s.indexOf(QLatin1String("\"error\":\""));
        if (i >= 0) {
            const int a = i + 9;
            const int b = s.indexOf(QLatin1Char('"'), a);
            if (b > a) msg = s.mid(a, b - a);
        }
        emit failed(msg);
        return;
    }
    switch (m_which) {
    case 1:
    case 2: {
        // Session/userId aus der Nutzlast übernehmen (für Folge-Requests).
        const QString sess = jsonStr(s, QLatin1String("session"));
        if (!sess.isEmpty()) m_session = sess;
        const qlonglong uid = jsonNum(s, QLatin1String("userId"));
        if (uid) m_userId = uid;
        if (m_which == 1) emit loggedIn(s); else emit created(s);
        break;
    }
    case 3: emit friendsLoaded(s); break;
    case 4: emit threadsLoaded(s); break;
    case 5: emit threadLoaded(s); break;
    case 6: emit inboxLoaded(s); break;
    case 7: emit gamesLoaded(s); break;
    case 8: emit questionsLoaded(s); break;
    case 9: emit matchStarted(s); break;
    case 10: emit roomsUpdated(s); break;
    case 11: emit roomUpdated(s); break;
    case 12: emit userFound(s); break;
    case 13: emit friendAdded(s); break;
    default: emit usersFound(s); break;
    }
}

void NetBridge::onImageFinished(int, QProcess::ExitStatus)
{
    QProcess *p = qobject_cast<QProcess *>(sender());
    if (!p) return;
    const QString url = p->property("url").toString();
    const QString o = QString::fromUtf8(p->readAllStandardOutput());
    p->deleteLater();
    // TATSÄCHLICHEN Pfad aus {"status":..,"path":"<pfad>"} lesen (der Holer hängt die
    // erkannte Endung an, darum nicht den vorab geratenen Pfad verwenden).
    QString path;
    const int i = o.indexOf(QLatin1String("\"path\":\""));
    if (i >= 0) {
        const int a = i + 8;
        const int b = o.indexOf(QLatin1Char('"'), a);
        if (b > a) path = o.mid(a, b - a);
    }
    const bool ok = !path.isEmpty() && QFileInfo(path).exists() && QFileInfo(path).size() > 0;
    emit imageFetched(url, ok ? path : QString());
}

void NetBridge::getUsersByDevice()
{
    run(QLatin1String("getUsersByDevice"), QLatin1String("{}"), 0);
}

void NetBridge::login(const QString &username, const QString &password)
{
    const QString f = QLatin1String("{\"username\":\"") + jsonEscape(username)
        + QLatin1String("\",\"password\":\"") + jsonEscape(password)
        + QLatin1String("\",\"quizDuelLocale\":\"") + m_locale + QLatin1String("\"}");
    run(QLatin1String("login"), f, 1);
}

void NetBridge::createAccount(const QString &username, const QString &password,
                              const QString &email)
{
    // E-Mail ist OPTIONAL (nur Passwort-Reset). Leer -> null, NICHT "" — sonst nimmt der
    // Server die (ggf. schon vergebene) Adresse und lehnt mit EMAIL_TAKEN ab.
    const QString emailField = email.trimmed().isEmpty()
        ? QLatin1String("null")
        : (QLatin1String("\"") + jsonEscape(email.trimmed()) + QLatin1String("\""));
    const QString f = QLatin1String("{\"username\":\"") + jsonEscape(username)
        + QLatin1String("\",\"password\":\"") + jsonEscape(password)
        + QLatin1String("\",\"email\":") + emailField
        + QLatin1String(",\"quizDuelLocale\":\"") + m_locale
        + QLatin1String("\",\"avatarCode\":null}");
    run(QLatin1String("create"), f, 2);
}

// --- v1: Messaging + Freunde (Formen aus recon/spiel-protokoll.md) ---

void NetBridge::getFriends()
{
    const QString f = QLatin1String("{\"quizDuelLocale\":\"") + m_locale + QLatin1String("\"}");
    run(QLatin1String("getFriends"), f, 3);
}

void NetBridge::getChatThreads()
{
    run(QLatin1String("chatthread/getAllMyChatThreads"), QLatin1String("{}"), 4);
}

void NetBridge::getChatThread(qlonglong threadId)
{
    const QString f = QLatin1String("{\"chatThreadId\":") + QString::number(threadId) + QLatin1String("}");
    run(QLatin1String("chatthread/getChatThread"), f, 5);
}

void NetBridge::sendMessageToThread(qlonglong threadId, const QString &message)
{
    const QString f = QLatin1String("{\"chatThreadId\":") + QString::number(threadId)
        + QLatin1String(",\"message\":\"") + jsonEscape(message) + QLatin1String("\",\"system\":false}");
    run(QLatin1String("chatthread/sendMessageToThread"), f, 5);
}

void NetBridge::startChatThread(qlonglong recipientUserId, const QString &message)
{
    const QString f = QLatin1String("{\"message\":\"") + jsonEscape(message)
        + QLatin1String("\",\"recipientUserIds\":[") + QString::number(recipientUserId)
        + QLatin1String("],\"tryFindExistingThread\":true,\"system\":false}");
    run(QLatin1String("chatthread/startNewChatThread"), f, 5);
}

void NetBridge::inboxList()
{
    run(QLatin1String("inbox/list"), QLatin1String("{}"), 6);
}

void NetBridge::gameList()
{
    // OFFEN: Sammlungsname laufzeitaufgelöst -> "default" vermutet (B bestätigt).
    run(QLatin1String("boards/by-collection"),
        QLatin1String("{\"boardCollectionName\":\"default\"}"), 7);
}

void NetBridge::fetchQuestions(int categoryId, int count)
{
    const int n = count > 0 ? count : 5;
    const QString f = QLatin1String("{\"questionSpecs\":[{\"locale\":\"") + m_locale
        + QLatin1String("\",\"categoryId\":") + QString::number(categoryId)
        + QLatin1String(",\"questionType\":\"TEXT\",\"questionCount\":") + QString::number(n)
        + QLatin1String(",\"questionLevel\":1}]}");   // OFFEN: DifficultyLevel vermutet
    run(QLatin1String("questions/difficulty"), f, 8);
}

// --- v1-Spiel: rooms + matchmaker + engine (spiegelt src/netclient.cpp) ---

void NetBridge::loginByDevice()
{
    const QString f = QLatin1String("{\"quizDuelLocale\":\"") + m_locale + QLatin1String("\"}");
    run(QLatin1String("loginByDevice"), f, 1);   // setzt session/userId, emit loggedIn
}

// clientParameters für Matchmaking/createRoom — ALLE value sind STRINGS (auch Zahlen).
static QString clientParamsJson(const QString &locale)
{
    return QLatin1String("[")
        + QLatin1String("{\"key\":\"difficultyMin\",\"value\":\"0\"},")
        + QLatin1String("{\"key\":\"difficultyMax\",\"value\":\"100\"},")
        + QLatin1String("{\"key\":\"locale\",\"value\":\"") + locale + QLatin1String("\"},")
        + QLatin1String("{\"key\":\"chanceToGetImageQuestion\",\"value\":\"20\"},")
        + QLatin1String("{\"key\":\"rankingNamespace\",\"value\":\"") + locale + QLatin1String("\"},")
        + QLatin1String("{\"key\":\"poolId\",\"value\":\"") + locale + QLatin1String("\"},")
        + QLatin1String("{\"key\":\"rankingTemplateName\",\"value\":\"qd_classic_game_elo\"}]");
}
static QString joinGameCmdsJson()
{
    return QLatin1String("[{\"dictionaryPayload\":null,\"bytePayload\":null,\"command\":\"join_game\"}]");
}

void NetBridge::refreshGames()
{
    const QString f = QLatin1String("{\"status\":null,\"cacheKey\":null,\"cacheTimestamp\":null,"
                                    "\"clanIds\":null,\"roomCacheEntries\":[]}");
    run(QLatin1String("multiplayer/refresh"), f, 10);
}

void NetBridge::openRoom(const QString &roomId)
{
    // Vollen Raum DIREKT aus der vom Holer geschriebenen Cache-Datei lesen (kein Holer,
    // kein JSON-Parsen in Qt 4) und wie die anderen Antworten als {payload:{room:…}} liefern.
    QFile f(cacheDir() + QLatin1Char('/') + roomId + QLatin1String(".json"));
    if (!f.open(QIODevice::ReadOnly)) {
        emit failed(QString::fromUtf8("Raum nicht im Zwischenspeicher — bitte Liste neu laden"));
        return;
    }
    const QByteArray data = f.readAll();
    f.close();
    emit roomOpened(QLatin1String("{\"status\":200,\"payload\":{\"room\":")
                    + QString::fromUtf8(data) + QLatin1String("}}"));
}

void NetBridge::startMatchmaking()
{
    const QString f = QLatin1String("{\"participantId\":") + QString::number(m_userId)
        + QLatin1String(",\"matchmakerTemplateName\":\"qd_classic_game_matchmaking_template\","
                        "\"matchmakerParameters\":{\"clientParameters\":") + clientParamsJson(m_locale)
        + QLatin1String(",\"commands\":") + joinGameCmdsJson() + QLatin1String("}}");
    run(QLatin1String("multiplayer/startMatchmaking"), f, 9);
}

void NetBridge::challengeUser(qlonglong opponentUserId)
{
    const QString f = QLatin1String("{\"participantId\":") + QString::number(m_userId)
        + QLatin1String(",\"roomTemplateName\":\"qd_classic_game_template\",\"invitees\":[")
        + QString::number(opponentUserId)
        + QLatin1String("],\"initEngineParameters\":{\"clientParameters\":") + clientParamsJson(m_locale)
        + QLatin1String("},\"commands\":") + joinGameCmdsJson()
        + QLatin1String(",\"matchAlreadyInvited\":false}");
    run(QLatin1String("multiplayer/createRoom"), f, 9);
}

void NetBridge::submitRound(const QString &roomId, qlonglong participantId, int roundIndex,
                            int categoryId, const QString &answersJson, bool selectCategory)
{
    // participantId ist RAUM-LOKAL (room.myParticipantId). 0 -> laut abbrechen.
    if (participantId == 0) {
        emit failed(QString::fromUtf8("participantId fehlt (bitte Spieleliste neu laden)"));
        return;
    }
    // dictionaryPayload MUSS ein JSON-STRING sein (doppelt kodiert).
    const QString sel = QLatin1String("{\"roundIndex\":") + QString::number(roundIndex)
        + QLatin1String(",\"categoryId\":") + QString::number(categoryId) + QLatin1String("}");
    const QString sub = QLatin1String("{\"roundIndex\":") + QString::number(roundIndex)
        + QLatin1String(",\"answers\":") + answersJson + QLatin1String("}");
    QString cmds = QLatin1String("[");
    if (selectCategory)
        cmds += QLatin1String("{\"command\":\"select_category\",\"dictionaryPayload\":\"")
             + jsonEscape(sel) + QLatin1String("\",\"bytePayload\":null},");
    cmds += QLatin1String("{\"command\":\"submit_answers\",\"dictionaryPayload\":\"")
         + jsonEscape(sub) + QLatin1String("\",\"bytePayload\":null}]");
    const QString f = QLatin1String("{\"participantId\":") + QString::number(participantId)
        + QLatin1String(",\"roomId\":\"") + jsonEscape(roomId) + QLatin1String("\",\"commands\":")
        + cmds + QLatin1String(",\"roomCacheEntry\":null}");
    run(QLatin1String("multiplayer/invokeEngineCommands"), f, 11);
}

void NetBridge::findUser(const QString &username)
{
    const QString f = QLatin1String("{\"username\":\"") + jsonEscape(username)
        + QLatin1String("\",\"quizDuelLocale\":\"") + m_locale + QLatin1String("\"}");
    run(QLatin1String("findUser"), f, 12);
}

void NetBridge::addFriend(qlonglong friendUserId)
{
    const QString f = QLatin1String("{\"friendIdToAdd\":") + QString::number(friendUserId)
        + QLatin1String(",\"quizDuelLocale\":\"") + m_locale + QLatin1String("\"}");
    run(QLatin1String("addFriend"), f, 13);
}

void NetBridge::fetchImage(const QString &url)
{
    if (url.isEmpty()) { emit imageFetched(url, QString()); return; }
    // EINDEUTIGER Dateiname aus dem letzten URL-Segment (das ist schon ein langer Hash).
    // qHash (32 Bit) kollidiert sonst und QtQuick 1.1 cacht Bilder PRO URL -> bei gleichem
    // Dateinamen würde das falsche (alte) Bild gezeigt.
    QString seg = url.section(QLatin1Char('/'), -1);
    QString safe;
    for (int i = 0; i < seg.length(); ++i) {
        const QChar c = seg.at(i);
        if (c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('-')) safe += c;
    }
    if (safe.isEmpty()) safe = QString::number(qHash(url), 16);
    const QString outBase = cacheDir() + QLatin1String("/img_") + safe;
    QProcess *p = new QProcess(this);
    p->setProperty("url", url);
    connect(p, SIGNAL(finished(int, QProcess::ExitStatus)),
            this, SLOT(onImageFinished(int, QProcess::ExitStatus)));
    QStringList args;
    args << QLatin1String("--fetch") << url << outBase;
    p->start(holerPath(), args);
    if (!p->waitForStarted(5000)) { emit imageFetched(url, QString()); p->deleteLater(); }
}
