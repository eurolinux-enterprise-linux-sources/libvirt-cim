/*
 * Copyright IBM Corp. 2007
 *
 * Authors:
 *  Dan Smith <danms@us.ibm.com>
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
#include <stdbool.h>
#include <inttypes.h>
#include <time.h>
#include <sys/time.h>
#include <libvirt/libvirt.h>

#include "cmpidt.h"
#include "cmpift.h"
#include "cmpimacs.h"

#include "cs_util.h"
#include "misc_util.h"
#include "device_parsing.h"
#include "xmlgen.h"

#include <libcmpiutil/libcmpiutil.h>
#include <libcmpiutil/std_invokemethod.h>
#include <libcmpiutil/std_indication.h>
#include <libcmpiutil/std_instance.h>

#include "misc_util.h"
#include "infostore.h"

#include "Virt_VirtualSystemManagementService.h"
#include "Virt_ComputerSystem.h"
#include "Virt_ComputerSystemIndication.h"
#include "Virt_VSSD.h"
#include "Virt_RASD.h"
#include "Virt_HostSystem.h"
#include "Virt_DevicePool.h"
#include "Virt_Device.h"
#include "svpc_types.h"

#include "config.h"

#define XEN_MAC_PREFIX "00:16:3e"
#define KVM_MAC_PREFIX "00:1A:4A"
#define DEFAULT_XEN_WEIGHT 1024
#define BRIDGE_TYPE "bridge"
#define NETWORK_TYPE "network"
#define USER_TYPE "user"
#define DIRECT_TYPE "direct"
#define RASD_IND_CREATED "ResourceAllocationSettingDataCreatedIndication"
#define RASD_IND_DELETED "ResourceAllocationSettingDataDeletedIndication"
#define RASD_IND_MODIFIED "ResourceAllocationSettingDataModifiedIndication"

#if LIBVIR_VERSION_NUMBER < 9000
/* Network QoS support */
#define QOSCMD_BANDWIDTH2ID "_ROOT=$(tc class show dev %s | awk '($4==\"root\")\
{print $3}')\n tc class show dev %s | awk -v rr=$_ROOT -v bw=%uKbit \
'($4==\"parent\" && $5==rr && $13==bw){print $3}'\n"

#define QOSCMD_RMVM "MAC=%s; ME1=$(echo $MAC | awk -F ':' '{ print $1$2$3$4 }'); \
ME2=$(echo $MAC | awk -F ':' '{ print $5$6 }'); MI1=$(echo $MAC | awk -F ':' \
'{ print $1$2 }'); MI2=$(echo $MAC | awk -F ':' '{ print $3$4$5$6 }'); \
HDL=$(tc filter show dev %s | awk 'BEGIN {RS=\"\\nfilter\"} (NR>2)\
{m1=substr($24,1,2);m2=substr($24,3,2);m3=substr($24,5,2);m4=substr($24,7,2);\
m5=substr($20,1,2);m6=substr($20,3,2);printf(\"%%s:%%s:%%s:%%s:%%s:%%s %%s\\n\",\
m1,m2,m3,m4,m5,m6,$9)}' | awk -v mm=%s '($1==mm){print $2}'); \
U32=\"tc filter del dev %s protocol ip parent 1:0 prio 1 handle $HDL u32\"; \
$U32 match u16 0x0800 0xFFFF at -2 match u16 0x$ME2 0xFFFF at -4 match u32 \
0x$ME1 0xFFFFFFFF at -8 flowid %s; U32=\"tc filter del dev %s protocol ip parent \
ffff: prio 50 u32\"; $U32 match u16 0x0800 0xFFFF at -2 match u32 0x$MI2 \
0xFFFFFFFF at -12 match u16 0x$MI1 0xFFFF at -14 police rate %uKbit burst 15k \
drop\n"

#define QOSCMD_ADDVM "MAC=%s; ME1=$(echo $MAC | awk -F ':' '{ print $1$2$3$4 }'); \
ME2=$(echo $MAC | awk -F ':' '{ print $5$6 }'); MI1=$(echo $MAC | awk -F ':' \
'{ print $1$2 }'); MI2=$(echo $MAC | awk -F ':' '{ print $3$4$5$6 }'); \
U32=\"tc filter add dev %s protocol ip parent 1:0 prio 1 u32\"; \
$U32 match u16 0x0800 0xFFFF at -2 match u16 0x$ME2 0xFFFF at -4 match u32 \
0x$ME1 0xFFFFFFFF at -8 flowid %s; U32=\"tc filter add dev %s protocol ip parent \
ffff: prio 50 u32\"; $U32 match u16 0x0800 0xFFFF at -2 match u32 0x$MI2 \
0xFFFFFFFF at -12 match u16 0x$MI1 0xFFFF at -14 police rate %uKbit burst 15k \
drop\n"
#endif

const static CMPIBroker *_BROKER;

enum ResourceAction {
        RESOURCE_ADD,
        RESOURCE_DEL,
        RESOURCE_MOD,
};

#ifndef USE_LIBVIRT_EVENT
static bool trigger_indication(const CMPIBroker *broker,
                               const CMPIContext *context,
                               const char *base_type,
                               const CMPIObjectPath *ref)
{
        char *type;
        CMPIStatus s;

        type = get_typed_class(CLASSNAME(ref), base_type);

        s = stdi_trigger_indication(broker, context, type, NAMESPACE(ref));

        free(type);

        return s.rc == CMPI_RC_OK;
}
#else
static bool trigger_indication(const CMPIBroker *broker;
                               const CMPIContext *context,
                               const char *base_type,
                               const CMPIObjectPath *ref)
{
        return true;
}
#endif

#if LIBVIR_VERSION_NUMBER < 9000
/* Network QoS support */
static CMPIStatus add_qos_for_mac(const uint64_t qos,
                                  const char *mac,
                                  const char *bridge)
{
        CMPIStatus s = {CMPI_RC_OK, NULL};
        char *cmd = NULL;
        int j;
        FILE *pipe = NULL;
        char id[16] = ""; /* should be adequate to hold short tc class ids... */

        /* Find tc performance class id which matches requested qos bandwidth */
        j = asprintf(&cmd, QOSCMD_BANDWIDTH2ID, bridge,
                                                bridge,
                                                (unsigned int)qos);
        if (j == -1)
                goto out;
        CU_DEBUG("add_qos_for_mac(): cmd = %s", cmd);

        if ((pipe = popen(cmd, "r")) != NULL) {
                if (fgets(id, sizeof(id), pipe) != NULL) {
                        /* Strip off trailing newline */
                        char *p = index(id, '\n');
                        if (p) *p = '\0';
                 }
                 pclose(pipe);
        }
        free(cmd);
        CU_DEBUG("qos id = '%s'", id);

        /* Add tc performance class id for this MAC addr */
        j = asprintf(&cmd, QOSCMD_ADDVM, mac,
                                         bridge,
                                         id,
                                         bridge,
                                         (unsigned int)qos);
        if (j == -1)
                goto out;
        CU_DEBUG("add_qos_for_mac(): cmd = %s", cmd);

        if (WEXITSTATUS(system(cmd)) != 0)
                CU_DEBUG("add_qos_for_mac(): qos add failed.");

 out:
        free(cmd);
        return s;
}

/* Network QoS support */
static CMPIStatus remove_qos_for_mac(const uint64_t qos,
                                     const char *mac,
                                     const char *bridge)
{
        CMPIStatus s = {CMPI_RC_OK, NULL};
        char *cmd = NULL;
        int j;
        FILE *pipe = NULL;
        char id[16] = ""; /* should be adequate to hold short tc class ids... */

        /* Find tc performance class id which matches requested qos bandwidth */
        j = asprintf(&cmd, QOSCMD_BANDWIDTH2ID, bridge,
                                                bridge,
                                                (unsigned int)qos);
        if (j == -1)
                goto out;
        CU_DEBUG("remove_qos_for_mac(): cmd = %s", cmd);

        if ((pipe = popen(cmd, "r")) != NULL) {
                if (fgets(id, sizeof(id), pipe) != NULL) {
                        /* Strip off trailing newline */
                        char *p = index(id, '\n');
                        if (p) *p = '\0';
                 }
                 pclose(pipe);
        }
        free(cmd);
        CU_DEBUG("qos id = '%s'", id);

        /* Remove tc perf class id for this MAC; ignore errors when none exists */
        j = asprintf(&cmd, QOSCMD_RMVM, mac,
                                        bridge,
                                        mac,
                                        bridge,
                                        id,
                                        bridge,
                                        (unsigned int)qos);
        if (j == -1)
                goto out;
        CU_DEBUG("remove_qos_for_mac(): cmd = %s", cmd);

        if (WEXITSTATUS(system(cmd)) != 0)
                CU_DEBUG("remove_qos_for_mac(): qos remove failed; ignoring...");

 out:
        free(cmd);
        return s;
}
#endif

static CMPIStatus check_uuid_in_use(const CMPIObjectPath *ref,
                                    struct domain *domain)
{
        CMPIStatus s = {CMPI_RC_OK, NULL};
        virConnectPtr conn = NULL;
        virDomainPtr dom = NULL;

        conn = connect_by_classname(_BROKER, CLASSNAME(ref), &s);
        if (conn == NULL) {
                cu_statusf(_BROKER, &s,
                           CMPI_RC_ERR_FAILED,
                           "Error connecting to libvirt");
                goto out;
        }

        dom = virDomainLookupByUUIDString(conn, domain->uuid);
        if (dom != NULL) {
                cu_statusf(_BROKER, &s,
                           CMPI_RC_ERR_FAILED,
                           "Guest '%s' is already defined with UUID %s",
                           virDomainGetName(dom),
                           domain->uuid);
        }

 out:
        virDomainFree(dom);
        virConnectClose(conn);

        return s;
}

static CMPIStatus define_system_parse_args(const CMPIArgs *argsin,
                                           CMPIInstance **sys,
                                           const char *ns,
                                           CMPIArray **res,
                                           CMPIObjectPath **refconf)
{
        CMPIStatus s = {CMPI_RC_OK, NULL};

        if (cu_get_inst_arg(argsin, "SystemSettings", sys) != CMPI_RC_OK) {
                CU_DEBUG("No SystemSettings string argument");
                cu_statusf(_BROKER, &s,
                           CMPI_RC_ERR_INVALID_PARAMETER,
                           "Missing argument `SystemSettings'");
                goto out;
        }

        if (cu_get_array_arg(argsin, "ResourceSettings", res) !=
            CMPI_RC_OK) {
                CU_DEBUG("Failed to get array arg");
                cu_statusf(_BROKER, &s,
                           CMPI_RC_ERR_INVALID_PARAMETER,
                           "Missing argument `ResourceSettings'");
                goto out;
        }

        if (cu_get_ref_arg(argsin, "ReferenceConfiguration", refconf) !=
            CMPI_RC_OK) {
                CU_DEBUG("Did not get ReferenceConfiguration arg");
                *refconf = NULL;
                goto out;
        }
 out:
        return s;
}

static int xenpv_vssd_to_domain(CMPIInstance *inst,
                                struct domain *domain)
{
        int ret;
        const char *val;

        domain->type = DOMAIN_XENPV;

        free(domain->bootloader);
        ret = cu_get_str_prop(inst, "Bootloader", &val);
        if (ret == CMPI_RC_OK)
                domain->bootloader = strdup(val);
        else
                domain->bootloader = NULL;

        free(domain->bootloader_args);
        ret = cu_get_str_prop(inst, "BootloaderArgs", &val);
        if (ret == CMPI_RC_OK)
                domain->bootloader_args = strdup(val);
        else
                domain->bootloader_args = NULL;

        free(domain->os_info.pv.kernel);
        ret = cu_get_str_prop(inst, "Kernel", &val);
        if (ret == CMPI_RC_OK)
                domain->os_info.pv.kernel = strdup(val);
        else
                domain->os_info.pv.kernel = NULL;

        free(domain->os_info.pv.initrd);
        ret = cu_get_str_prop(inst, "Ramdisk", &val);
        if (ret == CMPI_RC_OK)
                domain->os_info.pv.initrd = strdup(val);
        else
                domain->os_info.pv.initrd = NULL;

        free(domain->os_info.pv.cmdline);
        ret = cu_get_str_prop(inst, "CommandLine", &val);
        if (ret == CMPI_RC_OK)
                domain->os_info.pv.cmdline = strdup(val);
        else
                domain->os_info.pv.cmdline = NULL;

        return 1;
}

static bool make_space(struct virt_device **list, int cur, int new)
{
        struct virt_device *tmp;

        tmp = calloc(cur + new, sizeof(*tmp));
        if (tmp == NULL)
                return false;

        if (*list) {
                memcpy(tmp, *list, sizeof(*tmp) * cur);
                free(*list);
        }

        *list = tmp;

        return true;
}

static bool fv_set_emulator(struct domain *domain,
                            const char *emu)
{
        if ((domain->type == DOMAIN_XENFV) && (emu == NULL))
                emu = XEN_EMULATOR;

        /* No emulator value to set */
        if (emu == NULL)
                return true;

        if (!make_space(&domain->dev_emu, 0, 1)) {
                CU_DEBUG("Failed to alloc disk list");
                return false;
        }

        cleanup_virt_device(domain->dev_emu);

        domain->dev_emu->type = CIM_RES_TYPE_EMU;
        domain->dev_emu->dev.emu.path = strdup(emu);
        domain->dev_emu->id = strdup("emulator");

        return true;
}

static bool system_has_kvm(const char *pfx)
{
        CMPIStatus s;
        virConnectPtr conn = NULL;
        char *caps = NULL;
        bool disable_kvm = get_disable_kvm();
        xmlDocPtr doc = NULL;
        xmlNodePtr node = NULL;
        int len;
        bool kvm = false;

        /* sometimes disable KVM to avoid problem in nested KVM */
        if (disable_kvm) {
                CU_DEBUG("Enter disable kvm mode!");
                goto out;
        }

        conn = connect_by_classname(_BROKER, pfx, &s);
        if ((conn == NULL) || (s.rc != CMPI_RC_OK)) {
                goto out;
        }

        caps = virConnectGetCapabilities(conn);
        if (caps != NULL) {
            len = strlen(caps) + 1;

            doc = xmlParseMemory(caps, len);
            if (doc == NULL) {
                CU_DEBUG("xmlParseMemory() call failed!");
                goto out;
            }

            node = xmlDocGetRootElement(doc);
            if (node == NULL) {
                CU_DEBUG("xmlDocGetRootElement() call failed!");
                goto out;
            }

            if (has_kvm_domain_type(node)) {
                    CU_DEBUG("The system support kvm!");
                    kvm = true;
            }
        }

out:
        free(caps);
        free(doc);

        virConnectClose(conn);

        return kvm;
}

static int bootord_vssd_to_domain(CMPIInstance *inst,
                                  struct domain *domain)
{
        int ret;
        CMPICount i;
        CMPICount bl_size;
        CMPIArray *bootlist;
        CMPIStatus s = { CMPI_RC_OK, NULL };
        CMPIData boot_elem;
        char **tmp_str_arr;

        for (i = 0; i < domain->os_info.fv.bootlist_ct; i++)
                free(domain->os_info.fv.bootlist[i]);

        ret = cu_get_array_prop(inst, "BootDevices", &bootlist);

        if (ret != CMPI_RC_OK) {
                CU_DEBUG("Failed to get BootDevices property");
                domain->os_info.fv.bootlist_ct = 0;
                goto out;
        }

        bl_size = CMGetArrayCount(bootlist, &s);
        if (s.rc != CMPI_RC_OK) {
                CU_DEBUG("Invalid BootDevices array size");
                return 0;
        }

        tmp_str_arr = (char **)realloc(domain->os_info.fv.bootlist,
                                       bl_size * sizeof(char *));

        if (tmp_str_arr == NULL) {
                CU_DEBUG("Could not alloc BootDevices array");
                return 0;
        }

        for (i = 0; i < bl_size; i++) {
                const char *str;

                boot_elem = CMGetArrayElementAt(bootlist,
                                                i,
                                                NULL);

                if (CMIsNullValue(boot_elem)) {
                        CU_DEBUG("Null BootDevices");
                        free(tmp_str_arr);
                        return 0;
                }

                str = CMGetCharPtr(boot_elem.value.string);

                if (str == NULL) {
                        CU_DEBUG("Could not extract char pointer from "
                                 "CMPIArray");
                        free(tmp_str_arr);
                        return 0;
                }

                tmp_str_arr[i] = strdup(str);
        }
        domain->os_info.fv.bootlist_ct = bl_size;
        domain->os_info.fv.bootlist = tmp_str_arr;

 out:

        return 1;
}

static int fv_vssd_to_domain(CMPIInstance *inst,
                             struct domain *domain,
                             const char *pfx)
{
        int ret;
        const char *val;

        if (STREQC(pfx, "KVM")) {
                if (system_has_kvm(pfx))
                        domain->type = DOMAIN_KVM;
                else
                        domain->type = DOMAIN_QEMU;
        } else if (STREQC(pfx, "Xen")) {
                domain->type = DOMAIN_XENFV;
        } else {
                CU_DEBUG("Unknown fullvirt domain type: %s", pfx);
                return 0;
        }

        ret = bootord_vssd_to_domain(inst, domain);
        if (ret != 1)
                return 0;

        ret = cu_get_str_prop(inst, "Emulator", &val);
        if (ret != CMPI_RC_OK)
                val = NULL;
        else if (disk_type_from_file(val) == DISK_UNKNOWN) {
                CU_DEBUG("Emulator path does not exist: %s", val);
                return 0;
        }

        if (!fv_set_emulator(domain, val))
                return 0;

        return 1;
}

static int lxc_vssd_to_domain(CMPIInstance *inst,
                              struct domain *domain)
{
        int ret;
        const char *val;

        domain->type = DOMAIN_LXC;

        ret = cu_get_str_prop(inst, "InitPath", &val);
        if (ret != CMPI_RC_OK)
                val = "/bin/false";

        free(domain->os_info.lxc.init);
        domain->os_info.lxc.init = strdup(val);

        return 1;
}

static bool default_graphics_device(struct domain *domain)
{
        if (domain->type == DOMAIN_LXC)
                return true;

        free(domain->dev_graphics);
        domain->dev_graphics = calloc(1, sizeof(*domain->dev_graphics));
        if (domain->dev_graphics == NULL) {
                CU_DEBUG("Failed to allocate default graphics device");
                return false;
        }

        domain->dev_graphics->dev.graphics.type = strdup("vnc");
        domain->dev_graphics->dev.graphics.dev.vnc.port = strdup("-1");
        domain->dev_graphics->dev.graphics.dev.vnc.host = strdup("127.0.0.1");
        domain->dev_graphics->dev.graphics.dev.vnc.keymap = strdup("en-us");
        domain->dev_graphics->dev.graphics.dev.vnc.passwd = NULL;
        domain->dev_graphics_ct = 1;

        return true;
}

static bool default_input_device(struct domain *domain)
{
        if (domain->type == DOMAIN_LXC)
                return true;

        free(domain->dev_input);
        domain->dev_input = calloc(1, sizeof(*domain->dev_input));
        if (domain->dev_input == NULL) {
                CU_DEBUG("Failed to allocate default input device");
                return false;
        }

        domain->dev_input->dev.input.type = strdup("mouse");

        if (domain->type == DOMAIN_XENPV) {
                domain->dev_input->dev.input.bus = strdup("xen");
        } else {
                domain->dev_input->dev.input.bus = strdup("ps2");
        }

        domain->dev_input_ct = 1;

        return true;
}

static bool add_default_devs(struct domain *domain)
{
        if (domain->dev_graphics_ct < 1) {
                if (!default_graphics_device(domain))
                        return false;
        }

        if (domain->dev_input_ct < 1) {
                if (!default_input_device(domain))
                        return false;
        }

        return true;
}

static int vssd_to_domain(CMPIInstance *inst,
                          struct domain *domain)
{
        uint16_t tmp;
        int ret = 0;
        const char *val;
        const char *cn;
        char *pfx = NULL;
        bool bool_val;
        bool fullvirt;
        CMPIObjectPath *opathp = NULL;


        opathp = CMGetObjectPath(inst, NULL);
        if (opathp == NULL) {
                CU_DEBUG("Got a null object path");
                return 0;
        }

        cn = CLASSNAME(opathp);
        pfx = class_prefix_name(cn);
        if (pfx == NULL) {
                CU_DEBUG("Unknown prefix for class: %s", cn);
                return 0;
        }

        ret = cu_get_str_prop(inst, "VirtualSystemIdentifier", &val);
        if (ret != CMPI_RC_OK)
                goto out;

        free(domain->name);
        domain->name = strdup(val);

        ret = cu_get_str_prop(inst, "UUID", &val);
        if (ret == CMPI_RC_OK) {
                free(domain->uuid);
                domain->uuid = strdup(val);
        }

        ret = cu_get_u16_prop(inst, "AutomaticShutdownAction", &tmp);
        if (ret != CMPI_RC_OK)
                tmp = 0;

        domain->on_poweroff = (int)tmp;

        ret = cu_get_u16_prop(inst, "AutomaticRecoveryAction", &tmp);
        if (ret != CMPI_RC_OK)
                tmp = CIM_VSSD_RECOVERY_NONE;

        domain->on_crash = (int)tmp;

        if (cu_get_bool_prop(inst, "IsFullVirt", &fullvirt) != CMPI_RC_OK)
                fullvirt = false;

        if (cu_get_bool_prop(inst, "EnableACPI", &bool_val) != CMPI_RC_OK) {
                /* Always set for XenFV and KVM guests */
                if (fullvirt || STREQC(pfx, "KVM"))
                        bool_val = true;
                else
                        bool_val = false;
        }

        domain->acpi = bool_val;

        if (cu_get_bool_prop(inst, "EnableAPIC", &bool_val) != CMPI_RC_OK) {
                /* Always set for XenFV guests */
                if (fullvirt && !STREQC(pfx, "KVM"))
                        bool_val = true;
                else
                        bool_val = false;
        }

        domain->apic = bool_val;

        if (cu_get_bool_prop(inst, "EnablePAE", &bool_val) != CMPI_RC_OK) {
                /* Always set for XenFV guests */
                if (fullvirt && !STREQC(pfx, "KVM"))
                        bool_val = true;
                else
                        bool_val = false;
        }

        domain->pae = bool_val;

        if (cu_get_u16_prop(inst, "ClockOffset", &tmp) == CMPI_RC_OK) {
                if (tmp == VSSD_CLOCK_UTC)
                        domain->clock = strdup("utc");
                else if (tmp == VSSD_CLOCK_LOC)
                        domain->clock = strdup("localtime");
                else {
                        CU_DEBUG("Unknown clock offset value %hi", tmp);
                        ret = 0;
                        goto out;
                }
        }

        if (fullvirt || STREQC(pfx, "KVM"))
                ret = fv_vssd_to_domain(inst, domain, pfx);
        else if (STREQC(pfx, "Xen"))
                ret = xenpv_vssd_to_domain(inst, domain);
        else if (STREQC(pfx, "LXC"))
                ret = lxc_vssd_to_domain(inst, domain);
        else {
                CU_DEBUG("Unknown domain prefix: %s", pfx);
        }

 out:
        free(pfx);

        return ret;
}

static const char *_default_network(CMPIInstance *inst,
                                    const char* ns)
{
        CMPIInstance *pool;
        CMPIObjectPath *op;
        CMPIStatus s;
        const char *poolid = NULL;

        op = CMGetObjectPath(inst, &s);
        if ((op == NULL) || (s.rc != CMPI_RC_OK)) {
                CU_DEBUG("Failed to get path for instance: %s",
                         CMGetCharPtr(s.msg));
                return NULL;
        }

        s = CMSetNameSpace(op, ns);
        if (s.rc != CMPI_RC_OK) {
                CU_DEBUG("Failed to set the namespace of net objectpath");
                return NULL;
        }

        CU_DEBUG("No PoolID specified, looking up default network pool");
        pool = default_device_pool(_BROKER, op, CIM_RES_TYPE_NET, &s);
        if ((pool == NULL) || (s.rc != CMPI_RC_OK)) {
                CU_DEBUG("Failed to get default network pool: %s",
                         CMGetCharPtr(s.msg));
                return NULL;
        }

        if (cu_get_str_prop(pool, "InstanceID", &poolid) != CMPI_RC_OK) {
                CU_DEBUG("Unable to get pool's InstanceID");
        }

        return poolid;
}

static const char *_net_rand_mac(const CMPIObjectPath *ref)
{
        int r;
        int ret;
        unsigned int s;
        char *mac = NULL;
        const char *_mac = NULL;
        CMPIString *str = NULL;
        CMPIStatus status;
        struct timeval curr_time;
        const char *mac_prefix = NULL;
        char *cn_prefix = NULL;

        ret = gettimeofday(&curr_time, NULL);
        if (ret != 0)
                goto out;

        srand(curr_time.tv_usec);
        s = curr_time.tv_usec;
        r = rand_r(&s);

        cn_prefix = class_prefix_name(CLASSNAME(ref));

        if (STREQ(cn_prefix, "KVM"))
            mac_prefix = KVM_MAC_PREFIX;
        else
            mac_prefix = XEN_MAC_PREFIX;

        free(cn_prefix);

        ret = asprintf(&mac,
                       "%s:%02x:%02x:%02x",
                       mac_prefix,
                       r & 0xFF,
                       (r & 0xFF00) >> 8,
                       (r & 0xFF0000) >> 16);

        if (ret == -1)
                goto out;

        str = CMNewString(_BROKER, mac, &status);
        if ((str == NULL) || (status.rc != CMPI_RC_OK)) {
                str = NULL;
                CU_DEBUG("Failed to create string");
                goto out;
        }
 out:
        free(mac);

        if (str != NULL)
                _mac = CMGetCharPtr(str);
        else
                _mac = NULL;

        return _mac;
}

static const char *net_rasd_to_vdev(CMPIInstance *inst,
                                    struct virt_device *dev,
                                    const char *ns)
{
        const char *val = NULL;
        const char *msg = NULL;
        char *network = NULL;
        CMPIObjectPath *op = NULL;

        op = CMGetObjectPath(inst, NULL);
        if (op == NULL) {
                msg = "Unable to determine classname of NetRASD";
                goto out;
        }

        if (cu_get_str_prop(inst, "Address", &val) != CMPI_RC_OK) {
                val = _net_rand_mac(op);
                if (val == NULL) {
                        msg = "Unable to generate a MAC address";
                        goto out;
                }
        }

/*
        msg = check_duplicate_mac(inst, val, ns);
        if (msg != NULL) {
                goto out;
        }
*/

        free(dev->dev.net.mac);
        dev->dev.net.mac = strdup(val);

        free(dev->id);
        dev->id = strdup(dev->dev.net.mac);

        free(dev->dev.net.type);
        if (cu_get_str_prop(inst, "NetworkType", &val) != CMPI_RC_OK)
                return "No Network Type specified";

        free(dev->dev.net.source);
        if (STREQC(val, BRIDGE_TYPE)) {
                dev->dev.net.type = strdup(BRIDGE_TYPE);
                if (cu_get_str_prop(inst, "NetworkName", &val) == CMPI_RC_OK)
                        if (strlen(val) > 0)
                                dev->dev.net.source = strdup(val);
                        else
                                return "Bridge name is empty";
                else
                        return "No Network bridge name specified";
         } else if (STREQC(val, NETWORK_TYPE)) {
                dev->dev.net.type = strdup(NETWORK_TYPE);
                if (cu_get_str_prop(inst, "PoolID", &val) != CMPI_RC_OK)
                        val = _default_network(inst, ns);

                if (val == NULL)
                        return "No NetworkPool specified no default available";

                network = name_from_pool_id(val);
                if (network == NULL) {
                        msg = "PoolID specified is not formatted properly";
                        goto out;
                }

                dev->dev.net.source = strdup(network);
        } else if (STREQC(val, USER_TYPE)) {
                dev->dev.net.type = strdup(USER_TYPE);
        } else if (STREQC(val, DIRECT_TYPE)) {
                dev->dev.net.type = strdup(DIRECT_TYPE);
                if (cu_get_str_prop(inst, "SourceDevice", &val) == CMPI_RC_OK)
                        if (strlen(val) > 0)
                                dev->dev.net.source = strdup(val);
                        else
                                return "Source Device is empty";
                else
                        return "No Source Device specified";

                free(dev->dev.net.vsi.vsi_type);
                if (cu_get_str_prop(inst, "VSIType", &val) != CMPI_RC_OK)
                        dev->dev.net.vsi.vsi_type = NULL;
                else
                        dev->dev.net.vsi.vsi_type = strdup(val);

                free(dev->dev.net.vsi.manager_id);
                if (cu_get_str_prop(inst, "VSIManagerID", &val) != CMPI_RC_OK)
                        dev->dev.net.vsi.manager_id = NULL;
                else
                        dev->dev.net.vsi.manager_id = strdup(val);

                free(dev->dev.net.vsi.type_id);
                if (cu_get_str_prop(inst, "VSITypeID", &val) != CMPI_RC_OK)
                        dev->dev.net.vsi.type_id = NULL;
                else
                        dev->dev.net.vsi.type_id = strdup(val);

                free(dev->dev.net.vsi.type_id_version);
                if (cu_get_str_prop(inst, "VSITypeIDVersion", &val) !=
                    CMPI_RC_OK)
                        dev->dev.net.vsi.type_id_version = NULL;
                else
                        dev->dev.net.vsi.type_id_version = strdup(val);

                free(dev->dev.net.vsi.instance_id);
                if (cu_get_str_prop(inst, "VSIInstanceID", &val) != CMPI_RC_OK)
                        dev->dev.net.vsi.instance_id = NULL;
                else
                        dev->dev.net.vsi.instance_id = strdup(val);

                free(dev->dev.net.vsi.filter_ref);
                if (cu_get_str_prop(inst, "FilterRef", &val) != CMPI_RC_OK)
                        dev->dev.net.vsi.filter_ref = NULL;
                else
                        dev->dev.net.vsi.filter_ref = strdup(val);

                free(dev->dev.net.vsi.profile_id);
                if (cu_get_str_prop(inst, "ProfileID", &val) != CMPI_RC_OK)
                        dev->dev.net.vsi.profile_id = NULL;
                else
                        dev->dev.net.vsi.profile_id = strdup(val);

        } else
                return "Invalid Network Type specified";

        free(dev->dev.net.device);
        if (cu_get_str_prop(inst, "VirtualDevice", &val) != CMPI_RC_OK)
                dev->dev.net.device = NULL;
        else
                dev->dev.net.device = strdup(val);

        free(dev->dev.net.net_mode);
        if (cu_get_str_prop(inst, "NetworkMode", &val) != CMPI_RC_OK)
                dev->dev.net.net_mode = NULL;
        else
                dev->dev.net.net_mode = strdup(val);

        free(dev->dev.net.model);

        if (cu_get_str_prop(inst, "ResourceSubType", &val) != CMPI_RC_OK)
                dev->dev.net.model = NULL;
        else
                dev->dev.net.model = strdup(val);

        if (cu_get_u64_prop(inst, "Reservation",
                            &dev->dev.net.reservation) != CMPI_RC_OK)
                dev->dev.net.reservation = 0;

        if (cu_get_u64_prop(inst, "Limit",
                            &dev->dev.net.limit) != CMPI_RC_OK)
                dev->dev.net.limit = 0;

 out:
        free(network);
        return msg;
}

static const char *disk_rasd_to_vdev(CMPIInstance *inst,
                                     struct virt_device *dev,
                                     char **p_error)
{
        const char *val = NULL;
        uint16_t type;
        bool read = false;
        int rc;
        bool shareable = false;

        CU_DEBUG("Enter disk_rasd_to_vdev");
        if (cu_get_str_prop(inst, "VirtualDevice", &val) != CMPI_RC_OK)
                return "Missing `VirtualDevice' property";

        free(dev->dev.disk.virtual_dev);
        dev->dev.disk.virtual_dev = strdup(val);

        if (cu_get_str_prop(inst, "Address", &val) != CMPI_RC_OK)
                val = "/dev/null";

        free(dev->dev.disk.source);
        dev->dev.disk.source = strdup(val);
        if (dev->dev.disk.source == NULL) {
                return "dev->dev.disk.source is null!";
        }

        dev->dev.disk.disk_type = disk_type_from_file(val);

        if (cu_get_u16_prop(inst, "EmulatedType", &type) != CMPI_RC_OK)
                type = VIRT_DISK_TYPE_DISK;

        if ((type == VIRT_DISK_TYPE_DISK) ||
            (type == VIRT_DISK_TYPE_FS)){
            if (dev->dev.disk.disk_type == DISK_UNKNOWN) {
                /* on success or fail caller should try free it */
                rc = asprintf(p_error, "Device %s, Address %s, "
                              "make sure Address can be accessed on host system.",
                              dev->dev.disk.virtual_dev, dev->dev.disk.source);
                if (rc == -1) {
                        CU_DEBUG("error during recording exception!");
                        p_error = NULL;
                }
                return "Can't get a valid disk type";
            }
        }

        if (type == VIRT_DISK_TYPE_DISK)
                dev->dev.disk.device = strdup("disk");
        else if (type == VIRT_DISK_TYPE_CDROM) {
                dev->dev.disk.device = strdup("cdrom");
                /* following code is for the case that user defined cdrom device
                   without disk in it, or a empty disk "" */
                if (XSTREQ(dev->dev.disk.source, "")) {
                        dev->dev.disk.disk_type = DISK_FILE;
                }
                if (XSTREQ(dev->dev.disk.source, "/dev/null")) {
                        dev->dev.disk.disk_type = DISK_FILE;
                        free(dev->dev.disk.source);
                        dev->dev.disk.source = strdup("");
                }

                if (dev->dev.disk.disk_type == DISK_UNKNOWN)
                        dev->dev.disk.disk_type = DISK_PHY;
        }
        else if (type == VIRT_DISK_TYPE_FLOPPY)
                dev->dev.disk.device = strdup("floppy");
        else if (type == VIRT_DISK_TYPE_FS) 
                dev->dev.disk.device = strdup("filesystem");
        else
                return "Invalid value for EmulatedType";

        CU_DEBUG("device type is %s", dev->dev.disk.device);

        if (cu_get_bool_prop(inst, "readonly", &read) != CMPI_RC_OK)
                dev->dev.disk.readonly = false;
        else
                dev->dev.disk.readonly = read;

        free(dev->dev.disk.bus_type);
        if (cu_get_str_prop(inst, "BusType", &val) != CMPI_RC_OK)
                dev->dev.disk.bus_type = NULL;
        else
                dev->dev.disk.bus_type = strdup(val);

        free(dev->dev.disk.driver);
        if (cu_get_str_prop(inst, "DriverName", &val) != CMPI_RC_OK)
                dev->dev.disk.driver = NULL;
        else
                dev->dev.disk.driver = strdup(val);

        free(dev->dev.disk.driver_type);
        if (cu_get_str_prop(inst, "DriverType", &val) != CMPI_RC_OK)
                dev->dev.disk.driver_type = NULL;
        else
                dev->dev.disk.driver_type = strdup(val);

        free(dev->dev.disk.cache);
        if (cu_get_str_prop(inst, "DriverCache", &val) != CMPI_RC_OK)
                dev->dev.disk.cache = NULL;
        else
                dev->dev.disk.cache = strdup(val);

        free(dev->dev.disk.access_mode);
        if (cu_get_str_prop(inst, "AccessMode", &val) != CMPI_RC_OK)
                dev->dev.disk.access_mode = NULL;
        else 
                dev->dev.disk.access_mode = strdup(val);

        if (cu_get_bool_prop(inst, "shareable", &shareable) != CMPI_RC_OK)
                dev->dev.disk.shareable = false;
        else
                dev->dev.disk.shareable = shareable;

        free(dev->id);
        dev->id = strdup(dev->dev.disk.virtual_dev);

        return NULL;
}

static const char *lxc_disk_rasd_to_vdev(CMPIInstance *inst,
                                         struct virt_device *dev)
{
        const char *val = NULL;

        if (cu_get_str_prop(inst, "MountPoint", &val) != CMPI_RC_OK)
                return "Missing `MountPoint' field";

        free(dev->dev.disk.virtual_dev);
        dev->dev.disk.virtual_dev = strdup(val);

        if (cu_get_str_prop(inst, "Address", &val) != CMPI_RC_OK)
                return "Missing `Address' field";

        free(dev->dev.disk.source);
        dev->dev.disk.source = strdup(val);
        dev->dev.disk.disk_type = DISK_FS;

        free(dev->id);
        dev->id = strdup(dev->dev.disk.virtual_dev);

        return NULL;
}

static const char *mem_rasd_to_vdev(CMPIInstance *inst,
                                    struct virt_device *dev)
{
        const char *units;
        CMPIrc ret;
        int shift;

        ret = cu_get_u64_prop(inst, "VirtualQuantity", &dev->dev.mem.size);
        if (ret != CMPI_RC_OK)
                ret = cu_get_u64_prop(inst, "Reservation", &dev->dev.mem.size);

        if (ret != CMPI_RC_OK)
                return "Missing `VirtualQuantity' field in Memory RASD";

        ret = cu_get_u64_prop(inst, "Limit", &dev->dev.mem.maxsize);
        if (ret != CMPI_RC_OK)
                dev->dev.mem.maxsize = dev->dev.mem.size;

        if (cu_get_str_prop(inst, "AllocationUnits", &units) != CMPI_RC_OK) {
                CU_DEBUG("Memory RASD has no units, assuming bytes");
                units = "Bytes";
        }

        if (STREQC(units, "Bytes"))
                shift = -10;
        else if (STREQC(units, "KiloBytes"))
                shift = 0;
        else if (STREQC(units, "MegaBytes"))
                shift = 10;
        else if (STREQC(units, "GigaBytes"))
                shift = 20;
        else
                return "Unknown AllocationUnits in Memory RASD";

        if (shift < 0) {
                dev->dev.mem.size >>= -shift;
                dev->dev.mem.maxsize >>= -shift;
        } else {
                dev->dev.mem.size <<= shift;
                dev->dev.mem.maxsize <<= shift;
        }

        return NULL;
}

static const char *proc_rasd_to_vdev(CMPIInstance *inst,
                                     struct virt_device *dev)
{
        CMPIObjectPath *op = NULL;
        CMPIrc rc;
        uint32_t def_weight = 0;
        uint64_t def_limit = 0;

        rc = cu_get_u64_prop(inst, "VirtualQuantity", &dev->dev.vcpu.quantity);
        if (rc != CMPI_RC_OK)
                return "Missing `VirtualQuantity' field in Processor RASD";

        op = CMGetObjectPath(inst, NULL);
        if (op == NULL) {
                CU_DEBUG("Unable to determine class of ProcRASD");
                return NULL;
        }

        if (STARTS_WITH(CLASSNAME(op), "Xen"))
                def_weight = DEFAULT_XEN_WEIGHT;
        else if (STARTS_WITH(CLASSNAME(op), "QEMU"))
                def_weight = DEFAULT_KVM_WEIGHT;

        rc = cu_get_u64_prop(inst, "Limit", &dev->dev.vcpu.limit);
        if (rc != CMPI_RC_OK)
                dev->dev.vcpu.limit = def_limit;

        rc = cu_get_u32_prop(inst, "Weight", &dev->dev.vcpu.weight);
        if (rc != CMPI_RC_OK)
                dev->dev.vcpu.weight = def_weight;

        return NULL;
}

static const char *lxc_proc_rasd_to_vdev(CMPIInstance *inst,
                                         struct virt_device *dev)
{
        CMPIrc rc;
        uint32_t def_weight = 1024;

        rc = cu_get_u64_prop(inst, "VirtualQuantity", &dev->dev.vcpu.quantity);
        if (rc == CMPI_RC_OK)
                return "ProcRASD field VirtualQuantity not valid for LXC";

        rc = cu_get_u64_prop(inst, "Limit", &dev->dev.vcpu.limit);
        if (rc == CMPI_RC_OK)
                return "ProcRASD field Limit not valid for LXC";

        rc = cu_get_u32_prop(inst, "Weight", &dev->dev.vcpu.weight);
        if (rc != CMPI_RC_OK)
                dev->dev.vcpu.weight = def_weight;

        return NULL;
}

static int parse_console_address(const char *id,
                        char **path,
                        char **port)
{
        int ret;
        char *tmp_path = NULL;
        char *tmp_port = NULL;

        CU_DEBUG("Entering parse_console_address, address is %s", id);

        ret = sscanf(id, "%a[^:]:%as", &tmp_path, &tmp_port);

        if (ret != 2) {
                ret = 0;
                goto out;
        }

        if (path)
                *path = strdup(tmp_path);

        if (port)
                *port = strdup(tmp_port);

        ret = 1;

 out:
        if (path && port)
                CU_DEBUG("Exiting parse_console_address, ip is %s, port is %s",
                       *path, *port);

        free(tmp_path);
        free(tmp_port);

        return ret;
}

static int parse_sdl_address(const char *id,
                        char **display,
                        char **xauth)
{
        int ret;
        char *tmp_display = NULL;
        char *tmp_xauth = NULL;

        CU_DEBUG("Entering parse_sdl_address, address is %s", id);

        ret = sscanf(id, "%a[^:]:%as", &tmp_xauth, &tmp_display);

        if (ret <= 0) {
                ret = sscanf(id, ":%as", &tmp_display);
                if (ret <= 0) {
                        if (STREQC(id, ":")) {
                                /* do nothing, it is empty */
                        }
                        else {
                                ret = 0;
                                goto out;
                        }
                }
        }

        if (display) {
                if (tmp_display == NULL)
                    *display = NULL;
                else
                    *display = strdup(tmp_display);
        }
        if (xauth) {
                if (tmp_xauth == NULL)
                    *xauth = NULL;
                else
                    *xauth = strdup(tmp_xauth);
        }
        ret = 1;

 out:
        if (display && xauth)
                CU_DEBUG("Exiting parse_sdl_address, display is %s, xauth is %s",
                       *display, *xauth);

        free(tmp_display);
        free(tmp_xauth);

        return ret;
}

static int parse_vnc_address(const char *id,
                      char **ip,
                      char **port)
{
        int ret;
        char *tmp_ip = NULL;
        char *tmp_port = NULL;

        CU_DEBUG("Entering parse_vnc_address, address is %s", id);
        if (strstr(id, "[") != NULL) {
                /* its an ipv6 address */
                ret = sscanf(id, "%a[^]]]:%as",  &tmp_ip, &tmp_port);
                strcat(tmp_ip, "]");
        } else {
                ret = sscanf(id, "%a[^:]:%as", &tmp_ip, &tmp_port);
        }

        if (ret != 2) {
                ret = 0;
                goto out;
        }

        if (ip)
                *ip = strdup(tmp_ip);

        if (port)
                *port = strdup(tmp_port);

        ret = 1;

 out:
        if (ip && port)
                CU_DEBUG("Exiting parse_vnc_address, ip is %s, port is %s",
                        *ip, *port);

        free(tmp_ip);
        free(tmp_port);

        return ret;
}

static const char *graphics_rasd_to_vdev(CMPIInstance *inst,
                                         struct virt_device *dev)
{
        const char *val = NULL;
        const char *msg = NULL;
        bool ipv6 = false;
        int ret;

        if (cu_get_str_prop(inst, "ResourceSubType", &val) != CMPI_RC_OK) {
                msg = "GraphicsRASD ResourceSubType field not valid";
                goto out;
        }
        dev->dev.graphics.type = strdup(val);

        CU_DEBUG("graphics type = %s", dev->dev.graphics.type);

        /* FIXME: Add logic to prevent address:port collisions */
        if (STREQC(dev->dev.graphics.type, "vnc")) {
                if (cu_get_str_prop(inst, "Address", &val) != CMPI_RC_OK) {
                        CU_DEBUG("graphics Address empty, using default");

                        if (cu_get_bool_prop(inst, "IsIPV6Only", &ipv6) !=
                                CMPI_RC_OK)
                                ipv6 = false;

                        if(ipv6)
                                val = "[::1]:-1";
                        else
                                val = "127.0.0.1:-1";
                }

                ret = parse_vnc_address(val,
                                &dev->dev.graphics.dev.vnc.host,
                                &dev->dev.graphics.dev.vnc.port);
                if (ret != 1) {
                        msg = "GraphicsRASD field Address not valid";
                        goto out;
                }

                if (cu_get_str_prop(inst, "KeyMap", &val) != CMPI_RC_OK)
                        dev->dev.graphics.dev.vnc.keymap = strdup("en-us");
                else
                        dev->dev.graphics.dev.vnc.keymap = strdup(val);

                if (cu_get_str_prop(inst, "Password", &val) != CMPI_RC_OK) {
                        CU_DEBUG("vnc password is not set");
                        dev->dev.graphics.dev.vnc.passwd = NULL;
                } else {
                        CU_DEBUG("vnc password is set");
                        dev->dev.graphics.dev.vnc.passwd = strdup(val);
                }
        }
        else if (STREQC(dev->dev.graphics.type, "console") ||
                STREQC(dev->dev.graphics.type, "serial")) {
                if (cu_get_str_prop(inst, "Address", &val) != CMPI_RC_OK) {
                        CU_DEBUG("graphics Address empty, using default");
                        val = "/dev/pts/0:0";
                }

                ret = parse_console_address(val,
                                &dev->dev.graphics.dev.vnc.host,
                                &dev->dev.graphics.dev.vnc.port);
                if (ret != 1) {
                         msg = "GraphicsRASD field Address not valid";
                         goto out;
                }
        }
        else if (STREQC(dev->dev.graphics.type, "sdl")) {
                if (cu_get_str_prop(inst, "Address", &val) != CMPI_RC_OK) {
                         CU_DEBUG("sdl graphics Address empty, using default");
                         dev->dev.graphics.dev.sdl.display = NULL;
                         dev->dev.graphics.dev.sdl.xauth = NULL;
                }
                else {
                         ret = parse_sdl_address(val,
                                         &dev->dev.graphics.dev.sdl.display,
                                         &dev->dev.graphics.dev.sdl.xauth);
                         if (ret != 1) {
                                  msg = "GraphicsRASD sdl Address not valid";
                                  goto out;
                         }
                }
                dev->dev.graphics.dev.sdl.fullscreen = NULL;
                if (cu_get_bool_prop(inst, "IsIPV6Only", &ipv6) ==
                                CMPI_RC_OK) {
                                if (ipv6)
                                        dev->dev.graphics.dev.sdl.fullscreen = strdup("yes");
                                else
                                        dev->dev.graphics.dev.sdl.fullscreen = strdup("no");
                }
        } else {
                CU_DEBUG("Unsupported graphics type %s",
                        dev->dev.graphics.type);
                msg = "Unsupported graphics type";
                goto out;
        }

        free(dev->id);
        if ((STREQC(dev->dev.graphics.type, "vnc"))||
                (STREQC(dev->dev.graphics.type, "sdl")))
                ret = asprintf(&dev->id, "%s", dev->dev.graphics.type);
        else
                ret = asprintf(&dev->id, "%s:%s",
                       dev->dev.graphics.type, dev->dev.graphics.dev.vnc.port);

        if (ret == -1) {
                msg = "Failed to create graphics is string";
                goto out;
        }

        CU_DEBUG("graphics = %s", dev->id);

 out:
        return msg;
}

static const char *input_rasd_to_vdev(CMPIInstance *inst,
                                      struct virt_device *dev)
{
        const char *val;

        if (cu_get_str_prop(inst, "ResourceSubType", &val) != CMPI_RC_OK) {
                CU_DEBUG("InputRASD ResourceSubType field not valid");
                goto out;
        }
        dev->dev.input.type = strdup(val);

        if (cu_get_str_prop(inst, "BusType", &val) != CMPI_RC_OK) {
                if (STREQC(dev->dev.input.type, "mouse"))
                        dev->dev.input.bus = strdup("ps2");
                else if (STREQC(dev->dev.input.type, "tablet"))
                        dev->dev.input.bus = strdup("usb");
                else {
                        CU_DEBUG("Invalid value for ResourceSubType in InputRASD");
                        goto out;
                }
        } else
                dev->dev.input.bus = strdup(val);

 out:

        return NULL;
}

static const char *_sysvirt_rasd_to_vdev(CMPIInstance *inst,
                                         struct virt_device *dev,
                                         uint16_t type,
                                         const char *ns,
                                         char **p_error)
{
        if (type == CIM_RES_TYPE_DISK) {
                return disk_rasd_to_vdev(inst, dev, p_error);
        } else if (type == CIM_RES_TYPE_NET) {
                return net_rasd_to_vdev(inst, dev, ns);
        } else if (type == CIM_RES_TYPE_MEM) {
                return mem_rasd_to_vdev(inst, dev);
        } else if (type == CIM_RES_TYPE_PROC) {
                return proc_rasd_to_vdev(inst, dev);
        } else if (type == CIM_RES_TYPE_GRAPHICS) {
                return graphics_rasd_to_vdev(inst, dev);
        } else if (type == CIM_RES_TYPE_INPUT) {
                return input_rasd_to_vdev(inst, dev);
        }

        return "Resource type not supported on this platform";
}

static const char *_container_rasd_to_vdev(CMPIInstance *inst,
                                           struct virt_device *dev,
                                           uint16_t type,
                                           const char *ns)
{
        if (type == CIM_RES_TYPE_MEM) {
                return mem_rasd_to_vdev(inst, dev);
        } else if (type == CIM_RES_TYPE_DISK) {
                return lxc_disk_rasd_to_vdev(inst, dev);
        } else if (type == CIM_RES_TYPE_NET) {
                return net_rasd_to_vdev(inst, dev, ns);
        } else if (type == CIM_RES_TYPE_PROC) {
                return lxc_proc_rasd_to_vdev(inst, dev);
        } else if (type == CIM_RES_TYPE_INPUT) {
                return input_rasd_to_vdev(inst, dev);
        }

        return "Resource type not supported on this platform";
}

static const char *rasd_to_vdev(CMPIInstance *inst,
                                struct domain *domain,
                                struct virt_device *dev,
                                const char *ns,
                                char **p_error)
{
        uint16_t type;
        CMPIObjectPath *op;
        const char *msg = NULL;

        op = CMGetObjectPath(inst, NULL);
        if (op == NULL) {
                msg = "Unable to get path for device instance";
                goto out;
        }

        if (res_type_from_rasd_classname(CLASSNAME(op), &type) != CMPI_RC_OK) {
                msg = "Unable to get device type";
                goto out;
        }

        dev->type = (int)type;

        if (domain->type == DOMAIN_LXC)
                msg = _container_rasd_to_vdev(inst, dev, type, ns);
        else
                msg = _sysvirt_rasd_to_vdev(inst, dev, type, ns, p_error);
 out:
        if (msg && op)
                CU_DEBUG("rasd_to_vdev(%s): %s", CLASSNAME(op), msg);

        return msg;
}

static char *add_device_nodup(struct virt_device *dev,
                              struct virt_device *list,
                              int max,
                              int *index)
{
        int i;

        for (i = 0; i < *index; i++) {
                struct virt_device *ptr = &list[i];

                if (dev->type == CIM_RES_TYPE_DISK &&
                    STREQC(ptr->dev.disk.virtual_dev,
                           dev->dev.disk.virtual_dev))
                        return "VirtualDevice property must be unique for each "
                               "DiskResourceAllocationSettingData in a single "
                               "guest";

                if (STREQC(ptr->id, dev->id)) {
                        CU_DEBUG("Overriding device %s from refconf", ptr->id);
                        cleanup_virt_device(ptr);
                        memcpy(ptr, dev, sizeof(*ptr));
                        return NULL;
                }
        }

        if (*index == max)
                return "Internal error: no more device slots";

        memcpy(&list[*index], dev, sizeof(list[*index]));
        *index += 1;

        return NULL;
}

static const char *classify_resources(CMPIArray *resources,
                                      const char *ns,
                                      struct domain *domain,
                                      char **p_error)
{
        int i;
        uint16_t type;
        int count;

        count = CMGetArrayCount(resources, NULL);
        if (count < 1)
                return "No resources specified";

        if (!make_space(&domain->dev_disk, domain->dev_disk_ct, count))
                return "Failed to alloc disk list";

        if (!make_space(&domain->dev_vcpu, domain->dev_vcpu_ct, count))
                return "Failed to alloc vcpu list";

        if (!make_space(&domain->dev_mem, domain->dev_mem_ct, count))
                return "Failed to alloc mem list";

        if (!make_space(&domain->dev_net, domain->dev_net_ct, count))
                return "Failed to alloc net list";

        if (!make_space(&domain->dev_graphics, domain->dev_graphics_ct, count))
                return "Failed to alloc graphics list";

        if (!make_space(&domain->dev_input, domain->dev_input_ct, count))
                return "Failed to alloc input list";

        for (i = 0; i < count; i++) {
                CMPIObjectPath *op;
                CMPIData item;
                CMPIInstance *inst;
                const char *msg = NULL;

                item = CMGetArrayElementAt(resources, i, NULL);
                if (CMIsNullObject(item.value.inst))
                        return "Internal array error";

                inst = item.value.inst;

                op = CMGetObjectPath(inst, NULL);
                if (op == NULL)
                        return "Unknown resource instance type";

                if (res_type_from_rasd_classname(CLASSNAME(op), &type) !=
                    CMPI_RC_OK)
                        return "Unable to determine resource type";

                if (type == CIM_RES_TYPE_PROC) {
                        domain->dev_vcpu_ct = 1;
                        msg = rasd_to_vdev(inst,
                                           domain,
                                           &domain->dev_vcpu[0],
                                           ns,
                                           p_error);
                } else if (type == CIM_RES_TYPE_MEM) {
                        domain->dev_mem_ct = 1;
                        msg = rasd_to_vdev(inst,
                                           domain,
                                           &domain->dev_mem[0],
                                           ns,
                                           p_error);
                } else if (type == CIM_RES_TYPE_DISK) {
                        struct virt_device dev;
                        int dcount = count + domain->dev_disk_ct;

                        memset(&dev, 0, sizeof(dev));
                        msg = rasd_to_vdev(inst,
                                           domain,
                                           &dev,
                                           ns,
                                           p_error);
                        if (msg == NULL)
                                msg = add_device_nodup(&dev,
                                                       domain->dev_disk,
                                                       dcount,
                                                       &domain->dev_disk_ct);
                } else if (type == CIM_RES_TYPE_NET) {
                        struct virt_device dev;
                        int ncount = count + domain->dev_net_ct;
#if LIBVIR_VERSION_NUMBER < 9000
                        uint64_t qos_val = 0;
                        const char *qos_unitstr;
#endif

                        memset(&dev, 0, sizeof(dev));
                        msg = rasd_to_vdev(inst,
                                           domain,
                                           &dev,
                                           ns,
                                           p_error);
                        if (msg == NULL)
                                msg = add_device_nodup(&dev,
                                                       domain->dev_net,
                                                       ncount,
                                                       &domain->dev_net_ct);

#if LIBVIR_VERSION_NUMBER < 9000
			/* Network QoS support */
                        if (((&dev)->dev.net.mac != NULL) &&
                            ((&dev)->dev.net.source != NULL) &&
                            (cu_get_u64_prop(inst, "Reservation", &qos_val) == CMPI_RC_OK) &&
                            (cu_get_str_prop(inst, "AllocationUnits", &qos_unitstr) == CMPI_RC_OK) &&
                            STREQ(qos_unitstr,"KiloBits per Second")) {
                                remove_qos_for_mac(qos_val,
                                                   (&dev)->dev.net.mac,
                                                   (&dev)->dev.net.source);
                                add_qos_for_mac(qos_val,
                                                (&dev)->dev.net.mac,
                                                (&dev)->dev.net.source);
                        }
#endif
                } else if (type == CIM_RES_TYPE_GRAPHICS) {
                        struct virt_device dev;
                        int gcount = count + domain->dev_graphics_ct;

                        memset(&dev, 0, sizeof(dev));
                        msg = rasd_to_vdev(inst,
                                           domain,
                                           &dev,
                                           ns,
                                           p_error);
                        if (msg == NULL)
                                msg = add_device_nodup(&dev,
                                                domain->dev_graphics,
                                                gcount,
                                                &domain->dev_graphics_ct);
                } else if (type == CIM_RES_TYPE_INPUT) {
                        domain->dev_input_ct = 1;
                        msg = rasd_to_vdev(inst,
                                           domain,
                                           &domain->dev_input[0],
                                           ns,
                                           p_error);
                }
                if (msg != NULL)
                        return msg;

        }

       return NULL;
}

static CMPIInstance *connect_and_create(char *xml,
                                        const CMPIObjectPath *ref,
                                        CMPIStatus *s)
{
        virConnectPtr conn;
        virDomainPtr dom;
        const char *name;
        CMPIInstance *inst = NULL;

        conn = connect_by_classname(_BROKER, CLASSNAME(ref), s);
        if (conn == NULL) {
                CU_DEBUG("libvirt connection failed");
                return NULL;
        }

        dom = virDomainDefineXML(conn, xml);
        if (dom == NULL) {
                CU_DEBUG("Failed to define domain from XML");
                virt_set_status(_BROKER, s,
                                CMPI_RC_ERR_FAILED,
                                conn,
                                "Failed to define domain");
                goto out;
        }

        name = virDomainGetName(dom);

        *s = get_domain_by_name(_BROKER, ref, name, &inst);
        if (s->rc != CMPI_RC_OK) {
                CU_DEBUG("Failed to get new instance");
                cu_statusf(_BROKER, s,
                           CMPI_RC_ERR_FAILED,
                           "Failed to lookup resulting system");
                goto out;
        }

 out:
        virDomainFree(dom);
        virConnectClose(conn);

        return inst;
}

static CMPIStatus update_dominfo(const struct domain *dominfo,
                                 const char *refcn)
{
        CMPIStatus s = {CMPI_RC_OK, NULL};
        struct infostore_ctx *ctx = NULL;
        struct virt_device *dev = dominfo->dev_vcpu;
        virConnectPtr conn = NULL;
        virDomainPtr dom = NULL;

        CU_DEBUG("Enter update_dominfo");
        if (dominfo->dev_vcpu_ct != 1) {
                /* Right now, we only have extra info for processors */
                CU_DEBUG("Domain has no vcpu devices!");
                return s;
        }

        conn = connect_by_classname(_BROKER, refcn, &s);
        if (conn == NULL) {
                CU_DEBUG("Failed to connnect by %s", refcn);
                return s;
        }

        dom = virDomainLookupByName(conn, dominfo->name);
        if (dom == NULL) {
                virt_set_status(_BROKER, &s,
                                CMPI_RC_ERR_NOT_FOUND,
                                conn,
                                "Unable to lookup domain `%s'", dominfo->name);
                goto out;
        }

        ctx = infostore_open(dom);
        if (ctx == NULL) {
                cu_statusf(_BROKER, &s,
                           CMPI_RC_ERR_FAILED,
                           "Unable to open infostore");
                goto out;
        }

#if LIBVIR_VERSION_NUMBER < 9000
        /* Old libvirt versions dont save cpu cgroup setting for inactive */
        /* guests, so save in infostore instead */
        infostore_set_u64(ctx, "weight", dev->dev.vcpu.weight);
#else
        /* New libvirt versions save cpu cgroup setting in KVM guest config */
        if (STREQC(virConnectGetType(conn), "QEMU")) {
                int ret;
                virSchedParameter params;
                strncpy(params.field,
                        "cpu_shares",
                        VIR_DOMAIN_SCHED_FIELD_LENGTH);
                params.type = VIR_DOMAIN_SCHED_FIELD_ULLONG;
                params.value.ul = dev->dev.vcpu.weight;

                CU_DEBUG("setting %s scheduler param cpu_shares=%d",
                         dominfo->name,
                         dev->dev.vcpu.weight);
                ret = virDomainSetSchedulerParametersFlags(dom, &params, 1,
                            VIR_DOMAIN_AFFECT_CONFIG);
                if (ret != 0) {
                        CU_DEBUG("Failed to set config scheduler param");
                        cu_statusf(_BROKER, &s,
                                   CMPI_RC_ERR_FAILED,
                                   "Failed to set config scheduler param");
                        goto out;
                }
        }
        else
                infostore_set_u64(ctx, "weight", dev->dev.vcpu.weight);
#endif
        infostore_set_u64(ctx, "limit", dev->dev.vcpu.limit);

 out:
        infostore_close(ctx);

        virDomainFree(dom);
        virConnectClose(conn);

        return s;
}

static CMPIStatus match_prefixes(const CMPIObjectPath *a,
                                 const CMPIObjectPath *b)
{
        char *pfx1;
        char *pfx2;
        CMPIStatus s = {CMPI_RC_OK, NULL};

        pfx1 = class_prefix_name(CLASSNAME(a));
        pfx2 = class_prefix_name(CLASSNAME(b));

        if ((pfx1 == NULL) || (pfx2 == NULL)) {
                cu_statusf(_BROKER, &s,
                           CMPI_RC_ERR_FAILED,
                           "Unable compare ReferenceConfiguration prefix");
                goto out;
        }

        if (!STREQ(pfx1, pfx2)) {
                cu_statusf(_BROKER, &s,
                           CMPI_RC_ERR_INVALID_CLASS,
                           "ReferenceConfiguration domain is not compatible");
                goto out;
        }
 out:
        free(pfx1);
        free(pfx2);

        return s;
}

static CMPIStatus get_reference_domain(struct domain **domain,
                                       const CMPIObjectPath *ref,
                                       const CMPIObjectPath *refconf)
{
        virConnectPtr conn = NULL;
        virDomainPtr dom = NULL;
        char *name = NULL;
        const char *iid;
        const char *mac;
        CMPIStatus s;
        int ret;

        s = match_prefixes(ref, refconf);
        if (s.rc != CMPI_RC_OK)
                return s;

        conn = connect_by_classname(_BROKER, CLASSNAME(refconf), &s);
        if (conn == NULL) {
                if (s.rc != CMPI_RC_OK)
                        return s;
                else {
                        cu_statusf(_BROKER, &s,
                                   CMPI_RC_ERR_FAILED,
                                   "Unable to connect to libvirt");
                        return s;
                }
        }

        if (cu_get_str_path(refconf, "InstanceID", &iid) != CMPI_RC_OK) {
                CU_DEBUG("Missing InstanceID parameter");
                cu_statusf(_BROKER, &s,
                           CMPI_RC_ERR_INVALID_PARAMETER,
                           "Missing `InstanceID' from ReferenceConfiguration");
                goto out;
        }

        if (!parse_id(iid, NULL, &name)) {
                CU_DEBUG("Failed to parse InstanceID: %s", iid);
                cu_statusf(_BROKER, &s,
                           CMPI_RC_ERR_INVALID_PARAMETER,
                           "Invalid `InstanceID' from ReferenceConfiguration");
                goto out;
        }

        CU_DEBUG("Referenced domain: %s", name);

        dom = virDomainLookupByName(conn, name);
        if (dom == NULL) {
                virt_set_status(_BROKER, &s,
                                CMPI_RC_ERR_NOT_FOUND,
                                conn,
                                "Referenced domain `%s' does not exist", name);
                goto out;
        }

        ret = get_dominfo(dom, domain);
        if (ret == 0) {
                virt_set_status(_BROKER, &s,
                                CMPI_RC_ERR_FAILED,
                                conn,
                                "Error getting referenced configuration");
                goto out;
        }

        /* Scrub the unique bits out of the reference domain config */
        free((*domain)->name);
        (*domain)->name = NULL;
        free((*domain)->uuid);
        (*domain)->uuid = NULL;
        free((*domain)->dev_net->dev.net.mac);
        (*domain)->dev_net->dev.net.mac = NULL;

        mac = _net_rand_mac(ref);
        if (mac == NULL) {
                cu_statusf(_BROKER, &s,
                           CMPI_RC_ERR_INVALID_PARAMETER,
                           "Unable to generate a MAC address for guest %s",
                           name);
                goto out;
        }
        (*domain)->dev_net->dev.net.mac = strdup(mac);

 out:
        virDomainFree(dom);
        virConnectClose(conn);
        free(name);

        return s;
}

static CMPIStatus raise_rasd_indication(const CMPIContext *context,
                                        const char *base_type,
                                        CMPIInstance *prev_inst,
                                        const CMPIObjectPath *ref,
                                        struct inst_list *list)
{
        char *type;
        CMPIStatus s = {CMPI_RC_OK, NULL};
        CMPIInstance *instc = NULL;
        CMPIInstance *ind = NULL;
        CMPIObjectPath *op = NULL;
        int i;

        CU_DEBUG("raise_rasd_indication %s", base_type);

        type = get_typed_class(CLASSNAME(ref), base_type);

        for (i = 0; i < list->cur; i++) {
                ind = get_typed_instance(_BROKER,
                                         CLASSNAME(ref),
                                         base_type,
                                         NAMESPACE(ref),
                                         false);
                if (ind == NULL)  {
                        CU_DEBUG("Failed to get indication instance");
                        s.rc = CMPI_RC_ERR_FAILED;
                        goto out;
                }

                /* PreviousInstance is set only for modify case. */
                if (prev_inst != NULL)
                        CMSetProperty(ind,
                                      "PreviousInstance",
                                      (CMPIValue *)&prev_inst,
                                      CMPI_instance);

                instc = list->list[i];
                op = CMGetObjectPath(instc, NULL);
                CMPIString *str = CMGetClassName(op, NULL);

                CU_DEBUG("class name is %s\n", CMGetCharsPtr(str, NULL));

                CMSetProperty(ind,
                              "SourceInstance",
                              (CMPIValue *)&instc,
                              CMPI_instance);
                set_source_inst_props(_BROKER, context, ref, ind);

                s = stdi_raise_indication(_BROKER,
                                          context,
                                          type,
                                          NAMESPACE(ref),
                                          ind);
        }

 out:
        free(type);
        return s;

}

static CMPIStatus set_autostart(CMPIInstance *vssd,
                                const CMPIObjectPath *ref,
                                virDomainPtr dom)
{
        CMPIStatus s;
        const char *name = NULL;
        CMPIrc ret;
        virConnectPtr conn = NULL;
        virDomainPtr inst_dom = NULL;
        uint16_t val = 0;
        int i = 0;

        CU_DEBUG("Enter set_autostart");
        ret = cu_get_str_prop(vssd, "VirtualSystemIdentifier", &name);
        if (ret != CMPI_RC_OK) {
                CU_DEBUG("Missing VirtualSystemIdentifier");
                cu_statusf(_BROKER, &s,
                           ret,
                           "Missing VirtualSystemIdentifier");
                goto out;
        }

        conn = connect_by_classname(_BROKER, CLASSNAME(ref), &s);
        if (conn == NULL) {
                CU_DEBUG("Failed to connect");
                cu_statusf(_BROKER, &s,
                           CMPI_RC_ERR_FAILED,
                           "Failed to connect");
                goto out;
        }

        inst_dom = virDomainLookupByName(conn, name);
        if (inst_dom == NULL) {
                CU_DEBUG("reference domain '%s' does not exist", name);
                virt_set_status(_BROKER, &s,
                                CMPI_RC_ERR_NOT_FOUND,
                                conn,
                                "Referenced domain `%s' does not exist", name);
                goto out;
        }

        if (cu_get_u16_prop(vssd, "AutoStart", &val) != CMPI_RC_OK) {
                if (dom != NULL) {
                        /* Read the current domain's autostart setting.
                           Since the user did not specify any new
                           autostart, the updated VM will use the same
                           autostart setting as used before this
                           update. */
                           if (virDomainGetAutostart(dom, &i) != 0)
                                   i = 0;
                }
        }
        else
                i = val;
        CU_DEBUG("setting  VM's autostart to %d", i);
        if (virDomainSetAutostart(inst_dom, i) == -1) {
                CU_DEBUG("Failed to set autostart");
                cu_statusf(_BROKER, &s,
                           CMPI_RC_ERR_FAILED,
                           "Failed to set autostart");
        }

 out:
        virDomainFree(inst_dom);
        virConnectClose(conn);
        return s;
}

static CMPIInstance *create_system(const CMPIContext *context,
                                   CMPIInstance *vssd,
                                   CMPIArray *resources,
                                   const CMPIObjectPath *ref,
                                   const CMPIObjectPath *refconf,
                                   CMPIStatus *s)
{
        CMPIInstance *inst = NULL;
        char *xml = NULL;
        const char *msg = NULL;
        struct inst_list list;
        const char *props[] = {NULL};
        struct domain *domain = NULL;
        char *error_msg = NULL;

        inst_list_init(&list);

        CU_DEBUG("Enter create_system");
        if (refconf != NULL) {
                *s = get_reference_domain(&domain, ref, refconf);
                if (s->rc != CMPI_RC_OK)
                        goto out;
        } else {
                domain = calloc(1, sizeof(*domain));
                if (domain == NULL) {
                        cu_statusf(_BROKER, s,
                                   CMPI_RC_ERR_FAILED,
                                   "Failed to allocate memory");
                        goto out;
                }
        }

        if (!vssd_to_domain(vssd, domain)) {
                CU_DEBUG("Failed to create domain from VSSD");
                cu_statusf(_BROKER, s,
                           CMPI_RC_ERR_FAILED,
                           "SystemSettings Error");
                goto out;
        }

        *s = check_uuid_in_use(ref, domain);
        if (s->rc != CMPI_RC_OK)
                goto out;

        msg = classify_resources(resources, NAMESPACE(ref), domain, &error_msg);
        if (msg != NULL) {
                CU_DEBUG("Failed to classify resources: %s, %s",
                         msg, error_msg);
                cu_statusf(_BROKER, s,
                           CMPI_RC_ERR_FAILED,
                           "ResourceSettings Error: %s, %s", msg, error_msg);
                goto out;
        }

        if (!add_default_devs(domain)) {
                CU_DEBUG("Failed to add default devices");
                cu_statusf(_BROKER, s,
                           CMPI_RC_ERR_FAILED,
                           "ResourceSettings Error");
                goto out;
        }

        xml = system_to_xml(domain);
        CU_DEBUG("System XML:\n%s", xml);

        inst = connect_and_create(xml, ref, s);
        if (inst != NULL) {
                update_dominfo(domain, CLASSNAME(ref));
                set_autostart(vssd, ref, NULL);

                *s = enum_rasds(_BROKER,
                                ref,
                                domain->name,
                                CIM_RES_TYPE_ALL,
                                props,
                                &list);

                if (s->rc != CMPI_RC_OK) {
                        CU_DEBUG("Failed to enumerate rasd\n");
                        goto out;
                }

                raise_rasd_indication(context,
                                      RASD_IND_CREATED,
                                      NULL,
                                      ref,
                                      &list);
        }


 out:
        free(error_msg);
        cleanup_dominfo(&domain);
        free(xml);
        inst_list_free(&list);

        return inst;
}

static CMPIStatus define_system(CMPIMethodMI *self,
                                const CMPIContext *context,
                                const CMPIResult *results,
                                const CMPIObjectPath *reference,
                                const CMPIArgs *argsin,
                                CMPIArgs *argsout)
{
        CMPIObjectPath *refconf;
        CMPIObjectPath *result;
        CMPIInstance *vssd;
        CMPIInstance *sys;
        CMPIArray *res;
        CMPIStatus s;
        uint32_t rc = CIM_SVPC_RETURN_FAILED;

        CU_DEBUG("DefineSystem");

        s = define_system_parse_args(argsin,
                                     &vssd,
                                     NAMESPACE(reference),
                                     &res,
                                     &refconf);
        if (s.rc != CMPI_RC_OK)
                goto out;

        sys = create_system(context, vssd, res, reference, refconf, &s);
        if (sys == NULL)
                goto out;

        result = CMGetObjectPath(sys, &s);
        if ((result != NULL) && (s.rc == CMPI_RC_OK)) {
                CMSetNameSpace(result, NAMESPACE(reference));
                CMAddArg(argsout, "ResultingSystem", &result, CMPI_ref);
        }

        /* try trigger indication */
        bool ind_rc = trigger_indication(_BROKER, context,
                                 "ComputerSystemCreatedIndication", reference);
        if (!ind_rc) {
                const char *dom_name = NULL;
                cu_get_str_prop(vssd, "VirtualSystemIdentifier", &dom_name);
                CU_DEBUG("Unable to trigger indication for "
                         "system create, dom is '%s'", dom_name);
        }

 out:
        if (s.rc == CMPI_RC_OK)
                rc = CIM_SVPC_RETURN_COMPLETED;
        CMReturnData(results, &rc, CMPI_uint32);

        return s;
}

static CMPIStatus destroy_system(CMPIMethodMI *self,
                                 const CMPIContext *context,
                                 const CMPIResult *results,
                                 const CMPIObjectPath *reference,
                                 const CMPIArgs *argsin,
                                 CMPIArgs *argsout)
{
        const char *dom_name = NULL;
        CMPIStatus status;
        uint32_t rc = IM_RC_FAILED;
        CMPIObjectPath *sys;
        virConnectPtr conn = NULL;
        virDomainPtr dom = NULL;
        struct inst_list list;
        const char *props[] = {NULL};

        inst_list_init(&list);
        conn = connect_by_classname(_BROKER,
                                    CLASSNAME(reference),
                                    &status);
        if (conn == NULL) {
                rc = IM_RC_NOT_SUPPORTED;
                goto error;
        }

        if (cu_get_ref_arg(argsin, "AffectedSystem", &sys) != CMPI_RC_OK)
                goto error;

        dom_name = get_key_from_ref_arg(argsin, "AffectedSystem", "Name");
        if (dom_name == NULL)
                goto error;

        status = enum_rasds(_BROKER,
                            reference,
                            dom_name,
                            CIM_RES_TYPE_ALL,
                            props,
                            &list);

        if (status.rc != CMPI_RC_OK) {
                CU_DEBUG("Failed to enumerate rasd");
                goto error;
        }

        dom = virDomainLookupByName(conn, dom_name);
        if (dom == NULL) {
                CU_DEBUG("No such domain `%s'", dom_name);
                rc = IM_RC_SYS_NOT_FOUND;
                goto error;
        }

        infostore_delete(virConnectGetType(conn), dom_name);

        virDomainDestroy(dom); /* Okay for this to fail */

        virDomainFree(dom);

        dom = virDomainLookupByName(conn, dom_name);
        if (dom == NULL) {
                CU_DEBUG("Domain successfully destroyed");
                rc = IM_RC_OK;
                goto error;
        }

        if (virDomainUndefine(dom) == 0) {
                CU_DEBUG("Domain successfully destroyed and undefined");
                rc = IM_RC_OK;
        }

error:
        if (rc == IM_RC_SYS_NOT_FOUND)
                virt_set_status(_BROKER,
                                &status,
                                CMPI_RC_ERR_NOT_FOUND,
                                conn,
                                "Referenced domain `%s' does not exist",
                                dom_name);
        else if (rc == IM_RC_NOT_SUPPORTED)
                virt_set_status(_BROKER, &status,
                                CMPI_RC_ERR_NOT_FOUND,
                                conn,
                                "Unable to connect to libvirt");
        else if (rc == IM_RC_FAILED)
                virt_set_status(_BROKER, &status,
                                CMPI_RC_ERR_NOT_FOUND,
                                conn,
                                "Unable to retrieve domain name");
        else if (rc == IM_RC_OK) {
                status = (CMPIStatus){CMPI_RC_OK, NULL};
                raise_rasd_indication(context,
                                      RASD_IND_DELETED,
                                      NULL,
                                      reference,
                                      &list);

                /* try trigger indication */
                bool ind_rc = trigger_indication(_BROKER, context,
                                 "ComputerSystemDeletedIndication", reference);
                if (!ind_rc) {
                        CU_DEBUG("Unable to trigger indication for "
                                 "system delete, dom is '%s'", dom_name);
                }

        }

        virDomainFree(dom);
        virConnectClose(conn);
        CMReturnData(results, &rc, CMPI_uint32);
        inst_list_free(&list);
        return status;
}

static CMPIStatus update_system_settings(const CMPIContext *context,
                                         const CMPIObjectPath *ref,
                                         CMPIInstance *vssd)
{
        CMPIStatus s;
        const char *name = NULL;
        CMPIrc ret;
        virConnectPtr conn = NULL;
        virDomainPtr dom = NULL;
        struct domain *dominfo = NULL;
        char *xml = NULL;
        const char *uuid = NULL;

        CU_DEBUG("Enter update_system_settings");
        ret = cu_get_str_prop(vssd, "VirtualSystemIdentifier", &name);
        if (ret != CMPI_RC_OK) {
                cu_statusf(_BROKER, &s,
                           ret,
                           "Missing VirtualSystemIdentifier");
                goto out;
        }

        conn = connect_by_classname(_BROKER, CLASSNAME(ref), &s);
        if (conn == NULL)
                goto out;

        dom = virDomainLookupByName(conn, name);
        if (dom == NULL) {
                virt_set_status(_BROKER, &s,
                                CMPI_RC_ERR_NOT_FOUND,
                                conn,
                                "Referenced domain `%s' does not exist", name);
                goto out;
        }

        if (!get_dominfo(dom, &dominfo)) {
                virt_set_status(_BROKER, &s,
                                CMPI_RC_ERR_FAILED,
                                conn,
                                "Unable to find existing domain `%s' to modify",
                                name);
                goto out;
        }

        uuid = strdup(dominfo->uuid);

        if (!vssd_to_domain(vssd, dominfo)) {
                cu_statusf(_BROKER, &s,
                           CMPI_RC_ERR_FAILED,
                           "Invalid SystemSettings");
                goto out;
        }

        if ((dominfo->uuid == NULL) || (STREQ(dominfo->uuid, ""))) {
                dominfo->uuid = strdup(uuid);
        } else if (!STREQ(uuid, dominfo->uuid)) {
                cu_statusf(_BROKER, &s,
                           CMPI_RC_ERR_FAILED,
                           "%s is already defined with UUID %s - cannot change "
                           "UUID to the UUID specified %s",
                           name,
                           uuid,
                           dominfo->uuid);
                goto out;
        }

        xml = system_to_xml(dominfo);
        if (xml != NULL) {
                CU_DEBUG("New XML is:\n%s", xml);
                connect_and_create(xml, ref, &s);
        }

        if (s.rc == CMPI_RC_OK) {
                set_autostart(vssd, ref, dom);
                /* try trigger indication */
                bool ind_rc = trigger_indication(_BROKER, context,
                                      "ComputerSystemModifiedIndication", ref);
                if (!ind_rc) {
                        CU_DEBUG("Unable to trigger indication for "
                                 "system modify, dom is '%s'", name);
                }

        }

 out:
        free(xml);
        virDomainFree(dom);
        virConnectClose(conn);
        cleanup_dominfo(&dominfo);

        return s;
}


static CMPIStatus mod_system_settings(CMPIMethodMI *self,
                                      const CMPIContext *context,
                                      const CMPIResult *results,
                                      const CMPIObjectPath *reference,
                                      const CMPIArgs *argsin,
                                      CMPIArgs *argsout)
{
        CMPIInstance *inst;
        CMPIStatus s;
        uint32_t rc;

        if (cu_get_inst_arg(argsin, "SystemSettings", &inst) != CMPI_RC_OK) {

                cu_statusf(_BROKER, &s,
                           CMPI_RC_ERR_FAILED,
                           "Missing SystemSettings");
                goto out;
        }

        s = update_system_settings(context, reference, inst);
 out:
        if (s.rc == CMPI_RC_OK)
                rc = CIM_SVPC_RETURN_COMPLETED;
        else
                rc = CIM_SVPC_RETURN_FAILED;

        CMReturnData(results, &rc, CMPI_uint32);

        return s;
}

typedef CMPIStatus (*resmod_fn)(struct domain *,
                                CMPIInstance *,
                                uint16_t,
                                const char *,
                                const char*);

static struct virt_device **find_list(struct domain *dominfo,
                                      uint16_t type,
                                      int **count)
{
        struct virt_device **list = NULL;

        if (type == CIM_RES_TYPE_NET) {
                list = &dominfo->dev_net;
                *count = &dominfo->dev_net_ct;
        } else if (type == CIM_RES_TYPE_DISK) {
                list = &dominfo->dev_disk;
                *count = &dominfo->dev_disk_ct;
        } else if (type == CIM_RES_TYPE_PROC) {
                list = &dominfo->dev_vcpu;
                *count = &dominfo->dev_vcpu_ct;
        } else if (type == CIM_RES_TYPE_MEM) {
                list = &dominfo->dev_mem;
                *count = &dominfo->dev_mem_ct;
        } else if (type == CIM_RES_TYPE_GRAPHICS) {
                list = &dominfo->dev_graphics;
                *count = &dominfo->dev_graphics_ct;
        } else if (type == CIM_RES_TYPE_INPUT) {
                list = &dominfo->dev_input;
                *count = &dominfo->dev_input_ct;
        }

        return list;
}

static CMPIStatus _resource_dynamic(struct domain *dominfo,
                                    struct virt_device *dev,
                                    enum ResourceAction action,
                                    const char *refcn)
{
        CMPIStatus s = { CMPI_RC_OK, NULL };
        virConnectPtr conn;
        virDomainPtr dom;
        int (*func)(virDomainPtr, struct virt_device *);

        CU_DEBUG("Enter _resource_dynamic");
        if (action == RESOURCE_ADD)
                func = attach_device;
        else if (action == RESOURCE_DEL)
                func = detach_device;
        else if (action == RESOURCE_MOD)
                func = change_device;
        else {
                CU_DEBUG("Unknown dynamic resource action: %i", action);
                cu_statusf(_BROKER, &s,
                           CMPI_RC_ERR_FAILED,
                           "Internal error (undefined resource action)");
                return s;
        }

        update_dominfo(dominfo, refcn);

        conn = connect_by_classname(_BROKER, refcn, &s);
        if (conn == NULL) {
                CU_DEBUG("Failed to connect");
                return s;
        }

        dom = virDomainLookupByName(conn, dominfo->name);
        if (dom == NULL) {
                CU_DEBUG("Failed to lookup VS `%s'", dominfo->name);
                virt_set_status(_BROKER, &s,
                                CMPI_RC_ERR_NOT_FOUND,
                                conn,
                                "Virtual System `%s' not found", dominfo->name);
                goto out;
        }

        if (!domain_online(dom)) {
                CU_DEBUG("VS `%s' not online; skipping dynamic update",
                         dominfo->name);
                cu_statusf(_BROKER, &s,
                           CMPI_RC_OK,
                           "");
                goto out;
        }

        CU_DEBUG("Doing dynamic device update for `%s'", dominfo->name);

        if (func(dom, dev) == 0) {
                virt_set_status(_BROKER, &s,
                                CMPI_RC_ERR_FAILED,
                                conn,
                                "Unable to change (%i) device", action);
        } else {
                cu_statusf(_BROKER, &s,
                           CMPI_RC_OK,
                           "");
        }
 out:
        virDomainFree(dom);
        virConnectClose(conn);

        return s;
}

static CMPIStatus resource_del(struct domain *dominfo,
                               CMPIInstance *rasd,
                               uint16_t type,
                               const char *devid,
                               const char *ns)
{
        CMPIStatus s;
        CMPIObjectPath *op;
        struct virt_device **_list;
        struct virt_device *list;
        int *count = NULL;
        int i;

        if (devid == NULL) {
                cu_statusf(_BROKER, &s,
                           CMPI_RC_ERR_INVALID_PARAMETER,
                           "Missing or incomplete InstanceID");
                goto out;
        }

        op = CMGetObjectPath(rasd, &s);
        if ((op == NULL) || (s.rc != CMPI_RC_OK))
                goto out;

        _list = find_list(dominfo, type, &count);
        if ((type == CIM_RES_TYPE_MEM) || (type == CIM_RES_TYPE_PROC) ||
            (*_list == NULL)) {
                cu_statusf(_BROKER, &s,
                           CMPI_RC_ERR_FAILED,
                           "Cannot delete resources of type %" PRIu16, type);
                goto out;
        }

        list = *_list;

        cu_statusf(_BROKER, &s,
                   CMPI_RC_ERR_FAILED,
                   "Device `%s' not found", devid);

        for (i = 0; i < *count; i++) {
                struct virt_device *dev = &list[i];

                if (STREQ(dev->id, devid)) {
                        if ((type == CIM_RES_TYPE_GRAPHICS) ||
                           (type == CIM_RES_TYPE_INPUT))
                                cu_statusf(_BROKER, &s, CMPI_RC_OK, "");
                        else {
                                s = _resource_dynamic(dominfo,
                                                      dev,
                                                      RESOURCE_DEL,
                                                      CLASSNAME(op));
                        }

                        dev->type = CIM_RES_TYPE_UNKNOWN;

                        break;
                }
        }

 out:
        return s;
}

static CMPIStatus resource_add(struct domain *dominfo,
                               CMPIInstance *rasd,
                               uint16_t type,
                               const char *devid,
                               const char *ns)
{
        CMPIStatus s;
        CMPIObjectPath *op;
        struct virt_device **_list;
        struct virt_device *list;
        struct virt_device *dev;
        int *count = NULL;
        const char *msg = NULL;
        char *error_msg = NULL;

        op = CMGetObjectPath(rasd, &s);
        if ((op == NULL) || (s.rc != CMPI_RC_OK))
                goto out;

        _list = find_list(dominfo, type, &count);
        if ((type == CIM_RES_TYPE_MEM) || (type == CIM_RES_TYPE_PROC) ||
           (_list == NULL)) {
                cu_statusf(_BROKER, &s,
                           CMPI_RC_ERR_FAILED,
                           "Cannot add resources of type %" PRIu16, type);
                goto out;
        }

        if (*count < 0) {
                /* If count is negative, there was an error
                 * building the list for this device class
                 */
                cu_statusf(_BROKER, &s,
                           CMPI_RC_ERR_FAILED,
                           "[TEMP] Cannot add resources of type %" PRIu16, type);
                goto out;
        }

        list = realloc(*_list, ((*count)+1)*sizeof(struct virt_device));
        if (list == NULL) {
                /* No memory */
                cu_statusf(_BROKER, &s,
                           CMPI_RC_ERR_FAILED,
                           "Failed to allocate memory");
                goto out;
        }

        *_list = list;
        memset(&list[*count], 0, sizeof(list[*count]));

        dev = &list[*count];

        dev->type = type;
        msg = rasd_to_vdev(rasd, dominfo, dev, ns, &error_msg);
        if (msg != NULL) {
                cu_statusf(_BROKER, &s,
                           CMPI_RC_ERR_FAILED,
                           "Add resource failed: %s, %s",
                           msg, error_msg);
                goto out;
        }

        if ((type == CIM_RES_TYPE_GRAPHICS) || (type == CIM_RES_TYPE_INPUT)) {
                (*count)++;
                cu_statusf(_BROKER, &s, CMPI_RC_OK, "");
                goto out;
        }

        s = _resource_dynamic(dominfo, dev, RESOURCE_ADD, CLASSNAME(op));
        if (s.rc != CMPI_RC_OK)
                goto out;

        cu_statusf(_BROKER, &s,
                   CMPI_RC_OK,
                   "");
        (*count)++;

 out:
        free(error_msg);

        return s;
}

static CMPIStatus resource_mod(struct domain *dominfo,
                               CMPIInstance *rasd,
                               uint16_t type,
                               const char *devid,
                               const char *ns)
{
        CMPIStatus s;
        CMPIObjectPath *op;
        struct virt_device **_list;
        struct virt_device *list;
        int *count;
        int i;
        const char *msg = NULL;
        char *error_msg = NULL;

        CU_DEBUG("Enter resource_mod");
        if (devid == NULL) {
                cu_statusf(_BROKER, &s,
                           CMPI_RC_ERR_INVALID_PARAMETER,
                           "Missing or incomplete InstanceID");
                goto out;
        }

        op = CMGetObjectPath(rasd, &s);
        if ((op == NULL) || (s.rc != CMPI_RC_OK))
                goto out;

        _list = find_list(dominfo, type, &count);
        if (_list != NULL)
                list = *_list;
        else {
                cu_statusf(_BROKER, &s,
                           CMPI_RC_ERR_FAILED,
                           "Cannot modify resources of type %" PRIu16, type);
                goto out;
        }

        cu_statusf(_BROKER, &s,
                   CMPI_RC_ERR_FAILED,
                   "Device `%s' not found", devid);

        for (i = 0; i < *count; i++) {
                struct virt_device *dev = &list[i];

                if (STREQ(dev->id, devid)) {
                        msg = rasd_to_vdev(rasd, dominfo, dev, ns, &error_msg);
                        if (msg != NULL) {
                                cu_statusf(_BROKER, &s,
                                           CMPI_RC_ERR_FAILED,
                                           "Modify resource failed: %s",
                                           msg);
                                goto out;
                        }

                        if ((type == CIM_RES_TYPE_GRAPHICS) ||
                            (type == CIM_RES_TYPE_INPUT))
                                cu_statusf(_BROKER, &s, CMPI_RC_OK, "");
                        else {
#if LIBVIR_VERSION_NUMBER < 9000
                                uint64_t qos_val = 0;
                                const char *qos_unitstr;
#endif
                                s = _resource_dynamic(dominfo,
                                                      dev,
                                                      RESOURCE_MOD,
                                                      CLASSNAME(op));

#if LIBVIR_VERSION_NUMBER < 9000
                                /* Network QoS support */
                                if ((type == CIM_RES_TYPE_NET) &&
                                    (dev->dev.net.mac != NULL) &&
                                    (dev->dev.net.source != NULL) &&
                                    (cu_get_u64_prop(rasd, "Reservation", &qos_val) == CMPI_RC_OK) &&
                                    (cu_get_str_prop(rasd, "AllocationUnits", &qos_unitstr) == CMPI_RC_OK) &&
                                    STREQ(qos_unitstr,"KiloBits per Second")) {
                                        remove_qos_for_mac(qos_val,
                                                           dev->dev.net.mac,
                                                           dev->dev.net.source);
                                        s = add_qos_for_mac(qos_val,
                                                            dev->dev.net.mac,
                                                            dev->dev.net.source);
                                }
#endif
                        }
                        break;
                }
        }

 out:
        free(error_msg);

        return s;
}

static CMPIStatus _update_resources_for(const CMPIContext *context,
                                        const CMPIObjectPath *ref,
                                        virDomainPtr dom,
                                        const char *devid,
                                        CMPIInstance *rasd,
                                        resmod_fn func)
{
        CMPIStatus s = {CMPI_RC_OK, NULL};
        struct domain *dominfo = NULL;
        uint16_t type;
        char *xml = NULL;
        const char *indication;
        CMPIObjectPath *op;
        struct inst_list list;
        CMPIInstance *prev_inst = NULL;
        CMPIInstance *orig_inst = NULL;

        CU_DEBUG("Enter _update_resources_for");
        inst_list_init(&list);
        if (!get_dominfo(dom, &dominfo)) {
                virt_set_status(_BROKER, &s,
                                CMPI_RC_ERR_FAILED,
                                virDomainGetConnect(dom),
                                "Internal error (getting domain info)");
                goto out;
        }

        op = CMGetObjectPath(rasd, NULL);
        if (op == NULL) {
                cu_statusf(_BROKER, &s,
                           CMPI_RC_ERR_FAILED,
                           "Unable to get RASD path");
                goto out;
        }

        if (res_type_from_rasd_classname(CLASSNAME(op), &type) != CMPI_RC_OK) {
                cu_statusf(_BROKER, &s,
                           CMPI_RC_ERR_FAILED,
                           "Unable to determine RASD type");
                goto out;
        }

        if (func == &resource_add) {
                indication = RASD_IND_CREATED;
        } else if (func == &resource_del) {
                indication = RASD_IND_DELETED;
        } else {
                indication = RASD_IND_MODIFIED;
                char *dummy_name = NULL;

                if (asprintf(&dummy_name, "%s/%s",dominfo->name, devid) == -1) {
                        CU_DEBUG("Unable to set name");
                        goto out;
                }
                s = get_rasd_by_name(_BROKER,
                                     ref,
                                     dummy_name,
                                     type,
                                     NULL,
                                     &orig_inst);
                free(dummy_name);

                if (s.rc != CMPI_RC_OK) {
                        CU_DEBUG("Failed to get Previous Instance");
                        goto out;
                }

                prev_inst = orig_inst;
                s = cu_merge_instances(rasd, orig_inst);
                if (s.rc != CMPI_RC_OK) {
                        CU_DEBUG("Failed to merge Instances");
                        goto out;
                }
                rasd = orig_inst;

        }

        s = func(dominfo, rasd, type, devid, NAMESPACE(ref));
        if (s.rc != CMPI_RC_OK) {
                CU_DEBUG("Resource transform function failed");
                goto out;
        }

        xml = system_to_xml(dominfo);
        if (xml != NULL) {
                CU_DEBUG("New XML:\n%s", xml);
                connect_and_create(xml, ref, &s);

                if (inst_list_add(&list, rasd) == 0) {
                        CU_DEBUG("Unable to add RASD instance to the list\n");
                        goto out;
                }
                raise_rasd_indication(context,
                                      indication,
                                      prev_inst,
                                      ref,
                                      &list);
        } else {
                cu_statusf(_BROKER, &s,
                           CMPI_RC_ERR_FAILED,
                           "Internal error (xml generation failed)");
        }

 out:
        cleanup_dominfo(&dominfo);
        free(xml);
        inst_list_free(&list);

        return s;
}

static CMPIStatus get_instanceid(CMPIInstance *rasd,
                                 char **domain,
                                 char **devid)
{
        CMPIStatus s = {CMPI_RC_OK, NULL};
        const char *id;

        if (cu_get_str_prop(rasd, "InstanceID", &id) != CMPI_RC_OK) {
                cu_statusf(_BROKER, &s,
                           CMPI_RC_ERR_INVALID_PARAMETER,
                           "Missing InstanceID in RASD");
                return s;
        }

        if (!parse_fq_devid(id, domain, devid)) {
                cu_statusf(_BROKER, &s,
                           CMPI_RC_ERR_INVALID_PARAMETER,
                           "Invalid InstanceID `%s'", id);
                return s;
        }

        return s;
}

static CMPIStatus _update_resource_settings(const CMPIContext *context,
                                            const CMPIObjectPath *ref,
                                            const char *domain,
                                            CMPIArray *resources,
                                            const CMPIResult *results,
                                            resmod_fn func,
                                            struct inst_list *list)
{
        int i;
        virConnectPtr conn = NULL;
        CMPIStatus s;
        int count;
        uint32_t rc = CIM_SVPC_RETURN_FAILED;

        CU_DEBUG("Enter _update_resource_settings");
        conn = connect_by_classname(_BROKER, CLASSNAME(ref), &s);
        if (conn == NULL) {
                cu_statusf(_BROKER, &s,
                           CMPI_RC_ERR_FAILED,
                           "Unable to connect to hypervisor");
                goto out;
        }

        count = CMGetArrayCount(resources, NULL);

        for (i = 0; i < count; i++) {
                CMPIData item;
                CMPIInstance *inst;
                char *name = NULL;
                char *devid = NULL;
                virDomainPtr dom = NULL;

                item = CMGetArrayElementAt(resources, i, NULL);
                inst = item.value.inst;

                /* If we were passed a domain name, then we're doing
                 * an AddResources, which means we ignore the InstanceID
                 * of the RASD.  If not, then we get the domain name
                 * from the InstanceID of the RASD each time through.
                 */
                if (domain == NULL) {
                        s = get_instanceid(inst, &name, &devid);
                        if (s.rc != CMPI_RC_OK)
                                break;
                } else {
                        name = strdup(domain);
                }

                dom = virDomainLookupByName(conn, name);
                if (dom == NULL) {
                        virt_set_status(_BROKER, &s,
                                        CMPI_RC_ERR_NOT_FOUND,
                                        conn,
                                        "Referenced domain `%s' does not exist",
                                        name);
                        goto end;
                }

                s = _update_resources_for(context,
                                          ref,
                                          dom,
                                          devid,
                                          inst,
                                          func);

 end:
                free(name);
                free(devid);
                virDomainFree(dom);

                if (s.rc != CMPI_RC_OK)
                        break;

                inst_list_add(list, inst);
        }
 out:
        if (s.rc == CMPI_RC_OK)
                rc = CIM_SVPC_RETURN_COMPLETED;

        CMReturnData(results, &rc, CMPI_uint32);

        virConnectClose(conn);

        return s;
}

static CMPIStatus rasd_refs_to_insts(const CMPIContext *ctx,
                                     const CMPIObjectPath *reference,
                                     CMPIArray *arr,
                                     CMPIArray **ret_arr)
{
        CMPIStatus s = {CMPI_RC_OK, NULL};
        CMPIArray *tmp_arr;
        int i;
        int c;

        c = CMGetArrayCount(arr, &s);
        if (s.rc != CMPI_RC_OK)
                return s;

        tmp_arr = CMNewArray(_BROKER,
                             c,
                             CMPI_instance,
                             &s);

        for (i = 0; i < c; i++) {
                CMPIData d;
                CMPIObjectPath *ref;
                CMPIInstance *inst = NULL;
                const char *id;
                uint16_t type;

                d = CMGetArrayElementAt(arr, i, &s);
                ref = d.value.ref;
                if (s.rc != CMPI_RC_OK) {
                        CU_DEBUG("Unable to get ResourceSettings[%i]", i);
                        continue;
                }

                if (cu_get_str_path(ref, "InstanceID", &id) != CMPI_RC_OK) {
                        CU_DEBUG("Unable to get InstanceID of `%s'",
                                 REF2STR(ref));
                        continue;
                }

                if (res_type_from_rasd_classname(CLASSNAME(ref), &type) !=
                    CMPI_RC_OK) {
                        CU_DEBUG("Unable to get type of `%s'",
                                 REF2STR(ref));
                        continue;
                }

                s = get_rasd_by_name(_BROKER, reference, id, type, NULL, &inst);
                if (s.rc != CMPI_RC_OK)
                        continue;

                CMSetArrayElementAt(tmp_arr, i,
                                    &inst,
                                    CMPI_instance);

        }

        *ret_arr = tmp_arr;

        return s;
}

static CMPIArray *set_result_res(struct inst_list *list,
                                 const char *ns)
{
        CMPIStatus s = {CMPI_RC_OK, NULL};
        CMPIObjectPath *op = NULL;
        CMPIArray *res = NULL;
        int i = 0;

        if (list->cur == 0) {
                CU_DEBUG("No resources were added or modified");
                return res;
        }

        res = CMNewArray(_BROKER, list->cur, CMPI_ref, &s);
        if ((s.rc != CMPI_RC_OK) || (res == NULL)) {
                CU_DEBUG("Unable to create results array");
                goto out;
        }

        for (i = 0; list->list[i] != NULL; i++) {
                op = CMGetObjectPath(list->list[i], NULL);
                if (op == NULL) {
                       CU_DEBUG("Unable to RASD reference");
                       goto out;
                }

                CMSetNameSpace(op, ns);

                s = CMSetArrayElementAt(res, i, (CMPIValue *)&op, CMPI_ref);
                if (s.rc != CMPI_RC_OK) {
                       CU_DEBUG("Error setting results array element");
                       goto out;
                }
         }

 out:
         if (s.rc != CMPI_RC_OK)
                 res = NULL;

         return res;
}

static CMPIStatus add_resource_settings(CMPIMethodMI *self,
                                        const CMPIContext *context,
                                        const CMPIResult *results,
                                        const CMPIObjectPath *reference,
                                        const CMPIArgs *argsin,
                                        CMPIArgs *argsout)
{
        CMPIArray *arr;
        CMPIStatus s;
        CMPIObjectPath *sys;
        char *domain = NULL;
        CMPIArray *res = NULL;
        struct inst_list list;

        inst_list_init(&list);

        if (cu_get_array_arg(argsin, "ResourceSettings", &arr) != CMPI_RC_OK) {
                cu_statusf(_BROKER, &s,
                           CMPI_RC_ERR_FAILED,
                           "Missing ResourceSettings");
                return s;
        }

        if (cu_get_ref_arg(argsin,
                           "AffectedConfiguration",
                           &sys) != CMPI_RC_OK) {
                cu_statusf(_BROKER, &s,
                           CMPI_RC_ERR_INVALID_PARAMETER,
                           "Missing AffectedConfiguration parameter");
                return s;
        }

        if (!parse_instanceid(sys, NULL, &domain)) {
                cu_statusf(_BROKER, &s,
                           CMPI_RC_ERR_INVALID_PARAMETER,
                           "AffectedConfiguration has invalid InstanceID");
                return s;
        }

        s = _update_resource_settings(context,
                                      reference,
                                      domain,
                                      arr,
                                      results,
                                      resource_add,
                                      &list);

        free(domain);

        res = set_result_res(&list, NAMESPACE(reference));

        inst_list_free(&list);

        CMAddArg(argsout, "ResultingResourceSettings", &res, CMPI_refA);

        return s;
}

static CMPIStatus mod_resource_settings(CMPIMethodMI *self,
                                        const CMPIContext *context,
                                        const CMPIResult *results,
                                        const CMPIObjectPath *reference,
                                        const CMPIArgs *argsin,
                                        CMPIArgs *argsout)
{
        CMPIArray *arr;
        CMPIStatus s;
        CMPIArray *res = NULL;
        struct inst_list list;

        CU_DEBUG("Enter mod_resource_settings");
        inst_list_init(&list);

        if (cu_get_array_arg(argsin, "ResourceSettings", &arr) != CMPI_RC_OK) {
                cu_statusf(_BROKER, &s,
                           CMPI_RC_ERR_FAILED,
                           "Missing ResourceSettings");
                return s;
        }

        s = _update_resource_settings(context,
                                      reference,
                                      NULL,
                                      arr,
                                      results,
                                      resource_mod,
                                      &list);

        res = set_result_res(&list, NAMESPACE(reference));

        inst_list_free(&list);

        CMAddArg(argsout, "ResultingResourceSettings", &res, CMPI_refA);

        return s;
}

static CMPIStatus rm_resource_settings(CMPIMethodMI *self,
                                       const CMPIContext *context,
                                       const CMPIResult *results,
                                       const CMPIObjectPath *reference,
                                       const CMPIArgs *argsin,
                                       CMPIArgs *argsout)
{
        /* The RemoveResources case is different from either Add or
         * Modify, because it takes references instead of instances
         */

        CMPIArray *arr;
        CMPIArray *resource_arr;
        CMPIStatus s;
        struct inst_list list;

        inst_list_init(&list);

        if (cu_get_array_arg(argsin, "ResourceSettings", &arr) != CMPI_RC_OK) {
                cu_statusf(_BROKER, &s,
                           CMPI_RC_ERR_FAILED,
                           "Missing ResourceSettings");
                goto out;
        }

        s = rasd_refs_to_insts(context, reference, arr, &resource_arr);
        if (s.rc != CMPI_RC_OK)
                goto out;

        s = _update_resource_settings(context,
                                      reference,
                                      NULL,
                                      resource_arr,
                                      results,
                                      resource_del,
                                      &list);
 out:
        inst_list_free(&list);

        return s;
}

static struct method_handler DefineSystem = {
        .name = "DefineSystem",
        .handler = define_system,
        .args = {{"SystemSettings", CMPI_instance, false},
                 {"ResourceSettings", CMPI_instanceA, false},
                 {"ReferenceConfiguration", CMPI_ref, true},
                 ARG_END
        }
};

static struct method_handler DestroySystem = {
        .name = "DestroySystem",
        .handler = destroy_system,
        .args = {{"AffectedSystem", CMPI_ref, false},
                 ARG_END
        }
};

static struct method_handler AddResourceSettings = {
        .name = "AddResourceSettings",
        .handler = add_resource_settings,
        .args = {{"AffectedConfiguration", CMPI_ref, false},
                 {"ResourceSettings", CMPI_instanceA, false},
                 ARG_END
        }
};

static struct method_handler ModifyResourceSettings = {
        .name = "ModifyResourceSettings",
        .handler = mod_resource_settings,
        .args = {{"ResourceSettings", CMPI_instanceA, false},
                 ARG_END
        }
};

static struct method_handler ModifySystemSettings = {
        .name = "ModifySystemSettings",
        .handler = mod_system_settings,
        .args = {{"SystemSettings", CMPI_instance, false},
                 ARG_END
        }
};

static struct method_handler RemoveResourceSettings = {
        .name = "RemoveResourceSettings",
        .handler = rm_resource_settings,
        .args = {{"ResourceSettings", CMPI_refA, false},
                 ARG_END
        }
};

static struct method_handler *my_handlers[] = {
        &DefineSystem,
        &DestroySystem,
        &AddResourceSettings,
        &ModifyResourceSettings,
        &ModifySystemSettings,
        &RemoveResourceSettings,
        NULL,
};

STDIM_MethodMIStub(, Virt_VirtualSystemManagementService,
                   _BROKER, libvirt_cim_init(), my_handlers);

CMPIStatus get_vsms(const CMPIObjectPath *reference,
                    CMPIInstance **_inst,
                    const CMPIBroker *broker,
                    const CMPIContext *context,
                    bool is_get_inst)
{
        CMPIStatus s = {CMPI_RC_OK, NULL};
        CMPIInstance *inst = NULL;
        const char *name = NULL;
        const char *ccname = NULL;
        virConnectPtr conn = NULL;
        unsigned long hv_version = 0;
        unsigned long lv_version = 0;
        const char * hv_type = NULL;
        char *caption = NULL;
        char *lv_version_string = NULL;
        CMPIArray *array;
        uint16_t op_status;

        *_inst = NULL;
        conn = connect_by_classname(broker, CLASSNAME(reference), &s);
        if (conn == NULL) {
                if (is_get_inst)
                        cu_statusf(broker, &s,
                                   CMPI_RC_ERR_NOT_FOUND,
                                   "No such instance");

                return s;
        }

        inst = get_typed_instance(broker,
                                  pfx_from_conn(conn),
                                  "VirtualSystemManagementService",
                                  NAMESPACE(reference),
                                  true);

        if (inst == NULL) {
                CU_DEBUG("Failed to get typed instance");
                cu_statusf(broker, &s,
                           CMPI_RC_ERR_FAILED,
                           "Failed to create instance");
                goto out;
        }

        s = get_host_system_properties(&name,
                                       &ccname,
                                       reference,
                                       broker,
                                       context);
        if (s.rc != CMPI_RC_OK) {
                cu_statusf(broker, &s,
                           CMPI_RC_ERR_FAILED,
                           "Unable to get host attributes");
                goto out;
        }

        hv_type = virConnectGetType(conn);
        if (hv_type == NULL)
                hv_type = "Unkown";

        if (virConnectGetVersion(conn, &hv_version) < 0) {
                CU_DEBUG("Unable to get hypervisor version");
                hv_version = 0;
        }

        if (asprintf(&caption,
                     "%s %lu.%lu.%lu",
                     hv_type,
                     hv_version / 1000000,
                     (hv_version % 1000000) / 1000,
                     (hv_version % 1000000) % 1000) == -1)
                caption = NULL;

        if (caption != NULL)
                CMSetProperty(inst, "Caption",
                              (CMPIValue *)caption, CMPI_chars);
        else
                CMSetProperty(inst, "Caption",
                              (CMPIValue *)"Unknown Hypervisor", CMPI_chars);

	if (virGetVersion(&lv_version, hv_type, &hv_version) < 0) {
                CU_DEBUG("Unable to get libvirt version");
                lv_version= 0;
                hv_version= 0;
        }

        if (asprintf(&lv_version_string, "%lu.%lu.%lu",
                     lv_version / 1000000,
                     (lv_version % 1000000) / 1000,
                     (lv_version % 1000000) % 1000) == -1)
                lv_version_string = NULL;

        if (lv_version_string != NULL)
                CMSetProperty(inst, "LibvirtVersion",
                              (CMPIValue *)lv_version_string, CMPI_chars);
        else
                CMSetProperty(inst, "LibvirtVersion",
                              (CMPIValue *)"Unknown libvirt version",
                              CMPI_chars);

        CMSetProperty(inst, "Name",
                      (CMPIValue *)"Management Service", CMPI_chars);

        if (name != NULL)
                CMSetProperty(inst, "SystemName",
                              (CMPIValue *)name, CMPI_chars);

        if (ccname != NULL)
                CMSetProperty(inst, "SystemCreationClassName",
                              (CMPIValue *)ccname, CMPI_chars);

        CMSetProperty(inst, "Changeset",
                      (CMPIValue *)LIBVIRT_CIM_CS, CMPI_chars);

        CMSetProperty(inst, "Revision",
                      (CMPIValue *)LIBVIRT_CIM_RV, CMPI_chars);

        CMSetProperty(inst, "Release",
                      (CMPIValue *)PACKAGE_VERSION, CMPI_chars);

        array = CMNewArray(broker, 1, CMPI_uint16, &s);
        if ((s.rc != CMPI_RC_OK) || (CMIsNullObject(array)))
                goto out;

        op_status = CIM_OPERATIONAL_STATUS;
        CMSetArrayElementAt(array, 0, &op_status, CMPI_uint16);

        CMSetProperty(inst, "OperationalStatus",
                      (CMPIValue *)&array, CMPI_uint16A);

        if (is_get_inst) {
                s = cu_validate_ref(broker, reference, inst);
                if (s.rc != CMPI_RC_OK)
                        goto out;
        }

        cu_statusf(broker, &s,
                   CMPI_RC_OK,
                   "");
 out:
        free(caption);
        free(lv_version_string);
        virConnectClose(conn);
        *_inst = inst;

        return s;
}

static CMPIStatus return_vsms(const CMPIContext *context,
                              const CMPIObjectPath *reference,
                              const CMPIResult *results,
                              bool name_only,
                              bool is_get_inst)
{
        CMPIStatus s = {CMPI_RC_OK, NULL};
        CMPIInstance *inst;

        s = get_vsms(reference, &inst, _BROKER, context, is_get_inst);
        if (s.rc != CMPI_RC_OK || inst == NULL)
                goto out;

        if (name_only)
                cu_return_instance_name(results, inst);
        else
                CMReturnInstance(results, inst);
 out:
        return s;
}

static CMPIStatus EnumInstanceNames(CMPIInstanceMI *self,
                                    const CMPIContext *context,
                                    const CMPIResult *results,
                                    const CMPIObjectPath *reference)
{
        return return_vsms(context, reference, results, true, false);
}

static CMPIStatus EnumInstances(CMPIInstanceMI *self,
                                const CMPIContext *context,
                                const CMPIResult *results,
                                const CMPIObjectPath *reference,
                                const char **properties)
{

        return return_vsms(context, reference, results, false, false);
}

static CMPIStatus GetInstance(CMPIInstanceMI *self,
                              const CMPIContext *context,
                              const CMPIResult *results,
                              const CMPIObjectPath *ref,
                              const char **properties)
{
        return return_vsms(context, ref, results, false, true);
}

DEFAULT_CI();
DEFAULT_MI();
DEFAULT_DI();
DEFAULT_EQ();
DEFAULT_INST_CLEANUP();

STD_InstanceMIStub(,
                   Virt_VirtualSystemManagementService,
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
