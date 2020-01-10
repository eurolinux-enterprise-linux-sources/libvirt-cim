# -*- rpm-spec -*-

Summary: A CIM provider for libvirt
Name: libvirt-cim
Version: 0.6.3
Release: 19%{?dist}%{?extra_release}
License: LGPLv2+
Group: Development/Libraries
Source: ftp://libvirt.org/libvirt-cim/libvirt-cim-%{version}.tar.gz

# Update configure for aarch64 (bz #925923)
Patch1: libvirt-cim-aarch64.patch

# Patches 2 -> 10 were added as one submit for libvirt-cim.0.6.3-5.
# They are listed in the order they were added to the upstream
# libvirt-cim.git repository. Although Patch10 is the ultimate fix
# for BZ#1070346, the other patches were cumulative issues seen in
# since 0.6.3 was generated upstream and pulled into RHEL7. The only
# change not pulled in was commit id 'f18ba715' as it failed for the
# s390/s390x brew builds, perhaps due to $(SHELL) not being defined
# in whatever build environment is installed.

# libvirt-cim.spec.in: Don't install open-pegasus' specific providers
# Author: Michal Privoznik <mprivozn@redhat.com>
Patch2: libvirt-cim-0.6.3-19ffef8e.patch

# libvirt-cim.spec.in: Uninstall open-pegasus-specific providers from sfcb
# Author: Ján Tomko <jtomko@redhat.com>
Patch3: libvirt-cim-0.6.3-ee74ebc1.patch

# spec: Replace the path to the tar.gz file
# Author: John Ferlan <jferlan@redhat.com>
Patch4: libvirt-cim-0.6.3-3c3a541d.patch

# spec: Fix capitalization for version check
# Author: John Ferlan <jferlan@redhat.com>
Patch5: libvirt-cim-0.6.3-5d2626f6.patch

# build: Don't use /bin/sh unconditionally
# Author: Viktor Mihajlovski <mihajlov@linux.vnet.ibm.com>
Patch6: libvirt-cim-0.6.3-f18ba715.patch

# build: Fix incorrect provider registration in upgrade path
# Author: Viktor Mihajlovski <mihajlov@linux.vnet.ibm.com>
Patch7: libvirt-cim-0.6.3-1c7dfda2.patch

# build: Fix provider registration issues
# Author: Viktor Mihajlovski <mihajlov@linux.vnet.ibm.com>
Patch8: libvirt-cim-0.6.3-9c1d321b.patch

# schema: Fix class removal with Pegasus
# Author: Viktor Mihajlovski <mihajlov@linux.vnet.ibm.com>
Patch9: libvirt-cim-0.6.3-95f0d418.patch

# spec: Fix docs/*.html packaging issue
# Author: John Ferlan <jferlan@redhat.com>
Patch10: libvirt-cim-0.6.3-54778c78.patch

# Use of root/interop instead of root/PG_InterOp
# Author: John Ferlan <jferlan@redhat.com>
Patch11: libvirt-cim-0.6.3-a8cfd7dc.patch

# Patches 12 -> 15 were added as one submit for libvirt-cim.0.6.3-6.
# They are listed in order as there were added upstream. Since applying
# the changes without merge conflicts relies on previous changes being
# included, it was easier to make one submit for all 4 changes. Of the
# changes only Patch12 doesn't have an existing RHEL6* based bug, but
# it's an important enough change to be included.

# get_dominfo: Use VIR_DOMAIN_XML_SECURE more wisely
# Author: Michal Privoznik <mprivozn@redhat.com>
Patch12: libvirt-cim-0.6.3-7e164fbd.patch

# Add dumpCore tag support to memory
# Author: Xu Wang <gesaint@linux.vnet.ibm.com>
# Added to libvirt-cim.0.6.1-9 in RHEL 6.5 as part of BZ#1000937
Patch13: libvirt-cim-0.6.3-de03c66f.patch

