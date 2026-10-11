// Brücke QML <-> Rust-Holer für die MeeGo-Ausgabe. Qt 4.7 kann das magbackend
// (TLS 1.2+) nicht selbst erreichen; darum läuft der ganze davinci-Verkehr im
// statischen rustls-Helfer meego/fetch (quizduel-holer). Diese Klasse baut nur
// die kleine Request-Beschreibung (JSON), startet den Holer per QProcess und
// meldet dessen Antwort als Signal. deviceId/locale bleiben über QSettings
// erhalten (Kennung "harbour-quizduel", wie auf SFOS).
#ifndef NETBRIDGE_H
#define NETBRIDGE_H

#include <QObject>
#include <QProcess>
#include <QString>

class NetBridge : public QObject
{
    Q_OBJECT
    // Reaktiv, damit die lokalisierte Oberfläche beim Sprachwechsel sofort umschaltet.
    Q_PROPERTY(QString locale READ locale WRITE setLocale NOTIFY localeChanged)
public:
    explicit NetBridge(QObject *parent = 0);

    Q_INVOKABLE QString deviceId() const { return m_deviceId; }
    Q_INVOKABLE QString locale() const { return m_locale; }
    Q_INVOKABLE void setLocale(const QString &locale);

    Q_INVOKABLE qlonglong userId() const { return m_userId; }

    Q_INVOKABLE void getUsersByDevice();
    Q_INVOKABLE void login(const QString &username, const QString &password);
    Q_INVOKABLE void createAccount(const QString &username, const QString &password,
                                   const QString &email);

    // v1-Messaging/Freunde (requiresLogin -> m_session muss nach Login gesetzt sein).
    Q_INVOKABLE void getFriends();
    Q_INVOKABLE void getChatThreads();
    Q_INVOKABLE void getChatThread(qlonglong threadId);
    Q_INVOKABLE void sendMessageToThread(qlonglong threadId, const QString &message);
    Q_INVOKABLE void startChatThread(qlonglong recipientUserId, const QString &message);
    Q_INVOKABLE void inboxList();
    Q_INVOKABLE void gameList();                                 // boards/by-collection
    Q_INVOKABLE void fetchQuestions(int categoryId, int count);  // questions/difficulty

    // v1-Spiel (rooms + matchmaker + engine). Der Holer reichert die Raumantworten an
    // (flache Zug-Felder, wie SFOS parseRoom) und legt jeden vollen Raum als Datei ab;
    // openRoom liest diese Datei DIREKT (kein Holer, Qt 4 kann kein JSON).
    Q_INVOKABLE void loginByDevice();                            // Re-Auth beim Start
    Q_INVOKABLE void refreshGames();                             // multiplayer/refresh -> roomsUpdated (schlank)
    Q_INVOKABLE void openRoom(const QString &roomId);            // aus dem Cache -> roomOpened (voll)
    Q_INVOKABLE void startMatchmaking();                         // multiplayer/startMatchmaking -> matchStarted
    Q_INVOKABLE void challengeUser(qlonglong opponentUserId);    // multiplayer/createRoom -> matchStarted
    // participantId ist RAUM-LOKAL (nicht userId!) — QML reicht room.myParticipantId durch.
    // 0 -> laut abbrechen (sonst der schwedische „not present in room"-Fehler ohne Signal).
    Q_INVOKABLE void submitRound(const QString &roomId, qlonglong participantId, int roundIndex,
                                 int categoryId, const QString &answersJson, bool selectCategory);
    Q_INVOKABLE void findUser(const QString &username);          // user/quizduel/findUser -> userFound
    Q_INVOKABLE void addFriend(qlonglong friendUserId);          // user/quizduel/addFriend -> friendAdded
    // Bild über den Holer (rustls/TLS 1.2) lokal holen — Harmattans OpenSSL 0.9.8 kann die
    // CDN-https-Bilder NICHT. Erfüllt zugleich „Bild fertig, bevor der Timer läuft".
    Q_INVOKABLE void fetchImage(const QString &url);

signals:
    void usersFound(const QString &payload);
    void loggedIn(const QString &payload);
    void created(const QString &payload);
    void failed(const QString &message);
    void localeChanged();
    // Nutzlast als JSON-String (QML parst mit JSON.parse und liest .payload).
    void friendsLoaded(const QString &payload);
    void threadsLoaded(const QString &payload);
    void threadLoaded(const QString &payload);
    void inboxLoaded(const QString &payload);
    void gamesLoaded(const QString &payload);
    void questionsLoaded(const QString &payload);
    void roomsUpdated(const QString &payload);    // multiplayer/refresh (payload.rooms = schlank)
    void roomOpened(const QString &payload);      // openRoom (payload.room = voll)
    void matchStarted(const QString &payload);    // start/createRoom (payload.room/.updatedRooms)
    void roomUpdated(const QString &payload);     // invokeEngineCommands (payload.room = voll)
    void userFound(const QString &payload);       // findUser (payload.User)
    void friendAdded(const QString &payload);     // addFriend
    void imageFetched(const QString &url, const QString &path);   // leer path = fehlgeschlagen

private slots:
    void onFinished(int exitCode, QProcess::ExitStatus status);
    void onError(QProcess::ProcessError error);
    void onImageFinished(int exitCode, QProcess::ExitStatus status);

private:
    void run(const QString &method, const QString &fieldsJson, int which);
    QString holerPath() const;
    QString cacheDir() const;    // beschreibbarer Ordner für die Raum-/Bilddateien
    static QString jsonEscape(const QString &s);

    QString m_deviceId;
    QString m_locale;
    QString m_session;
    qlonglong m_userId;
    QProcess *m_proc;
    int m_which;   // 0 getUsers,1 login,2 create,3 friends,4 threads,5 thread,6 inbox,
                   // 7 games,8 questions,9 match,10 rooms,11 roomUpdated,12 userFound,13 friendAdded
};

#endif
