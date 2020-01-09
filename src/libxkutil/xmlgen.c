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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <uuid.h>

#include <libxml/tree.h>
#include <libxml/xmlsave.h>

#include "xmlgen.h"
#include "list_util.h"

#ifndef TEST
#include "misc_util.h"
#include <libcmpiutil/libcmpiutil.h>
#include "cmpimacs.h"
#endif

#define XML_ERROR "Failed to allocate XML memory"

typedef const char *(*devfn_t)(xmlNodePtr node, struct domain *dominfo);
typedef const char *(*poolfn_t)(xmlNodePtr node, struct virt_pool *pool);
typedef const char *(*resfn_t)(xmlNodePtr node, struct virt_pool_res *res);

static char *disk_block_xml(xmlNodePtr root, struct disk_device *dev)
{
        xmlNodePtr disk;
        xmlNodePtr tmp;

        disk = xmlNewChild(root, NULL, BAD_CAST "disk", NULL);
        if (disk == NULL)
                return XML_ERROR;
        xmlNewProp(disk, BAD_CAST "type", BAD_CAST "block");
        if (dev->device)
                xmlNewProp(disk, BAD_CAST "device", BAD_CAST dev->device);

        if (dev->driver) {
                tmp = xmlNewChild(disk, NULL, BAD_CAST "driver", NULL);
                if (tmp == NULL)
                        return XML_ERROR;
                xmlNewProp(tmp, BAD_CAST "name", BAD_CAST dev->driver);
                if (dev->driver_type)
                        xmlNewProp(tmp, BAD_CAST "type",
                                   BAD_CAST dev->driver_type);
                if (dev->cache)
                        xmlNewProp(tmp, BAD_CAST "cache", BAD_CAST dev->cache);
        }

        if ((dev->source != NULL) && (!XSTREQ(dev->source, "/dev/null"))) {
                tmp = xmlNewChild(disk, NULL, BAD_CAST "source", NULL);
                if (tmp == NULL)
                       return XML_ERROR;
                xmlNewProp(tmp, BAD_CAST "dev", BAD_CAST dev->source);
        }

        tmp = xmlNewChild(disk, NULL, BAD_CAST "target", NULL);
        if (tmp == NULL)
                return XML_ERROR;
        xmlNewProp(tmp, BAD_CAST "dev", BAD_CAST dev->virtual_dev);
        if (dev->bus_type)
                xmlNewProp(tmp, BAD_CAST "bus", BAD_CAST dev->bus_type);

        if (dev->readonly)
                xmlNewChild(disk, NULL, BAD_CAST "readonly", NULL);

        if (dev->shareable)
                xmlNewChild(disk, NULL, BAD_CAST "shareable", NULL);

        return NULL;
}

static const char *disk_file_xml(xmlNodePtr root, struct disk_device *dev)
{
        xmlNodePtr disk;
        xmlNodePtr tmp;

        disk = xmlNewChild(root, NULL, BAD_CAST "disk", NULL);
        if (disk == NULL)
                return XML_ERROR;
        xmlNewProp(disk, BAD_CAST "type", BAD_CAST "file");
        if (dev->device)
                xmlNewProp(disk, BAD_CAST "device", BAD_CAST dev->device);

        if (dev->driver) {
                tmp = xmlNewChild(disk, NULL, BAD_CAST "driver", NULL);
                if (tmp == NULL)
                        return XML_ERROR;
                xmlNewProp(tmp, BAD_CAST "name", BAD_CAST dev->driver);
                if (dev->driver_type)
                        xmlNewProp(tmp, BAD_CAST "type",
                                   BAD_CAST dev->driver_type);
                if (dev->cache)
                        xmlNewProp(tmp, BAD_CAST "cache", BAD_CAST dev->cache);
        }

        if (dev->device != NULL && XSTREQ(dev->device, "cdrom") &&
            XSTREQ(dev->source, "")) {
                /* This is the situation that user defined a cdrom device without
                 disk in it, so skip generating a line saying "source", for that
                 xml defination for libvirt should not have this defined in this
                 situation. */
        } else {
                tmp = xmlNewChild(disk, NULL, BAD_CAST "source", NULL);
                if (tmp == NULL)
                        return XML_ERROR;
                xmlNewProp(tmp, BAD_CAST "file", BAD_CAST dev->source);
        }

        tmp = xmlNewChild(disk, NULL, BAD_CAST "target", NULL);
        if (tmp == NULL)
                return XML_ERROR;
        xmlNewProp(tmp, BAD_CAST "dev", BAD_CAST dev->virtual_dev);
        if (dev->bus_type)
                xmlNewProp(tmp, BAD_CAST "bus", BAD_CAST dev->bus_type);


        if (dev->readonly)
                xmlNewChild(disk, NULL, BAD_CAST "readonly", NULL);

        if (dev->shareable)
                xmlNewChild(disk, NULL, BAD_CAST "shareable", NULL);


        return NULL;
}

static const char *disk_fs_xml(xmlNodePtr root, struct disk_device *dev)
{
        xmlNodePtr fs;
        xmlNodePtr tmp;

        fs = xmlNewChild(root, NULL, BAD_CAST "filesystem", NULL);
        if (fs == NULL)
                return XML_ERROR;

        tmp = xmlNewChild(fs, NULL, BAD_CAST "source", NULL);
        if (tmp == NULL)
                return XML_ERROR;
        xmlNewProp(tmp, BAD_CAST "dir", BAD_CAST dev->source);

        tmp = xmlNewChild(fs, NULL, BAD_CAST "target", NULL);
        if (tmp == NULL)
                return XML_ERROR;
        xmlNewProp(tmp, BAD_CAST "dir", BAD_CAST dev->virtual_dev);

        return NULL;
}

