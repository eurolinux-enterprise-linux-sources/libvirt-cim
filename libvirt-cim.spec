# -*- rpm-spec -*-

Summary: A CIM provider for libvirt
Name: libvirt-cim
Version: 0.6.1
Release: 12%{?dist}%{?extra_release}
License: LGPLv2+
Group: Development/Libraries
Source: ftp://libvirt.org/libvirt-cim/libvirt-cim-%{version}.tar.gz
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

Patch0: libvirt-cim-0.6.1-leak.patch
Patch1: libvirt-cim-fix-an-incomplete-substitution-in-Makefile.patch
Patch2: libvirt-cim-0.6.1-state.patch
Patch3: libvirt-cim-0.6.1-bridge.patch
Patch4: libvirt-cim-0.6.1-domain-type.patch
Patch5: libvirt-cim-get_dominfo-Use-VIR_DOMAIN_XML_SECURE-more-wisely.patch
Patch6: libvirt-cim-0.6.1-shareable.patch
Patch7: libvirt-cim-0.6.1-dumpCore.patch
Patch8: libvirt-cim-0.6.1-use-top_srcdir-in-Makefile.patch
Patch9: libvirt-cim-0.6.1-Coverity-Resolve-RESOURCE_LEAK-parse_os.patch
Patch10: libvirt-cim-0.6.1-libxkutil-Improve-domain.os_info-cleanup.patch
Patch11: libvirt-cim-0.6.1-VSSD-Add-properties-for-arch-and-machine.patch
Patch12: libvirt-cim-0.6.1-fix-id-parsing-with-white-space.patch
Patch13: libvirt-cim-0.6.1-Complete-the-support-for-dumpCore.patch

# In RHEL5 uuid-devel is provided by e2fsprogs
%if 0%{?el5}
BuildRequires: e2fsprogs-devel
%else
BuildRequires: libuuid-devel
BuildRequires: libconfig-devel
%endif

BuildRequires: libxml2-devel
BuildRequires: libcmpiutil-devel
BuildConflicts: sblim-cmpi-devel

BuildRequires: libtool
BuildRequires: automake
BuildRequires: autoconf

%description
Libvirt-cim is a CMPI CIM provider that implements the DMTF SVPC
virtualization model. The goal is to support most of the features
exported by libvirt itself, enabling management of multiple
platforms with a single provider.

%prep
%setup -q
%patch0 -p1
%patch1 -p1
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
%patch12 -p1
%patch13 -p1

%build
# Rebuild makefiles changed by Patch1
#  since build machine may have different versions than originally produced,
#  run everything in the same order as autoconfiscate.sh to be safe
libtoolize --copy --force --automake
aclocal --force
autoheader --force
automake -i --add-missing --copy --foreign
autoconf --force

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

# Remove open-pegasus-specific providers installed in sfcb repository
# by older libvirt-cim packages
%{_datadir}/%{name}/provider-register.sh -d -t sfcb \
    -n root/PG_InterOp \
    -r %{PGINTEROP_REG} -m %{PGINTEROP_MOF} >/dev/null 2>&1 || true


%post
/sbin/ldconfig

%{_datadir}/%{name}/install_base_schema.sh %{_datadir}/%{name}

%if 0%{?fedora} >= 17 || 0%{?rhel} >= 7
    if [ "`systemctl is-active tog-pegasus.service`" = "active" ]
    then
        systemctl restart tog-pegasus.service
    fi

    if [ "`systemctl is-active sblim-sfcb.service`" = "active" ]
    then
        systemctl restart sblim-sfcb.service
    fi
%else
    if [ -x /etc/init.d/tog-pegasus ]
    then
        /etc/init.d/tog-pegasus condrestart
    fi
    if [ -x /etc/init.d/sblim-sfcb ]
    then
        /etc/init.d/sblim-sfcb condrestart
    fi
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
        -n root/cimv2\
        -r %{CIMV2_REG} -m %{CIMV2_MOF} -v >/dev/null 2>&1 || true
fi

%preun
if [ -x /usr/sbin/cimserver ]
then
#Deregister only in uninstall, do nothing during upgrade
	if [ "$1" = "0" ]; then
		echo "Deleting registered classes in libvirt-cim."
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
fi
if [ -x /usr/sbin/sfcbd ]
then
#Deregister only in uninstall, do nothing during upgrade
	if [ "$1" = "0" ]; then
		echo "Deleting registered classes in libvirt-cim."
		%{_datadir}/%{name}/provider-register.sh -d -t sfcb \
			-n root/virt \
			-r %{REGISTRATION} -m %{SCHEMA} >/dev/null 2>&1 || true
		%{_datadir}/%{name}/provider-register.sh -d -t sfcb \
			-n root/interop \
			-r %{INTEROP_REG} -m %{INTEROP_MOF} >/dev/null 2>&1 || true
		%{_datadir}/%{name}/provider-register.sh -d -t sfcb \
			-n root/cimv2 \
			-r %{CIMV2_REG} -m %{CIMV2_MOF} >/dev/null 2>&1 || true
	fi
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
* Mon Sep 1 2014 John Ferlan <jferlan@redhat.com> - 0.6.1.12.el6
- Complete the support for dumpCore rhbz#1119165

