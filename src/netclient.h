// Netzclient gegen das magbackend (davinci/spitfire). Baut den Request-Umschlag
// über das davinci-Modul (AES-256-CBC/PKCS7 über gzip(JSON), Schlüssel =
// ServerSharedKeyV2; siehe davinciprotocol.h und BEFUND.md "★ KRYPTO GELÖST").
//
// Der frühere "force upgrade"-Riegel ist damit gelöst: er war die fehlende
// davinci-Verschlüsselung, kein erratbares Versionssignal. Echter Pfad ist der
// prod-Zweig /user/quizduel/<methode> (applicationId:0 + applicationKey
// identifizieren die App) — nicht mehr Staging/autoRegister.
#ifndef NETCLIENT_H
#define NETCLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QHash>

class QNetworkReply;
class QJsonObject;

class NetClient : public QObject
{
    Q_OBJECT
    // Reaktiv: Bindings auf net.locale (z.B. die übersetzten Beschriftungen)
    // werten bei Sprachwechsel neu aus.
    Q_PROPERTY(QString locale READ locale WRITE setLocale NOTIFY localeChanged)
public:
    explicit NetClient(QObject *parent = 0);

    // Basis-URL steckt in davinci::kBaseUrl (prod /user/quizduel/).
    static constexpr const char *kGameId = "nqd"; // games/nqd/ im CDN

    // Stabile, pro Installation einmalig erzeugte Geräte-Kennung (32 Hex).
    Q_INVOKABLE QString deviceId() const { return m_deviceId; }

    // Gewählte Sprache/Markt im QDLanguageHelper-Format "xx_YY" (z.B. "sv_SE").
    // Fließt in den Umschlag (locale) UND in quizDuelLocale bei login/create. Bleibt
    // über QSettings erhalten; ein schwedisches Konto meldet sich mit "sv_SE" an.
    Q_INVOKABLE QString locale() const { return m_locale; }
    Q_INVOKABLE void setLocale(const QString &locale);

    // Angemeldete Benutzer-ID (0 vor dem Login) — für „meine vs. fremde Nachricht".
    Q_INVOKABLE qlonglong userId() const { return m_userId; }

    // Echte Endpunkte (methoden am prod-Zweig /user/quizduel/).
    Q_INVOKABLE void getUsersByDevice();                       // Konten zu diesem Gerät
    Q_INVOKABLE void login(const QString &username, const QString &password);
    Q_INVOKABLE void createAccount(const QString &username, const QString &password,
                                   const QString &email);

    // --- v1: Messaging + Freunde. Alle requiresLogin -> m_session muss gesetzt sein
    // (nach login/create). chatthread/* + inbox/* liegen unter /api/, nicht
    // /api/user/quizduel/ (davinci::requestFor erkennt das am '/').
    Q_INVOKABLE void getFriends();                      // user/quizduel/getFriends
    // Freund suchen (exakter Name inkl. #-Kennung) / hinzufügen / herausfordern.
    Q_INVOKABLE void findUser(const QString &username);     // user/quizduel/findUser -> userFound
    Q_INVOKABLE void addFriend(qlonglong friendUserId);     // user/quizduel/addFriend -> friendAdded
    // Freund/Gegner zu einem neuen Duell herausfordern (createRoom mit invitee). Das
    // beantwortet auch „nochmal spielen" (Rematch) mit der oppUserId eines beendeten Spiels.
    Q_INVOKABLE void challengeUser(qlonglong opponentUserId); // multiplayer/createRoom -> matchStarted
    Q_INVOKABLE void getChatThreads();                  // chatthread/getAllMyChatThreads
    Q_INVOKABLE void getChatThread(qlonglong threadId); // chatthread/getChatThread
    Q_INVOKABLE void sendMessageToThread(qlonglong threadId, const QString &message);
    Q_INVOKABLE void startChatThread(qlonglong recipientUserId, const QString &message);
    Q_INVOKABLE void inboxList();                       // inbox/list

