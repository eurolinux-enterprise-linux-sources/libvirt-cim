/*
 * Copyright IBM Corp. 2008
 *
 * Authors:
 *  Kaitlin Rupert <karupert@us.ibm.com>
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>

#include "cmpidt.h"
#include "cmpift.h"
#include "cmpimacs.h"

#include "misc_util.h"
#include "svpc_types.h"
#include "device_parsing.h"
#include "cs_util.h"

#include <libcmpiutil/libcmpiutil.h>
#include <libcmpiutil/std_invokemethod.h>
#include <libcmpiutil/std_instance.h>

#include "Virt_KVMRedirectionSAP.h"

#define PROC_TCP4 "/proc/net/tcp"
#define PROC_TCP6 "/proc/net/tcp6"

const static CMPIBroker *_BROKER;

struct vnc_port {
        char *name;
        int port;
        int remote_port;
};

struct vnc_ports {
        struct vnc_port **list;
        unsigned int max;
        unsigned int cur;
};

static int inst_from_dom(const CMPIBroker *broker,
                         const CMPIObjectPath *ref,
                         struct vnc_port *port,
                         CMPIInstance *inst)
{
        char *sccn = NULL;
        char *id = NULL;
        char *pfx = NULL;
        uint16_t prop_val;
        int ret = 1;

        if (asprintf(&id, "%d:%d", port->port, port->remote_port) == -1) {
                CU_DEBUG("Unable to format name");
                ret = 0;
                goto out;
        }

        pfx = class_prefix_name(CLASSNAME(ref));
        sccn = get_typed_class(pfx, "ComputerSystem");

        if (id != NULL)
                CMSetProperty(inst, "Name",
                              (CMPIValue *)id, CMPI_chars);

        if (port->name != NULL)
                CMSetProperty(inst, "SystemName",
                              (CMPIValue *)port->name, CMPI_chars);

        if (sccn != NULL)
                CMSetProperty(inst, "SystemCreationClassName",
                              (CMPIValue *)sccn, CMPI_chars);

        if (id != NULL)
                CMSetProperty(inst, "ElementName",
                              (CMPIValue *)id, CMPI_chars);

        prop_val = (uint16_t)CIM_CRS_VNC;
        CMSetProperty(inst, "KVMProtocol",
                      (CMPIValue *)&prop_val, CMPI_uint16);

        if (port->remote_port < 0)
                prop_val = (uint16_t)CIM_SAP_INACTIVE_STATE;
        else if (port->remote_port == 0)
                prop_val = (uint16_t)CIM_SAP_AVAILABLE_STATE;
        else
                prop_val = (uint16_t)CIM_SAP_ACTIVE_STATE;
        CMSetProperty(inst, "EnabledState",
                      (CMPIValue *)&prop_val, CMPI_uint16);

 out:
        free(pfx);
        free(id);
        free(sccn);

        return ret;
}

static CMPIInstance *get_console_sap(const CMPIBroker *broker,
                                     const CMPIObjectPath *reference,
                                     virConnectPtr conn,
                                     struct vnc_port *port,
                                     CMPIStatus *s)

{ 
        CMPIInstance *inst = NULL;

        inst = get_typed_instance(broker,
                                  pfx_from_conn(conn),
                                  "KVMRedirectionSAP",
                                  NAMESPACE(reference));

        if (inst == NULL) {
                cu_statusf(broker, s,
                           CMPI_RC_ERR_FAILED,
                           "Unable to create instance");
                goto out;
        }

        if (inst_from_dom(broker, reference, port, inst) != 1) {
                cu_statusf(broker, s,
                           CMPI_RC_ERR_FAILED,
                           "Unable to get instance from domain");
        }

 out:
        return inst;
}

static CMPIStatus read_tcp_file(const CMPIBroker *broker,
                                const CMPIObjectPath *ref,
                                virConnectPtr conn,
                                struct vnc_ports ports,
                                struct inst_list *list,
                                FILE *fl)
{
        CMPIStatus s = {CMPI_RC_OK, NULL};
        CMPIInstance *inst;
        unsigned int lport = 0;
        unsigned int rport = 0;
        char *line = NULL;
        size_t len = 0;
        int val;
        int ret;
        int i;

        if (getline(&line, &len, fl) == -1) {
                cu_statusf(broker, 
                           &s,
                           CMPI_RC_ERR_FAILED,
                           "Failed to read from %s", 
                           fl);
                goto out;
        }

        while (getline(&line, &len, fl) > 0) {
                ret = sscanf(line, 
                             "%d: %*[^:]:%X %*[^:]:%X", 
                             &val, 
                             &lport, 
                             &rport);
                if (ret != 3) {
                        cu_statusf(broker, 
                                   &s,
                                   CMPI_RC_ERR_FAILED,
                                   "Unable to determine active sessions");
                        goto out;
                }

                for (i = 0; i < ports.max; i++) { 
                       if (lport != ports.list[i]->port)
                               continue;

                       ports.list[i]->remote_port = rport;
                       inst = get_console_sap(broker, 
                                              ref, 
                                              conn, 
                                              ports.list[i], 
                                              &s);
                       if ((s.rc != CMPI_RC_OK) || (inst == NULL))
                               goto out;

                       inst_list_add(list, inst);
                }
        }
 
 out:
        free(line);

        return s;
}

static CMPIStatus get_vnc_sessions(const CMPIBroker *broker,
                                   const CMPIObjectPath *ref,
                                   virConnectPtr conn,
                                   struct vnc_ports ports,
                                   struct inst_list *list)
{
        CMPIStatus s = {CMPI_RC_OK, NULL};
        CMPIInstance *inst;
        const char *path[2] = {PROC_TCP4, PROC_TCP6};
        FILE *tcp_info;
        int i, j;
        int error = 0;

        for (j = 0; j < 2; j++) {
                tcp_info = fopen(path[j], "r");
                if (tcp_info == NULL) {
                        cu_statusf(broker, &s,
                                   CMPI_RC_ERR_FAILED,
                                   "Failed to open %s: %m", tcp_info);
                        error++;
                        continue;
                }

                s = read_tcp_file(broker,
                                  ref,
                                  conn,
                                  ports,
                                  list,
                                  tcp_info);

                fclose(tcp_info);

                if (s.rc != CMPI_RC_OK)
                        error++; 
        }

        /* Handle any guests that were missed.  These guest don't have active 
           or enabled sessions. */
        for (i = 0; i < ports.max; i++) { 
                if (ports.list[i]->remote_port != -1)
                        continue;

                inst = get_console_sap(broker, ref, conn, ports.list[i], &s);
                if ((s.rc != CMPI_RC_OK) || (inst == NULL))
                        goto out;

                inst_list_add(list, inst);
        }

        if (error != 2)
                s.rc = CMPI_RC_OK; 

 out:
        return s;
}

