#!/bin/sh
# Baut quizduel-holer (meego/fetch) für den N9/N950 — statisch gegen musl — auf
# dem Build-Rechner. Zwei Modi:
#
#   meego/build-rust.sh selftest <vektor.json>   Host-Build (x86) + Krypto-KAT
#                                                 (arch-unabhängig, kein qemu nötig)
#   meego/build-rust.sh                            Cross-Build -> build/meego/fetch/quizduel-holer
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd)
. "$HERE/meego/cross.env"
[ -x "$CARGO_HOME/bin/cargo" ] || { echo "cargo fehlt unter $CARGO_HOME (tools/toolchain.sh)" >&2; exit 1; }
cd "$HERE/meego/fetch"

if [ "${1:-}" = selftest ]; then
    VEC=${2:?Pfad zum krypto-testvektor.json}
    echo "[*] Host-Build für den KAT …"
    CARGO_TARGET_DIR=/tmp/quizduel-holer-host cargo build --release
    echo "[*] KAT:"
    exec /tmp/quizduel-holer-host/release/quizduel-holer --selftest "$VEC"
fi

CARGO_TARGET_DIR=${CARGO_TARGET_DIR:-/tmp/quizduel-holer-target}
export CARGO_TARGET_DIR
echo "[*] Cross-Build $ZIEL …"
nice cargo build --release --target "$ZIEL"
mkdir -p "$HERE/build/meego/fetch"
cp "$CARGO_TARGET_DIR/$ZIEL/release/quizduel-holer" "$HERE/build/meego/fetch/quizduel-holer"
echo "[*] fertig:"
ls -la "$HERE/build/meego/fetch/quizduel-holer"
file "$HERE/build/meego/fetch/quizduel-holer"
