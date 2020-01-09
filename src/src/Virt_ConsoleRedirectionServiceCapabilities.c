/*
 * Copyright IBM Corp. 2007
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
#include <stdlib.h>
#include <unistd.h>

#include <cmpidt.h>
#include <cmpift.h>
#include <cmpimacs.h>

#include <libcmpiutil/libcmpiutil.h>
#include <libcmpiutil/std_instance.h>

#include "misc_util.h"
#include "svpc_types.h"

#include "Virt_ConsoleRedirectionService.h"
#include "Virt_ConsoleRedirectionServiceCapabilities.h"

const static CMPIBroker *_BROKER;

static CMPIStatus set_inst_properties(const CMPIBroker *broker,
                                      const CMPIObjectPath *ref,
                                      CMPIInstance *inst)
{
        CMPIStatus s = {CMPI_RC_OK, NULL};
        CMPIBoolean element_edit = false;
        CMPIArray *array;
        uint16_t prop_val;

        CMSetProperty(inst, "InstanceID",
                      (CMPIValue *)"ConsoleRedirectionCapabilities", 
                      CMPI_chars);

        CMSetProperty(inst, "ElementName",
                      (CMPIValue *)"ConsoleRedirectionCapabilities", 
                      CMPI_chars);

        CMSetProperty(inst, "ElementNameEditSupported",
                      (CMPIValue *)&element_edit, CMPI_boolean);

        array = CMNewArray(broker, 1, CMPI_uint16, &s);
        if ((s.rc != CMPI_RC_OK) || CMIsNullObject(array))
                goto out;

        prop_val = (uint16_t)CIM_CRS_SHARING_MODE;
        CMSetArrayElementAt(array, 0, &prop_val, CMPI_uint16);

        CMSetProperty(inst, "SharingModeSupported",
                      (CMPIValue *)&array, CMPI_uint16A);

 out:
        return s;
}

CMPIStatus get_console_rs_caps(const CMPIBroker *broker,
                               const CMPIObjectPath *ref,
                               CMPIInstance **_inst,
                               bool is_get_inst)
{
        CMPIStatus s = {CMPI_RC_OK, NULL};
        CMPIInstance *inst = NULL;
        virConnectPtr conn = NULL;

        conn = connect_by_classname(broker, CLASSNAME(ref), &s);
        if (conn == NULL) {
                if (is_get_inst)
                        cu_statusf(broker, &s,
                                   CMPI_RC_ERR_NOT_FOUND,
                                   "No such instance");
                goto out;
        }

        inst = get_typed_instance(broker,
                                  pfx_from_conn(conn),
                                  "ConsoleRedirectionServiceCapabilities",
                                  NAMESPACE(ref));
        if (inst == NULL) {
                cu_statusf(broker, &s,
                           CMPI_RC_ERR_FAILED,
                           "Can not create instance for %s", CLASSNAME(ref));
                goto out;
        }

        s = set_inst_properties(broker, ref, inst);

        if (is_get_inst) {
                s = cu_validate_ref(broker, ref, inst);
                if (s.rc != CMPI_RC_OK)
                        goto out;
        }

        *_inst = inst;

 out:
        virConnectClose(conn);

        return s;
}

static CMPIStatus return_rs_cap(const CMPIObjectPath *ref,
                                const CMPIResult *results,
                                bool names_only,
                                bool is_get_inst)
{
        CMPIStatus s = {CMPI_RC_OK, NULL};
        CMPIInstance *inst = NULL;

        s = get_console_rs_caps(_BROKER, ref, &inst, is_get_inst);
        if ((s.rc != CMPI_RC_OK) || (inst == NULL))
                goto out;

        if (names_only)
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
        return return_rs_cap(reference, results, true, false);
}

static CMPIStatus EnumInstances(CMPIInstanceMI *self,
                                const CMPIContext *context,
                                const CMPIResult *results,
                                const CMPIObjectPath *reference,
                                const char **properties)
{

        return return_rs_cap(reference, results, false, false);
}

static CMPIStatus GetInstance(CMPIInstanceMI *self,
                              const CMPIContext *context,
                              const CMPIResult *results,
                              const CMPIObjectPath *reference,
                              const char **properties)
{
        return return_rs_cap(reference, results, false, true);
}

DEFAULT_CI();
DEFAULT_MI();
DEFAULT_DI();
DEFAULT_EQ();
DEFAULT_INST_CLEANUP();

STD_InstanceMIStub(,
                   Virt_ConsoleRedirectionServiceCapabilities, 
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
