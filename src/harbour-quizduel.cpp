// QuizDuel für Sailfish OS — Einstieg.
#include <QtQuick>
#include <QQmlContext>
#include <sailfishapp.h>
#include "netclient.h"
#include "schluesselbund.h"

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    qmlRegisterType<NetClient>("harbour.quizduel", 1, 0, "NetClient");
    QScopedPointer<QQuickView> view(SailfishApp::createView());
    // Zugangsdaten im Geräteschloss (Sailfish Secrets), wie Storeman. Als Kontext-
    // Eigenschaft, da es EINE Instanz je App ist (kein qmlRegisterType nötig).
    Schluesselbund schluesselbund;
    view->rootContext()->setContextProperty(QStringLiteral("Schluesselbund"), &schluesselbund);
    view->setSource(SailfishApp::pathTo("qml/harbour-quizduel.qml"));
    view->show();
    return app->exec();
}
