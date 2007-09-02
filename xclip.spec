Summary: Command line interface to the X11 clipboard
Name: xclip
Version: 0.10
Release: 1
License: GPL; see COPYING
Group: User Interface/X 
Source: xclip-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-buildroot
Packager: Peter Åstrand <astrand@lysator.liu.se>

%description
xclip is a command line utility that is designed to run on any system with an
X11 implementation. It provides an interface to X selections ("the clipboard")
from the command line. It can read data from standard in or a file and place it
in an X selection for pasting into other X applications. xclip can also print
an X selection to standard out, which can then be redirected to a file or
another program.

%prep
rm -rf $RPM_BUILD_ROOT

%setup
%build 
./configure --prefix=%{_prefix} --bindir=%{_bindir} --mandir=%{_mandir}
make

%install
make install DESTDIR=$RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc COPYING ChangeLog
%{_bindir}/xclip
%{_bindir}/xclip-copyfile
%{_bindir}/xclip-pastefile
%{_bindir}/xclip-cutfile
%{_mandir}/man1/xclip.1*

%post

%postun

%clean
rm -rf $RPM_BUILD_ROOT

