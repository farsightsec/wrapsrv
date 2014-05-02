Name:		wrapsrv
Version:	0.3
Release:	1%{?dist}
Summary:	DNS SRV record command line wrapper

Group:		Applications/Networking
License:	Apache 2.0
URL:		https://github.com/farsightsec/wrapsrv
Source0:	https://dl.farsightsecurity.com/dist/wrapsrv/wrapsrv-0.3.tar.gz
BuildRoot:	%(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)

%description
wrapsrv adds support for connecting to a network service based on DNS SRV
record lookups to commands that do not support the DNS SRV record. wrapsrv
implements the weighted priority client connection algorithm in RFC 2782.
The specified command line will be invoked one or more times with %h and %p
sequences in the command line substituted for the hostname and port elements
of the selected SRV record.

%prep
%setup -q


%build
make %{?_smp_mflags}


%install
rm -rf %{buildroot}
make install PREFIX=/usr DESTDIR=%{buildroot}


%clean
rm -rf %{buildroot}


%files
/usr/bin/wrapsrv
/usr/share/man/man1/wrapsrv.1.gz


%changelog