static const char *disk_xml(xmlNodePtr root, struct domain *dominfo)
{
        int i;
        const char *msg = NULL;;

        for (i = 0; (i < dominfo->dev_disk_ct) && (msg == NULL); i++) {
                struct virt_device *dev = &dominfo->dev_disk[i];
                if (dev->type == CIM_RES_TYPE_UNKNOWN)
                        continue;

                struct disk_device *disk = &dominfo->dev_disk[i].dev.disk;
                CU_DEBUG("Disk: %i %s %s",
                         disk->disk_type,
                         disk->source,
                         disk->virtual_dev);
                if (disk->disk_type == DISK_PHY)
                        msg = disk_block_xml(root, disk);
                else if (disk->disk_type == DISK_FILE)
                        /* If it's not a block device, we assume a file,
                           which should be a reasonable fail-safe */
                        msg = disk_file_xml(root, disk);
                else if (disk->disk_type == DISK_FS)
                        msg = disk_fs_xml(root, disk);
                else
                        msg = "Unknown disk type";
        }

        return msg;
}

static const char *set_net_vsi(xmlNodePtr nic, struct vsi_device *dev)
{
        xmlNodePtr tmp;

        tmp = xmlNewChild(nic, NULL, BAD_CAST "virtualport", NULL);
        if (tmp == NULL)
                return XML_ERROR;
        xmlNewProp(tmp, BAD_CAST "type", BAD_CAST dev->vsi_type);

        tmp = xmlNewChild(tmp, NULL, BAD_CAST "parameters", NULL);
        if (tmp == NULL)
                return XML_ERROR;
        if (STREQ(dev->vsi_type, "802.1Qbh")) {
                if (dev->profile_id != NULL)
                        xmlNewProp(tmp, BAD_CAST "profileid",
                                   BAD_CAST dev->profile_id);
        } else {
                if (dev->manager_id != NULL)
                        xmlNewProp(tmp, BAD_CAST "managerid",
                                   BAD_CAST dev->manager_id);
                if (dev->type_id != NULL)
                        xmlNewProp(tmp, BAD_CAST "typeid",
                                   BAD_CAST dev->type_id);
                if (dev->type_id_version != NULL)
                        xmlNewProp(tmp, BAD_CAST "typeidversion",
                                   BAD_CAST dev->type_id_version);
                if (dev->instance_id != NULL)
                        xmlNewProp(tmp, BAD_CAST "instanceid",
                                   BAD_CAST dev->instance_id);
        }

        return NULL;
}

static const char *set_net_source(xmlNodePtr nic,
                                  struct net_device *dev,
                                  const char *src_type)
{
        xmlNodePtr tmp;

        if (dev->source != NULL) {
                tmp = xmlNewChild(nic, NULL, BAD_CAST "source", NULL);
                if (tmp == NULL)
                        return XML_ERROR;
                if (STREQ(src_type, "direct")) {
                        xmlNewProp(tmp, BAD_CAST "dev", BAD_CAST dev->source);
                        if (dev->net_mode != NULL)
                                xmlNewProp(tmp, BAD_CAST "mode",
                                           BAD_CAST dev->net_mode);
                } else
                        xmlNewProp(tmp, BAD_CAST src_type,
                                   BAD_CAST dev->source);
        } else
                return XML_ERROR;

        return NULL;
}


static const char *bridge_net_to_xml(xmlNodePtr nic, struct net_device *dev)
{
        const char *script = "vif-bridge";
        xmlNodePtr tmp;
        const char *msg = NULL;

        tmp = xmlNewChild(nic, NULL, BAD_CAST "script", NULL);
        if (tmp == NULL)
                return XML_ERROR;
        xmlNewProp(tmp, BAD_CAST "path", BAD_CAST script);

        msg = set_net_source(nic, dev, "bridge");

        return msg;
}

static const char *net_xml(xmlNodePtr root, struct domain *dominfo)
{
        int i;
        const char *msg = NULL;
        xmlNodePtr nic;
        xmlNodePtr tmp;

        for (i = 0; (i < dominfo->dev_net_ct) && (msg == NULL); i++) {
                struct virt_device *dev = &dominfo->dev_net[i];
                if (dev->type == CIM_RES_TYPE_UNKNOWN)
                        continue;

                struct net_device *net = &dev->dev.net;

                nic = xmlNewChild(root, NULL, BAD_CAST "interface", NULL);
                if (nic == NULL)
                        return XML_ERROR;
                xmlNewProp(nic, BAD_CAST "type", BAD_CAST net->type);

                if (net->mac != NULL) {
                        tmp = xmlNewChild(nic, NULL, BAD_CAST "mac", NULL);
                        if (tmp == NULL)
                                return XML_ERROR;
                        xmlNewProp(tmp, BAD_CAST "address", BAD_CAST net->mac);
                }

                if (net->device != NULL) {
                        tmp = xmlNewChild(nic, NULL, BAD_CAST "target", NULL);
                        if (tmp == NULL)
                                return XML_ERROR;
                        xmlNewProp(tmp, BAD_CAST "dev", BAD_CAST net->device);
                }


                if (net->model != NULL) {
                        tmp = xmlNewChild(nic, NULL, BAD_CAST "model", NULL);
                        if (tmp == NULL)
                                return XML_ERROR;
                        xmlNewProp(tmp, BAD_CAST "type", BAD_CAST net->model);
                }

                if (net->filter_ref != NULL) {
                        tmp = xmlNewChild(nic, NULL,
                                BAD_CAST "filterref", NULL);
                        if (tmp == NULL)
                                return XML_ERROR;
                        xmlNewProp(tmp, BAD_CAST "filter",
                                BAD_CAST net->filter_ref);
                }

#if LIBVIR_VERSION_NUMBER >= 9000
                /* Network QoS settings saved under <bandwidth> XML section */
                if (net->reservation || net->limit) {
                        int ret;
                        char *string = NULL;

                        tmp = xmlNewChild(nic, NULL,
                                          BAD_CAST "bandwidth", NULL);
                        if (tmp == NULL)
                                return XML_ERROR;

                        /* Set inbound bandwidth from Reservation & Limit */
                        tmp = xmlNewChild(tmp, NULL,
                                          BAD_CAST "inbound", NULL);
                        if (tmp == NULL)
                                return XML_ERROR;

                        if (net->reservation) {
                                ret = asprintf(&string, "%" PRIu64,
                                               net->reservation);
                                if (ret == -1)
                                        return XML_ERROR;
                                xmlNewProp(tmp, BAD_CAST "average",
                                           BAD_CAST string);
                                free(string);
                        }

                        if (net->limit) {
                                ret = asprintf(&string, "%" PRIu64,
                                               net->limit);
                                if (ret == -1)
                                        return XML_ERROR;
                                xmlNewProp(tmp, BAD_CAST "peak",
                                           BAD_CAST string);
                                free(string);
                        }
                }
#endif

                if (STREQ(dev->dev.net.type, "network"))
                        msg = set_net_source(nic, net, "network");
                else if (STREQ(dev->dev.net.type, "bridge"))
                        msg = bridge_net_to_xml(nic, net);
                else if (STREQ(dev->dev.net.type, "user"))
                        continue;
                else if (STREQ(dev->dev.net.type, "direct")) {
                        msg = set_net_source(nic, net, "direct");
                        if (net->vsi.vsi_type != NULL) {
                                struct vsi_device *vsi = &dev->dev.net.vsi;
                                msg = set_net_vsi(nic, vsi);
                        }
                }
                else
                        msg = "Unknown interface type";
        }

        return msg;
}

