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
 * License along with this library.  If not, see
 * <http://www.gnu.org/licenses/>.
 */
#ifndef __XMLGEN_H
#define __XMLGEN_H

#include "device_parsing.h"
#include "pool_parsing.h"
#include "acl_parsing.h"

#include "cmpidt.h"

struct kv {
	const char *key;
	const char *val;
};

char *system_to_xml(struct domain *dominfo);
char *device_to_xml(struct virt_device *dev);

char *pool_to_xml(struct virt_pool *pool);

char *res_to_xml(struct virt_pool_res *res);

char *filter_to_xml(struct acl_filter *filter);

#endif
