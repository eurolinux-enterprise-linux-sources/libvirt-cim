/*
 * Copyright IBM Corp. 2007
 *
 * Authors:
 *  Dan Smith <danms@us.ibm.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "cmpidt.h"
#include "cmpift.h"
#include "cmpimacs.h"

#include <libcmpiutil/libcmpiutil.h>
#include <libcmpiutil/std_association.h>
#include "misc_util.h"

#include "Virt_ComputerSystem.h"
#include "Virt_HostSystem.h"

static const CMPIBroker *_BROKER;

static CMPIStatus vs_to_host(const CMPIObjectPath *ref,
                             struct std_assoc_info *info,
                             struct inst_list *list)
{
        CMPIStatus s = {CMPI_RC_OK, NULL};
        CMPIInstance *instance = NULL;

        if (!match_hypervisor_prefix(ref, info))
                goto out;

        s = get_domain_by_ref(_BROKER, ref, &instance);
        if (s.rc != CMPI_RC_OK)
                goto out;

        s = get_host(_BROKER, info->context, ref, &instance, false);
        if (s.rc == CMPI_RC_OK)
                inst_list_add(list, instance);

 out:
        return s;
}

static CMPIStatus host_to_vs(const CMPIObjectPath *ref,
                             struct std_assoc_info *info,
                             struct inst_list *list)
{
        CMPIStatus s = {CMPI_RC_OK, NULL};
        CMPIInstance *instance = NULL;
        CMPIObjectPath *vref = NULL;

        if (!STARTS_WITH(CLASSNAME(ref), "Linux_") &&
            !match_hypervisor_prefix(ref, info))
                goto out;

        s = get_host(_BROKER, info->context, ref, &instance, true);
        if (s.rc != CMPI_RC_OK)
                goto out;

        vref = convert_sblim_hostsystem(_BROKER, ref, info);
        if (vref == NULL)
                goto out;

        s = enum_domains(_BROKER, vref, list);

 out:
        return s;
}

LIBVIRT_CIM_DEFAULT_MAKEREF()

static char* antecedent[] = {
        "Xen_ComputerSystem",
        "KVM_ComputerSystem",
        "LXC_ComputerSystem",
        NULL
};

static char* dependent[] = {
        "Xen_HostSystem",
        "KVM_HostSystem",
        "LXC_HostSystem",
        "Linux_ComputerSystem",
        NULL
};

static char* assoc_classname[] = {
        "Xen_HostedDependency",
        "KVM_HostedDependency",
        "LXC_HostedDependency",
        NULL
};

static struct std_assoc _vs_to_host = {
        .source_class = (char**)&antecedent,
        .source_prop = "Antecedent",

        .target_class = (char**)&dependent,
        .target_prop = "Dependent",

        .assoc_class = (char**)&assoc_classname,

        .handler = vs_to_host,
        .make_ref = make_ref
};

static struct std_assoc _host_to_vs = {
        .source_class = (char**)&dependent,
        .source_prop = "Dependent",

        .target_class = (char**)&antecedent,
        .target_prop = "Antecedent",

        .assoc_class = (char**)&assoc_classname,

        .handler = host_to_vs,
        .make_ref = make_ref
};

static struct std_assoc *handlers[] = {
        &_vs_to_host,
        &_host_to_vs,
        NULL
};

STDA_AssocMIStub(,
                 Virt_HostedDependency,
                 _BROKER, 
                 libvirt_cim_init(), 
                 handlers);

/*
 * Local Variables:
 * mode: C
 * c-set-style: "K&R"
 * tab-width: 8
 * c-basic-offset: 8
 * indent-tabs-mode: nil
 * End:
 */