static const char *vcpu_xml(xmlNodePtr root, struct domain *dominfo)
{
        struct vcpu_device *vcpu;
        xmlNodePtr tmp;
        int ret;
        char *string = NULL;

        if (dominfo->dev_vcpu == NULL)
                return NULL;

        vcpu = &dominfo->dev_vcpu[0].dev.vcpu;

        ret = asprintf(&string, "%" PRIu64, vcpu->quantity);
        if (ret == -1)
                return XML_ERROR;

        tmp = xmlNewChild(root, NULL, BAD_CAST "vcpu", BAD_CAST string);
        free(string);

        if (tmp == NULL)
                return XML_ERROR;
        else
                return NULL;
}

#if LIBVIR_VERSION_NUMBER >= 9000
static const char *cputune_xml(xmlNodePtr root, struct domain *dominfo)
{
        struct vcpu_device *vcpu;
        xmlNodePtr cputune, tmp;
        int ret;
        char *string = NULL;

        if (dominfo->dev_vcpu == NULL)
                return NULL;

        vcpu = &dominfo->dev_vcpu[0].dev.vcpu;

        /* CPU cgroup setting saved by libvirt under <cputune> XML section */
        cputune = xmlNewChild(root, NULL, BAD_CAST "cputune", NULL);
        if (cputune == NULL)
                return XML_ERROR;

        /* Get the CPU cgroup setting from the VCPU RASD.Weight property */
        ret = asprintf(&string,
                       "%d",
                       vcpu->weight);
        if (ret == -1)
                return XML_ERROR;

        tmp = xmlNewChild(cputune,
                          NULL,
                          BAD_CAST "shares",
                          BAD_CAST string);
        free(string);

        if (tmp == NULL)
                return XML_ERROR;
        else
                return NULL;
}
#endif

static const char *mem_xml(xmlNodePtr root, struct domain *dominfo)
{
        struct mem_device *mem;
        xmlNodePtr tmp = NULL;
        int ret;
        char *string = NULL;

        if (dominfo->dev_mem == NULL)
                return NULL;

        mem = &dominfo->dev_mem[0].dev.mem;

        ret = asprintf(&string, "%" PRIu64, mem->size);
        if (ret == -1)
                goto out;
        tmp = xmlNewChild(root,
                          NULL,
                          BAD_CAST "currentMemory",
                          BAD_CAST string);

        free(string);
        tmp = NULL;

        ret = asprintf(&string, "%" PRIu64, mem->maxsize);
        if (ret == -1)
                goto out;
        tmp = xmlNewChild(root,
                          NULL,
                          BAD_CAST "memory",
                          BAD_CAST string);

        free(string);
 out:
        if (tmp == NULL)
                return XML_ERROR;
        else
                return NULL;
}

static const char *emu_xml(xmlNodePtr root, struct domain *dominfo)
{
        struct emu_device *emu;
        xmlNodePtr tmp;

        if (dominfo->dev_emu == NULL)
                return NULL;

        emu = &dominfo->dev_emu->dev.emu;
        tmp = xmlNewChild(root, NULL, BAD_CAST "emulator", BAD_CAST emu->path);
        if (tmp == NULL)
                return XML_ERROR;

        return NULL;
}

static const char *graphics_vnc_xml(xmlNodePtr root,
                       struct graphics_device *dev)
{
        xmlNodePtr tmp = NULL;

        tmp = xmlNewChild(root, NULL, BAD_CAST "graphics", NULL);
        if (tmp == NULL)
                return XML_ERROR;

        xmlNewProp(tmp, BAD_CAST "type", BAD_CAST dev->type);

        if (STREQC(dev->type, "sdl")) {
                if (dev->dev.sdl.display) {
                        xmlNewProp(tmp, BAD_CAST "display",
                                        BAD_CAST dev->dev.sdl.display);
                }
                if (dev->dev.sdl.xauth) {
                        xmlNewProp(tmp, BAD_CAST "xauth",
                                        BAD_CAST dev->dev.sdl.xauth);
                }
                if (dev->dev.sdl.fullscreen) {
                        xmlNewProp(tmp, BAD_CAST "fullscreen",
                                        BAD_CAST dev->dev.sdl.fullscreen);
                }
                return NULL;
        }

        if (dev->dev.vnc.port) {
                xmlNewProp(tmp, BAD_CAST "port", BAD_CAST dev->dev.vnc.port);
                if (STREQC(dev->dev.vnc.port, "-1"))
                        xmlNewProp(tmp, BAD_CAST "autoport", BAD_CAST "yes");
                else
                        xmlNewProp(tmp, BAD_CAST "autoport", BAD_CAST "no");
        }

