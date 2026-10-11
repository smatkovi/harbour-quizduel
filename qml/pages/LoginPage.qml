import QtQuick 2.0
import Sailfish.Silica 1.0
import "../Strings.js" as S
import "../Session.js" as Session

// Anmelden / Registrieren. Alle Beschriftungen über S.t(net.locale, key) — da
// net.locale ein NOTIFY-Property ist, schaltet die Oberfläche beim Sprachwechsel
// sofort um. Die Sprachnamen im Wähler bleiben Endonyme.
Page {
    id: page
    property var net
    property bool registerMode: false
    property bool busy: false
    property string lastErrorRaw: ""   // rohes Fehler-JSON (zum Antippen/Kopieren)

    property variant langNames: ["Deutsch","Svenska","English (UK)","English (US)","Dansk","Norsk","Suomi","Nederlands","Français","Italiano","Polski","Bahasa Melayu","Bahasa Indonesia","ไทย","Русский","العربية"]
    property variant langCodes: ["de_DE","sv_SE","en_GB","en_US","da_DK","nb_NO","fi_FI","nl_NL","fr_FR","it_IT","pl_PL","ms_MY","id_ID","th_TH","ru_RU","ar_SA"]
    function codeIndex(c) { for (var i = 0; i < langCodes.length; i++) if (langCodes[i] === c) return i; return 0 }

    function submit() {
        if (!net) return
        status.text = ""
        page.lastErrorRaw = ""
        page.busy = true
        if (registerMode) net.createAccount(username.text, password.text, email.text)
        else net.login(username.text, password.text)
    }

    Connections {
        target: net
        onUsersFound: {
            page.busy = false
            status.text = (users.length === 0) ? S.t(net.locale, "noAccount") : S.t(net.locale, "accountFound")
        }
        onLoggedIn: {
            page.busy = false
            // Erfolgreiche Anmeldung -> Zugangsdaten merken, wenn gewünscht (wie Storeman).
            if (typeof Schluesselbund !== "undefined" && Schluesselbund.verfuegbar && merken.checked)
                Schluesselbund.merken(username.text, password.text)
            pageStack.animatorReplace(Qt.resolvedUrl("HomePage.qml"), { net: net, account: username.text })
        }
        onCreated:  { page.busy = false; pageStack.animatorReplace(Qt.resolvedUrl("HomePage.qml"), { net: net, account: username.text }) }
        onFailed:   { page.busy = false; page.lastErrorRaw = message; status.text = S.errText(net.locale, message) }
    }

    // Schlüsselbund (Sailfish Secrets), wie Storeman: liegen Zugangsdaten vor, prüft das
    // Gerät beim Start selbst (Fingerabdruck/Code) und meldet automatisch an. Kein eigener
    // Sperrcode-Text; das Passwortformular ist nur Rückfall.
    Connections {
        target: (typeof Schluesselbund !== "undefined") ? Schluesselbund : null
        onGefunden: {
            username.text = benutzername
            password.text = passwort
            page.submit()
        }
        onFehlgeschlagen: { page.busy = false; status.text = "" }
    }

    Component.onCompleted: {
        if (typeof Schluesselbund === "undefined" || !Schluesselbund.verfuegbar)
            return
        merken.checked = Schluesselbund.hinterlegt()
        if (!registerMode && !Session.autoLoginDone && Schluesselbund.hinterlegt()) {
            Session.autoLoginDone = true
            status.text = S.t(net.locale, "unlocking")
            page.busy = true
            Schluesselbund.holen()
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge
        VerticalScrollDecorator {}

        Column {
            id: column
            width: page.width
            spacing: Theme.paddingMedium

            PageHeader { title: registerMode ? S.t(net.locale, "toRegister") : S.t(net.locale, "login") }

            ComboBox {
                id: langCombo
                width: parent.width
                label: S.t(net.locale, "language")
                description: S.t(net.locale, "langDesc")
                menu: ContextMenu {
                    Repeater {
                        model: langNames
                        MenuItem { text: modelData }
                    }
                }
                Component.onCompleted: currentIndex = net ? codeIndex(net.locale) : 0
                onCurrentIndexChanged: if (net && currentIndex >= 0) net.locale = langCodes[currentIndex]
            }

            TextField {
                id: username
                width: parent.width
                label: S.t(net.locale, "username")
                placeholderText: S.t(net.locale, "username")
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: password.focus = true
            }
            PasswordField {
                id: password
                width: parent.width
                label: S.t(net.locale, "password")
                placeholderText: S.t(net.locale, "password")
                EnterKey.iconSource: registerMode ? "image://theme/icon-m-enter-next" : "image://theme/icon-m-enter-accept"
                EnterKey.onClicked: { if (registerMode) email.focus = true; else submit() }
            }
            TextField {
                id: email
                width: parent.width
                visible: registerMode
                label: S.t(net.locale, "email")
                // E-Mail ist OPTIONAL (nur Passwort-Reset). Leer lassen ist erlaubt — so
                // scheitert die Registrierung nicht an einer schon vergebenen Adresse.
                placeholderText: S.t(net.locale, "optional")
                description: S.t(net.locale, "optional")
                inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                EnterKey.iconSource: "image://theme/icon-m-enter-accept"
                EnterKey.onClicked: submit()
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: registerMode ? S.t(net.locale, "btnCreate") : S.t(net.locale, "btnLogin")
                // E-Mail NICHT mehr verlangt (optional) — nur Benutzername + Passwort.
                enabled: !page.busy && username.text.length > 0 && password.text.length > 0
                onClicked: submit()
            }
            TextSwitch {
                id: merken
                visible: (typeof Schluesselbund !== "undefined") && Schluesselbund.verfuegbar && !registerMode
                text: S.t(net.locale, "rememberCreds")
                // Beim Ausschalten (und war etwas hinterlegt) sofort vergessen.
                onClicked: if (!checked && typeof Schluesselbund !== "undefined" && Schluesselbund.hinterlegt())
                               Schluesselbund.vergessen()
            }
            TextSwitch {
                text: S.t(net.locale, "toRegister")
                checked: registerMode
                onClicked: registerMode = !registerMode
            }
            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: S.t(net.locale, "deviceAccounts")
                enabled: !page.busy
                onClicked: { if (net) { status.text = ""; page.busy = true; net.getUsersByDevice() } }
            }

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                size: BusyIndicatorSize.Medium
                running: page.busy
            }
            Label {
                id: status
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                // Fehler rot, Infos (Konto gefunden …) im Hervorhebungston.
                color: page.lastErrorRaw.length ? Theme.errorColor : Theme.highlightColor
                visible: text.length > 0
            }
            // „Antippen zum Kopieren" + rohes JSON nur bei einem Fehler (zum Weitergeben).
            BackgroundItem {
                width: parent.width
                height: copyHint.height + Theme.paddingSmall
                visible: page.lastErrorRaw.length > 0
                onClicked: Clipboard.text = page.lastErrorRaw
                Label {
                    id: copyHint
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                    text: S.t(net.locale, "copyError")
                }
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                text: S.t(net.locale, "device") + ": " + (net ? net.deviceId() : "—")
            }
        }
    }
}