# libxkutil: Improve domain.os_info cleanup
# Author: Viktor Mihajlovski <mihajlov@linux.vnet.ibm.com>
# Added to libvirt-cim.0.6.1-10 in RHEL 6.6 as BZ#1046280
# Added to libvirt-cim.0.6.1-9.el6_5.1 in RHEL 6.5.z as BZ#1055626
Patch14: libvirt-cim-0.6.3-0a742856.patch

# VSSD: Add properties for arch and machine
# Author: Viktor Mihajlovski <mihajlov@linux.vnet.ibm.com>
# Added to libvirt-cim.0.6.1-10 in RHEL 6.6 as BZ#1046280
# Added to libvirt-cim.0.6.1-9.el6_5.1 in RHEL 6.5.z as BZ#1055626
Patch15: libvirt-cim-0.6.3-6024403e.patch

# Patch 16 -> 18 follow-on patches 14 & 15 from upstream, while perhaps
# not specifically RHEL releated, they'll make it far easier to apply future
# patches that will be necessary.

# S390: Avoid the generation of default input and graphics
# Author: Viktor Mihajlovski <mihajlov@linux.vnet.ibm.com>
Patch16: libvirt-cim-0.6.3-1fae439d.patch

# libxkutil: Provide easy access to the libvirt capabilities
# uthor: Boris Fiuczynski <fiuczy@linux.vnet.ibm.com>
Patch17: libvirt-cim-0.6.3-3e6f1489.patch

# VSSM: Set default values based on libvirt capabilities on DefineSystem calls
# Author: Boris Fiuczynski <fiuczy@linux.vnet.ibm.com>
Patch18: libvirt-cim-0.6.3-117dabb9.patch

# Patches 19 & 20 fix a couple of memory leaks

# libxkutil: Plug memory leaks in device parsing
# Author: Viktor Mihajlovski <mihajlov@linux.vnet.ibm.com>
Patch19: libvirt-cim-0.6.3-f6b7eeaf.patch

# xml_parse_test: Call cleanup_dominfo before exiting
# Author: Viktor Mihajlovski <mihajlov@linux.vnet.ibm.com>
Patch20: libvirt-cim-0.6.3-605090b6.patch

# Patch 21 -> 27 add support for full function console. Again, although
# more s390 work, applying these make future patches easier to add

# VSMS: Set resource types for default devices
# Author: Viktor Mihajlovski <mihajlov@linux.vnet.ibm.com>
Patch21: libvirt-cim-0.6.3-ee84e10f.patch

# schema: New SVPC types for chardev/consoles
# Author: Thilo Boehm <tboehm@linux.vnet.ibm.com>
Patch22: libvirt-cim-0.6.3-93ea8130.patch

# libxkutil: Console Support
# Author: Thilo Boehm <tboehm@linux.vnet.ibm.com>
Patch23: libvirt-cim-0.6.3-fffbde4e.patch

# RASD: Schema and Provider Support for Console RASDs
# Author: Thilo Boehm <tboehm@linux.vnet.ibm.com>
Patch24: libvirt-cim-0.6.3-8a060e0d.patch

# Device: CIM_LogicalDevice for consoles
# Author: Viktor Mihajlovski <mihajlov@linux.vnet.ibm.com>
Patch25: libvirt-cim-0.6.3-a3649c21.patch

# VSMS: Support for domains with console devices
# Author: Thilo Boehm <tboehm@linux.vnet.ibm.com>
Patch26: libvirt-cim-0.6.3-21dea212.patch

# VSMS: add default console
# Author: Thilo Boehm <tboehm@linux.vnet.ibm.com>
Patch27: libvirt-cim-0.6.3-583ea685.patch

# Patch 28 -> 30 - add the patches for console fixes and enhancements

# libxkutil: Simplify XML handling of consoles
# Author: Viktor Mihajlovski <mihajlov@linux.vnet.ibm.com>
Patch28: libvirt-cim-0.6.3-f70a8ea0.patch

