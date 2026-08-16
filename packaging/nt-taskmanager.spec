# RPM-Bauvorschrift fuer nt-taskmanager.
#
# Bauen aus dem Quellbaum heraus:
#
#     ./packaging/mach-rpm.sh
#
# Der Release-Zaehler steht hier ausgeschrieben und nicht als
# %autorelease. Letzteres ist in Fedoras eigener Paketverwaltung ueblich,
# braucht aber rpmautospec - ausserhalb davon scheitert der Bau daran
# wortlos. Wer das Paket bei Fedora einreicht, tauscht beides (Release
# und %changelog) gegen %autorelease und %autochangelog.

Name:           nt-taskmanager
Version:        1.0.0
Release:        1%{?dist}
Summary:        Processes, services and load in a classic shape

# SPDX-Bezeichner, wie es die Fedora-Richtlinien seit 2022 verlangen.
License:        GPL-2.0-or-later
URL:            https://github.com/huppiflupp/nt-taskmanager
Source0:        %{url}/archive/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.20
BuildRequires:  gcc-c++
BuildRequires:  qt6-qtbase-devel
# Fuer die Pruefungen im %%check-Abschnitt.
BuildRequires:  desktop-file-utils
BuildRequires:  appstream

# Absichtlich KEIN Requires auf einen Treiber oder eine Bibliothek fuer
# die Grafikkarte: NVML wird zur Laufzeit nachgeladen, und wo sie fehlt,
# zeigt der Reiter "no adapter" statt zu scheitern. Ein hartes Requires
# haette das Paket auf Rechner mit NVIDIA-Treiber eingeschraenkt.

%description
A system monitor laid out like the task manager of the mid-nineties: a
process list with CPU and memory usage, the services of the system, and
load meters with a running graph beside them.

The window draws nothing by itself. Its appearance comes from the widget
style and the colour scheme of the system, so it follows whatever theme
is set instead of imposing one.

Processes and CPU time come from /proc, services from systemd over
D-Bus, graphics card load from NVML on NVIDIA and from sysfs on AMD, and
throughput for Ethernet, Wi-Fi and Bluetooth from the kernel directly.
Nothing but Qt is required.

%prep
%autosetup

%build
%cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo
%cmake_build

%install
%cmake_install

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/%{name}.desktop
appstreamcli validate --no-net --explain \
    %{buildroot}%{_metainfodir}/io.github.huppiflupp.%{name}.metainfo.xml

%files
%license LICENSE
%doc README.md
%{_bindir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_metainfodir}/io.github.huppiflupp.%{name}.metainfo.xml

%changelog
* Sun Aug 16 2026 huppiflupp <huppiflupp@users.noreply.github.com> - 1.0.0-1
- Erste Fassung: Prozesse, Dienste, Auslastung und Netzwerk
