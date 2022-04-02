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
#include "vcml/module.h"

namespace vcml {
namespace net {

ostream& operator<<(ostream& os, const mac_addr& addr) {
    stream_guard guard(os);
    os << std::hex << std::setw(2) << std::setfill('0') << (int)addr[0];
    for (int i = 1; i < 6; i++)
        os << ":" << std::setw(2) << std::setfill('0') << (int)addr[i];
    return os;
}

unordered_map<string, adapter*> adapter::s_adapters;

bool adapter::cmd_create_client(const vector<string>& args, ostream& os) {
    try {
        int id = create_client(args[0]);
        os << "created client " << id;
        return true;
    } catch (std::exception& ex) {
        os << "error creating backend " << args[0] << ":" << ex.what();
        return false;
    }
}

bool adapter::cmd_destroy_client(const vector<string>& args, ostream& os) {
    for (const string& arg : args) {
        int id = from_string<int>(arg);
        if (id < 0) {
            for (auto it : m_clients)
                delete it.second;
            m_clients.clear();
            return true;
        }

        if (!destroy_client(id)) {
            os << "invalid client id: " << id;
            return false;
        }
    }

    return true;
}

bool adapter::cmd_list_clients(const vector<string>& args, ostream& os) {
    if (args.empty()) {
        for (auto it : m_clients)
            os << it.first << ": " << it.second->type() << ",";
        return true;
    }

    for (const string& arg : args) {
        size_t id = from_string<size_t>(arg);

        auto it = m_clients.find(id);
        os << id << ": ";
        os << (it == m_clients.end() ? "none" : it->second->type());
        os << ",";
    }

    return true;
}

bool adapter::cmd_link_up(const vector<string>& args, ostream& os) {
    if (!m_link_up) {
        m_link_up = true;
        on_link_up();
    }

    return true;
}

bool adapter::cmd_link_down(const vector<string>& args, ostream& os) {
    if (m_link_up) {
        m_link_up = false;
        on_link_down();
    }

    return true;
}

bool adapter::cmd_link_status(const vector<string>& args, ostream& os) {
    if (m_link_up)
        os << "link up";
    else
        os << "link down";
    return true;
}

adapter::adapter():
    m_name(),
    m_next_id(),
    m_clients(),
    m_listener(),
    m_link_up(true),
    backends("backends", "") {
    module* host = hierarchy_search<module>();
    VCML_ERROR_ON(!host, "serial port declared outside module");
    m_name = host->name();

    if (stl_contains(s_adapters, m_name))
        VCML_ERROR("network adapter '%s' already exists", m_name.c_str());
    s_adapters[m_name] = this;

    vector<string> types = split(backends);
    for (const string& type : types) {
        try {
            create_client(type);
        } catch (std::exception& ex) {
            log_warn("%s", ex.what());
        }
    }

    host->register_command(
        "create_client", 1, this, &adapter::cmd_create_client,
        "creates a new net client for this "
        "adapter of given type, usage: create_client <type>");
    host->register_command(
        "destroy_client", 1, this, &adapter::cmd_destroy_client,
        "destroys the net clients of this "
        "adapter with the specified IDs, usage: destroy_client <ID>...");
    host->register_command("list_clients", 0, this, &adapter::cmd_list_clients,
                           "lists all known clients of this "
                           "network adapter");
    host->register_command("link_up", 0, this, &adapter::cmd_link_up,
                           "trigger link-up event");
    host->register_command("link_down", 0, this, &adapter::cmd_link_down,
                           "trigger link-down event");
    host->register_command("link_status", 0, this, &adapter::cmd_link_status,
                           "shows the link status");
}

adapter::~adapter() {
    for (auto it : m_clients)
        delete it.second;
    s_adapters.erase(m_name);
}

void adapter::attach(backend* cl) {
    if (stl_contains(m_listener, cl))
        VCML_ERROR("attempt to attach client twice");
    m_listener.push_back(cl);
}

void adapter::detach(backend* cl) {
    if (!stl_contains(m_listener, cl))
        VCML_ERROR("attempt to detach unknown client");
    stl_remove_erase(m_listener, cl);
}

int adapter::create_client(const string& type) {
    m_clients[m_next_id] = backend::create(m_name, type);
    return m_next_id++;
}

bool adapter::destroy_client(int id) {
    auto it = m_clients.find(id);
    if (it == m_clients.end())
        return false;

    delete it->second;
    m_clients.erase(it);
    return true;
}

bool adapter::recv_packet(vector<u8>& packet) {
    if (m_link_up) {
        for (backend* cl : m_listener)
            if (cl->recv_packet(packet))
                return true;
    }

    return false;
}

void adapter::send_packet(const vector<u8>& packet) {
    if (m_link_up) {
        for (backend* cl : m_listener)
            cl->send_packet(packet);
    }
}

adapter* adapter::find(const string& name) {
    auto it = s_adapters.find(name);
    return it != s_adapters.end() ? it->second : nullptr;
}

vector<adapter*> adapter::all() {
    vector<adapter*> all;
    all.reserve(s_adapters.size());
    for (const auto& it : s_adapters)
        all.push_back(it.second);
    return all;
}

void adapter::on_link_up() {
    // to be overloaded
}

void adapter::on_link_down() {
    // to be overloaded
}

} // namespace net
} // namespace vcml
