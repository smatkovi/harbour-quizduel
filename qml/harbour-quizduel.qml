import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.quizduel 1.0
// Pflicht, damit LoginPage/HomePage aus qml/pages/ als Typen auflösen. Ohne
// diese relative Verzeichnis-Einfuhr ist "LoginPage" kein Typ, initialPage
// schlägt fehl und die App zeigt nur eine weiße Fläche (wie harbour-deepfish,
// -audio-output, -advanced-camera es ebenfalls per import "pages" machen).
import "pages"

// Einstieg. Der NetClient (einziger Netz-/Protokollbesitzer) lebt hier einmal und
// wird den Seiten als Eigenschaft 'net' mitgegeben — so überlebt die Sitzung
// (deviceId, session) den Seitenwechsel Login -> Startseite.
ApplicationWindow {
    id: app

    NetClient { id: client }

    initialPage: Component { LoginPage { net: client } }
    cover: Qt.resolvedUrl("cover/CoverPage.qml")
    allowedOrientations: Orientation.All
}
