%{!?directory:%define directory /usr}

%define buildroot %{_tmppath}/%{name}

Name:          tclmonetdb
Summary:       MonetDB MAPI library wrapper for Tcl
Version:       0.9.8
Release:       1
License:       Mozilla Public License, Version 2.0
Group:         Development/Libraries/Tcl
Source:        tclmonetdb_%{version}.tar.gz
URL:           https://sites.google.com/site/tclmonetdb
BuildRequires: autoconf
BuildRequires: make
BuildRequires: tcl-devel >= 8.6
Requires:      tcl >= 8.6
BuildRoot:     %{buildroot}

%description
tclmonetdb is a Tcl extension by using MAPI library to connect MonetDB.

This extension is using Tcl_LoadFile to load mapi library.

%prep
%setup -q -n %{name}

%build
./configure \
	--prefix=%{directory} \
	--exec-prefix=%{directory} \
	--libdir=%{directory}/%{_lib}
make 

%install
make DESTDIR=%{buildroot} pkglibdir=%{tcl_archdir}/%{name}%{version} install

%clean
rm -rf %buildroot

%files
%defattr(-,root,root)
%{tcl_archdir}
