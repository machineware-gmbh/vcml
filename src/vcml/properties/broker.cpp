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

#include "vcml/properties/broker.h"
#include "vcml/logging/logger.h"

namespace vcml {

    bool broker::lookup(const string& name, string& value) {
        if (!stl_contains(m_values, name))
            return false;

        struct value& val = m_values[name];
        value = val.value;
        val.uses++;
        return true;
    }

    broker::broker():
        m_values() {
        register_provider(this);
    }

    broker::~broker() {
        unregister_provider(this);

        for (auto val: m_values) {
            if (val.second.uses == 0)
                log_warn("unused property '%s'", val.first.c_str());
        }
    }

    void broker::add(const string& name, const string& value) {
        struct value val;
        val.value = value;
        val.uses = 0;
        m_values[name] = val;
    }

    list<broker*> broker::brokers;

    void broker::register_provider(broker* p) {
        if (!stl_contains(brokers, p))
            brokers.push_front(p);
    }

    void broker::unregister_provider(broker* p) {
        stl_remove_erase(brokers, p);
    }

    bool broker::init(const string& name, string& value) {
        bool found = false;
        for (auto provider : brokers)
            found |= provider->lookup(name, value);
        return found;
    }

}
