/******************************************************************************
 *                                                                            *
 * Copyright 2018 Jan Henrik Weinstock                                        *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License");            *
 * you may not use this file except in compliance with the License.           *
 * You may obtain a copy of the License at                                    *
 *                                                                            *
 *     http://www.apache.org/licenses/LICENSE-2.0                             *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 *                                                                            *
 ******************************************************************************/

#include "vcml/properties/property_base.h"

namespace vcml {

    static sc_module* find_parent(sc_module* parent) {
        if (parent)
            return parent;

        return sc_get_curr_simcontext()->hierarchy_curr();
    }

    static string gen_hierarchy_name(const char* nm, sc_module* parent) {
        parent = find_parent(parent);
        if (parent == nullptr)
            return nm;

        stringstream ss;
        ss << parent->name() << SC_HIERARCHY_CHAR << nm;
        return ss.str();

    }

    property_base::property_base(const char* nm, sc_module* parent):
        sc_attr_base(gen_hierarchy_name(nm, parent)),
        m_base(nm),
        m_parent(find_parent(parent)) {
        VCML_ERROR_ON(!m_parent, "property '%s' declared outside module", nm);
        m_parent->add_attribute(*this);
    }

    property_base::~property_base() {
        m_parent->remove_attribute(name());
    }

    const char property_base::ARRAY_DELIMITER = ',';

}

