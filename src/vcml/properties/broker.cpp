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

    static set<broker*, priority_compare> g_brokers;

    broker::broker(const string& nm, int prio):
        m_name(nm),
        m_values(),
        priority(prio) {
        g_brokers.insert(this);
    }

    broker::~broker() {
        g_brokers.erase(this);
    }

    bool broker::lookup(const string& key, string& value) {
        if (!stl_contains(m_values, key))
            return false;

        auto& val = m_values[key];
        value = val.value;
        val.uses++;
        return true;
    }

    bool broker::defines(const string& key) const {
        return stl_contains(m_values, key);
    }

    template <>
    void broker::define(const string& key, const string& value) {
        if (!key.empty()) {
            struct value val;
            val.value = value;
            val.uses = 0;
            m_values[key] = val;
        }
    }

    template <>
    broker* broker::init(const string& name, string& value) {
        for (auto broker : g_brokers)
            if (broker->lookup(name, value))
                return broker;
        return nullptr;
    }

    vector<pair<string, broker*>> broker::collect_unused() {
        vector<pair<string, broker*>> unused;
        for (auto& brkr : g_brokers) {
            for (auto& value : brkr->m_values)
                if (value.second.uses == 0)
                    unused.push_back({value.first, brkr});
        }

        for (auto& brkr : g_brokers) {
            for (auto& value : brkr->m_values) {
                if (value.second.uses > 0) {
                    stl_remove_erase_if(unused,
                    [&](const pair<string, broker*>& entry) -> bool {
                        return entry.first == value.first;
                    });
                }
            }
        }

        return unused;
    }

    void broker::report_unused() {
        auto unused = collect_unused();
        if (unused.empty())
            return;

        log_warn("unused properties:");
        for (auto& prop : unused)
            log_warn("  %s (%s)", prop.first.c_str(), prop.second->name());
    }
}