static bool check_graphics(virDomainPtr dom,
                           struct domain **dominfo)
{
        int ret = 0;

        ret = get_dominfo(dom, dominfo);
        if (!ret) {
                CU_DEBUG("Unable to get domain info");
                return false;
        }

        if ((*dominfo)->dev_graphics == NULL) {
                CU_DEBUG("No graphics device associated with guest");
                return false;
        } 

        if (!STREQC((*dominfo)->dev_graphics->dev.graphics.type, "vnc")) {
                CU_DEBUG("Only vnc devices have console redirection sessions");
                return false;
        }

        return true;
}

static CMPIStatus return_console_sap(const CMPIObjectPath *ref,
                                     const CMPIResult *results,
                                     bool names_only)
{
        CMPIStatus s = {CMPI_RC_OK, NULL};
        struct inst_list list;

        inst_list_init(&list);

        s = enum_console_sap(_BROKER, ref, &list);
        if (s.rc != CMPI_RC_OK)
                goto out;

        if (names_only)
                cu_return_instance_names(results, &list);
        else
                cu_return_instances(results, &list);

 out:
        inst_list_free(&list);
        return s;
}

CMPIStatus enum_console_sap(const CMPIBroker *broker,
                            const CMPIObjectPath *ref,
                            struct inst_list *list)
{
        CMPIStatus s = {CMPI_RC_OK, NULL};
        virConnectPtr conn = NULL;
        virDomainPtr *domain_list;
        struct domain *dominfo = NULL;
        struct vnc_ports port_list;
        int count;
        int lport;
        int ret;
        int i;