* Wed Apr 16 2014 John Ferlan <jferlan@redhat.com> - 0.6.1.11.el6
- Fix id parsing with white space (rhbz#1010283)

* Mon Jan 06 2014 John Ferlan <jferlan@redhat.com> - 0.6.1.10.el6
- VSSD: Add properties for arch and machine (rhbz#1046280)
- libxkutil: Improve domain.os_info cleanup
- Coverity: Resolve RESOURCE_LEAK - parse_os()

* Wed Aug 28 2013 John Ferlan <jferlan@redhat.com> - 0.6.1-9.el6
- Add shareable property to disk and dumpCore tag support (rhbz#1000937)
- Update spec file to more closely match upstream
- Fixed dates in changelog

* Thu Aug 22 2013 Michal Privoznik <mprivozn@redhat.com> - 0.6.1-8.el6
- Uninstall open-pegasus-specific providers from sfcb.eml (rhbz#833633)

* Fri Aug 09 2013 Michal Privoznik <mprivozn@redhat.com> - 0.6.1-7.el6
- libvirt-cim-get_dominfo-Use-VIR_DOMAIN_XML_SECURE-more-wisely.patch (rhbz#826179)
- Don't install open-pegasus' specific providers (rhbz#859122)

* Tue May 21 2013 John Ferlan <jferlan@redhat.com> - 0.6.1-6
- Force domain->type default to be UNKNOWN

* Wed May 15 2013 John Ferlan <jferlan@redhat.com> - 0.6.1-5
- Distinguish running or inactive state (rhbz#913164)
- Only support script on bridge for xen domains (rhbz#908083)

* Fri Oct 12 2012 Jiri Denemark <jdenemar@redhat.com> - 0.6.1-4.el6
- Fix an incomplete substitution in Makefile (rhbz#805892)
- Only deregister cim classes on uninstall (rhbz#864096)

* Tue Mar 13 2012 Daniel Veillard <veillard@redhat.com> - 0.6.1-3.el6
- fix leaks raised by Coverity (rhbz#750418)

* Thu Mar  8 2012 Daniel Veillard <veillard@redhat.com> - 0.6.1-2.el6
- do not require specifically tog-pegasus server (rhbz#799037)
- (de)register the schemas with sblim-sfcb if found

* Fri Feb 10 2012 Daniel Veillard <veillard@redhat.com> - 0.6.1-1.el6
- Rebase to libvirt-cim 0.6.1 rhbz#739154

* Thu Jan 12 2012 Daniel Veillard <veillard@redhat.com> - 0.6.0-1.el6
- Rebase to libvirt-cim 0.6.0 rhbz#739154
- Support libvirt domain events rhbz#739153
- Network QoS support rhbz#633338
- cpu cgroup support rhbz#739156

* Tue Aug 16 2011 Daniel Veillard <veillard@redhat.com> - 0.5.14-2.el6
- apply various fixes found by Coverity and upstream since 0.5.14
- Resolves: rhbz#728245

* Mon Jul 25 2011 Daniel Veillard <veillard@redhat.com> - 0.5.14-1.el6
- Rebase to libvirt-cim-0.5.14
- Resolves: rhbz#633337

* Thu Jul  7 2011 Daniel Veillard <veillard@redhat.com> - 0.5.13-1.el6
- Rebase to libvirt-cim-0.5.13
- Resolves: rhbz#633337

* Wed Apr 13 2011 Daniel Veillard <veillard@redhat.com> - 0.5.11-3.el6
- re-add require to tog-pegasus which went missing
- Resolves: rhbz#694749

* Mon Jan 17 2011 Daniel Veillard <veillard@redhat.com> - 0.5.11-2.el6
- add one missing patch in 0.5.11 rebase version
- Resolves: rhbz#633336

* Wed Jan 12 2011 Daniel Veillard <veillard@redhat.com> - 0.5.11-1.el6
- Rebase to 0.5.11 for RHEL-6.1
- needed to bump libcmpiutil requirement to 0.5.4 due to incompatibilities
- Resolves: rhbz#633336
- Resolves: rhbz#633331

* Fri May 14 2010 Daniel Veillard <veillard@redhat.com> - 0.5.8-3.el6
- fix a multi arch problem
- Resolves: rhbz#587232

* Fri Apr  9 2010 Daniel Veillard <veillard@redhat.com> - 0.5.8-2.el6
- add a set of patches coming from upstream after 0.5.8
- Resolves: rhbz#569570

* Wed Jan 20 2010 Daniel Veillard <veillard@redhat.com> - 0.5.8-1.el6
- update to new version 0.5.8
- Resolves: rhbz#557300

* Mon Oct 5 2009 Kaitlin Rupert <kaitlin@us.ibm.com> - 0.5.7-1
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
