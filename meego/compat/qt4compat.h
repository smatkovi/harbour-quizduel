// Force-included (-include) into every translation unit of the MeeGo build.
//
// Here stands only what Qt 4.7 has no spelling for at all, so that the shared
// sources in src/ keep the form they have on Sailfish. Everything else the
// port needs is a header next to this one (QJsonObject, QStandardPaths, ...),
// found because meego/compat comes first on the include path.
#pragma once
#include <QtGlobal>
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)

#include <QByteArray>
#include <QString>
#include <QStringList>

// Qt 5 builds these at compile time; here they are ordinary conversions. The
// sources use QStringLiteral in five hundred places, and every one of them is
// a plain UTF-8 string.
#ifndef QStringLiteral
#define QStringLiteral(str) QString::fromUtf8(str)
#endif
#ifndef QByteArrayLiteral
#define QByteArrayLiteral(str) QByteArray(str)
#endif

// Q_ENUM is Qt 5; Q_ENUMS does the same registration here. moc 4.7 swallows
// the declaration that follows Q_ENUMS, so whatever comes after it in a class
// must be something moc does not need to see.
#ifndef Q_ENUM
#define Q_ENUM(x) Q_ENUMS(x)
#endif

// QDebug::noquote() gibt es erst ab Qt 5.4. Hier sind Zeichenketten im
// Log ohnehin unquotiert, sobald ein Leerzeichen dazwischen darf -- space()
// ist also genau das, was gemeint war.
#ifndef noquote
#define noquote() space()
#endif

// Qt folgt Weiterleitungen erst ab 5.6 von selbst; die Marke dafuer gibt es
// hier nicht. Sie wird auf ein eigenes Attribut umgebogen: setzen schadet
// nichts, und der Fahrplanserver leitet nicht um. Kacheln, die weiterleiten,
// wuerden hier allerdings nicht ankommen.
#ifndef FollowRedirectsAttribute
#define FollowRedirectsAttribute User
#endif

#endif // QT_VERSION
