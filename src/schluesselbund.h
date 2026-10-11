// Zugangsdaten (Benutzername + Passwort) im Geräteschloss von Sailfish hinterlegen —
// genau wie Storeman es mit dem OpenRepos-Login macht. Angepasst aus dem Briar-Port
// (harbour-briar/src/tresorsecrets.h): dort lag ein Passwort, hier ein kleines JSON
// {username,password}, sonst dieselbe Mechanik.
//
// Sailfish Secrets verschlüsselt das Geheimnis mit einem Schlüssel, den der Dienst hält,
// und gibt es nur an DIESE Anwendung heraus (OwnerOnlyMode), nachdem das Gerät den Benutzer
// erkannt hat (Fingerabdruck/Code). DeviceLock + KeepUnlocked: zugänglich, solange das Telefon
// entsperrt ist; ist es gesperrt, kommt niemand daran. Das Passwort liegt NICHT im Klartext.
//
// Alles unblockierend (Signale + eigene Frist), damit ein liegengelassener Systemdialog die
// Oberfläche nicht einfriert. Fehlt der Dienst oder lehnt er ab, bleibt es beim Tippen.
#ifndef SCHLUESSELBUND_H
#define SCHLUESSELBUND_H

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QStandardPaths>
#include <QString>
#include <QTimer>

#include <Sailfish/Secrets/createcollectionrequest.h>
#include <Sailfish/Secrets/deletecollectionrequest.h>
#include <Sailfish/Secrets/deletesecretrequest.h>
#include <Sailfish/Secrets/request.h>
#include <Sailfish/Secrets/secret.h>
#include <Sailfish/Secrets/secretmanager.h>
#include <Sailfish/Secrets/storedsecretrequest.h>
#include <Sailfish/Secrets/storesecretrequest.h>

class Schluesselbund : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool verfuegbar READ verfuegbar CONSTANT)
    Q_PROPERTY(bool laeuft READ laeuft NOTIFY laeuftChanged)

public:
    explicit Schluesselbund(QObject *parent = 0)
        : QObject(parent), m_holen(0), m_laeuft(false)
    {
        m_frist.setSingleShot(true);
        m_frist.setInterval(90000);
        connect(&m_frist, SIGNAL(timeout()), this, SLOT(fristAbgelaufen()));
    }

    /// Gibt es den Geheimnisdienst überhaupt?
    bool verfuegbar() const { return verwalter.isInitialized(); }

    /// Haben WIR dort je Zugangsdaten hinterlegt? (leere Markendatei neben dem Speicher;
    /// so steht der „mit dem Telefon anmelden"-Pfad nicht da, wo nie etwas abgelegt wurde.)
    Q_INVOKABLE bool hinterlegt() const { return QFile::exists(markenpfad()); }

    bool laeuft() const { return m_laeuft; }

    /// Zugangsdaten hinterlegen. Ablage an das Geräteschloss gebunden (DeviceLock/
    /// KeepUnlocked, OwnerOnly). Ergebnis als `gemerkt`.
    Q_INVOKABLE void merken(const QString &benutzername, const QString &passwort)
    {
        if (!verfuegbar()) { emit gemerkt(false); return; }
        QJsonObject o;
        o.insert(QStringLiteral("username"), benutzername);
        o.insert(QStringLiteral("password"), passwort);
        m_blob = QJsonDocument(o).toJson(QJsonDocument::Compact);

        Sailfish::Secrets::CreateCollectionRequest *anlegen =
                new Sailfish::Secrets::CreateCollectionRequest(this);
        anlegen->setManager(&verwalter);
        anlegen->setCollectionName(ABLAGE);
        anlegen->setCollectionLockType(Sailfish::Secrets::CreateCollectionRequest::DeviceLock);
        anlegen->setDeviceLockUnlockSemantic(Sailfish::Secrets::SecretManager::DeviceLockKeepUnlocked);
        anlegen->setAccessControlMode(Sailfish::Secrets::SecretManager::OwnerOnlyMode);
        anlegen->setStoragePluginName(Sailfish::Secrets::SecretManager::DefaultStoragePluginName);
        anlegen->setEncryptionPluginName(Sailfish::Secrets::SecretManager::DefaultEncryptionPluginName);
        anlegen->setUserInteractionMode(Sailfish::Secrets::SecretManager::SystemInteraction);
        setzeLaeuft(true);
        connect(anlegen, SIGNAL(statusChanged()), this, SLOT(ablageFertig()));
        anlegen->startRequest();
        m_frist.start();
    }

    /// Zugangsdaten holen. Das Gerät fragt (Fingerabdruck/Code); Antwort als
    /// `gefunden(username,password)` oder `fehlgeschlagen`.
    Q_INVOKABLE void holen()
    {
        if (!verfuegbar()) { emit fehlgeschlagen(); return; }
        if (m_laeuft) return;
        m_holen = new Sailfish::Secrets::StoredSecretRequest(this);
        m_holen->setManager(&verwalter);
        m_holen->setIdentifier(Sailfish::Secrets::Secret::Identifier(
                NAME, ABLAGE, Sailfish::Secrets::SecretManager::DefaultStoragePluginName));
        m_holen->setUserInteractionMode(
                m_zweiterVersuch ? Sailfish::Secrets::SecretManager::SystemInteraction
                                 : Sailfish::Secrets::SecretManager::PreventInteraction);
        setzeLaeuft(true);
        connect(m_holen, SIGNAL(statusChanged()), this, SLOT(holenFertig()));
        m_holen->startRequest();
        m_frist.start();
    }

    /// Wieder wegnehmen (Abmelden / nicht mehr merken wollen). Fragt nicht nach.
    Q_INVOKABLE void vergessen()
    {
        if (!verfuegbar()) { markeSetzen(false); return; }
        Sailfish::Secrets::DeleteSecretRequest weg;
        weg.setManager(&verwalter);
        weg.setIdentifier(Sailfish::Secrets::Secret::Identifier(
                NAME, ABLAGE, Sailfish::Secrets::SecretManager::DefaultStoragePluginName));
        weg.setUserInteractionMode(Sailfish::Secrets::SecretManager::PreventInteraction);
        weg.startRequest();
        weg.waitForFinished();
        markeSetzen(false);
    }

