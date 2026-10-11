import QtQuick 1.1
import com.nokia.meego 1.0
import "../Strings.js" as S

Page {
    id: page
    orientationLock: PageOrientation.LockPortrait
    property bool registerMode: false
    property bool busy: false
    property string errRaw: ""
    // net.locale ist ein NOTIFY-Property -> langIdx und alle S.t(...) aktualisieren
    // sich beim Sprachwechsel von selbst.
    property int langIdx: net ? codeIndex(net.locale) : 0

    property variant langNames: ["Deutsch","Svenska","English (UK)","English (US)","Dansk","Norsk","Suomi","Nederlands","Français","Italiano","Polski","Bahasa Melayu","Bahasa Indonesia","ไทย","Русский","العربية"]
    property variant langCodes: ["de_DE","sv_SE","en_GB","en_US","da_DK","nb_NO","fi_FI","nl_NL","fr_FR","it_IT","pl_PL","ms_MY","id_ID","th_TH","ru_RU","ar_SA"]
    function codeIndex(c) { for (var i = 0; i < langCodes.length; i++) if (langCodes[i] === c) return i; return 0 }

    function submit() {
        if (!net) return
        status.text = ""
        page.errRaw = ""
        page.busy = true
        if (registerMode) net.createAccount(username.text, password.text, email.text)
        else net.login(username.text, password.text)
    }

    Connections {
        target: net
        onUsersFound: {
            page.busy = false
            status.text = (payload.indexOf("null") >= 0) ? S.t(net.locale, "noAccount") : S.t(net.locale, "accountFound")
        }
        onLoggedIn: { page.busy = false; pageStack.push(Qt.resolvedUrl("HomePage.qml"), { account: username.text, payload: payload }) }
        onCreated:  { page.busy = false; pageStack.push(Qt.resolvedUrl("HomePage.qml"), { account: username.text, payload: payload }) }
        onFailed:   { page.busy = false; page.errRaw = message; status.text = S.errText(net.locale, message) }
    }

    SelectionDialog {
        id: langDialog
        titleText: S.t(net.locale, "language")
        selectedIndex: page.langIdx
        model: langNames
        onAccepted: { if (net) net.locale = langCodes[selectedIndex] }
    }

    Flickable {
        anchors.fill: parent
        contentHeight: col.height + 40
        Column {
            id: col
            x: 16
            width: page.width - 32
            spacing: 18

            Item { width: 1; height: 8 }
            Label { text: "QuizDuel"; font.pixelSize: 42; font.bold: true; color: "white" }
            Label { text: registerMode ? S.t(net.locale, "toRegister") : S.t(net.locale, "login"); font.pixelSize: 26; color: "#b0b0b0" }

            Button {
                width: parent.width
                text: S.t(net.locale, "language") + ": " + langNames[page.langIdx]
                onClicked: langDialog.open()
            }

            TextField {
                id: username
                width: parent.width
                placeholderText: S.t(net.locale, "username")
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
            }
            TextField {
                id: password
                width: parent.width
                placeholderText: S.t(net.locale, "password")
                echoMode: TextInput.Password
            }
            TextField {
                id: email
                width: parent.width
                visible: registerMode
                // E-Mail OPTIONAL (nur Passwort-Reset) — leer lassen ist erlaubt.
                placeholderText: S.t(net.locale, "email") + " — " + S.t(net.locale, "optional")
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText | Qt.ImhEmailCharactersOnly
            }

            Button {
                width: parent.width
                text: registerMode ? S.t(net.locale, "btnCreate") : S.t(net.locale, "btnLogin")
                // E-Mail NICHT mehr verlangt (optional).
                enabled: !page.busy && username.text.length > 0 && password.text.length > 0
                onClicked: submit()
            }
            Button {
                width: parent.width
                text: registerMode ? S.t(net.locale, "toLogin") : S.t(net.locale, "toRegister")
                onClicked: registerMode = !registerMode
            }
            Button {
                width: parent.width
                text: S.t(net.locale, "deviceAccounts")
                enabled: !page.busy
                onClicked: { if (net) { status.text = ""; page.busy = true; net.getUsersByDevice() } }
            }

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                running: page.busy
                visible: page.busy
            }
            Label {
                id: status
                width: parent.width
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                color: page.errRaw.length ? "#ff6b6b" : "#7eb3ff"
            }
            Label {
                visible: page.errRaw.length > 0
                width: parent.width
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 14
                color: "#707070"
                text: page.errRaw
            }
            Label {
                width: parent.width
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 18
                color: "#808080"
                text: S.t(net.locale, "device") + ": " + (net ? net.deviceId() : "—")
            }
        }
    }
}
