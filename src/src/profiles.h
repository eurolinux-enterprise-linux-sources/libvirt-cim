/*
 * Copyright IBM Corp. 2007, 2008
 *
 * Authors:
 *  Jay Gagnon <grendel@linux.vnet.ibm.com>
 *  Heidi Eckhart <heidieck@linux.vnet.ibm.com>
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

/* Interop Namespace */
#define CIM_INTEROP_NS "root/interop"

struct reg_prof {
        uint16_t reg_org; // Valid: 1 = Other, 2 = DMTF
        char *reg_id;
        char *reg_name;
        char *reg_version;
        int ad_types;
        char *other_reg_org;
        char *ad_type_descriptions;
        char *scoping_class;
        char *central_class;
        struct reg_prof *scoping_profile;
};

struct reg_prof VirtualSystem = {
        .reg_org = 2,
        .reg_id = "CIM:DSP1057-VirtualSystem-1.0.0a",
        .reg_name = "Virtual System Profile",
        .reg_version = "1.0.0a",
        .ad_types = 3,
        .scoping_class = "ComputerSystem",
        .scoping_profile = NULL
};

struct reg_prof SystemVirtualization = {
        .reg_org = 2,
        .reg_id = "CIM:DSP1042-SystemVirtualization-1.0.0",
        .reg_name = "System Virtualization",
        .reg_version = "1.0.0",
        .ad_types = 3,
        .scoping_class = "HostSystem",
        .scoping_profile = &VirtualSystem
};

struct reg_prof GDRVP_Disk = {
        .reg_org = 2,
        .reg_id = "CIM:DSP1059-GenericDeviceResourceVirtualization-1.0.0_d",
        .reg_name = "Generic Device Resource Virtualization",
        .reg_version = "1.0.0",
        .ad_types = 3,
        .central_class = "DiskPool",
        .scoping_class = NULL,
        .scoping_profile = &SystemVirtualization
};

struct reg_prof GDRVP_Net = {
        .reg_org = 2,
        .reg_id = "CIM:DSP1059-GenericDeviceResourceVirtualization-1.0.0_n",
        .reg_name = "Generic Device Resource Virtualization",
        .reg_version = "1.0.0",
        .ad_types = 3,
        .central_class = "NetworkPool",
        .scoping_class = NULL,
        .scoping_profile = &SystemVirtualization
};

struct reg_prof GDRVP_Proc = {
        .reg_org = 2,
        .reg_id = "CIM:DSP1059-GenericDeviceResourceVirtualization-1.0.0_p",
        .reg_name = "Generic Device Resource Virtualization",
        .reg_version = "1.0.0",
        .ad_types = 3,
        .central_class = "ProcessorPool",
        .scoping_class = NULL,
        .scoping_profile = &SystemVirtualization
};

struct reg_prof MemoryResourceVirtualization = {
        .reg_org = 2,
        .reg_id = "CIM:DSP1045-MemoryResourceVirtualization-1.0.0",
        .reg_name = "Memory Resource Virtualization",
        .reg_version = "1.0.0",
        .ad_types = 3,
        .scoping_class = NULL,
        .central_class = "MemoryPool",
        .scoping_profile = &SystemVirtualization
};

struct reg_prof VirtualSystemMigration = {
        .reg_org = 2,
        .reg_id = "CIM:DSP1081-VirtualSystemMigration-0.8.1",
        .reg_name = "Virtual System Migration",
        .reg_version = "0.8.1",
        .ad_types = 3,
        .scoping_class = NULL,
        .central_class = "VirtualSystemMigrationService",
        .scoping_profile = &SystemVirtualization
};

struct reg_prof KVMRedirection = {
        .reg_org = 2,
        .reg_id = "CIM:DSP1076-KVMRedirection-1.0.0",
        .reg_name = "KVM Redirection",
        .reg_version = "1.0.0",
        .ad_types = 3,
        .scoping_class = "HostSystem",
        .central_class = "ConsoleRedirectionService",
        .scoping_profile = &SystemVirtualization
};

struct reg_prof AllocationCapabilities = {
        .reg_org = 2,
        .reg_id = "CIM:DSP1043-AllocationCapabilities-1.0.0a",
        .reg_name = "Allocation Capabilities",
        .reg_version = "1.0.0a",
        .ad_types = 3,
        .scoping_class = "ComputerSystem",
        .central_class = "AllocationCapabilities",
        .scoping_profile = &SystemVirtualization
};

struct reg_prof ResourceAllocation_Disk = {
        .reg_org = 2,
        .reg_id = "CIM:DSP1041-ResourceAllocation-1.1.0c_d",
        .reg_name = "Resource Allocation",
        .reg_version = "1.1.0c",
        .ad_types = 3,
        .scoping_class = "ComputerSystem",
        .central_class = "DiskPool",
        .scoping_profile = &SystemVirtualization
};

struct reg_prof ResourceAllocation_Net = {
        .reg_org = 2,
        .reg_id = "CIM:DSP1041-ResourceAllocation-1.1.0c_n",
        .reg_name = "Resource Allocation",
        .reg_version = "1.1.0c",
        .ad_types = 3,
        .scoping_class = "ComputerSystem",
        .central_class = "NetworkPool",
        .scoping_profile = &SystemVirtualization
};

struct reg_prof ResourceAllocation_Proc = {
        .reg_org = 2,
        .reg_id = "CIM:DSP1041-ResourceAllocation-1.1.0c_p",
        .reg_name = "Resource Allocation",
        .reg_version = "1.1.0c",
        .ad_types = 3,
        .scoping_class = "ComputerSystem",
        .central_class = "ProcessorPool",
        .scoping_profile = &SystemVirtualization
};

struct reg_prof ResourceAllocation_Mem = {
        .reg_org = 2,
        .reg_id = "CIM:DSP1041-ResourceAllocation-1.1.0c_m",
        .reg_name = "Resource Allocation",
        .reg_version = "1.1.0c",
        .ad_types = 3,
        .scoping_class = "ComputerSystem",
        .central_class = "MemoryPool",
        .scoping_profile = &SystemVirtualization
};

// Make sure to add pointer to your reg_prof struct here.
struct reg_prof *profiles[] = {
        &SystemVirtualization,
        &VirtualSystem,
        &GDRVP_Disk,
        &GDRVP_Net,
        &GDRVP_Proc,
        &MemoryResourceVirtualization,
        &VirtualSystemMigration,
        &KVMRedirection,
        &AllocationCapabilities,
        &ResourceAllocation_Disk,
        &ResourceAllocation_Net,
        &ResourceAllocation_Proc,
        &ResourceAllocation_Mem,
        NULL
};

/*
 * Local Variables:
 * mode: C
 * c-set-style: "K&R"
 * tab-width: 8
 * c-basic-offset: 8
 * indent-tabs-mode: nil
 * End:
 */
