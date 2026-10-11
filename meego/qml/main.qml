import QtQuick 1.1
import com.nokia.meego 1.0
import "pages"

// Harmattan-Fenster. com.nokia.meego hat keinen Cover wie Silica; es bleibt der
// Seitenstapel. net (die C++-Brücke) ist als Kontexteigenschaft gesetzt.
PageStackWindow {
    id: appWindow
    showStatusBar: true
    showToolBar: true
    initialPage: LoginPage { }
    // Dunkles Thema inkl. Plattformhülle (harmattan-qml-ui-fallen: global setzbar).
    Component.onCompleted: theme.inverted = true
}
