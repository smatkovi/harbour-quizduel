#!/bin/sh
# Baut und läuft den davinci-KAT (Bekanntantwort-Test: DECODE-Richtung + ticket)
# gegen recon/krypto-testvektor.json. Rein offline, KEIN Netz, keine Live-API.
# Braucht Qt5Core, Qt5Network, libcrypto und zlib (pkg-config). Linkt nur
# src/davinciprotocol.cpp hinzu (kein QObject, kein moc). Nicht Teil der .pro.
#
# Aufruf:  tools/build-kat.sh [pfad/zu/krypto-testvektor.json]
set -e
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
vec=${1:-"$root/../quizduel-port/recon/krypto-testvektor.json"}
out=${TMPDIR:-/tmp}/kat-davinci

g++ -std=c++17 -O0 -g -Wall -fPIC \
    $(pkg-config --cflags Qt5Core Qt5Network libcrypto zlib) \
    "$here/kat-davinci.cpp" "$root/src/davinciprotocol.cpp" \
    $(pkg-config --libs Qt5Core Qt5Network libcrypto zlib) \
    -o "$out"

"$out" "$vec"
