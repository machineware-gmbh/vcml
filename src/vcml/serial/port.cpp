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

#include "vcml/serial/port.h"
#include "vcml/module.h"

namespace vcml {
namespace serial {

unordered_map<string, port*> port::s_ports;

bool port::cmd_create_backend(const vector<string>& args, ostream& os) {
    try {
        int id = create_backend(args[0]);
        os << "created backend " << id;
        return true;
    } catch (std::exception& ex) {
        os << "error creating backend " << args[0] << ":" << ex.what();
        return false;
    }
}

bool port::cmd_destroy_backend(const vector<string>& args, ostream& os) {
    for (const string& arg : args) {
        int id = from_string<int>(arg);
        if (id < 0) {
            for (auto it : m_backends)
                delete it.second;
            m_backends.clear();
            return true;
        }

        if (!destroy_backend(id))
            os << "invalid backend id: " << id;
    }

    return true;
}

bool port::cmd_list_backends(const vector<string>& args, ostream& os) {
    if (args.empty()) {
        for (auto it : m_backends)
            os << it.first << ": " << it.second->type() << ",";
        return true;
    }

    for (const string& arg : args) {
        size_t id = from_string<size_t>(arg);

        auto it = m_backends.find(id);
        os << id << ": ";
        os << (it == m_backends.end() ? "none" : it->second->type());
        os << ",";
    }

    return true;
}

bool port::cmd_history(const vector<string>& args, ostream& os) {
    vector<u8> history;
    fetch_history(history);
    for (u8 val : history) {
        if (val == ',')
            os << '\\'; // escape commas
        os << (char)val;
    }

    return true;
}

port::port():
    m_name(),
    m_hist(),
    m_next_id(),
    m_backends(),
    m_listeners(),
    backends("backends", "") {
    module* host = hierarchy_search<module>();
    VCML_ERROR_ON(!host, "serial port declared outside module");
    m_name = host->name();

    if (stl_contains(s_ports, m_name))
        VCML_ERROR("serial port '%s' already exists", m_name.c_str());
    s_ports[m_name] = this;

    vector<string> types = split(backends);
    for (const auto& type : types) {
        try {
            create_backend(type);
        } catch (std::exception& ex) {
            log_warn("%s", ex.what());
        }
    }

    host->register_command("create_backend", 1, this,
                           &port::cmd_create_backend,
                           "creates a new serial backend for this "
                           "port of given type, usage: create_backend <type>");
    host->register_command(
        "destroy_backend", 1, this, &port::cmd_destroy_backend,
        "destroys the serial backends of this "
        "port with the specified IDs, usage: destroy_backend <ID> [ID]..");
    host->register_command("list_backends", 0, this, &port::cmd_list_backends,
                           "lists all known backends of this port");
    host->register_command(
        "history", 0, this, &port::cmd_history,
        "show previously transmitted data from this serial port");
}

port::~port() {
    for (auto it : m_backends)
        delete it.second;

    s_ports.erase(m_name);
}

void port::attach(backend* b) {
    if (stl_contains(m_listeners, b))
        VCML_ERROR("attempt to attach backend twice");
    m_listeners.push_back(b);
}

void port::detach(backend* b) {
    if (!stl_contains(m_listeners, b))
        VCML_ERROR("attempt to detach unknown backend");
    stl_remove_erase(m_listeners, b);
}

int port::create_backend(const string& type) {
    m_backends[m_next_id] = backend::create(m_name, type);
    return m_next_id++;
}

bool port::destroy_backend(int id) {
    auto it = m_backends.find(id);
    if (it == m_backends.end())
        return false;

    delete it->second;
    m_backends.erase(it);
    return true;
}

bool port::serial_in(u8& val) {
    for (backend* b : m_listeners)
        if (b->read(val))
            return true;
    return false;
}

void port::serial_out(u8 val) {
    m_hist.insert(val);
    for (backend* b : m_listeners)
        b->write(val);
}

port* port::find(const string& name) {
    auto it = s_ports.find(name);
    return it != s_ports.end() ? it->second : nullptr;
}

vector<port*> port::all() {
    vector<port*> all;
    all.reserve(s_ports.size());
    for (const auto& it : s_ports)
        all.push_back(it.second);
    return all;
}

} // namespace serial
} // namespace vcml
