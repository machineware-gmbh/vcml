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

namespace vcml { namespace serial {

    unordered_map<string, port*> port::s_ports;

    port::port(const string& name):
        m_name(name),
        m_hist(),
        m_backends() {
        if (stl_contains(s_ports, name))
            VCML_ERROR("serial port '%s' already exists", name.c_str());
        s_ports[name] = this;
    }

    port::~port() {
        s_ports.erase(m_name);
    }

    void port::attach(backend* b) {
        if (stl_contains(m_backends, b))
            VCML_ERROR("attempt to attach backend twice");
        m_backends.push_back(b);
    }

    void port::detach(backend* b) {
        if (!stl_contains(m_backends, b))
            VCML_ERROR("attempt to detach unknown backend");
        stl_remove_erase(m_backends, b);
    }

    bool port::serial_peek() {
        for (backend* b : m_backends)
            if (b->peek())
                return true;
        return false;
    }

    bool port::serial_in(u8& val) {
        for (backend* b : m_backends)
            if (b->read(val))
                return true;
        return false;
    }

    void port::serial_out(u8 val) {
        m_hist.insert(val);
        for (backend* b : m_backends)
            b->write(val);
    }

    port* port::find(const string& name) {
        auto it = s_ports.find(name);
        return it != s_ports.end() ? it->second : nullptr;
    }

    vector<port*> port::all() {
        vector<port*> all;
        all.reserve(s_ports.size());
        for (auto it : s_ports)
            all.push_back(it.second);
        return std::move(all);
    }

}}
