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

    struct priority_compare {
        bool operator() (const broker* a, const broker* b) const {
            return a->priority > b->priority;
        }
    };

    static set<broker*, priority_compare> brokers;


    broker::broker(const string& nm, int prio):
        m_name(nm),
        m_values(),
        priority(prio) {
        brokers.insert(this);
    }

    broker::~broker() {
        brokers.erase(this);

        for (auto val: m_values) {
            if (val.second.uses == 0)
                log_warn("unused property '%s'", val.first.c_str());
        }
    }

    bool broker::provides(const string& name) const {
        return stl_contains(m_values, name);
    }

    bool broker::lookup(const string& name, string& value) {
        if (!stl_contains(m_values, name))
            return false;

        auto& val = m_values[name];
        value = val.value;
        val.uses++;
        return true;
    }

    template <>
    void broker::insert(const string& name, const string& value) {
        struct value val;
        val.value = value;
        val.uses = 0;
        m_values[name] = val;
    }

    template <>
    broker* broker::init(const string& name, string& value) {
        for (auto broker : brokers)
            if (broker->lookup(name, value))
                return broker;
        return nullptr;
    }

}