# Virt_Device: Add a device class for consoles
# Author: Viktor Mihajlovski <mihajlov@linux.vnet.ibm.com>
Patch29: libvirt-cim-0.6.3-ace5e8fd.patch

# KVMRedirectionSAP: Only return redirection SAPs for VNC graphics
# Author: Viktor Mihajlovski <mihajlov@linux.vnet.ibm.com>
Patch30: libvirt-cim-0.6.3-242ddaa6.patch

# Patch 31 -> 35 - Persistent Device Address Support

# RASD/schema: Add properties for device address representation
# Author: Viktor Mihajlovski <mihajlov@linux.vnet.ibm.com
Patch31: libvirt-cim-0.6.3-5940d2c8.patch

# libxkutil: Support for device addresses
# Author: Viktor Mihajlovski <mihajlov@linux.vnet.ibm.com>
Patch32: libvirt-cim-0.6.3-4f74864c.patch

# RASD: Support for device address properties
# Author: Viktor Mihajlovski <mihajlov@linux.vnet.ibm.com>
Patch33: libvirt-cim-0.6.3-a72ab39b.patch

# VSMS: Support for device addresses
# Author: Viktor Mihajlovski <mihajlov@linux.vnet.ibm.com>
Patch34: libvirt-cim-0.6.3-6bc7bfdf.patch

# VSMS: Improve device cleanup
# Author: Viktor Mihajlovski <mihajlov@linux.vnet.ibm.com>
Patch35: libvirt-cim-0.6.3-d75cae45.patch

# Patch 36 - Bugfix: Changed resource type value EMU

# libvirt-cim: Changed resource type value EMU
# Author: Daniel Hansel <daniel.hansel@linux.vnet.ibm.com>
Patch36: libvirt-cim-0.6.3-6f050582.patch

# Patch 37 -> 40 Resolve endianness issues

# VSDC: Fix endianess issues
# Author: Viktor Mihajlovski <mihajlov@linux.vnet.ibm.com>
Patch37: libvirt-cim-0.6.3-7e5f561c.patch

# VSSM: Fix endianness issue in domain properties
# Author: Thilo Boehm <tboehm@linux.vnet.ibm.com>
Patch38: libvirt-cim-0.6.3-9a4f2a32.patch

# libxkutil: clean entire device structure to avoid memory corruption
# Author: Viktor Mihajlovski <mihajlov@linux.vnet.ibm.com>
Patch39: libvirt-cim-0.6.3-14883f33.patch

# FilterEntry: Fix endianness issues
# Author: Thilo Boehm <tboehm@linux.vnet.ibm.com>
Patch40: libvirt-cim-0.6.3-2e9c18d6.patch

# Patch 41 - Bugfix: Added missing address element for filesystem 'disk'

# libxkutil: Added missing address element for filesystem 'disk'
# Author: Viktor Mihajlovski <mihajlov@linux.vnet.ibm.com>
Patch41: libvirt-cim-0.6.3-6a13c463.patch

# Patch 42 - 48 Coverity cleanups

# VSMS: Coverity cleanups
# Author: John Ferlan <jferlan@redhat.com>
Patch42: libvirt-cim-0.6.3-f9fc5821.patch

# libxkutil:pool_parsing: Coverity cleanups
# Author: John Ferlan <jferlan@redhat.com>
Patch43: libvirt-cim-0.6.3-7f3288be.patch

# libxkutil:device_parsing: Coverity cleanups
# Author: John Ferlan <jferlan@redhat.com>
Patch44: libvirt-cim-0.6.3-4013f9a0.patch

# libxkutil/xml_parse_test: Coverity cleanup
# Author: John Ferlan <jferlan@redhat.com>
Patch45: libvirt-cim-0.6.3-a6cbafc6.patch

# RAFP: Coverity cleanup
# Author: John Ferlan <jferlan@redhat.com>
Patch46: libvirt-cim-0.6.3-55d3f9fc.patch