        conn = connect_by_classname(broker, CLASSNAME(ref), &s);
        if (conn == NULL)
                return s;

        port_list.list = NULL;
        port_list.max = 0;
        port_list.cur = 0;

        count = get_domain_list(conn, &domain_list);
        if (count < 0) {
                cu_statusf(broker, &s,
                           CMPI_RC_ERR_FAILED,
                           "Failed to enumerate domains");
                goto out;
        } else if (count == 0)
                goto out;

        port_list.list = malloc(count * sizeof(struct vnc_port *));
        if (port_list.list == NULL) {
                cu_statusf(broker, &s,
                           CMPI_RC_ERR_FAILED,
                           "Unable to allocate guest port list");
                goto out;
        }

        for (i = 0; i < count; i++) {
                port_list.list[i] = malloc(sizeof(struct vnc_port));
                if (port_list.list[i] == NULL) {
                        cu_statusf(broker, &s,
                                   CMPI_RC_ERR_FAILED,
                                   "Unable to allocate guest port list");
                        goto out;
                }
                port_list.list[i]->name = NULL;
        }

        for (i = 0; i < count; i++) {
                if (!check_graphics(domain_list[i], &dominfo)) {
                        cleanup_dominfo(&dominfo);
                        continue;
                }

                ret = sscanf(dominfo->dev_graphics->dev.graphics.dev.vnc.port,
                             "%d",
                             &lport);
                if (ret != 1) {
                        cu_statusf(broker, &s,
                                   CMPI_RC_ERR_FAILED,
                                   "Unable to guest's console port");
                        cleanup_dominfo(&dominfo);
                        goto out;
                }

                port_list.list[port_list.cur]->name = strdup(dominfo->name);
                if (port_list.list[port_list.cur]->name == NULL) {
                        cu_statusf(broker, &s,
                                   CMPI_RC_ERR_FAILED,
                                   "Unable to allocate string");
                        cleanup_dominfo(&dominfo);
                        goto out;
                }

                port_list.list[port_list.cur]->port = lport;
                port_list.list[port_list.cur]->remote_port = -1;
                port_list.cur++;

                cleanup_dominfo(&dominfo);
        }

        port_list.max = port_list.cur;
        port_list.cur = 0;
 
        s = get_vnc_sessions(broker, ref, conn, port_list, list);
        if (s.rc != CMPI_RC_OK)
                goto out;

 out:
        free_domain_list(domain_list, count);
        free(domain_list);

        for (i = 0; i < count; i++) {
                free(port_list.list[i]->name);
                free(port_list.list[i]);
                port_list.list[i] = NULL;
        }
        free(port_list.list);

        virConnectClose(conn);
        return s;
}

