#!/bin/sh
# Packt den ARM-Build als Harmattan-.deb. Reihenfolge: meego/build-rust.sh (Holer)
# und meego/build.sh arm (Qt-4-App) müssen vorher gelaufen sein.
#
# Layout auf dem Gerät:
#   /opt/harbour-quizduel/bin/harbour-quizduel      (Qt-4-App)
#   /opt/harbour-quizduel/bin/quizduel-holer        (Rust/rustls-Helfer)
#   /opt/harbour-quizduel/qml                        (meego/qml)
#   /usr/share/applications/harbour-quizduel.desktop
#   /usr/share/icons/hicolor/80x80/apps/harbour-quizduel.png
#
# mkdeb.py schreibt das .deb selbst (Harmattan-dpkg 1.15 will gzip-Mitglieder
# und keinen "/"-Anhang an den ar-Namen).
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd)
PKG=$HERE/meego
OUT=$HERE/build/meego
BIN=$OUT/arm/harbour-quizduel
FETCH=$OUT/fetch/quizduel-holer
XGCC=${XGCC:-/tmp/xgcc-harmattan}
VERSION=${VERSION:-$(sh "$PKG/version.sh")}

[ -x "$BIN" ] || { echo "ARM-Binary fehlt: $BIN (meego/build.sh arm)" >&2; exit 1; }
[ -x "$FETCH" ] || { echo "Holer fehlt: $FETCH (meego/build-rust.sh)" >&2; exit 1; }

STAGE=$OUT/stage
rm -rf "$STAGE"
mkdir -p "$STAGE/DEBIAN" "$STAGE/opt/harbour-quizduel/bin" \
         "$STAGE/usr/share/applications" "$STAGE/usr/share/icons/hicolor/80x80/apps" \
         "$STAGE/usr/share/doc/harbour-quizduel"

cp "$BIN" "$STAGE/opt/harbour-quizduel/bin/harbour-quizduel"
"$XGCC/bin/arm-none-linux-gnueabi-strip" "$STAGE/opt/harbour-quizduel/bin/harbour-quizduel" || true
cp "$FETCH" "$STAGE/opt/harbour-quizduel/bin/quizduel-holer"
chmod 755 "$STAGE/opt/harbour-quizduel/bin/harbour-quizduel" "$STAGE/opt/harbour-quizduel/bin/quizduel-holer"

cp -a "$PKG/qml" "$STAGE/opt/harbour-quizduel/qml"
cp "$PKG/icons/icon-80.png" "$STAGE/usr/share/icons/hicolor/80x80/apps/harbour-quizduel.png"
cp "$PKG/harbour-quizduel.desktop" "$STAGE/usr/share/applications/harbour-quizduel.desktop"
find "$STAGE" -type f ! -path "*/bin/*" -exec chmod 644 {} +
find "$STAGE" -type d -exec chmod 755 {} +

VERSION="$VERSION" ICON="$PKG/icons/icon-64.png" python3 - "$PKG/control.in" "$STAGE/DEBIAN/control" <<'PY'
import base64, os, sys, textwrap
src, dst = sys.argv[1], sys.argv[2]
ctl = open(src).read()
b64 = base64.b64encode(open(os.environ["ICON"], "rb").read()).decode("ascii")
icon = "\n".join(" " + line for line in textwrap.wrap(b64, 76))
ctl = ctl.replace("@VERSION@", os.environ["VERSION"]).replace("@ICON@", icon)
open(dst, "w").write(ctl)
PY

DEB="$OUT/harbour-quizduel_${VERSION}_armel.deb"
python3 "$PKG/mkdeb.py" "$STAGE" "$DEB"
echo "== $DEB"