# EAFP: Coverity cleanup
# Author: John Ferlan <jferlan@redhat.com>
Patch47: libvirt-cim-0.6.3-8eb5c1e7.patch

# Adjust sscanf format string
# Author: John Ferlan <jferlan@redhat.com>
Patch48: libvirt-cim-0.6.3-fb5d2fcf.patch

# Patch 49 - 50 rawio and sgio support

# Add rawio property support
# Author: Xu Wang <gesaint@linux.vnet.ibm.com>
Patch49: libvirt-cim-0.6.3-d9414e36.patch

# Add sgio property support
# Author: Xu Wang <gesaint@linux.vnet.ibm.com>
Patch50: libvirt-cim-0.6.3-1a91ecd3.patch

# Patch 51 - 59 Controller and Controller Pools support

# Add virtual controller device types
# Author: Xu Wang <gesaint@linux.vnet.ibm.com>
Patch51: libvirt-cim-0.6.3-4954aa8c.patch

# Parse/Store controller XML tags
# Author: Xu Wang <gesaint@linux.vnet.ibm.com>
Patch52: libvirt-cim-0.6.3-48b28b6a.patch

# Add virtual controller object definitions to mofs
# Author: Xu Wang <gesaint@linux.vnet.ibm.com>
Patch53: libvirt-cim-0.6.3-a16ca9d0.patch

# Set fields in mofs for Controller Device/RASD
# Author: Xu Wang <gesaint@linux.vnet.ibm.com>
Patch54: libvirt-cim-0.6.3-de34dda2.patch

# VSMS: Support for domains with controller devices
# Author: Xu Wang <gesaint@linux.vnet.ibm.com>
Patch55: libvirt-cim-0.6.3-ca8e81b3.patch

# Controller: Add associations for KVM_Controller
# Author: Xu Wang <gesaint@linux.vnet.ibm.com>
Patch56: libvirt-cim-0.6.3-53a4dff9.patch

# Add MOFS and change install for ControllerPools
# Author: John Ferlan <jferlan@redhat.com>
Patch57: libvirt-cim-0.6.3-222a3219.patch

# Add code and associations for ControllerPool
# Author: John Ferlan <jferlan@redhat.com>
Patch58: libvirt-cim-0.6.3-58d6e308.patch

# xmlgen: fix build issue
# Author: Pavel Hrdina <phrdina@redhat.com>
Patch59: libvirt-cim-0.6.3-2cbbac52.patch

# Patch 59 - 61 - Bug fixes for missing portions of previous patches
#                 and fix for build breaker with newer gcc

# Add disk device='lun' support
# Author: Xu Wang <gesaint@linux.vnet.ibm.com>
# Note: Avoids merge conflicts for Patch61
Patch60: libvirt-cim-0.6.3-5787acc15.patch

# Complete the support for dumpCore
# Author: Xu Wang <gesaint@linux.vnet.ibm.com
# Note: From Patch13
Patch61: libvirt-cim-0.6.3-43ea7135.patch

# list_util.h: Drop inline modifiers
# Author: Michal Privoznik <mprivozn@redhat.com>
# Fixes build issue with gcc 5.0
Patch62: libvirt-cim-0.6.3-63acad05.patch

BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
URL: http://libvirt.org/CIM/
Requires: libxml2 >= 2.6.0
Requires: libvirt >= 0.9.0
Requires: unzip
# either tog-pegasus or sblim-sfcb should provide cim-server
Requires: cim-server
BuildRequires: libtool
BuildRequires: autoconf
BuildRequires: automake
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

