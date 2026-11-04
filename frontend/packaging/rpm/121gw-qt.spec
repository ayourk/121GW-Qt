# SPDX-License-Identifier: MIT
# packaging/rpm/121gw-qt.spec: 121gw-qt RPM (Fedora). Part of 121gw-qt (ayourk/121GW-Qt, frontend/).
#
# Used by .github/workflows/frontend-rpm.yml. BuildRequires is the job's list of build
# dependencies (fed to dnf builddep). The job configures, builds, tests and installs into
# rpm-root/ (LICENSE included, under usr/share/licenses) before rpmbuild runs, and passes the version (cmake/GW121Version.cmake) as gw_version.
%{!?gw_version: %global gw_version 0.0.0}
# The program is built outside rpmbuild (see above), so there are no sources for a debuginfo package.
%global debug_package %{nil}

Name:           121gw-qt
Version:        %{gw_version}
Release:        1%{?dist}
Summary:        Front panel and MCP server for the EEVblog 121GW multimeter
License:        MIT
URL:            https://github.com/ayourk/121GW-Qt

BuildRequires:  gcc-c++
BuildRequires:  cmake
BuildRequires:  ninja-build
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtdeclarative-devel
BuildRequires:  qt6-qtconnectivity-devel
BuildRequires:  qt6-qtsvg-devel
# The QML modules load at run time, so the automatic soname dependencies miss them.
Requires:       qt6-qtdeclarative
Recommends:     bluez

%description
121gw-qt connects to an EEVblog 121GW multimeter over Bluetooth Low Energy and
shows its whole front panel live, presses its buttons, logs readings, manages
the meter's SD card (custom firmware) and serves an MCP server for AI
assistants on this computer only.

%install
cp -a %{_builddir}/rpm-root/. %{buildroot}/

%files
/usr/share/licenses/121gw-qt/LICENSE
/usr/bin/121gw-qt
/usr/share/applications/121gw-qt.desktop
/usr/share/icons/hicolor/scalable/apps/121gw-qt.svg
/usr/share/man/man1/121gw-qt.1*

%changelog
* %(LC_ALL=C date '+%a %b %d %Y') Aaron Yourk <ayourk@gmail.com> - %{gw_version}-1
- Automated build from CI