        if (dev->dev.vnc.host)
                xmlNewProp(tmp, BAD_CAST "listen", BAD_CAST dev->dev.vnc.host);

        if (dev->dev.vnc.passwd)
                xmlNewProp(tmp, BAD_CAST "passwd", BAD_CAST dev->dev.vnc.passwd);

        if (dev->dev.vnc.keymap)
                xmlNewProp(tmp, BAD_CAST "keymap", BAD_CAST dev->dev.vnc.keymap);

        return NULL;
}

static const char *graphics_pty_xml(xmlNodePtr root,
                       struct graphics_device *dev)
{
        xmlNodePtr pty = NULL;
        xmlNodePtr tmp = NULL;

        pty = xmlNewChild(root, NULL, BAD_CAST dev->type, NULL);
        if (pty == NULL)
                return XML_ERROR;

        xmlNewProp(pty, BAD_CAST "type", BAD_CAST "pty");

        tmp = xmlNewChild(pty, NULL, BAD_CAST "source", NULL);
        if (tmp == NULL)
                return XML_ERROR;

        if(dev->dev.vnc.host)
                xmlNewProp(tmp, BAD_CAST "path", BAD_CAST dev->dev.vnc.host);

        tmp = xmlNewChild(pty, NULL, BAD_CAST "target", NULL);
        if (tmp == NULL)
                return XML_ERROR;

        if(dev->dev.vnc.port)
                xmlNewProp(tmp, BAD_CAST "port", BAD_CAST dev->dev.vnc.port);

        return NULL;
}

static const char *graphics_xml(xmlNodePtr root, struct domain *dominfo)
{
        const char *msg = NULL;
        int i;

        for (i = 0; i < dominfo->dev_graphics_ct; i++) {
                struct virt_device *_dev = &dominfo->dev_graphics[i];
                if (_dev->type == CIM_RES_TYPE_UNKNOWN)
                        continue;

                struct graphics_device *dev = &_dev->dev.graphics;

                if (STREQC(dev->type, "vnc") || STREQC(dev->type, "sdl"))
                        msg = graphics_vnc_xml(root, dev);
                else if (STREQC(dev->type, "console") ||
                        STREQC(dev->type, "serial"))
                        msg = graphics_pty_xml(root, dev);
                else
                        continue;

                if(msg != NULL)
                        return msg;
        }

        return NULL;
}

static const char *input_xml(xmlNodePtr root, struct domain *dominfo)
{
        int i;

        for (i = 0; i < dominfo->dev_input_ct; i++) {
                xmlNodePtr tmp;
                struct virt_device *_dev = &dominfo->dev_input[i];
                if (_dev->type == CIM_RES_TYPE_UNKNOWN)
                        continue;

                struct input_device *dev = &_dev->dev.input;

                tmp = xmlNewChild(root, NULL, BAD_CAST "input", NULL);
                if (tmp == NULL)
                        return XML_ERROR;

                xmlNewProp(tmp, BAD_CAST "type", BAD_CAST dev->type);
                xmlNewProp(tmp, BAD_CAST "bus", BAD_CAST dev->bus);
        }

        return NULL;
}

static char *system_xml(xmlNodePtr root, struct domain *domain)
{
        xmlNodePtr tmp;

        tmp = xmlNewChild(root, NULL, BAD_CAST "name", BAD_CAST domain->name);

        if (domain->bootloader) {
                tmp = xmlNewChild(root,
                                  NULL,
                                  BAD_CAST "bootloader",
                                  BAD_CAST domain->bootloader);
        }

        if (domain->bootloader_args) {
                tmp = xmlNewChild(root,
                                  NULL,
                                  BAD_CAST "bootloader_args",
                                  BAD_CAST domain->bootloader_args);
        }

        tmp = xmlNewChild(root,
                          NULL,
                          BAD_CAST "on_poweroff",
                          BAD_CAST vssd_recovery_action_str(domain->on_poweroff));

        tmp = xmlNewChild(root,
                          NULL,
                          BAD_CAST "on_crash",
                          BAD_CAST vssd_recovery_action_str(domain->on_crash));

        tmp = xmlNewChild(root,
                          NULL,
                          BAD_CAST "uuid",
                          BAD_CAST domain->uuid);

        if (domain->clock != NULL) {
                tmp = xmlNewChild(root, NULL, BAD_CAST "clock", NULL);
                xmlNewProp(tmp, BAD_CAST "offset", BAD_CAST domain->clock);
        }

        return NULL;
}

static char *_xenpv_os_xml(xmlNodePtr root, struct domain *domain)
{
        struct pv_os_info *os = &domain->os_info.pv;
        xmlNodePtr tmp;

        if (os->type == NULL)
                os->type = strdup("linux");

        if (os->kernel == NULL)
                os->kernel = strdup("/dev/null");

        tmp = xmlNewChild(root, NULL, BAD_CAST "type", BAD_CAST os->type);
        if (tmp == NULL)
                return XML_ERROR;

        tmp = xmlNewChild(root, NULL, BAD_CAST "kernel", BAD_CAST os->kernel);
        if (tmp == NULL)
                return XML_ERROR;

        tmp = xmlNewChild(root, NULL, BAD_CAST "initrd", BAD_CAST os->initrd);
        if (tmp == NULL)
                return XML_ERROR;

        tmp = xmlNewChild(root, NULL, BAD_CAST "cmdline", BAD_CAST os->cmdline);
        if (tmp == NULL)
                return XML_ERROR;

        return NULL;
}

static int _fv_bootlist_xml(xmlNodePtr root, struct fv_os_info *os)
{
        unsigned i;
        xmlNodePtr tmp;

        for (i = 0; i < os->bootlist_ct; i++) {
                tmp = xmlNewChild(root, NULL, BAD_CAST "boot", NULL);
                if (tmp == NULL)
                        return 0;

                xmlNewProp(tmp, BAD_CAST "dev", BAD_CAST os->bootlist[i]);
        }

        return 1;
}

