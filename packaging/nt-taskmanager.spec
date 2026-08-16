# RPM build recipe for nt-taskmanager.
#
# Building from the source tree:
#
#     ./packaging/make-rpm.sh
#
# The release counter is written out rather than using %%autorelease.
# The latter is customary inside Fedora's own packaging but needs
# rpmautospec; outside of it the build fails on that silently. Whoever
# submits this package to Fedora swaps both (Release and %%changelog)
# for %%autorelease and %%autochangelog.

Name:           nt-taskmanager
Version:        1.0.0
Release:        1%{?dist}
Summary:        Processes, services and load in a classic shape

# SPDX identifier, as the Fedora guidelines have required since 2022.
License:        GPL-2.0-or-later
URL:            https://github.com/huppiflupp/nt-taskmanager
Source0:        %{url}/archive/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.20
BuildRequires:  gcc-c++
BuildRequires:  qt6-qtbase-devel
# For the checks in the %%check section.
BuildRequires:  desktop-file-utils
BuildRequires:  appstream

# Deliberately NO Requires on a graphics driver or library: NVML is
# loaded at runtime, and where it is missing the tab shows "no adapter"
# instead of failing. A hard Requires would have limited the package to
# machines with the NVIDIA driver installed.

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
- First release: processes, services, performance and networking
