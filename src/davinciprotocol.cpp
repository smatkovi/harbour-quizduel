#include "davinciprotocol.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonDocument>
#include <QLocale>
#include <QNetworkRequest>
#include <QTimeZone>
#include <QUrl>

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <zlib.h>

#include <cstring>

// --- Primitive -------------------------------------------------------------

QByteArray davinci::securedRandom(int n)
{
    // = Encryption.SecuredRandom: kryptographisch sichere Bytes (OpenSSL).
    QByteArray b(n, '\0');
    if (n > 0 && RAND_bytes(reinterpret_cast<unsigned char *>(b.data()), n) != 1)
        return QByteArray();   // Zufallsquelle versagt -> leer; Aufrufer prüfen.
    return b;
}

QByteArray davinci::toHex(const QByteArray &bytes)
{
    return bytes.toHex();      // = ByteArrayToHexString, hier kleingeschrieben.
}

QByteArray davinci::fromHex(const QByteArray &hex)
{
    return QByteArray::fromHex(hex);   // = StringToByteArray (Groß-/Kleinschrift egal).
}

QByteArray davinci::gzipCompress(const QByteArray &data)
{
    // Echtes gzip (RFC 1952), NICHT qCompress: qCompress ist rohes zlib mit einem
    // nicht-standardkonformen 4-Byte-Längenpräfix und erzeugt keinen gzip-Strom.
    // windowBits = 15 (max. Fenster) + 16 wählt den gzip-Kopf/-Schwanz.
    z_stream zs;
    std::memset(&zs, 0, sizeof(zs));
    if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8,
                     Z_DEFAULT_STRATEGY) != Z_OK)
        return QByteArray();
    zs.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(data.constData()));
    zs.avail_in = static_cast<uInt>(data.size());
    QByteArray out;
    char buf[32768];
    int ret;
    do {
        zs.next_out = reinterpret_cast<Bytef *>(buf);
        zs.avail_out = sizeof(buf);
        ret = deflate(&zs, Z_FINISH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            deflateEnd(&zs);
            return QByteArray();
        }
        out.append(buf, sizeof(buf) - zs.avail_out);
    } while (ret == Z_OK);
    deflateEnd(&zs);
    return out;
}

QByteArray davinci::gzipDecompress(const QByteArray &data)
{
    // windowBits 15 + 32: erkennt gzip ODER zlib automatisch (die innere Schicht
    // ist gzip, die äußere HTTP-Schicht ebenfalls gzip).
    z_stream zs;
    std::memset(&zs, 0, sizeof(zs));
    if (inflateInit2(&zs, 15 + 32) != Z_OK)
        return QByteArray();
    zs.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(data.constData()));
    zs.avail_in = static_cast<uInt>(data.size());
    QByteArray out;
    char buf[32768];
    int ret;
    do {
        zs.next_out = reinterpret_cast<Bytef *>(buf);
        zs.avail_out = sizeof(buf);
        ret = inflate(&zs, Z_NO_FLUSH);
        // Da immer Ausgabeplatz da ist, heißt Z_BUF_ERROR "Eingabe zu Ende ohne
        // STREAM_END" = abgeschnitten -> Fehler. Nur Z_OK läuft weiter.
        if (ret != Z_OK && ret != Z_STREAM_END) {
            inflateEnd(&zs);
            return QByteArray();
        }
        out.append(buf, sizeof(buf) - zs.avail_out);
    } while (ret == Z_OK);
    inflateEnd(&zs);
    if (ret != Z_STREAM_END)
        return QByteArray();
    return out;
}

QByteArray davinci::aes256CbcEncrypt(const QByteArray &plain, const QByteArray &key,
                                     const QByteArray &iv)
{
    // OpenSSL-3-EVP-API (die alte AES_*-Low-Level-API ist veraltet). PKCS7 ist
    // EVP-Default — NICHT abschalten. Ausgabepuffer = Eingabe + ein Blockrand (16).
    if (key.size() != 32 || iv.size() != 16)
        return QByteArray();
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        return QByteArray();
    QByteArray out(plain.size() + 16, '\0');
    int outl = 0, finl = 0;
    unsigned char *o = reinterpret_cast<unsigned char *>(out.data());
    const bool ok =
        EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), 0,
                           reinterpret_cast<const unsigned char *>(key.constData()),
                           reinterpret_cast<const unsigned char *>(iv.constData())) == 1
        && EVP_EncryptUpdate(ctx, o, &outl,
                             reinterpret_cast<const unsigned char *>(plain.constData()),
                             plain.size()) == 1
        && EVP_EncryptFinal_ex(ctx, o + outl, &finl) == 1;   // schreibt den PKCS7-Padblock
    EVP_CIPHER_CTX_free(ctx);
    if (!ok)
        return QByteArray();
    out.resize(outl + finl);
    return out;
}