# Update configure for aarch64 (bz #925923)
%patch1 -p1
# Patches for installation issues (bz #1070346)
%patch2 -p1
%patch3 -p1
%patch4 -p1
%patch5 -p1
%patch6 -p1
%patch7 -p1
%patch8 -p1
%patch9 -p1
%patch10 -p1
%patch11 -p1
# Patches for adding properties for arch and machine
%patch12 -p1
%patch13 -p1
%patch14 -p1
%patch15 -p1
# Patches for 3390 Enablement
%patch16 -p1
%patch17 -p1
%patch18 -p1
# Patches for fixing memory leaks
%patch19 -p1
%patch20 -p1
# Patches for full function consoles
%patch21 -p1
%patch22 -p1
%patch23 -p1
%patch24 -p1
%patch25 -p1
%patch26 -p1
%patch27 -p1
# Patches for Console Fixes and Enhancements
%patch28 -p1
%patch29 -p1
%patch30 -p1
# Patches for Persistent Device Address Support
%patch31 -p1
%patch32 -p1
%patch33 -p1
%patch34 -p1
%patch35 -p1
# Bugfix: Changed resource type value EMU
%patch36 -p1
# Patches for Endianness issues
%patch37 -p1
%patch38 -p1
%patch39 -p1
%patch40 -p1
# Bugfix: Added missing address element for filesystem 'disk'
%patch41 -p1
# Patches for Coverity cleanups
%patch42 -p1
%patch43 -p1
%patch44 -p1
%patch45 -p1
%patch46 -p1
%patch47 -p1
%patch48 -p1
# Patches for sgio and rawio suport
%patch49 -p1
%patch50 -p1
# Patches for Controller and ControllerPool
%patch51 -p1
%patch52 -p1
%patch53 -p1
%patch54 -p1
%patch55 -p1
%patch56 -p1
%patch57 -p1
%patch58 -p1
%patch59 -p1
# Patches for bug fixes, potential build breakers
%patch60 -p1
%patch61 -p1
%patch62 -p1

%build
# Run the upstream commands from autoconfiscate.sh
libtoolize --copy --force --automake
aclocal --force
autoheader --force
automake -i --add-missing --copy --foreign
autoconf --force

# Need to update .revision in order to allow cimtest to recognize the
# controller and controller pools which were added as of upstream revision
# number 1312

echo "1316" > .revision
echo "63acad0" > .changeset

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
rm -rf $RPM_BUILD_ROOT%{_datadir}/doc/libvirt-cim-%{version}
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
* Mon May  4 2015 John Ferlan <jferlan@redhat.com> 0.6.3-19
- Add in upstream changes post bz#1075874 to fix a couple of issues
  with supporting patch and to avoid future potential build issue

* Mon May  4 2015 John Ferlan <jferlan@redhat.com> 0.6.3-18
- Changes to resolve bz#1075874
- Add code to support controller and controller pools

* Fri May  1 2015 John Ferlan <jferlan@redhat.com> 0.6.3-17
- Add in upstream submit to support bz#1075874
- rawio and sgio support

* Fri May  1 2015 John Ferlan <jferlan@redhat.com> 0.6.3-16
- Add in upstream submit to support bz#1075874
- Coverity cleanups

* Fri May  1 2015 John Ferlan <jferlan@redhat.com> 0.6.3-15
- Add in upstream submit to support bz#1075874
- Added missing address element for filesystem 'disk'

* Fri May  1 2015 John Ferlan <jferlan@redhat.com> 0.6.3-14
- Add in upstream submit to support bz#1075874
- Resolve endianness issues

* Fri May  1 2015 John Ferlan <jferlan@redhat.com> 0.6.3-13
- Add in upstream submit to support bz#1075874
- Changed resource type value EMU

* Fri May  1 2015 John Ferlan <jferlan@redhat.com> 0.6.3-12
- Add in upstream submit to support bz#1075874
- Persistent Device Address Support

* Fri May  1 2015 John Ferlan <jferlan@redhat.com> 0.6.3-11
- Add in upstream submit to support bz#1075874
- Console Fixes and Enhancements

* Fri May  1 2015 John Ferlan <jferlan@redhat.com> 0.6.3-10
- Add in upstream submit to support bz#1075874
- Support for full function consoles

* Fri May  1 2015 John Ferlan <jferlan@redhat.com> 0.6.3-9
- Add in upstream submit to support bz#1075874
- Remove memory leaks in XML device parsing