static char *_xenfv_os_xml(xmlNodePtr root, struct domain *domain)
{
        struct fv_os_info *os = &domain->os_info.fv;
        xmlNodePtr tmp;
        unsigned ret;

        if (os->type == NULL)
                os->type = strdup("hvm");

        if (os->loader == NULL)
                os->loader = strdup("/usr/lib/xen/boot/hvmloader");

        if (os->bootlist_ct == 0) {
                os->bootlist_ct = 1;
                os->bootlist = (char **)calloc(1, sizeof(char *));
                os->bootlist[0] = strdup("hd");
        }

        tmp = xmlNewChild(root, NULL, BAD_CAST "type", BAD_CAST os->type);
        if (tmp == NULL)
                return XML_ERROR;

        tmp = xmlNewChild(root, NULL, BAD_CAST "loader", BAD_CAST os->loader);
        if (tmp == NULL)
                return XML_ERROR;

        ret = _fv_bootlist_xml(root, os);
        if (ret == 0)
                return XML_ERROR;

        return NULL;
}

static char *_kvm_os_xml(xmlNodePtr root, struct domain *domain)
{
        struct fv_os_info *os = &domain->os_info.fv;
        xmlNodePtr tmp;
        unsigned ret;

        if (os->type == NULL)
                os->type = strdup("hvm");

        if (os->bootlist_ct == 0) {
                os->bootlist_ct = 1;
                os->bootlist = (char **)calloc(1, sizeof(char *));
                os->bootlist[0] = strdup("hd");
        }

        tmp = xmlNewChild(root, NULL, BAD_CAST "type", BAD_CAST os->type);
        if (tmp == NULL)
                return XML_ERROR;

        ret = _fv_bootlist_xml(root, os);
        if (ret == 0)
                return XML_ERROR;

        return NULL;
}

static char *_lxc_os_xml(xmlNodePtr root, struct domain *domain)
{
        struct lxc_os_info *os = &domain->os_info.lxc;
        xmlNodePtr tmp;

        if (os->type == NULL)
                os->type = strdup("exe");

        tmp = xmlNewChild(root, NULL, BAD_CAST "init", BAD_CAST os->init);
        if (tmp == NULL)
                return XML_ERROR;

        tmp = xmlNewChild(root, NULL, BAD_CAST "type", BAD_CAST os->type);
        if (tmp == NULL)
                return XML_ERROR;

        return NULL;
}

static char *os_xml(xmlNodePtr root, struct domain *domain)
{
        xmlNodePtr os;

        os = xmlNewChild(root, NULL, BAD_CAST "os", NULL);
        if (os == NULL)
                return "Failed to allocate XML memory";

        if (domain->type == DOMAIN_XENPV)
                return _xenpv_os_xml(os, domain);
        else if (domain->type == DOMAIN_XENFV)
                return _xenfv_os_xml(os, domain);
        else if ((domain->type == DOMAIN_KVM) || (domain->type == DOMAIN_QEMU))
                return _kvm_os_xml(os, domain);
        else if (domain->type == DOMAIN_LXC)
                return _lxc_os_xml(os, domain);
        else
                return "Unsupported domain type";
}

static char *features_xml(xmlNodePtr root, struct domain *domain)
{
        xmlNodePtr features;

        features = xmlNewChild(root, NULL, BAD_CAST "features", NULL);
        if (features == NULL)
                return "Failed to allocate XML memory";

        if (domain->acpi)
                xmlNewChild(features, NULL, BAD_CAST "acpi", NULL);

        if (domain->apic)
                xmlNewChild(features, NULL, BAD_CAST "apic", NULL);

        if (domain->pae)
                xmlNewChild(features, NULL, BAD_CAST "pae", NULL);

        return NULL;
}

static char *tree_to_xml(xmlNodePtr root)
{
        xmlBufferPtr buffer = NULL;
        xmlSaveCtxtPtr savectx = NULL;
        char *xml = NULL;
        bool done = false;

        buffer = xmlBufferCreate();
        if (buffer == NULL) {
                CU_DEBUG("Failed to allocate XML buffer");
                goto out;
        }

        savectx = xmlSaveToBuffer(buffer, NULL, XML_SAVE_FORMAT);
        if (savectx == NULL) {
                CU_DEBUG("Failed to create save context");
                goto out;
        }

        if (xmlSaveTree(savectx, root) < 0) {
                CU_DEBUG("Failed to generate XML tree");
                goto out;
        }

        done = true;
 out:
        xmlSaveClose(savectx);

        if (done) {
                xml = strdup((char *)xmlBufferContent(buffer));
                if (xml == NULL) {
                        CU_DEBUG("Failed to allocate memory for XML");
                }
        }

        xmlBufferFree(buffer);

        return xml;
}

