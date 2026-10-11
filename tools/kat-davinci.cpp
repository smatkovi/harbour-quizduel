// Offline-Bekanntantwort-Test (KAT) für die davinci-Protokollschicht.
//
// Prüft die DECODE-Richtung Byte für Byte gegen recon/krypto-testvektor.json
// (Grundwahrheit aus rev-flows.mitm) und die ticket-Signatur gegen den dort
// festgehaltenen Wert. KEIN Netz, KEINE Live-API — rein rechnerisch, damit das
// davinci-Modul offline nachweisbar richtig dekodiert (Jolla-Regel: Rechenarbeit
// nie blind aufs Gerät, Prüfung reproduzierbar).
//
// Bauen/Ausführen: tools/build-kat.sh (eine kurze Zeile). Gehört bewusst NICHT in
// die ausgelieferte harbour-quizduel.pro — eigenes, separates Testprogramm, das nur
// src/davinciprotocol.cpp hinzulinkt (kein QObject, kein moc).
#include "../src/davinciprotocol.h"

#include <QByteArray>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <cstdio>

static int g_fail = 0;

static void check(const char *name, bool ok)
{
    std::printf("[%s] %s\n", ok ? " OK " : "FAIL", name);
    if (!ok)
        ++g_fail;
}

int main(int argc, char *argv[])
{
    // Vektorpfad als Argument; Vorgabe relativ zum Baum (recon liegt im Schwester-
    // projekt quizduel-port). Nur gelesen — nie geschrieben.
    const QString path = argc > 1
        ? QString::fromLocal8Bit(argv[1])
        : QStringLiteral("../quizduel-port/recon/krypto-testvektor.json");
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        std::fprintf(stderr, "Testvektor nicht lesbar: %s\n", qPrintable(path));
        return 2;
    }
    const QJsonObject v = QJsonDocument::fromJson(f.readAll()).object();
    const QByteArray ivHex = v.value(QStringLiteral("iv_hex")).toString().toLatin1();
    const QJsonObject req = v.value(QStringLiteral("request")).toObject();
    const QJsonObject res = v.value(QStringLiteral("response")).toObject();

    // (d) ticket-Signatur — die einzige ENCODE-Richtung, die sich byte-genau
    //     festnageln lässt (deterministisch, kein Zufall). Gegen den Umschlagwert
    //     aus dem Vektor.
    {
        const QString t = davinci::ticket(QStringLiteral("getUsersByDevice"),
                                           QString(), QStringLiteral("1791619940280"));
        check("ticket(getUsersByDevice,\"\",1791619940280) == a0206b8e…8bae8eff",
              t == QStringLiteral("a0206b8efb53b3ff59073b59795a16a48bae8eff"));
    }

    // (a) Request-Chiffrat -> plaintext_json. 2-Schicht-Pfad (AES -> innere gzip);
    //     das Chiffrat beginnt nicht mit 1f8b, daher überspringt parseResponse die
    //     HTTP-gzip-Schicht korrekt.
    {
        const QByteArray ct =
            davinci::fromHex(req.value(QStringLiteral("ciphertext_hex")).toString().toLatin1());
        bool ok = false;
        const QByteArray got = davinci::parseResponse(ct, ivHex, &ok);
        check("parseResponse(request.ciphertext) == request.plaintext_json",
              ok && got == req.value(QStringLiteral("plaintext_json")).toString().toUtf8());
    }

    // (b) volle Wire-Antwort: 3 Schichten (HTTP-gzip -> AES -> innere gzip). Beginnt
    //     mit 1f8b; parseResponse zieht die HTTP-gzip selbst ab.
    {
        const QByteArray wire =
            davinci::fromHex(res.value(QStringLiteral("wire_hex")).toString().toLatin1());
        bool ok = false;
        const QByteArray got = davinci::parseResponse(wire, ivHex, &ok);
        check("parseResponse(response.wire) == \"null\"",
              ok && got == res.value(QStringLiteral("plaintext_json")).toString().toUtf8());
    }

    // (c) das, was Qt zur Laufzeit tatsächlich liefert: HTTP-gzip ist von QNAM schon
    //     abgezogen (siehe requestFor-Kommentar). parseResponse erkennt das an der
    //     fehlenden 1f8b-Magic und geht direkt in die AES-Schicht.
    {
        const QByteArray inner = davinci::fromHex(
            res.value(QStringLiteral("inner_after_http_gunzip_hex")).toString().toLatin1());
        bool ok = false;
        const QByteArray got = davinci::parseResponse(inner, ivHex, &ok);
        check("parseResponse(response.inner, Qt-Fall) == \"null\"",
              ok && got == res.value(QStringLiteral("plaintext_json")).toString().toUtf8());
    }

    // (e) Roundtrip der ENCODE-Richtung: buildBody (JSON->BOM->gzip->AES, zufälliger
    //     IV) und parseResponse mit DEMSELBEN IV müssen den Umschlag exakt
    //     rekonstruieren. Byte-Match gegen die Mitschrift geht hier nicht (IV zufällig),
    //     die Inversität schon.
    {
        QJsonObject env = davinci::baseEnvelope(
            QStringLiteral("dc4b897c55b4c7117e883f599d69ad66"), QStringLiteral("login"));
        env.insert(QStringLiteral("username"), QStringLiteral("probe"));
        const QByteArray want = QJsonDocument(env).toJson(QJsonDocument::Compact);
        QByteArray outIv;
        const QByteArray body = davinci::buildBody(env, outIv);
        bool ok = false;
        const QByteArray got = davinci::parseResponse(body, outIv, &ok);
        check("buildBody -> parseResponse rekonstruiert den Umschlag", ok && got == want);

        // Und: das ticket im selbst gebauten Umschlag passt zu Methode + eigenem
        // timestamp (epochMs == timestamp-Feld — die Kernbedingung der Signatur).
        const QJsonObject back = QJsonDocument::fromJson(got).object();
        const QString recomputed = davinci::ticket(
            QStringLiteral("login"), QString(),
            back.value(QStringLiteral("timestamp")).toString());
        check("Umschlag-ticket == ticket(login,\"\",timestamp)",
              back.value(QStringLiteral("ticket")).toString() == recomputed);
    }

    if (g_fail)
        std::printf("\n%d KAT fehlgeschlagen\n", g_fail);
    else
        std::printf("\nalle KAT OK\n");
    return g_fail ? 1 : 0;
}