    // --- v1-Spiel: ECHTES Modell (rooms + matchmaker + engine). Das alte boards/
    // by-collection + questions/difficulty ist ABGELÖST (bestätigt am Mitschnitt,
    // recon/spiel-protokoll-v2.md). Pfade verbatim aus den [API("...")]-Attributen.
    Q_INVOKABLE void loginByDevice();                    // user/quizduel/loginByDevice (Startup-Re-Auth)
    Q_INVOKABLE void getQuizzes();                       // user/quizduel/getQuizzes (Spezial-/Monatsquiz)
    Q_INVOKABLE void getQuiz(const QString &quizId);     // user/quizduel/getQuiz
    Q_INVOKABLE void settings();                         // game/common/settings (Config + Kategorienamen)
    Q_INVOKABLE void startMatchmaking();                 // multiplayer/startMatchmaking (neues Duell, join_game)
    Q_INVOKABLE void refreshGames(const QVariantList &roomCacheEntries); // multiplayer/refresh (aktive Spiele)
    Q_INVOKABLE void invokeEngineCommands(const QString &roomId, const QVariantList &commands,
                                          const QVariant &roomCacheEntry); // multiplayer/invokeEngineCommands
    // Kompletter Rundenzug. answers = Liste von {answer:0..3 (kanonisch), questionRevisionId:<int>}.
    // selectCategory=true -> select_category + submit_answers (ich wähle die Kategorie);
    // false -> NUR submit_answers (Kategorie war in dieser Runde schon gewählt, sonst lehnt der
    // Server mit not_in_turn_to_select_category ab). dictionaryPayload wird als JSON-STRING gesetzt.
    Q_INVOKABLE void submitRound(const QString &roomId, int roundIndex, int categoryId,
                                 const QVariantList &answers, bool selectCategory = true);

signals:
    void usersFound(const QVariantList &users);  // getUsersByDevice-Ergebnis (leer = kein Konto)
    void loggedIn(const QVariantMap &response);   // login-Ergebnis (voller Antwort-Umschlag)
    void created(const QVariantMap &response);    // create-Ergebnis
    void failed(const QString &message);
    void localeChanged();
    void friendsLoaded(const QVariantList &friends);   // {userId,username,avatarCode,rating}
    void userFound(const QVariantMap &user);           // findUser -> {userId,username,avatarCode,rating} (leer = nichts)
    void friendAdded(const QVariantMap &user);         // addFriend ok
    void threadsLoaded(const QVariantList &threads);   // {chatThreadId,members,lastUpdated}
    void threadLoaded(const QVariantMap &thread);      // {chatThreadId,members,messages[]}
    void inboxLoaded(const QVariantList &messages);    // {messageId,subject,message,senderId,...}
    void quizzesLoaded(const QVariantList &quizzes);   // getQuizzes -> apiQuizzes[]
    void quizLoaded(const QVariantMap &quiz);          // getQuiz -> apiQuizzes (1 Objekt)
    void settingsLoaded(const QVariantMap &settings);  // game/common/settings (refreshTimes, minimalVersion, Kategorienamen)
    void matchStarted(const QVariantMap &match);       // startMatchmaking -> {matchId,roomId,room}
    void roomsUpdated(const QVariantList &rooms);       // multiplayer/refresh -> rooms[]
    void roomUpdated(const QVariantMap &room);          // invokeEngineCommands -> room (gameState schon geparst)

private:
    // Schickt envelope an <methode> und ruft handler(klartextJson) bei Erfolg.
    void send(const QString &method, const QJsonObject &envelope,
              void (NetClient::*handler)(const QByteArray &));
    void onUsers(const QByteArray &json);
    void onLogin(const QByteArray &json);
    void onCreated(const QByteArray &json);
    void onFriends(const QByteArray &json);
    void onUserFound(const QByteArray &json);
    void onFriendAdded(const QByteArray &json);
    void onRoomCreated(const QByteArray &json);
    // QuizDuelUser-DTO (userId/username/avatarCode/rating) aus einem JSON-Objekt in eine
    // flache QVariantMap ziehen — geteilt von onUserFound/onFriendAdded.
    static QVariantMap quizUser(const QJsonObject &u);
    void onThreads(const QByteArray &json);
    void onThread(const QByteArray &json);
    void onInbox(const QByteArray &json);
    void onQuizzes(const QByteArray &json);
    void onQuiz(const QByteArray &json);
    void onSettings(const QByteArray &json);
    void onMatch(const QByteArray &json);
    void onRooms(const QByteArray &json);
    void onRoom(const QByteArray &json);
    // gameState.json ist ein JSON-STRING im room -> einmal extra parsen; liefert die
    // verschachtelte Map inkl. dictionaryState.rounds (Kategorien+Fragen) für QML/RoomCache.
    // Parst den Raum (gameState.json doppelt) UND rechnet die Zug-Infos in C++ vor
    // (opponent/turnStatus/turnRound/selCatId als FLACHE Felder) — sonst müsste QML-JS
    // 18 große Raum-Maps durchkauen, was den V4-Stack sprengt.
    QVariantMap parseRoom(const QJsonObject &roomObj);
    static QString nowClientTime();   // C#-Round-trip-ISO, 7 Nachkommastellen + 'Z'

    QString loadOrCreateDeviceId();

    QNetworkAccessManager m_nam;
    QString m_deviceId;
    QString m_locale;    // gewählte Sprache "xx_YY" (persistiert)
    QString m_session;   // leer (= JSON null) bis zum Login
    qint64  m_userId;    // 0 bis zum Login
    // roomId -> MEINE participantId in diesem Raum. WICHTIG: participantId ist RAUM-LOKAL
    // und NICHT gleich der userId (nur beim de-Konto zufällig gleich; beim sv-Konto weicht
    // sie ab -> "participantId ... not present in room"). invokeEngineCommands MUSS diese
    // raum-lokale Kennung schicken, nicht m_userId. Gefüllt in parseRoom.
    QHash<QString, qint64> m_roomParticipant;
};

#endif