char *device_to_xml(struct virt_device *_dev)
{
        char *xml = NULL;
        int type = _dev->type;
        xmlNodePtr root = NULL;
        const char *msg;
        struct domain *dominfo;
        devfn_t func;
        struct virt_device *dev = NULL;

        dominfo = calloc(1, sizeof(*dominfo));
        if (dominfo == NULL)
                goto out;

        dev = virt_device_dup(_dev);
        if (dev == NULL)
                goto out;

        root = xmlNewNode(NULL, BAD_CAST "tmp");
        if (root == NULL) {
                cleanup_virt_devices(&dev, 1);
                goto out;
        }

        switch (type) {
        case CIM_RES_TYPE_DISK:
                func = disk_xml;
                dominfo->dev_disk_ct = 1;
                dominfo->dev_disk = dev;
                break;
        case CIM_RES_TYPE_PROC:
                func = vcpu_xml;
                dominfo->dev_vcpu_ct = 1;
                dominfo->dev_vcpu = dev;
                break;
        case CIM_RES_TYPE_NET:
                func = net_xml;
                dominfo->dev_net_ct = 1;
                dominfo->dev_net = dev;
                break;
        case CIM_RES_TYPE_MEM:
                func = mem_xml;
                dominfo->dev_mem_ct = 1;
                dominfo->dev_mem = dev;
                break;
        case CIM_RES_TYPE_EMU:
                func = emu_xml;
                dominfo->dev_emu = dev;
                break;
        case CIM_RES_TYPE_GRAPHICS:
                func = graphics_xml;
                dominfo->dev_graphics_ct = 1;
                dominfo->dev_graphics = dev;
                break;
        case CIM_RES_TYPE_INPUT:
                func = input_xml;
                dominfo->dev_input_ct = 1;
                dominfo->dev_input = dev;
                break;
        default:
                cleanup_virt_devices(&dev, 1);
                goto out;
        }

        msg = func(root, dominfo);
        if (msg != NULL) {
                CU_DEBUG("Failed to create device XML: %s", msg);
                goto out;
        }

        xml = tree_to_xml(root->children);
 out:
        CU_DEBUG("Created Device XML:\n%s\n", xml);

        cleanup_dominfo(&dominfo);
        xmlFreeNode(root);

        return xml;
}

char *system_to_xml(struct domain *dominfo)
{
        xmlNodePtr root = NULL;
        xmlNodePtr devices = NULL;
        char *xml = NULL;
        uint8_t uuid[16];
        char uuidstr[37];
        const char *domtype;
        const char *msg = XML_ERROR;
        int i;
        devfn_t device_handlers[] = {
                &disk_xml,
                &net_xml,
                &input_xml,
                &graphics_xml,
                &emu_xml,
                NULL
        };

        if ((dominfo->type == DOMAIN_XENPV) || (dominfo->type == DOMAIN_XENFV))
                domtype = "xen";
        else if (dominfo->type == DOMAIN_KVM)
                domtype = "kvm";
        else if (dominfo->type == DOMAIN_QEMU)
                domtype = "qemu";
        else if (dominfo->type == DOMAIN_LXC)
                domtype = "lxc";
        else
                domtype = "unknown";

        if (dominfo->uuid) {
                CU_DEBUG("Using existing UUID: %s", dominfo->uuid);
        } else {
                CU_DEBUG("New UUID");
                uuid_generate(uuid);
                uuid_unparse(uuid, uuidstr);
                dominfo->uuid = strdup(uuidstr);
        }

        root = xmlNewNode(NULL, BAD_CAST "domain");
        if (root == NULL)
                goto out;

        if (xmlNewProp(root, BAD_CAST "type", BAD_CAST domtype) == NULL)
                goto out;

        msg = system_xml(root, dominfo);
        if (msg != NULL)
                goto out;

        msg = os_xml(root, dominfo);
        if (msg != NULL)
                goto out;

        msg = features_xml(root, dominfo);
        if (msg != NULL)
                goto out;

        msg = mem_xml(root, dominfo);
        if (msg != NULL)
                goto out;

        msg = vcpu_xml(root, dominfo);
        if (msg != NULL)
                goto out;

#if LIBVIR_VERSION_NUMBER >= 9000
        /* Recent libvirt versions add new <cputune> section to XML */
        msg = cputune_xml(root, dominfo);
        if (msg != NULL)
                goto out;
#endif

        devices = xmlNewChild(root, NULL, BAD_CAST "devices", NULL);
        if (devices == NULL) {
                msg = XML_ERROR;
                goto out;
        }

        for (i = 0; device_handlers[i] != NULL; i++) {
                devfn_t fn = device_handlers[i];

                msg = fn(devices, dominfo);
                if (msg != NULL)
                        goto out;
        }

        msg = XML_ERROR;
        if (dominfo->type == DOMAIN_LXC) {
                xmlNodePtr cons;

                cons = xmlNewChild(devices, NULL, BAD_CAST "console", NULL);
                if (cons == NULL)
                        goto out;

                if (xmlNewProp(cons, BAD_CAST "type", BAD_CAST "pty") == NULL)
                        goto out;
        }

        msg = NULL;
        xml = tree_to_xml(root);
        if (xml == NULL)
                msg = "XML generation failed";
 out:
        if (msg != NULL) {
                CU_DEBUG("Failed to create XML: %s", msg);
        }

        xmlFreeNode(root);

        return xml;
}

static const char *net_pool_xml(xmlNodePtr root,
                                struct virt_pool *_pool)
{
        xmlNodePtr net = NULL;
        xmlNodePtr ip = NULL;
        xmlNodePtr forward = NULL;
        xmlNodePtr dhcp = NULL;
        xmlNodePtr range = NULL;
        struct net_pool *pool = &_pool->pool_info.net;

        net = xmlNewChild(root, NULL, BAD_CAST "network", NULL);
        if (net == NULL)
                goto out;

        if (xmlNewChild(net, NULL, BAD_CAST "name", BAD_CAST _pool->id) == NULL)
                goto out;

        if (xmlNewChild(net, NULL, BAD_CAST "bridge", NULL) == NULL)
                goto out;

        if (pool->forward_mode != NULL) {
                forward = xmlNewChild(net, NULL, BAD_CAST "forward", NULL);
                if (forward == NULL)
                        goto out;

                if (xmlNewProp(forward,
                               BAD_CAST "mode",
                               BAD_CAST pool->forward_mode) == NULL)
                        goto out;

                if (pool->forward_dev != NULL) {
                        if (xmlNewProp(forward,
                                       BAD_CAST "dev",
                                       BAD_CAST pool->forward_dev) == NULL)
                                goto out;
                }
        }

        ip = xmlNewChild(net, NULL, BAD_CAST "ip", NULL);
        if (ip == NULL)
                goto out;

        if (xmlNewProp(ip, BAD_CAST "address", BAD_CAST pool->addr) == NULL)
                goto out;

