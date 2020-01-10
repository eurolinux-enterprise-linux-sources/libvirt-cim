/*
 * Copyright IBM Corp. 2007
 *
 * Authors:
 *  Dan Smith <danms@us.ibm.com>
 *  Guolian Yun <yunguol@cn.ibm.com>
 *  Jay Gagnon <grendel@linux.vnet.ibm.com>
 *  Zhengang Li <lizg@cn.ibm.com>
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
#ifndef __DEVICE_PARSING_H
#define __DEVICE_PARSING_H

#include <stdint.h>
#include <stdbool.h>
#include <libvirt/libvirt.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>

#include "../src/svpc_types.h"

struct vsi_device {
        char *vsi_type;
        char *manager_id;
        char *type_id;
        char *type_id_version;
        char *instance_id;
        char *filter_ref;
        char *profile_id;
};

struct disk_device {
        char *type;
        char *device;
        char *driver;
        char *driver_type;
        char *source;
        char *virtual_dev;
        enum {DISK_UNKNOWN, DISK_PHY, DISK_FILE, DISK_FS} disk_type;
        bool readonly;
        bool shareable;
        char *bus_type;
        char *cache;
        char *access_mode; /* access modes for DISK_FS (filesystem) type */
};

struct net_device {
        char *type;
        char *mac;
        char *source;
        char *poolid;
        char *model;
        char *device;
        char *net_mode;
        char *filter_ref;
        uint64_t reservation;
        uint64_t limit;
        struct vsi_device vsi;
};

struct mem_device {
        uint64_t size;
        uint64_t maxsize;
};

struct vcpu_device {
        uint64_t quantity;
        uint32_t weight;
        uint64_t limit;
};

struct emu_device {
        char *path;
};

struct vnc_device {
        char *port;
        char *host;
        char *keymap;
        char *passwd;
};

struct sdl_device {
        char *display;
        char *xauth;
        char *fullscreen;
};

struct graphics_device {
        char *type;
        union {
            struct vnc_device vnc;
            struct sdl_device sdl;
        } dev;
};

struct input_device {
        char *type;
        char *bus;
};

struct virt_device {
        uint16_t type;
        union {
                struct disk_device disk;
                struct net_device net;
                struct mem_device mem;
                struct vcpu_device vcpu;
                struct emu_device emu;
                struct graphics_device graphics;
                struct input_device input;
        } dev;
        char *id;
};

struct pv_os_info {
        char *type;
        char *kernel;
        char *initrd;
        char *cmdline;
};

struct fv_os_info {
        char *type; /* Should always be 'hvm' */
        char *loader;
        unsigned bootlist_ct;
        char **bootlist;
};

struct lxc_os_info {
        char *type; /* Should always be 'exe' */
        char *init;
};

struct domain {
        enum { DOMAIN_UNKNOWN,
               DOMAIN_XENPV,
               DOMAIN_XENFV, 
               DOMAIN_KVM,
               DOMAIN_QEMU,
               DOMAIN_LXC } type;
        char *name;
        char *typestr; /*xen, kvm, etc */
        char *uuid;
        char *bootloader;
        char *bootloader_args;
        char *clock;
        bool acpi;
        bool apic;
        bool pae;
        int autostrt;

        union {
                struct pv_os_info pv;
                struct fv_os_info fv;
                struct lxc_os_info lxc;
        } os_info;

        int on_poweroff;
        int on_reboot;
        int on_crash;

        struct virt_device *dev_graphics;
        int dev_graphics_ct;

        struct virt_device *dev_emu;

        struct virt_device *dev_input;
        int dev_input_ct;

        struct virt_device *dev_mem;
        int dev_mem_ct;

        struct virt_device *dev_net;
        int dev_net_ct;

        struct virt_device *dev_disk;
        int dev_disk_ct;

        struct virt_device *dev_vcpu;
        int dev_vcpu_ct;
};

struct virt_device *virt_device_dup(struct virt_device *dev);

int disk_type_from_file(const char *path);

int get_dominfo(virDomainPtr dom, struct domain **dominfo);
int get_dominfo_from_xml(const char *xml, struct domain **dominfo);

void cleanup_dominfo(struct domain **dominfo);

/* VIR_DOMAIN_XML_SECURE will always be set besides flags */
int get_devices(virDomainPtr dom, struct virt_device **list, int type,
                                                    unsigned int flags);

void cleanup_virt_device(struct virt_device *dev);
void cleanup_virt_devices(struct virt_device **devs, int count);

char *get_node_content(xmlNode *node);
char *get_attr_value(xmlNode *node, char *attrname);

char *get_fq_devid(char *host, char *_devid);
int parse_fq_devid(const char *devid, char **host, char **device);

int attach_device(virDomainPtr dom, struct virt_device *dev);
int detach_device(virDomainPtr dom, struct virt_device *dev);
int change_device(virDomainPtr dom, struct virt_device *dev);

bool has_kvm_domain_type(xmlNodePtr node);

#define XSTREQ(x, y) (STREQ((char *)x, y))
#define STRPROP(d, p, n) (d->p = get_node_content(n))

#endif

/*
 * Local Variables:
 * mode: C
 * c-set-style: "K&R"
 * tab-width: 8
 * c-basic-offset: 8
 * indent-tabs-mode: nil
 * End:
 */
