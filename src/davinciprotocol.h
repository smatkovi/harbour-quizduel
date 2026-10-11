// davinci-Protokollschicht: Krypto + Request-Umschlag des magbackend ("davinci").
//
// Nachbau der C#-Klasse Core.DavinciConnector.Encryption (recon/il2cpp.cs
// Z.151286-151348) und des Umschlags Request<T>. Alles als reine Funktionen im
// namespace davinci — KEIN QObject, KEINE Netzarbeit, KEINE QML-Registrierung;
// NetClient bleibt der einzige Netz-/QObject-Besitzer und kann dieses Modul
// offline gegen die Mitschrift prüfen (recon/krypto-testvektor.json).
//
// Schichtung pro Request (BEFUND.md "★ KRYPTO GELÖST + validiert", 10.10.2026):
//   JSON-Umschlag  ->  gzip (RFC 1952, innere Schicht)  ->  AES-256-CBC/PKCS7
// Schlüssel = ServerSharedKeyV2 (32 B, statisch), IV = 16 Zufallsbytes je Request,
// als Hex im Header payload-session. Die Antwort nutzt denselben Request-IV
// (es gibt KEINE Antwort-payload-session) und liegt zusätzlich unter einer
// HTTP-Transport-gzip-Schicht (content-encoding: gzip) — siehe parseResponse.
#ifndef DAVINCIPROTOCOL_H
#define DAVINCIPROTOCOL_H

#include <QByteArray>
#include <QJsonObject>
#include <QString>

class QNetworkRequest;

namespace davinci
{
    // --- Konstanten (Herkunft: recon/BEFUND.md "★ KRYPTO GELÖST", il2cpp.cs) ---
    // ServerSharedKeyV2: statisches Feld SpitfireApi.Spitfire (il2cpp.cs Z.429100),
    // per Brute-Orakel gegen die Mitschrift bestätigt. 32 Byte, hier als Hex.
    static const char *const kServerSharedKeyV2Hex =
        "REDIGIERT_ServerSharedKeyV2_64hex_aus_der_App";
    // Identifiziert die App im Umschlag (applicationId ist bei /user/quizduel/* = 0).
    static const char *const kApplicationKey  = "REDIGIERT_applicationKey";
    // Pflicht-Header-Werte (ohne sie: "force upgrade"-Riegel).
    static const char *const kUserAgent       = "Android/New QuizDuel/1.70.04";
    static const char *const kUnityVersion    = "6000.3.22f1";
    static const char *const kRequestVersion  = "2";
    static const char *const kAppVersion      = "1.70.04";
    static const char *const kPlatform        = "android";
    // Endpunkt-Stamm: POST <base><methode> (getUsersByDevice/login/create/...).
    static const char *const kBaseUrl =
        "https://api.prod.magbackend.se/api/user/quizduel/";
    // Wurzel für Endpunkte außerhalb von user/quizduel/ (chatthread/, inbox/, boards/):
    // Methoden MIT '/' hängen hier an, kurze (login, getFriends) am kBaseUrl.
    static const char *const kApiRoot = "https://api.prod.magbackend.se/api/";
    // Kosmetische Telemetriefelder: der Server liest sie fürs Gate nicht, sie
    // füllen nur den Umschlag. Werte an die Mitschrift angelehnt (platform bleibt
    // "android" — die App tarnt sich als Android, so auch der user-agent).
    static const char *const kDeviceModel = "Jolla (Sailfish OS, AppSupport)";
    static const char *const kOsVersion   = "Android OS 15 / API-35";
    // SALT der ticket-Signatur: ticket = SHA1(SALT + version + methode + session +
    // epochMs). Per Disassembly isoliert (SignRequest@0x03750074, 5-teilige
    // Verkettung) und per Substring-Brute an allen 95 mitgeschnittenen Requests
    // bestätigt (BEFUND.md "★★ ticket-Signatur GELÖST"). KEIN Zufall mehr.
    static const char *const kTicketSalt  = "REDIGIERT_ticketSalt";

    // --- Primitive (Spiegel der C#-Encryption-Hilfen) ---
    QByteArray securedRandom(int n);                 // = SecuredRandom (RAND_bytes)
    QByteArray toHex(const QByteArray &bytes);       // = ByteArrayToHexString (klein)
    QByteArray fromHex(const QByteArray &hex);       // = StringToByteArray
    QByteArray gzipCompress(const QByteArray &data);   // innere Schicht, RFC 1952
    QByteArray gzipDecompress(const QByteArray &data); // akzeptiert gzip ODER zlib
    QByteArray aes256CbcEncrypt(const QByteArray &plain, const QByteArray &key, const QByteArray &iv);
    QByteArray aes256CbcDecrypt(const QByteArray &cipher, const QByteArray &key, const QByteArray &iv);
    QByteArray sharedKey();                           // kServerSharedKeyV2Hex -> 32 Byte
    // ticket-Signatur: kleingeschriebenes Hex (40 Zeichen) von SHA1(UTF-8(
    // kTicketSalt + kAppVersion + methodLastSegment + session + epochMs )). epochMs
    // MUSS gleich dem timestamp-Feld des Umschlags sein (dieselbe GetEpochTimestamp),
    // session = "" solange nicht angemeldet. Kodierung UTF-8 (NICHT UTF-16).
    QString    ticket(const QString &method, const QString &session, const QString &epochMs);

    // --- Umschlag / Protokoll ---
    // Die gemeinsamen Request<T>-Pflichtfelder (Reihenfolge egal: Newtonsoft liest
    // nach Namen). session=null vor dem Login; userId 0 bis zum Login. method =
    // Endpunkt-Methode (z.B. "login"); nötig, weil die ticket-Signatur das letzte
    // Pfadsegment einschließt und ticket + timestamp aus DEMSELBEN Zeitstempel kommen.
    QJsonObject baseEnvelope(const QString &deviceId,
                             const QString &method,
                             const QString &session = QString(),
                             qint64 userId = 0,
                             const QString &locale = QString());
    // JSON -> UTF-8 (mit vorangestellter BOM, wie die echte App) -> gzip -> AES.
    // Der zufällige IV wird erzeugt und als Hex (Großschrift, wie in der Mitschrift)
    // in outIvHex zurückgegeben — genau der Wert des payload-session-Headers.
    QByteArray  buildBody(const QJsonObject &envelope, QByteArray &outIvHex);
    // Antwort entschlüsseln: optional HTTP-gzip abziehen (automatisch erkannt),
    // dann AES-256-CBC mit DEMSELBEN IV wie der Request, dann innere gzip.
    // Ergebnis = UTF-8-JSON-Bytes (BOM entfernt); kann literal "null" sein.
    // ok (falls gesetzt) = alle Schichten sauber dekodiert.
    QByteArray  parseResponse(const QByteArray &body, const QByteArray &ivHex, bool *ok = 0);
    // Baut die QNetworkRequest für <methode> mit allen Pflicht-Headern inkl.
    // payload-session. accept-encoding wird bewusst NICHT gesetzt (Qt-Falle).
    QNetworkRequest requestFor(const QString &method, const QByteArray &ivHex);
}

#endif
