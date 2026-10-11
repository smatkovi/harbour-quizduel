Name:       harbour-quizduel
Summary:    QuizDuel-Client für Sailfish OS
Version:    0.6.7
Release:    1
License:    proprietary
URL:        https://github.com/smatkovi/harbour-quizduel
Source0:    %{name}-%{version}.tar.bz2
Requires:   sailfishsilica-qt5 >= 0.10.9
BuildRequires: pkgconfig(sailfishapp) >= 1.0.2
BuildRequires: pkgconfig(Qt5Core)
BuildRequires: pkgconfig(Qt5Qml)
BuildRequires: pkgconfig(Qt5Quick)
BuildRequires: pkgconfig(Qt5Network)
# davinci-Modul: AES-256-CBC/PKCS7 (OpenSSL-EVP) + innere gzip-Schicht.
# libcrypto liefert openssl-devel (mb2 build installiert es automatisch), zlib
# das bereits vorhandene zlib-devel. KEINE expliziten Requires: — RPM leitet die
# .so-Abhängigkeiten (libcrypto.so.3, libz.so.1, beides SFOS-Grundsystem) selbst ab.
BuildRequires: pkgconfig(libcrypto)
BuildRequires: pkgconfig(zlib)
# Sailfish Secrets: Zugangsdaten im Geräteschloss hinterlegen (wie Storeman).
BuildRequires: pkgconfig(sailfishsecrets)
BuildRequires: desktop-file-utils

%description
Eigener QuizDuel-Client (kein Port; die App ist Unity/IL2CPP) gegen das
magbackend. Siehe quizduel-port/BEFUND.md.

%prep
%setup -q -n %{name}-%{version}

%build
%qmake5
make %{?_smp_mflags}

%install
%qmake5_install

%files
%defattr(-,root,root,-)
%{_bindir}/%{name}
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png
