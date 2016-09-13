Name:		xclip
Version:	0.13
Release:	1%{?dist}
License:	GPLv2+
Group:		Applications/System
Summary:	Command line clipboard grabber
URL:		https://github.com/astrand/xclip
Source0:	https://github.com/astrand/xclip/archive/%{version}.tar.gz
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires:  libXmu-devel, libICE-devel, libX11-devel, libXext-devel
Packager: Peter Åstrand <astrand@lysator.liu.se>

%description
xclip is a command line utility that is designed to run on any system with an
X11 implementation. It provides an interface to X selections ("the clipboard")
from the command line. It can read data from standard in or a file and place it
in an X selection for pasting into other X applications. xclip can also print
an X selection to standard out, which can then be redirected to a file or
another program.

%prep
%setup -q

%build
%configure
make CDEBUGFLAGS="$RPM_OPT_FLAGS" %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
make DESTDIR=$RPM_BUILD_ROOT install
make DESTDIR=$RPM_BUILD_ROOT install.man

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc COPYING README ChangeLog
%{_bindir}/xclip
%{_bindir}/xclip-copyfile
%{_bindir}/xclip-cutfile
%{_bindir}/xclip-pastefile
%{_mandir}/man1/xclip.1*
%{_mandir}/man1/xclip-copyfile.1*
