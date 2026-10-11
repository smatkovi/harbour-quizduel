# QuizDuel für Sailfish OS — eigener Client gegen api.prod.magbackend.se.
# Kein Port: die App ist Unity/IL2CPP. Siehe ~/ps/quizduel-port/BEFUND.md.
TARGET = harbour-quizduel
CONFIG += sailfishapp
QT += network

# Icons installieren: sailfishapp.prf legt aus SAILFISHAPP_ICONS die Regeln
# icons/<größe>/harbour-quizduel.png -> /usr/share/icons/hicolor/<größe>/apps an.
# Passt zum %files-Glob icons/hicolor/*/apps/%{name}.png der .spec; ohne diese Zeile
# wird kein Icon installiert und rpmbuild bricht in %files ab.
SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172

SOURCES += src/harbour-quizduel.cpp \
           src/netclient.cpp \
           src/davinciprotocol.cpp
HEADERS += src/netclient.h \
           src/davinciprotocol.h \
           src/schluesselbund.h

# davinci-Modul: libcrypto (OpenSSL-EVP) für AES-256-CBC/PKCS7, zlib für die
# innere gzip-Schicht. Beide im SFOS-Grundsystem; davinci wird in die Binary
# kompiliert, KEINE eigene .so.
#
# link_pkgconfig-Falle: sailfishapp.prf linkt libsailfishapp selbst per
# "PKGCONFIG += sailfishapp" und lädt dafür link_pkgconfig. qmake lädt eine
# Feature-Datei aber nur EINMAL — und unser CONFIG += link_pkgconfig hat sie schon
# ausgewertet, als PKGCONFIG erst libcrypto+zlib enthielt. Die spätere Ergänzung
# "sailfishapp" der .prf wird dann NICHT mehr zu -lsailfishapp (Beleg: undefined
# reference auf SailfishApp::application beim Linken). Abhilfe: sailfishapp hier
# selbst in PKGCONFIG, damit unser link_pkgconfig alle drei zusammen expandiert.
CONFIG += link_pkgconfig
PKGCONFIG += libcrypto zlib sailfishapp sailfishsecrets

# Sailfish Secrets (Geräteschloss-Schlüsselbund, wie Storeman). Dazu muss die Binary eine
# PIE bleiben, sonst kann der Booster sie nicht per dlopen laden (Briar-Erfahrung: ab dem
# Bau mit sailfishsecrets kam sonst eine feste ET_EXEC heraus -> "Kann das Programm nicht
# dynamisch Laden"). Darum ortsunabhängig binden und main() ausführbar lassen.
QMAKE_LFLAGS += -pie -rdynamic

OTHER_FILES += qml/harbour-quizduel.qml \
               qml/cover/CoverPage.qml \
               qml/pages/LoginPage.qml \
               qml/pages/HomePage.qml \
               rpm/harbour-quizduel.spec \
               harbour-quizduel.desktop

DISTFILES += qml/pages/*.qml

harbour-quizduel.files = qml
harbour-quizduel.path = /usr/share/harbour-quizduel
INSTALLS += harbour-quizduel
