#!/bin/sh
# Baut die MeeGo-Harmattan-Ausgabe (N9/N950) auf dem Build-Rechner:
#   meego/build.sh arm   -> build/meego/arm/harbour-quizduel
# Braucht die GCC-Cross-Toolchain (XGCC /tmp/xgcc-harmattan), die MADDE-Sysroot
# (SYSROOT) und moc aus dem Simulator-Qt 4.7.4 (SIMQT). libstdc++ statisch, damit
# Qt auf dem Gerät bei seiner eigenen GCC-4.4-Laufzeit bleibt.
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd)
MODE=${1:-arm}
XGCC=${XGCC:-/tmp/xgcc-harmattan}
SYSROOT=${SYSROOT:-$HOME/QtSDK/Madde/sysroots/harmattan_sysroot_10.2011.34-1_slim}
SIMQT=${SIMQT:-$HOME/QtSDK/Simulator/Qt/gcc}
JOBS=${JOBS:-8}
VERSION=${VERSION:-$(sh "$HERE/meego/version.sh")}
OUT=$HERE/build/meego/$MODE
mkdir -p "$OUT"

APP_SRC="meego/main.cpp meego/netbridge.cpp"
MOC_HEADERS="meego/netbridge.h"
INCLUDES="-I$HERE -I$HERE/meego -I$HERE/meego/compat"
DEFINES="-DAPP_VERSION='\"$VERSION\"' -DQT_NO_DEBUG"
COMMON="-O2 -Wall -Wno-register -Wno-deprecated-declarations -Wno-unused-parameter $DEFINES $INCLUDES"
CXX_ONLY="-std=gnu++11 -include $HERE/meego/compat/qt4compat.h"
QT4_MODULES="QtCore QtGui QtNetwork QtDeclarative"

if [ "$MODE" != arm ]; then echo "nur 'arm' unterstützt" >&2; exit 2; fi
CXX=$XGCC/bin/arm-none-linux-gnueabi-g++
[ -x "$CXX" ] || { echo "Cross-g++ fehlt: $CXX" >&2; exit 1; }
[ -d "$SYSROOT" ] || { echo "MADDE-Sysroot fehlt: $SYSROOT" >&2; exit 1; }
[ -x "$SIMQT/bin/moc" ] || { echo "Simulator-moc fehlt: $SIMQT/bin/moc" >&2; exit 1; }
MOC=$SIMQT/bin/moc
QTINC=$SYSROOT/usr/include/qt4
CXXFLAGS="--sysroot=$SYSROOT $COMMON $CXX_ONLY -I$QTINC"
for m in $QT4_MODULES; do CXXFLAGS="$CXXFLAGS -I$QTINC/$m"; done
LDFLAGS="--sysroot=$SYSROOT -static-libstdc++ -static-libgcc -Wl,-O1 -Wl,--as-needed -Wl,--dynamic-linker=/lib/ld-linux.so.3"
LIBS="-lQtDeclarative -lQtGui -lQtNetwork -lQtCore"

MK=$OUT/Makefile
{
    echo "CXX=$CXX"; echo "MOC=$MOC"
    echo "CXXFLAGS=$CXXFLAGS"; echo "LDFLAGS=$LDFLAGS"; echo "LIBS=$LIBS"; echo "SRC=$HERE"; echo
    objs=
    for s in $APP_SRC; do
        o=$(basename "$s" .cpp).o; objs="$objs $o"
        echo "$o: \$(SRC)/$s"; printf '\t$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@\n'
    done
    for h in $MOC_HEADERS; do
        n=$(basename "$h" .h); objs="$objs moc_$n.o"
        echo "moc_$n.cpp: \$(SRC)/$h"; printf '\t$(MOC) $< -o $@\n'
        echo "moc_$n.o: moc_$n.cpp"; printf '\t$(CXX) $(CXXFLAGS) -c $< -o $@\n'
    done
    echo "OBJS=$objs"; echo
    echo "all: harbour-quizduel"
    echo "harbour-quizduel: \$(OBJS)"; printf '\t$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)\n'
    echo "-include \$(wildcard *.d)"
} > "$MK"
nice make -C "$OUT" -j"$JOBS" all
echo "== gebaut $OUT/harbour-quizduel"
