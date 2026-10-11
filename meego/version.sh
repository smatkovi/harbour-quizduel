#!/bin/sh
# Version für Binary und Paket: die SFOS-Fassung, der dieser Port folgt, plus
# eigene MeeGo-Revision.
HERE=$(cd "$(dirname "$0")/.." && pwd)
UP=$(sed -n 's/^Version: *//p' "$HERE/rpm/harbour-quizduel.spec" | head -1)
echo "${UP:-0.1.1}-meego1"
