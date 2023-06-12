/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/properties/property_base.h"

namespace vcml {

static string gen_hierarchy_name(const char* nm, sc_object* parent) {
    stringstream ss;

    if (parent)
        ss << parent->name() << SC_HIERARCHY_CHAR;

    ss << nm;
    return ss.str();
}

property_base::property_base(const char* nm):
    property_base(hierarchy_top(), nm) {
}

property_base::property_base(sc_object* parent, const char* nm):
    sc_attr_base(nm),
    m_parent(parent),
    m_fullname(gen_hierarchy_name(nm, parent)) {
    VCML_ERROR_ON(!m_parent, "property '%s' has no parent object", nm);
    if (!m_parent->add_attribute(*this))
        VCML_ERROR("property %s already defined", fullname());
}

property_base::~property_base() {
    m_parent->remove_attribute(name());
}

} // namespace vcml