QByteArray davinci::aes256CbcDecrypt(const QByteArray &cipher, const QByteArray &key,
                                     const QByteArray &iv)
{
    if (key.size() != 32 || iv.size() != 16 || cipher.isEmpty() || (cipher.size() % 16) != 0)
        return QByteArray();
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        return QByteArray();
    QByteArray out(cipher.size() + 16, '\0');
    int outl = 0, finl = 0;
    unsigned char *o = reinterpret_cast<unsigned char *>(out.data());
    const bool ok =
        EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), 0,
                           reinterpret_cast<const unsigned char *>(key.constData()),
                           reinterpret_cast<const unsigned char *>(iv.constData())) == 1
        && EVP_DecryptUpdate(ctx, o, &outl,
                             reinterpret_cast<const unsigned char *>(cipher.constData()),
                             cipher.size()) == 1
        && EVP_DecryptFinal_ex(ctx, o + outl, &finl) == 1;   // prüft/entfernt PKCS7
    EVP_CIPHER_CTX_free(ctx);
    if (!ok)
        return QByteArray();
    out.resize(outl + finl);
    return out;
}

QByteArray davinci::sharedKey()
{
    return QByteArray::fromHex(QByteArray(kServerSharedKeyV2Hex));
}

QString davinci::ticket(const QString &method, const QString &session, const QString &epochMs)
{
    // ticket = SignRequest@0x03750074: kleingeschriebenes Hex von SHA1(UTF-8(
    // SALT + version + methodLastSegment + session + epochMs )). Die 5-teilige
    // Verkettung stammt aus dem Disassembly, der SALT "REDIGIERT_ticketSalt" (kTicketSalt) aus
    // einem Substring-Brute gegen die Mitschrift; an allen 95 mitgeschnittenen
    // Requests bestätigt (BEFUND.md "★★ ticket-Signatur GELÖST").
    //   methodLastSegment = letztes "/"-Segment des Pfads (hier schon die Methode).
    //   session = "" solange null/vor dem Login.
    //   epochMs = Millisekunden-Epoch-STRING, identisch mit dem timestamp-Feld.
    // Kodierung UTF-8 (NICHT UTF-16); Ausgabe 40 Hex klein (toHex ist bereits klein).
    // KAT: ticket("getUsersByDevice", "", "1791619940280")
    //      == "a0206b8efb53b3ff59073b59795a16a48bae8eff" (recon/krypto-testvektor.json).
    const QString s = QString::fromLatin1(kTicketSalt)
                    + QString::fromLatin1(kAppVersion)
                    + method.section(QLatin1Char('/'), -1)
                    + session
                    + epochMs;
    return QString::fromLatin1(
        QCryptographicHash::hash(s.toUtf8(), QCryptographicHash::Sha1).toHex());
}

// --- Umschlag / Protokoll --------------------------------------------------

QJsonObject davinci::baseEnvelope(const QString &deviceId, const QString &method,
                                  const QString &session, qint64 userId,
                                  const QString &localeIn)
{
    // Pflichtfelder des Request<T>-Umschlags (bestätigt an 7 entschlüsselten Flows
    // aus rev-flows.mitm). timestamp/localTimestamp sind Epoch-MILLISEKUNDEN als
    // String; localTimestamp = timestamp + lokaler UTC-Versatz (Beleg: 1791627140280
    // - 1791619940280 = 7200000 ms = +02:00 Europe/Vienna). WICHTIG: timestamp und
    // ticket kommen aus DEMSELBEN ts — die ticket-Signatur verlangt epochMs == timestamp.
    const qint64 ts = QDateTime::currentMSecsSinceEpoch();
    const QString tsStr = QString::number(ts);
    const qint64 localTs =
        ts + static_cast<qint64>(QDateTime::currentDateTime().offsetFromUtc()) * 1000;

    // Vom Aufrufer gewählte Sprache (QDLanguageHelper-Format "xx_YY", z.B. "sv_SE");
    // leer = Gerätesprache. Steuert Markt/Fragensprache; quizDuelLocale (login/create)
    // übernimmt denselben Wert.
    QString locale = localeIn;
    if (locale.isEmpty()) {
        locale = QLocale::system().name();                  // z.B. "de_DE"
        if (!locale.contains(QLatin1Char('_')))
            locale = QStringLiteral("de_DE");
    }
    QString tz = QString::fromLatin1(QTimeZone::systemTimeZone().id());   // z.B. "Europe/Vienna"
    if (tz.isEmpty())
        tz = QStringLiteral("Europe/Vienna");

    QJsonObject activity;                                   // applicationActivityStatus
    activity.insert("activityId", 0);
    activity.insert("presenceBucketUpdates", QJsonValue(QJsonValue::Null));

    QJsonObject o;
    o.insert("userId", QJsonValue(userId));
    o.insert("applicationId", 0);
    o.insert("applicationKey", QString::fromLatin1(kApplicationKey));
    o.insert("timestamp", tsStr);
    o.insert("localTimestamp", QString::number(localTs));
    o.insert("localTimeZone", tz);
    o.insert("version", QString::fromLatin1(kAppVersion));
    o.insert("session", session.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(session));
    // Signatur über DEMSELBEN tsStr; session leer = "" in der Verkettung (Umschlagfeld
    // bleibt null). So stimmt ticket mit dem gesendeten timestamp überein.
    o.insert("ticket", ticket(method, session, tsStr));
    o.insert("platform", QString::fromLatin1(kPlatform));
    o.insert("locale", locale);
    o.insert("premium", false);
    o.insert("deviceModel", QString::fromLatin1(kDeviceModel));
    o.insert("osVersion", QString::fromLatin1(kOsVersion));
    o.insert("advertisingId", QString());                   // leer wie in der Mitschrift
    o.insert("deviceId", deviceId);
    o.insert("clientGeneratedId", QJsonValue(QJsonValue::Null));
    o.insert("attributionProviderId", QJsonValue(QJsonValue::Null));
    o.insert("applicationActivityStatus", activity);
    o.insert("tracks", QJsonValue(QJsonValue::Null));
    return o;
}

