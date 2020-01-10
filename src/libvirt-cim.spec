# -*- rpm-spec -*-

Summary: A CIM provider for libvirt
Name: libvirt-cim
Version: 0.6.3
Release: 1%{?dist}%{?extra_release}
License: LGPLv2+
Group: Development/Libraries
Source: libvirt-cim-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
URL: http://libvirt.org/CIM/
Requires: libxml2 >= 2.6.0
Requires: libvirt >= 0.9.0
Requires: unzip
# either tog-pegasus or sblim-sfcb should provide cim-server
Requires: cim-server
BuildRequires: libcmpiutil >= 0.5.4
BuildRequires: tog-pegasus-devel
BuildRequires: libvirt-devel >= 0.9.0

# In RHEL5 uuid-devel is provided by e2fsprogs
%if 0%{?el5}
BuildRequires: e2fsprogs-devel
%else
BuildRequires: libuuid-devel
BuildRequires: libconfig-devel
%endif

BuildRequires: libxml2-devel
BuildRequires: libcmpiutil-devel
%if 0%{?fedora} >= 17 || 0%{?rhel} >= 7
BuildRequires: systemd-units
%endif
BuildConflicts: sblim-cmpi-devel

%description
Libvirt-cim is a CMPI CIM provider that implements the DMTF SVPC
virtualization model. The goal is to support most of the features
exported by libvirt itself, enabling management of multiple
platforms with a single provider.

%prep
%setup -q

%build
%configure --disable-werror
sed -i 's|^hardcode_libdir_flag_spec=.*|hardcode_libdir_flag_spec=""|g' libtool
sed -i 's|^runpath_var=LD_RUN_PATH|runpath_var=DIE_RPATH_DIE|g' libtool
make %{?_smp_mflags}

%install
rm -fr $RPM_BUILD_ROOT

