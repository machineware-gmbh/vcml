/******************************************************************************
 *                                                                            *
 * Copyright 2021 Jan Henrik Weinstock                                        *
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

#include "vcml/net/adapter.h"
#include "vcml/net/client.h"

namespace vcml { namespace net {

    unordered_map<string, adapter*> adapter::s_adapters;

    adapter::adapter(const string& name):
        m_name(name) {
        if (stl_contains(s_adapters, name))
            VCML_ERROR("network adapter '%s' already exists", name.c_str());
        s_adapters[name] = this;
    }

    adapter::~adapter() {
        s_adapters.erase(m_name);
    }

    void adapter::attach(client* cl) {
        if (stl_contains(m_clients, cl))
            VCML_ERROR("attempt to attach client twice");
        m_clients.push_back(cl);
    }

    void adapter::detach(client* cl) {
        if (!stl_contains(m_clients, cl))
            VCML_ERROR("attempt to detach unknown client");
        stl_remove_erase(m_clients, cl);
    }

    bool adapter::recv_packet(vector<u8>& packet) {
        for (client* cl : m_clients)
            if (cl->recv_packet(packet))
                return true;
        return false;
    }

    void adapter::send_packet(const vector<u8>& packet) {
        for (client* cl : m_clients)
            cl->send_packet(packet);
    }

    adapter* adapter::find(const string& name) {
        auto it = s_adapters.find(name);
        return it != s_adapters.end() ? it->second : nullptr;
    }

    vector<adapter*> adapter::all() {
        vector<adapter*> all;
        all.reserve(s_adapters.size());
        for (auto it : s_adapters)
            all.push_back(it.second);
        return std::move(all);
    }

}}