CMPIStatus get_console_sap_by_name(const CMPIBroker *broker,
                                   const CMPIObjectPath *ref,
                                   const char *sys,
                                   CMPIInstance **_inst)
{
        virConnectPtr conn;
        virDomainPtr dom = NULL;
        CMPIStatus s = {CMPI_RC_OK, NULL};
        CMPIInstance *inst = NULL;
        struct domain *dominfo = NULL;
        struct vnc_port *port = NULL;
        const char *name = NULL;
        int lport;
        int rport;

        conn = connect_by_classname(broker, CLASSNAME(ref), &s);
        if (conn == NULL) {
                cu_statusf(broker, &s,
                           CMPI_RC_ERR_NOT_FOUND,
                           "No such instance");
                goto out;
        }

        dom = virDomainLookupByName(conn, sys);
        if (dom == NULL) {
                virt_set_status(broker, &s,
                                CMPI_RC_ERR_NOT_FOUND,
                                conn,
                                "No such instance (%s)",
                                sys);
                goto out;
        }

        if (!check_graphics(dom, &dominfo)) {
                virt_set_status(broker, &s,
                                CMPI_RC_ERR_FAILED,
                                conn,
                                "No console device for this guest");
                goto out;
        }

        if (cu_get_str_path(ref, "Name", &name) != CMPI_RC_OK) {
                cu_statusf(broker, &s,
                           CMPI_RC_ERR_NOT_FOUND,
                           "No such instance (Name)");
                goto out;
        }

        if (sscanf(name, "%d:%d", &lport, &rport) != 2) {
                cu_statusf(broker, &s,
                           CMPI_RC_ERR_FAILED,
                           "Unable to determine console port for guest '%s'",
                           sys);
                goto out;
        }

        port = malloc(sizeof(struct vnc_port));
        if (port == NULL) {
                cu_statusf(broker, &s,
                           CMPI_RC_ERR_FAILED,
                           "Unable to allocate guest port struct");
                goto out;
        }

        port->name = strdup(dominfo->name);
        if (port->name == NULL) {
                cu_statusf(broker, &s,
                           CMPI_RC_ERR_FAILED,
                           "Unable to allocate string");
                goto out;
        }

        port->port = lport;
        port->remote_port = rport;

        inst = get_console_sap(broker, ref, conn, port, &s);

        if (s.rc != CMPI_RC_OK)
                goto out;

        *_inst = inst;

 out:
        virDomainFree(dom);
        virConnectClose(conn);
        if (port != NULL)
                free(port->name);
        free(port);
        cleanup_dominfo(&dominfo);

        return s;
}

CMPIStatus get_console_sap_by_ref(const CMPIBroker *broker,
                                  const CMPIObjectPath *reference,
                                  CMPIInstance **_inst)
{
        CMPIStatus s = {CMPI_RC_OK, NULL};
        CMPIInstance *inst = NULL;
        const char *sys = NULL;

        if (cu_get_str_path(reference, "SystemName", &sys) != CMPI_RC_OK) {
                cu_statusf(broker, &s,
                           CMPI_RC_ERR_NOT_FOUND,
                           "No such instance (SystemName)");
                goto out;
        }

        s = get_console_sap_by_name(broker, reference, sys, &inst);
        if (s.rc != CMPI_RC_OK)
                goto out;

        s = cu_validate_ref(broker, reference, inst);
        if (s.rc != CMPI_RC_OK)
                goto out;

        *_inst = inst;

 out:
        return s;
}

static CMPIStatus EnumInstanceNames(CMPIInstanceMI *self,
                                    const CMPIContext *context,
                                    const CMPIResult *results,
                                    const CMPIObjectPath *reference)
{
        return return_console_sap(reference, results, true);
}

static CMPIStatus EnumInstances(CMPIInstanceMI *self,
                                const CMPIContext *context,
                                const CMPIResult *results,
                                const CMPIObjectPath *reference,
                                const char **properties)
{

        return return_console_sap(reference, results, false);
}

static CMPIStatus GetInstance(CMPIInstanceMI *self,
                              const CMPIContext *context,
                              const CMPIResult *results,
                              const CMPIObjectPath *ref,
                              const char **properties)
{
        CMPIStatus s;
        CMPIInstance *inst = NULL;

        s = get_console_sap_by_ref(_BROKER, ref, &inst);
        if (s.rc != CMPI_RC_OK)
                goto out;

        CMReturnInstance(results, inst);

 out:
        return s;
}

DEFAULT_CI();
DEFAULT_MI();
DEFAULT_DI();
DEFAULT_EQ();
DEFAULT_INST_CLEANUP();

STD_InstanceMIStub(, 
                   Virt_KVMRedirectionSAP, 
                   _BROKER,
                   libvirt_cim_init());


/*
 * Local Variables:
 * mode: C
 * c-set-style: "K&R"
 * tab-width: 8
 * c-basic-offset: 8
 * indent-tabs-mode: nil
 * End:
 */
