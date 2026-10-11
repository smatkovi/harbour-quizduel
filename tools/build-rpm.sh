#!/bin/bash
# Baut das SFOS-RPM auf arch im Container sfossdk52.
# Wichtig: NICHT im share-Mount bauen (nsrreader-Falle) -> Quelle wird in die
# Container-eigene FS /home/mersdk/build-hq kopiert und dort gebaut.
#
# Erwartet die Quelle unter /tmp/hq-src/harbour-quizduel (dorthin entpackt der
# Aufrufer das tar). Aufruf auf arch:  bash tools/build-rpm.sh [aarch64|armv7hl]
set -euo pipefail
ARCH="${1:-aarch64}"
TARGET="SailfishOS-5.2.0.15-${ARCH}"
C="sfossdk52"
B="/home/mersdk/build-hq"
SRC="/tmp/hq-src/harbour-quizduel"
OUT="/tmp/hq-out"

echo "[*] Ziel $TARGET, Quelle $SRC"
docker exec -u root "$C" bash -lc "rm -rf $B && mkdir -p $B"
docker cp "$SRC/." "$C:$B/"
docker exec -u root "$C" bash -lc "chown -R mersdk:mersdk $B"
echo "[*] mb2 build …"
docker exec -u mersdk "$C" bash -lc "cd $B && mb2 -t $TARGET build"

rm -rf "$OUT"; mkdir -p "$OUT"
# mb2 legt die RPMs unter RPMS/ ab
mapfile -t RPMS < <(docker exec "$C" bash -lc "ls $B/RPMS/*.rpm 2>/dev/null" | tr -d '\r')
[ "${#RPMS[@]}" -gt 0 ] || { echo "!! kein RPM gebaut"; exit 1; }
for f in "${RPMS[@]}"; do docker cp "$C:$f" "$OUT/"; done
echo "[*] herausgeholt nach $OUT:"
for r in "$OUT"/*.rpm; do
  printf '    %s  ->  ' "$(basename "$r")"
  rpm -qp --qf '%{NAME} %{VERSION}-%{RELEASE}.%{ARCH}\n' "$r" 2>/dev/null || echo '(rpm-query n/v)'
done
