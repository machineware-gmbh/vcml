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

string broker::expand(const string& s) {
    string str = s;
    size_t pos = 0;
    while ((pos = str.find("${", pos)) != str.npos) {
        size_t end = str.find('}', pos + 2);
        VCML_REPORT_ON(end == str.npos, "missing '}' near '%s'", str.c_str());

        string val, key = str.substr(pos + 2, end - pos - 2);
        if (!init(key, val))
            VCML_REPORT("%s is not defined", key.c_str());

        str = strcat(str.substr(0, pos), val, str.substr(end + 1));
    }

    return trim(str);
}

static vector<broker*> g_brokers;

broker::broker(const string& nm): m_name(nm), m_values() {
    define("app", progname(), 1);
    define("bin", dirname(progname()), 1);
    define("pwd", curr_dir(), 1);
    define("tmp", temp_dir(), 1);
    define("usr", username(), 1);
    define("pid", getpid(), 1);
    g_brokers.push_back(this);
}

broker::~broker() {
    stl_remove_erase(g_brokers, this);
}

bool broker::lookup(const string& key, string& value) {
    if (!stl_contains(m_values, key))
        return false;

    auto it = m_values.find(key);
    if (it == m_values.end())
        return false;

    value = it->second.value;
    it->second.uses++;
    return true;
}

bool broker::defines(const string& key) const {
    return stl_contains(m_values, key);
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
                unused.push_back({ value.first, brkr });
    }

    for (auto& brkr : g_brokers) {
        for (auto& value : brkr->m_values) {
            if (value.second.uses > 0) {
                stl_remove_erase_if(
                    unused, [&](const pair<string, broker*>& entry) -> bool {
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

} // namespace vcml
