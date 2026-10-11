// QuizDuel für MeeGo Harmattan (Nokia N9 / N950). Qt 4.7 hat weder SailfishApp
// noch QQuickView; die Oberfläche ist com.nokia.meego unter einem
// QDeclarativeView. Die NetBridge (Netz/Protokoll über den rustls-Holer) steht
// den Seiten als Kontexteigenschaft "net" zur Verfügung.
#include <QApplication>
#include <QDeclarativeContext>
#include <QDeclarativeEngine>
#include <QDeclarativeError>
#include <QDeclarativeView>
#include <QDir>
#include <QFileInfo>
#include <QTextCodec>
#include <QUrl>
#include <QtDebug>

#include "netbridge.h"

// QML/Daten nach der Installation; ein Start aus dem Quellbaum nimmt den Baum.
static QString dataDir()
{
    const QString installed = QLatin1String("/opt/harbour-quizduel");
    if (QFileInfo(installed + "/qml/main.qml").exists())
        return installed;
    const QString here = QCoreApplication::applicationDirPath();
    for (QDir dir(here); !dir.isRoot(); dir.cdUp()) {
        if (QFileInfo(dir.absolutePath() + "/meego/qml/main.qml").exists())
            return dir.absolutePath() + "/meego";
    }
    return installed;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QTextCodec::setCodecForTr(QTextCodec::codecForName("UTF-8"));
    QTextCodec::setCodecForCStrings(QTextCodec::codecForName("UTF-8"));
    app.setOrganizationName(QLatin1String("harbour-quizduel"));
    app.setApplicationName(QLatin1String("harbour-quizduel"));

    NetBridge net;
    const QString data = dataDir();

    QDeclarativeView view;
    view.rootContext()->setContextProperty(QLatin1String("net"), &net);
    view.engine()->addImportPath(data + QLatin1String("/qml"));
    view.setResizeMode(QDeclarativeView::SizeRootObjectToView);
    view.setSource(QUrl::fromLocalFile(data + QLatin1String("/qml/main.qml")));
    if (view.status() == QDeclarativeView::Error) {
        const QList<QDeclarativeError> errs = view.errors();
        for (int i = 0; i < errs.size(); ++i)
            qWarning() << "QML:" << errs.at(i).toString();
        return 1;
    }

    view.showFullScreen();
    return app.exec();
}
