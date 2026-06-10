Name:           mygestures
Version:        4.1.1
Release:        1%{?dist}
Summary:        Pure Wayland/Evdev mouse gestures for Linux
License:        GPL-2.0-or-later
URL:            https://github.com/deters/mygestures
Source0:        %{url}/archive/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:  meson
BuildRequires:  ninja-build
BuildRequires:  cargo
BuildRequires:  gtk4-devel
BuildRequires:  libevdev-devel
BuildRequires:  gcc

Requires:       gtk4
Requires:       libevdev
Requires:       systemd

%description
Mouse gestures - "draw" commands using your mouse/touchscreen/touchpad.
Now written in Rust and completely independent of X11 and legacy drivers.

%prep
%autosetup

%build
%meson -Dudevdir=%{_udevrulesdir}
%meson_build

%install
%meson_install

%pre
# Ensure the 'input' group exists before installing the files
getent group input >/dev/null || groupadd -r input

%post
%udev_rules_update

%postun
%udev_rules_update

%files
%license COPYING
%doc README.md ChangeLog AUTHORS NEWS
# Set secure Set-Group-ID (SGID) permission on mygestures binary
%attr(2755, root, input) %{_bindir}/mygestures
%{_bindir}/gestos
%{_udevrulesdir}/99-mygestures.rules
%{_datadir}/applications/gestos.desktop
%{_datadir}/icons/hicolor/scalable/apps/gestos.svg
%{_datadir}/mygestures/mygestures.yaml

%changelog
* Wed Jun 10 2026 Lucas Augusto Deters <lucasdeters@gmail.com> - 4.1.1-1
- New release 4.1.1.
* Wed Jun 10 2026 Lucas Augusto Deters <lucasdeters@gmail.com> - 4.1.0-1
- New release 4.1.0.
* Wed Jun 10 2026 Lucas Augusto Deters <lucasdeters@gmail.com> - 4.0.0-1
- Initial Fedora packaging release with secure SGID permissions configuration.