        if (xmlNewProp(ip, BAD_CAST "netmask", BAD_CAST pool->netmask) == NULL)
                goto out;

        if ((pool->ip_start != NULL) && (pool->ip_end != NULL)) {
                dhcp = xmlNewChild(ip, NULL, BAD_CAST "dhcp", NULL);
                if (dhcp == NULL)
                        goto out;

                range = xmlNewChild(dhcp, NULL, BAD_CAST "range", NULL);
                if (range == NULL)
                        goto out;

                if (xmlNewProp(range,
                               BAD_CAST "start",
                               BAD_CAST pool->ip_start) == NULL)
                        goto out;

                if (xmlNewProp(range,
                               BAD_CAST "end",
                               BAD_CAST pool->ip_end) == NULL)
                        goto out;
        }

        return NULL;

 out:
        return XML_ERROR;
}

static const char *disk_pool_type_to_str(uint16_t type)
{
        switch (type) {
        case DISK_POOL_DIR:
                return "dir";
        case DISK_POOL_FS:
                return "fs";
        case DISK_POOL_NETFS:
                return "netfs";
        case DISK_POOL_DISK:
                return "disk";
        case DISK_POOL_ISCSI:
                return "iscsi";
        case DISK_POOL_LOGICAL:
                return "logical";
        case DISK_POOL_SCSI:
                return "scsi";
        default:
                CU_DEBUG("Unsupported disk pool type");
        }

        return NULL;
}

static const char *set_disk_pool_source(xmlNodePtr disk,
                                        struct disk_pool *pool)
{
        xmlNodePtr src;
        xmlNodePtr tmp;
        uint16_t i;

        src = xmlNewChild(disk, NULL, BAD_CAST "source", NULL);
        if (src == NULL)
                return XML_ERROR;

        for (i = 0; i < pool->device_paths_ct; i++) {
                tmp = xmlNewChild(src, NULL, BAD_CAST "device", BAD_CAST NULL);
                if (tmp == NULL)
                        return XML_ERROR;

                if (xmlNewProp(tmp,
                               BAD_CAST "path",
                               BAD_CAST pool->device_paths[i]) == NULL)
                        return XML_ERROR;
        }

        if (pool->host != NULL) {
                tmp = xmlNewChild(src, NULL, BAD_CAST "host", BAD_CAST NULL);
                if (tmp == NULL)
                        return XML_ERROR;

                if (xmlNewProp(tmp,
                               BAD_CAST "name",
                               BAD_CAST pool->host) == NULL)
                        return XML_ERROR;
        }

        if (pool->src_dir != NULL) {
                tmp = xmlNewChild(src, NULL, BAD_CAST "dir", BAD_CAST NULL);
                if (tmp == NULL)
                        return XML_ERROR;

                if (xmlNewProp(tmp,
                               BAD_CAST "path",
                               BAD_CAST pool->src_dir) == NULL)
                        return XML_ERROR;
        }

        if (pool->adapter != NULL) {
                tmp = xmlNewChild(src, NULL, BAD_CAST "adapter", BAD_CAST NULL);
                if (tmp == NULL)
                        return XML_ERROR;

                if (xmlNewProp(tmp,
                               BAD_CAST "name",
                               BAD_CAST pool->adapter) == NULL)
                        return XML_ERROR;

                if (pool->port_name != NULL) {
                        if (xmlNewProp(tmp,
                                       BAD_CAST "wwpn",
                                       BAD_CAST pool->port_name) == NULL)
                                return XML_ERROR;
                }

                if (pool->node_name != NULL) {
                        if (xmlNewProp(tmp,
                                       BAD_CAST "wwnn",
                                       BAD_CAST pool->node_name) == NULL)
                                return XML_ERROR;
                }
        }

        return NULL;
}

static const char *disk_pool_xml(xmlNodePtr root,
                                 struct virt_pool *_pool)
{
        xmlNodePtr disk = NULL;
        xmlNodePtr name = NULL;
        xmlNodePtr target = NULL;
        xmlNodePtr path = NULL;
        const char *type = NULL;
        const char *msg = NULL;
        struct disk_pool *pool = &_pool->pool_info.disk;

        type = disk_pool_type_to_str(pool->pool_type);
        if (type == NULL)
                goto out;

        disk = xmlNewChild(root, NULL, BAD_CAST "pool", NULL);
        if (disk == NULL)
                goto out;

        if (xmlNewProp(disk, BAD_CAST "type", BAD_CAST type) == NULL)
                goto out;

        name = xmlNewChild(disk, NULL, BAD_CAST "name", BAD_CAST _pool->id);
        if (name == NULL)
                goto out;

        if (pool->pool_type != DISK_POOL_DIR) {
                msg = set_disk_pool_source(disk, pool);
                if (msg != NULL)
                        return msg;
        }

        target = xmlNewChild(disk, NULL, BAD_CAST "target", NULL);
        if (target == NULL)
                goto out;

        path = xmlNewChild(target, NULL, BAD_CAST "path", BAD_CAST pool->path);
        if (path == NULL)
                goto out;

        return NULL;

 out:
        return XML_ERROR;
 }

char *pool_to_xml(struct virt_pool *pool) {
        char *xml = NULL;
        xmlNodePtr root = NULL;
        int type = pool->type;
        const char *msg = NULL;
        poolfn_t func;

        root = xmlNewNode(NULL, BAD_CAST "tmp");
        if (root == NULL) {
                msg = XML_ERROR;
                goto out;
        }

        switch (type) {
        case CIM_RES_TYPE_NET:
                func = net_pool_xml;
                break;
        case CIM_RES_TYPE_DISK:
                func = disk_pool_xml;
                break;
        default:
                CU_DEBUG("pool_to_xml: invalid type specified: %d", type);
                msg = "pool_to_xml: invalid type specified";
                goto out;
        }

        msg = func(root, pool);
        if (msg != NULL)
                goto out;

        xml = tree_to_xml(root->children);
        if (xml == NULL)
                msg = "XML generation failed";
 out:
        if (msg != NULL) {
                CU_DEBUG("Failed to create pool XML: %s", msg);
        } else {
                CU_DEBUG("Created pool XML:\n%s\n", xml);
        }

        xmlFreeNode(root);

        return xml;
}