make DESTDIR=$RPM_BUILD_ROOT install
rm -f $RPM_BUILD_ROOT%{_libdir}/*.la
rm -f $RPM_BUILD_ROOT%{_libdir}/*.a
rm -f $RPM_BUILD_ROOT%{_libdir}/cmpi/*.la
rm -f $RPM_BUILD_ROOT%{_libdir}/cmpi/*.a
rm -f $RPM_BUILD_ROOT%{_libdir}/libxkutil.so
mkdir -p $RPM_BUILD_ROOT/etc/ld.so.conf.d
echo %{_libdir}/cmpi > $RPM_BUILD_ROOT/etc/ld.so.conf.d/libvirt-cim.%{_arch}.conf
mkdir -p $RPM_BUILD_ROOT/etc/libvirt/cim

%clean
rm -fr $RPM_BUILD_ROOT

%pre
%define REGISTRATION %{_datadir}/%{name}/*.registration
%define SCHEMA %{_datadir}/%{name}/*.mof

%define INTEROP_REG %{_datadir}/%{name}/{RegisteredProfile,ElementConformsToProfile,ReferencedProfile}.registration
%define INTEROP_MOF %{_datadir}/%{name}/{ComputerSystem,HostSystem,RegisteredProfile,DiskPool,MemoryPool,NetPool,ProcessorPool,VSMigrationService,ElementConformsToProfile,ReferencedProfile,AllocationCapabilities}.mof

%define PGINTEROP_REG %{_datadir}/%{name}/{RegisteredProfile,ElementConformsToProfile,ReferencedProfile}.registration
%define PGINTEROP_MOF %{_datadir}/%{name}/{RegisteredProfile,ElementConformsToProfile,ReferencedProfile}.mof

%define CIMV2_REG %{_datadir}/%{name}/{HostedResourcePool,ElementCapabilities,HostedService,HostedDependency,ElementConformsToProfile,HostedAccessPoint}.registration
%define CIMV2_MOF %{_datadir}/%{name}/{HostedResourcePool,ElementCapabilities,HostedService,HostedDependency,RegisteredProfile,ComputerSystem,ElementConformsToProfile,HostedAccessPoint}.mof

# _If_ there is already a version of this installed, we must deregister
# the classes we plan to install in post, otherwise we may corrupt
# the pegasus repository.  This is convention in other provider packages
%{_datadir}/%{name}/provider-register.sh -d -t pegasus \
	-n root/virt \
	-r %{REGISTRATION} -m %{SCHEMA} >/dev/null 2>&1 || true

%post
/sbin/ldconfig

%{_datadir}/%{name}/install_base_schema.sh %{_datadir}/%{name}

%if 0%{?Fedora} >= 17 || 0%{?rhel} >= 7
    if [ "`systemctl is-active tog-pegasus.service`" = "active" ]
    then
        systemctl restart tog-pegasus.service
    fi

    if [ "`systemctl is-active sblim-sfcb.service`" = "active" ]
    then
        systemctl restart sblim-sfcb.service
    fi
%else
    /etc/init.d/tog-pegasus condrestart
%endif

if [ -x /usr/sbin/cimserver ]
then
%{_datadir}/%{name}/provider-register.sh -t pegasus \
	-n root/virt \
	-r %{REGISTRATION} -m %{SCHEMA} >/dev/null 2>&1 || true
%{_datadir}/%{name}/provider-register.sh -t pegasus \
        -n root/virt \
        -r %{REGISTRATION} -m %{SCHEMA} >/dev/null 2>&1 || true
%{_datadir}/%{name}/provider-register.sh -t pegasus \
        -n root/interop \
        -r %{INTEROP_REG} -m %{INTEROP_MOF} -v >/dev/null 2>&1 || true
%{_datadir}/%{name}/provider-register.sh -t pegasus \
        -n root/PG_InterOp \
        -r %{PGINTEROP_REG} -m %{PGINTEROP_MOF} -v >/dev/null 2>&1 || true
%{_datadir}/%{name}/provider-register.sh -t pegasus \
        -n root/cimv2\
        -r %{CIMV2_REG} -m %{CIMV2_MOF} -v >/dev/null 2>&1 || true
fi
if [ -x /usr/sbin/sfcbd ]
then
%{_datadir}/%{name}/provider-register.sh -t sfcb \
	-n root/virt \
	-r %{REGISTRATION} -m %{SCHEMA} >/dev/null 2>&1 || true
%{_datadir}/%{name}/provider-register.sh -t sfcb \
        -n root/virt \
        -r %{REGISTRATION} -m %{SCHEMA} >/dev/null 2>&1 || true
%{_datadir}/%{name}/provider-register.sh -t sfcb \
        -n root/interop \
        -r %{INTEROP_REG} -m %{INTEROP_MOF} -v >/dev/null 2>&1 || true
%{_datadir}/%{name}/provider-register.sh -t sfcb \
        -n root/PG_InterOp \
        -r %{PGINTEROP_REG} -m %{PGINTEROP_MOF} -v >/dev/null 2>&1 || true
%{_datadir}/%{name}/provider-register.sh -t sfcb \
        -n root/cimv2\
        -r %{CIMV2_REG} -m %{CIMV2_MOF} -v >/dev/null 2>&1 || true
fi

%preun
if [ -x /usr/sbin/cimserver ]
then
%{_datadir}/%{name}/provider-register.sh -d -t pegasus \
	-n root/virt \
	-r %{REGISTRATION} -m %{SCHEMA} >/dev/null 2>&1 || true
%{_datadir}/%{name}/provider-register.sh -d -t pegasus \
	-n root/interop \
	-r %{INTEROP_REG} -m %{INTEROP_MOF} >/dev/null 2>&1 || true
%{_datadir}/%{name}/provider-register.sh -d -t pegasus \
	-n root/PG_InterOp \
	-r %{PGINTEROP_REG} -m %{PGINTEROP_MOF} >/dev/null 2>&1 || true
%{_datadir}/%{name}/provider-register.sh -d -t pegasus \
	-n root/cimv2 \
	-r %{CIMV2_REG} -m %{CIMV2_MOF} >/dev/null 2>&1 || true
fi
if [ -x /usr/sbin/sfcbd ]
then
%{_datadir}/%{name}/provider-register.sh -d -t sfcb \
	-n root/virt \
	-r %{REGISTRATION} -m %{SCHEMA} >/dev/null 2>&1 || true
%{_datadir}/%{name}/provider-register.sh -d -t sfcb \
	-n root/interop \
	-r %{INTEROP_REG} -m %{INTEROP_MOF} >/dev/null 2>&1 || true
%{_datadir}/%{name}/provider-register.sh -d -t sfcb \
	-n root/PG_InterOp \
	-r %{PGINTEROP_REG} -m %{PGINTEROP_MOF} >/dev/null 2>&1 || true
%{_datadir}/%{name}/provider-register.sh -d -t sfcb \
	-n root/cimv2 \
	-r %{CIMV2_REG} -m %{CIMV2_MOF} >/dev/null 2>&1 || true
fi

%postun -p /sbin/ldconfig

%files
%defattr(-, root, root)
%{_sysconfdir}/libvirt/cim

%doc README COPYING doc/CodingStyle doc/SubmittingPatches
%doc base_schema/README.DMTF
%doc doc/*.html
%{_libdir}/lib*.so*
%{_libdir}/cmpi/lib*.so*
%{_datadir}/libvirt-cim
%{_datadir}/libvirt-cim/*.sh
%{_datadir}/libvirt-cim/*.mof
%{_datadir}/libvirt-cim/cimv*-interop_mof
%{_datadir}/libvirt-cim/cimv*-cimv2_mof
%{_datadir}/libvirt-cim/*.registration
%{_datadir}/libvirt-cim/cim_schema_*-MOFs.zip
%{_sysconfdir}/ld.so.conf.d/libvirt-cim.%{_arch}.conf
%config(noreplace) %{_sysconfdir}/libvirt-cim.conf

%changelog
* Wed Oct 28 2009 Richard Maciel <rmaciel@linux.vnet.ibm.com> - 0.1-1
- Added profile classes to the PG_InterOp namespace
* Fri Oct 26 2007 Daniel Veillard <veillard@redhat.com> - 0.1-1
- created
