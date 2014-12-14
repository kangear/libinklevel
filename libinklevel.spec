Name:           libinklevel
Version:        0.8.0
Release:        1%{?dist}
Summary:        Libinklevel is a library to check the ink level of ink jet printers 

Group:          System Environment/Libraries
License:        GPL
URL:            http://libinklevel.sourceforge.net/
Source0:        libinklevel-%{version}.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:  libieee1284-devel
Requires:       libieee1284

%description	
Libinklevel is a library to check the ink level of ink jet printers connected via USB or parallel port.
Libinklevel can also check the ink level of Canon network printers using the Canon proprietary BJNP TCP/IP protocol.



%package        devel
Summary:        Development files for %{name}
Group:          Development/Libraries
Requires:       %{name} = %{version}-%{release}

%description    devel
The %{name}-devel package contains libraries and header files for
developing applications that use %{name}.


%prep
%setup -q

%build
%configure 
sed -i 's|^hardcode_libdir_flag_spec=.*|hardcode_libdir_flag_spec=""|g' libtool
sed -i 's|^runpath_var=LD_RUN_PATH|runpath_var=DIE_RPATH_DIE|g' libtool
make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT
# find $RPM_BUILD_ROOT -name '*.la' -exec rm -f {} ';'


%clean
rm -rf $RPM_BUILD_ROOT


%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig


%files
%defattr(-,root,root,-)
%doc
%{_libdir}/*.so.*
%{_libdir}/*.la
%{_docdir}/*

%files devel
%defattr(-,root,root,-)
%doc
%{_includedir}/*
%{_libdir}/*.so
%{_libdir}/*.a


%changelog