signals:
    void laeuftChanged();
    void gefunden(const QString &benutzername, const QString &passwort);
    void fehlgeschlagen();
    void gemerkt(bool erfolg);

private slots:
    void ablageFertig()
    {
        Sailfish::Secrets::CreateCollectionRequest *anlegen =
                qobject_cast<Sailfish::Secrets::CreateCollectionRequest *>(sender());
        if (!anlegen || anlegen->status() != Sailfish::Secrets::Request::Finished) return;
        const bool schonDa = anlegen->result().errorCode()
                == Sailfish::Secrets::Result::CollectionAlreadyExistsError;
        const bool gut = anlegen->result().code() == Sailfish::Secrets::Result::Succeeded;
        anlegen->deleteLater();
        if (!gut && !schonDa) { fertig(); emit gemerkt(false); return; }
        if (schonDa && !m_neuAngelegt) {
            // evtl. mit anderer Semantik angelegt -> einmal wegnehmen und neu anlegen.
            m_neuAngelegt = true;
            Sailfish::Secrets::DeleteCollectionRequest weg;
            weg.setManager(&verwalter);
            weg.setCollectionName(ABLAGE);
            weg.setStoragePluginName(Sailfish::Secrets::SecretManager::DefaultStoragePluginName);
            weg.setUserInteractionMode(Sailfish::Secrets::SecretManager::PreventInteraction);
            weg.startRequest();
            weg.waitForFinished();
            setzeLaeuft(false);
            const QByteArray blob = m_blob;
            // merken() neu anstoßen (baut m_blob erneut aus den Feldern -> hier direkt wieder setzen):
            m_blob = blob;
            Sailfish::Secrets::CreateCollectionRequest *neu =
                    new Sailfish::Secrets::CreateCollectionRequest(this);
            neu->setManager(&verwalter);
            neu->setCollectionName(ABLAGE);
            neu->setCollectionLockType(Sailfish::Secrets::CreateCollectionRequest::DeviceLock);
            neu->setDeviceLockUnlockSemantic(Sailfish::Secrets::SecretManager::DeviceLockKeepUnlocked);
            neu->setAccessControlMode(Sailfish::Secrets::SecretManager::OwnerOnlyMode);
            neu->setStoragePluginName(Sailfish::Secrets::SecretManager::DefaultStoragePluginName);
            neu->setEncryptionPluginName(Sailfish::Secrets::SecretManager::DefaultEncryptionPluginName);
            neu->setUserInteractionMode(Sailfish::Secrets::SecretManager::SystemInteraction);
            setzeLaeuft(true);
            connect(neu, SIGNAL(statusChanged()), this, SLOT(ablageFertig()));
            neu->startRequest();
            m_frist.start();
            return;
        }

        Sailfish::Secrets::Secret geheimnis(Sailfish::Secrets::Secret::Identifier(
                NAME, ABLAGE, Sailfish::Secrets::SecretManager::DefaultStoragePluginName));
        geheimnis.setData(m_blob);
        geheimnis.setType(Sailfish::Secrets::Secret::TypeBlob);

        Sailfish::Secrets::StoreSecretRequest *legen =
                new Sailfish::Secrets::StoreSecretRequest(this);
        legen->setManager(&verwalter);
        legen->setSecretStorageType(Sailfish::Secrets::StoreSecretRequest::CollectionSecret);
        legen->setSecret(geheimnis);
        legen->setUserInteractionMode(Sailfish::Secrets::SecretManager::SystemInteraction);
        connect(legen, SIGNAL(statusChanged()), this, SLOT(legenFertig()));
        legen->startRequest();
    }

    void legenFertig()
    {
        Sailfish::Secrets::StoreSecretRequest *legen =
                qobject_cast<Sailfish::Secrets::StoreSecretRequest *>(sender());
        if (!legen || legen->status() != Sailfish::Secrets::Request::Finished) return;
        const bool gut = legen->result().code() == Sailfish::Secrets::Result::Succeeded;
        legen->deleteLater();
        m_blob.clear();
        markeSetzen(gut);
        fertig();
        emit gemerkt(gut);
    }

    void holenFertig()
    {
        if (!m_holen || m_holen->status() != Sailfish::Secrets::Request::Finished) return;
        const bool gut = m_holen->result().code() == Sailfish::Secrets::Result::Succeeded;
        const QByteArray data = gut ? m_holen->secret().data() : QByteArray();
        m_holen->deleteLater();
        m_holen = 0;
        if (gut && !data.isEmpty()) {
            const QJsonObject o = QJsonDocument::fromJson(data).object();
            m_zweiterVersuch = false;
            fertig();
            emit gefunden(o.value(QStringLiteral("username")).toString(),
                          o.value(QStringLiteral("password")).toString());
            return;
        }
        if (!m_zweiterVersuch) {   // stiller Versuch gescheitert -> einmal mit Rückfrage
            m_zweiterVersuch = true;
            setzeLaeuft(false);
            holen();
            return;
        }
        m_zweiterVersuch = false;
        fertig();
        emit fehlgeschlagen();
    }

    void fristAbgelaufen()
    {
        if (!m_laeuft) return;
        if (m_holen) { m_holen->deleteLater(); m_holen = 0; }
        m_blob.clear();
        fertig();
        emit fehlgeschlagen();
    }

private:
    QString markenpfad() const
    {
        return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                + QLatin1String("/harbour-quizduel/schluesselbund");
    }
    void markeSetzen(bool gesetzt)
    {
        const QString pfad = markenpfad();
        if (!gesetzt) { QFile::remove(pfad); return; }
        QDir().mkpath(QFileInfo(pfad).absolutePath());
        QFile datei(pfad);
        if (datei.open(QIODevice::WriteOnly | QIODevice::Truncate)) datei.close();
    }
    void setzeLaeuft(bool wert) { if (m_laeuft != wert) { m_laeuft = wert; emit laeuftChanged(); } }
    void fertig() { m_frist.stop(); setzeLaeuft(false); }

    Sailfish::Secrets::SecretManager verwalter;
    Sailfish::Secrets::StoredSecretRequest *m_holen;
    QTimer m_frist;
    QByteArray m_blob;
    bool m_zweiterVersuch = false;
    bool m_neuAngelegt = false;
    bool m_laeuft;
    const QString ABLAGE = QStringLiteral("harbour-quizduel");
    const QString NAME = QStringLiteral("zugangsdaten");
};

#endif