* Fri May  1 2015 John Ferlan <jferlan@redhat.com> 0.6.3-8
- Add in upstream submit to support bz#1075874
- Support for s390

* Tue Apr 28 2015 John Ferlan <jferlan@redhat.com> 0.6.3-7
- Build in RHEL 7.2 branch was failing, add upstream autoconfiscate.sh
  contents to the %build section, plus the prerequisite BuildRequires for
  libtool. Also generates the .revision and .changeset for each build.

* Thu Feb 27 2014 John Ferlan <jferlan@redhat.com> 0.6.3-6
- Add properties for arch and machine bz#1070337

* Thu Feb 27 2014 John Ferlan <jferlan@redhat.com> 0.6.3-5
- Use root/interop instead of root/PG_InterOp for tog-pegasus bz#1070346

* Fri Jan 24 2014 Daniel Mach <dmach@redhat.com> - 0.6.3-4
- Mass rebuild 2014-01-24

* Fri Dec 27 2013 Daniel Mach <dmach@redhat.com> - 0.6.3-3
- Mass rebuild 2013-12-27

* Tue Aug 6 2013 John Ferlan <jferlan@redhat.com> 0.6.3-2
- Replace the path to the source tar.gz file

* Thu Jul 25 2013 Daniel Veillard <veillard@redhat.com> 0.6.3-1
- update to 0.6.3 release

