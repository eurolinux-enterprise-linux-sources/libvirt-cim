/*
 * Copyright IBM Corp. 2007
 *
 * Authors:
 *  Dan Smith <danms@us.ibm.com>
 *  Heidi Eckhart <heidieck@linux.vnet.ibm.com>
 *  Jay Gagnon <grendel@linux.vnet.ibm.com>
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
 * License along with this library.  If not, see
 * <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <libvirt/libvirt.h>

#include <cmpidt.h>
#include <cmpift.h>
#include <cmpimacs.h>

#include "cs_util.h"
#include <libcmpiutil/libcmpiutil.h>

#define USE_VIR_CONNECT_LIST_ALL_DOMAINS 0

#if LIBVIR_VERSION_NUMBER >= 9013 && USE_VIR_CONNECT_LIST_ALL_DOMAINS
int get_domain_list(virConnectPtr conn, virDomainPtr **_list)
{
        virDomainPtr *nameList = NULL;
        int n_names;
        int flags = VIR_CONNECT_LIST_DOMAINS_ACTIVE |
                    VIR_CONNECT_LIST_DOMAINS_INACTIVE;

        n_names = virConnectListAllDomains(conn,
                                           &nameList,
                                           flags);
        if (n_names > 0) {
                *_list = nameList;
        } else if (n_names == 0) {
                /* Since there are no elements, no domain ptrs to free
                 * but still must free the nameList returned
                 */
                free(nameList);
        }

        return n_names;
}
#else
int get_domain_list(virConnectPtr conn, virDomainPtr **_list)
{
        char **names = NULL;
        int n_names;
        int *ids = NULL;
        int n_ids;
        virDomainPtr *list = NULL;
        int i;
        int idx = 0;

        n_names = virConnectNumOfDefinedDomains(conn);
        n_ids = virConnectNumOfDomains(conn);

        if ((n_names < 0) || (n_ids < 0))
                goto out;

        list = calloc(n_names + n_ids, sizeof(virDomainPtr));
        if (!list)
                return 0;

        if (n_names > 0) {
                names = calloc(n_names, sizeof(char *));
                if (!names)
                        goto out;
                
                
#if LIBVIR_VERSION_NUMBER<=2000
                n_names = virConnectListDefinedDomains(conn, 
                                                       (const char **)names, 
                                                       n_names);
#else
                n_names = virConnectListDefinedDomains(conn, 
                                                       (char ** const)names, 
                                                       n_names);
#endif
                if (n_names < 0)
                        goto out;
        }

        for (i = 0; i < n_names; i++) {
                virDomainPtr dom;
                
                dom = virDomainLookupByName(conn, names[i]);
                if (dom)
                        list[idx++] = dom;

                free(names[i]);
        }

        if (n_ids > 0) {
                ids = calloc(n_ids, sizeof(int));
                if (!ids)
                        goto out;
                
                n_ids = virConnectListDomains(conn, ids, n_ids);
                if (n_ids < 0)
                        goto out;
        }

        for (i = 0; i < n_ids; i++) {
                virDomainPtr dom;

                dom = virDomainLookupByID(conn, ids[i]);
                if (dom)
                        list[idx++] = dom;
        }

 out:
        free(names);
        free(ids);

        if (idx == 0) {
                free(list);
                list = NULL;
        }

        *_list = list;

        return idx;
}
#endif /* LIBVIR_VERSION_NUMBER >= 0913 */

void set_instance_class_name(CMPIInstance *instance, char *name)
{
        CMSetProperty(instance, "CreationClassName",
                      (CMPIValue *)name, CMPI_chars);
}

void set_instances_class_name(CMPIInstance **list, 
                              virConnectPtr conn,
                              char *classname)
{
        int i;
        char *name;
        const char *type;
        char *prefix;

        type = virConnectGetType(conn);
        if (!type)
                prefix = "ERROR";
        else if (strstr(type, "Xen") == type)
                prefix = "Xen";
        else if (strstr(type, "qemu") == type) /* FIXME */
                prefix = "KVM";
        else
                prefix = "UNKNOWN";

        if (asprintf(&name, "%s_%s", prefix, classname) < 0)
                return;

        for (i = 0; list[i] != NULL; i++)
                set_instance_class_name(list[i], name);

        free(name);
}

/*
 * Local Variables:
 * mode: C
 * c-set-style: "K&R"
 * tab-width: 8
 * c-basic-offset: 8
 * indent-tabs-mode: nil
 * End:
 */
