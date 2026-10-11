# harbour-quizduel

Unabhängiger QuizDuel-Client für **Sailfish OS** und **MeeGo/Harmattan** (Nokia N9/N950).
Kein Port — die Original-App ist Unity/IL2CPP; dieser Client spricht selbst das davinci-REST-
Protokoll von `api.prod.magbackend.se` (AES-256-CBC über gzip(JSON), ticket-Signatur per SHA1).

## Funktionsumfang
Spielbares klassisches Duell, Übersicht (aktive + beendete Spiele, Zugstatus, Punkte),
Freunde suchen/hinzufügen/herausfordern, Rematch, Profilbilder, Bildfragen, 16 Sprachen,
Zugangsdaten im Geräteschloss (Sailfish Secrets, wie Storeman).

## Bauen
- **SFOS** (Silica/Qt5): `tools/build-rpm.sh aarch64` bzw. `armv7hl` im Sailfish-SDK-Container.
- **MeeGo** (Qt4 + Rust-Holer für TLS 1.2): `meego/build-rust.sh`, `meego/build.sh arm`, `meego/build-deb.sh`.

## Schlüssel entfernt
Der Krypto-Schlüssel (ServerSharedKeyV2), der `applicationKey` und der ticket-`salt` sind in dieser
öffentlichen Fassung **durch `REDIGIERT_…`-Platzhalter ersetzt** (`src/davinciprotocol.h`,
`meego/fetch/src/main.rs`). Sie stecken in der Original-App; zum Bauen eines funktionierenden
Clients dort extrahieren und einsetzen.