* Fri Jun 28 2013 Cole Robinson <crobinso@redhat.com> - 0.6.2-2
- Update configure for aarch64 (bz #925923)

* Mon Apr 15 2013 Daniel Veillard <veillard@redhat.com> 0.6.2-1
- update to 0.6.2 release

* Thu Feb 14 2013 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.6.1-4
- Rebuilt for https://fedoraproject.org/wiki/Fedora_19_Mass_Rebuild

* Thu Jul 19 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.6.1-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_18_Mass_Rebuild

* Mon Mar 12 2012 Daniel Veillard <veillard@redhat.com> - 0.6.1-2
- fix build in the presence of sblim-sfcb
- add schemas (de)registration with sfcb if found

* Mon Mar  5 2012 Daniel Veillard <veillard@redhat.com> - 0.6.1-1
- update to upstream release 0.6.1
- allow to use tog-pegasus or sblimfscb
- switch for systemctl for conditional restart of the server

* Fri Jan 13 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.5.14-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_17_Mass_Rebuild

* Mon Jul 25 2011 Daniel Veillard <veillard@redhat.com> - 0.5.14-1
- update to upstream release 0.5.14

* Wed Jul  6 2011 Daniel Veillard <veillard@redhat.com> - 0.5.13-1
- update to upstream release 0.5.13

* Tue Feb 08 2011 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.5.8-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_15_Mass_Rebuild

* Mon Dec 07 2009 Kaitlin Rupert <kaitlin@us.ibm.com> - 0.5.8-2
- Add missing namespace unreg bits for root/interop, root/cimv2 
- Remove additional reg call of root/virt from postinstall 
- Do not use /etc directly.  Use sysconfigdir instead
- Remove additional DESTDIR definition
- Fix Xen migration URI to not include 'system'
- Change net->name to net->source

* Wed Dec 02 2009 Kaitlin Rupert <kaitlin@us.ibm.com> - 0.5.8-1
- Updated to latest upstream source

* Mon Oct 05 2009 Kaitlin Rupert <kaitlin@us.ibm.com> - 0.5.7-1
- Updated to latest upstream source

* Sat Jul 25 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.5.6-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_12_Mass_Rebuild

* Tue Jul 14 2009 Kaitlin Rupert <kaitlin@us.ibm.com> - 0.5.6-1
- Updated to latest upstream source

* Tue Apr 21 2009 Kaitlin Rupert <kaitlin@us.ibm.com> - 0.5.5-1
- Updated to latest upstream source

* Wed Feb 25 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.5.4-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_11_Mass_Rebuild

* Tue Feb 17 2009 Kaitlin Rupert <kaitlin@us.ibm.com> - 0.5.4-1
- Updated to latest upstream source

* Thu Jan 15 2009 Kaitlin Rupert <kaitlin@us.ibm.com> - 0.5.3-1
- Updated to latest upstream source

* Mon Oct 06 2008 Kaitlin Rupert <kaitlin@us.ibm.com> - 0.5.2-1
- Updated to latest upstream source

* Tue Sep 23 2008 Kaitlin Rupert <kaitlin@us.ibm.com> - 0.5.1-5
- Added vsmigser_schema patch to remove dup method name from VSMigrationService
- Added mem_parse patch to set proper mem max_size and mem values
- Added mig_prof_ver patch to report the proper Migration Profile version
- Added hyp_conn_fail patch to fix when not connecting to hyp returns a failure
- Added rm_def_virtdev patch to remove default DiskRADSD virtual device
- Added rm_eafp_err patch to remove error status when EAFP no pool link exists
- Added sdc_unsup patch to make SDC not return unsup for RASD to AC case

* Wed Aug 27 2008 Kaitlin Rupert <kaitlin@us.ibm.com> - 0.5.1-4
- Added nostate patch to consider XenFV no state guests as running guests
- Added createsnap_override patch to add vendor defined values to CreateSnapshot
- Added add_shutdown_rsc patch to add support for shutdown operation
- Added vsmc_add_remove patch to expose Add/Remove resources via VSMC
- Added override_refconf patch to fix dup devs where ID matches refconf dev ID

* Thu Aug 07 2008 Dan Smith <danms@us.ibm.com> - 0.5.1-3
- Added infostore_trunc patch to fix infostore corruption
- Added vsss_paramname patch to fix VSSS parameter name
- Added vsss_logic patch to fix terminal memory snapshot logic
- Added /etc/libvirt/cim directory for infostore

* Thu Jul 31 2008 Dan Smith <danms@us.ibm.com> - 0.5.1-1
- Updated to latest upstream source

* Tue Jun 03 2008 Dan Smith <danms@us.ibm.com> - 0.5-1
- Updated to latest upstream source

* Fri May 30 2008 Dan Smith <danms@us.ibm.com> - 0.4-2
- Fixed schema registration to pick up ECTP in root/virt properly
- Fixed schema registration to include ReferencedProfile in interop
- Added RASD namespace fix

* Wed May 21 2008 Dan Smith <danms@us.ibm.com> - 0.4-1
- Updated to latest upstream source
- Added default disk pool configuration patch

* Fri Mar 14 2008 Dan Smith <danms@us.ibm.com> - 0.3-4
- Fixed loader config for 64-bit systems
- Added missing root/interop schema install
- Added RegisteredProfile.registration to install

* Fri Mar 07 2008 Dan Smith <danms@us.ibm.com> - 0.3-2
- Added KVM method enablement patch

* Mon Mar 03 2008 Dan Smith <danms@us.ibm.com> - 0.3-1
- Updated to latest upstream source

* Wed Feb 13 2008 Dan Smith <danms@us.ibm.com> - 0.2-1
- Updated to latest upstream source

* Thu Jan 17 2008 Dan Smith <danms@us.ibm.com> - 0.1-8
- Add ld.so.conf.d configuration

* Mon Jan 14 2008 Dan Smith <danms@us.ibm.com> - 0.1-7
- Update to offical upstream release
- Patch source to fix parallel make issue until fixed upstream

* Mon Jan 07 2008 Dan Smith <danms@us.ibm.com> - 0.1-3
- Remove RPATH on provider modules

* Mon Jan 07 2008 Dan Smith <danms@us.ibm.com> - 0.1-2
- Cleaned up Release
- Removed unnecessary Requires
- After install, condrestart pegasus
- Updated to latest source snapshot

* Fri Oct 26 2007 Daniel Veillard <veillard@redhat.com> - 0.1-1
- created