QByteArray davinci::buildBody(const QJsonObject &envelope, QByteArray &outIvHex)
{
    // JSON kompakt -> UTF-8 MIT vorangestellter BOM. Die echte App stellt eine
    // UTF-8-BOM (ef bb bf) voran, bevor sie gzip-packt (Beleg: Testvektor, das
    // gzip-ausgepackte JSON beginnt mit ef bb bf).
    const QByteArray json = QJsonDocument(envelope).toJson(QJsonDocument::Compact);
    const QByteArray payload = QByteArray("\xEF\xBB\xBF", 3) + json;
    const QByteArray gz = gzipCompress(payload);
    const QByteArray iv = securedRandom(16);
    // payload-session in Großschrift — so steht der IV in der Mitschrift.
    outIvHex = toHex(iv).toUpper();
    return aes256CbcEncrypt(gz, sharedKey(), iv);
}

QByteArray davinci::parseResponse(const QByteArray &body, const QByteArray &ivHex, bool *ok)
{
    if (ok)
        *ok = false;
    if (body.isEmpty())
        return QByteArray();

    // Schicht 1 (HTTP-Transport-gzip): nur noch da, wenn die HTTP-Bibliothek sie
    // nicht selbst abgezogen hat. Wir erkennen die gzip-Magic (1f 8b); das
    // AES-Chiffrat beginnt praktisch nie damit, daher eindeutig. (Auf ein Request-
    // Chiffrat angewandt greift diese Erkennung nicht und der Body geht direkt in
    // Schicht 2 — so prüft der KAT die Request-Richtung mit derselben Funktion.)
    QByteArray blob = body;
    if (blob.size() >= 2
        && static_cast<unsigned char>(blob.at(0)) == 0x1f
        && static_cast<unsigned char>(blob.at(1)) == 0x8b) {
        const QByteArray unzipped = gzipDecompress(blob);
        // Rückfall: ein AES-Chiffrat beginnt mit p~1/65536 zufällig mit 1f 8b.
        // Lässt es sich nicht auspacken, war es keine HTTP-gzip-Schicht -> roh weiter.
        if (!unzipped.isEmpty())
            blob = unzipped;
    }

    // Schicht 2: AES-256-CBC mit DEMSELBEN IV wie der Request (keine Antwort-
    // payload-session). fromHex nimmt Groß- wie Kleinschrift.
    const QByteArray gz = aes256CbcDecrypt(blob, sharedKey(), fromHex(ivHex));
    if (gz.isEmpty())
        return QByteArray();

    // Schicht 3: innere davinci-gzip -> UTF-8-JSON (ggf. mit BOM).
    QByteArray json = gzipDecompress(gz);
    if (json.isEmpty())
        return QByteArray();
    if (json.startsWith("\xEF\xBB\xBF"))
        json.remove(0, 3);

    if (ok)
        *ok = true;
    return json;   // Kann literal "null" sein (z.B. getUsersByDevice ohne Konto).
}

QNetworkRequest davinci::requestFor(const QString &method, const QByteArray &ivHex)
{
    // Methoden mit '/' (z.B. "chatthread/getAllMyChatThreads", "inbox/list") hängen an
    // /api/; kurze Methoden (login, create, getUsersByDevice, getFriends) an
    // /api/user/quizduel/. Die ticket-Signatur nutzt in beiden Fällen das letzte
    // Pfadsegment — der validierte Login bleibt damit unverändert.
    const QString url = method.contains(QLatin1Char('/'))
        ? (QString::fromLatin1(kApiRoot) + method)
        : (QString::fromLatin1(kBaseUrl) + method);
    QNetworkRequest req((QUrl(url)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QByteArray("application/json"));
    // user-agent als bekannten Header setzen, damit Qt keinen zweiten Default-UA anhängt.
    req.setHeader(QNetworkRequest::UserAgentHeader, QString::fromLatin1(kUserAgent));
    req.setRawHeader("request-version", kRequestVersion);
    req.setRawHeader("x-davinci-gzip-compression", "True");
    req.setRawHeader("x-unity-version", kUnityVersion);
    req.setRawHeader("payload-session", ivHex);
    // KEIN accept-encoding von Hand: sonst hört QNAM auf, content-encoding: gzip
    // transparent zu entpacken (Qt-Falle). So zieht Qt die äußere HTTP-gzip selbst
    // ab; parseResponse erkennt beide Fälle an der gzip-Magic.
    return req;
}