static const char *vol_format_type_to_str(uint16_t type)
{
        switch (type) {
        case VOL_FORMAT_RAW:
                return "raw";
        case VOL_FORMAT_QCOW2:
                return "qcow2";
        default:
                CU_DEBUG("Unsupported storage volume type");
        }

        return NULL;
}

static const char *storage_vol_xml(xmlNodePtr root,
                                   struct virt_pool_res *res)
{
        xmlNodePtr v = NULL;
        xmlNodePtr name = NULL;
        xmlNodePtr alloc = NULL;
        xmlNodePtr cap = NULL;
        xmlNodePtr target = NULL;
        xmlNodePtr path = NULL;
        xmlNodePtr format = NULL;
        const char *type = NULL;
        struct storage_vol *vol = &res->res.storage_vol;
        char *string = NULL;
        int ret;

        type = vol_format_type_to_str(vol->format_type);
        if (type == NULL)
                goto out;

        v = xmlNewChild(root, NULL, BAD_CAST "volume", NULL);
        if (v == NULL)
                goto out;

        name = xmlNewChild(v, NULL, BAD_CAST "name", BAD_CAST vol->vol_name);
        if (name == NULL)
                goto out;

        ret = asprintf(&string, "%" PRIu16, vol->alloc);
        if (ret == -1)
                return XML_ERROR;

        alloc = xmlNewChild(v, NULL, BAD_CAST "allocation", BAD_CAST string);
        if (alloc == NULL)
                goto out;

        free(string);
        string = NULL;

        if (vol->cap_units != NULL) {
                xmlAttrPtr tmp = NULL;
                tmp = xmlNewProp(cap, BAD_CAST "unit", BAD_CAST vol->cap_units);
                if (tmp == NULL)
                        goto out;
        }

        ret = asprintf(&string, "%" PRIu16, vol->cap);
        if (ret == -1)
                return XML_ERROR;

        cap = xmlNewChild(v, NULL, BAD_CAST "capacity", BAD_CAST string);
        if (cap == NULL)
                goto out;

        free(string);
        string = NULL;

        if (vol->cap_units != NULL) {
                xmlAttrPtr tmp = NULL;
                tmp = xmlNewProp(cap, BAD_CAST "unit", BAD_CAST vol->cap_units);
                if (tmp == NULL)
                        goto out;
        }

        target = xmlNewChild(v, NULL, BAD_CAST "target", NULL);
        if (target == NULL)
                goto out;

        path = xmlNewChild(target, NULL, BAD_CAST "path", BAD_CAST vol->path);
        if (path == NULL)
                goto out;

        format = xmlNewChild(target, NULL, BAD_CAST "format", NULL);
        if (format == NULL)
                goto out;

        if (xmlNewProp(format, BAD_CAST "type", BAD_CAST type) == NULL)
                goto out;

        /* FIXME:  Need to add permissions and label tags here */

        return NULL;

 out:
        free(string);
        return XML_ERROR;
 }

char *res_to_xml(struct virt_pool_res *res) {
        char *xml = NULL;
        xmlNodePtr root = NULL;
        int type = res->type;
        const char *msg = NULL;
        resfn_t func;

        root = xmlNewNode(NULL, BAD_CAST "tmp");
        if (root == NULL) {
                msg = XML_ERROR;
                goto out;
        }

        switch (type) {
        case CIM_RES_TYPE_IMAGE:
                func = storage_vol_xml;
                break;
        default:
                msg = "res_to_xml: invalid type specified";
                CU_DEBUG("%s %d", msg, type);
                goto out;
        }

        msg = func(root, res);
        if (msg != NULL)
                goto out;

        xml = tree_to_xml(root->children);
        if (xml == NULL)
                msg = "XML generation failed";
 out:
        if (msg != NULL) {
                CU_DEBUG("Failed to create res XML: %s", msg);
        } else {
                CU_DEBUG("Created res XML:\n%s\n", xml);
        }

        xmlFreeNode(root);

        return xml;
}

static bool filter_ref_foreach(void *list_data, void *user_data)
{
        char *filter = (char *) list_data;
        xmlNodePtr root = (xmlNodePtr) user_data;
        xmlNodePtr tmp = NULL;

        tmp = xmlNewChild(root, NULL, BAD_CAST "filterref", NULL);
        if (tmp == NULL) {
                CU_DEBUG("Error creating filterref node");
                return false;
        }

        if (xmlNewProp(tmp, BAD_CAST "filter", BAD_CAST list_data) == NULL) {
                CU_DEBUG("Error adding filter attribute '%s'", filter);
                return false;
        }

        return true;
}

char *filter_to_xml(struct acl_filter *filter)
{
        char *xml = NULL;
        xmlNodePtr root = NULL;
        xmlNodePtr tmp = NULL;

        root = xmlNewNode(NULL, BAD_CAST "filter");
        if (root == NULL)
                goto out;

        if (xmlNewProp(root, BAD_CAST "name", BAD_CAST filter->name) == NULL)
                goto out;

        if (filter->chain != NULL) {
                if (xmlNewProp(root, BAD_CAST "chain",
                        BAD_CAST filter->chain) == NULL)
                        goto out;
        }

        if (filter->uuid != NULL) {
                tmp = xmlNewChild(root, NULL, BAD_CAST "uuid",
                        BAD_CAST filter->uuid);
                if (tmp == NULL)
                        goto out;
        }

        if (!list_foreach(filter->refs, filter_ref_foreach, (void *) root))
                goto out;

        xml = tree_to_xml(root);

 out:
        CU_DEBUG("Filter XML: %s", xml);

        xmlFreeNode(root);

        return xml;
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
