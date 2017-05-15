%{!?directory:%define directory /usr}

%define buildroot %{_tmppath}/%{name}

Name:          tclmonetdb
Summary:       MonetDB MAPI library wrapper for Tcl
Version:       0.9.5
Release:       1
License:       Mozilla Public License, Version 2.0
Group:         Development/Libraries/Tcl
Source:        https://sites.google.com/site/tclmonetdb/tclmonetdb_0.9.5.zip
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
make DESTDIR=%{buildroot} pkglibdir=%{directory}/%{_lib}/tcl/%{name}%{version} install

%clean
rm -rf %buildroot

%files
%defattr(-,root,root)
%{directory}/%{_lib}/tcl
