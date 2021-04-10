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

#include "vcml/serial/backend.h"
#include "vcml/serial/backend_file.h"
#include "vcml/serial/backend_tcp.h"
#include "vcml/serial/backend_fd.h"
#include "vcml/serial/backend_term.h"

namespace vcml { namespace serial {

    backend::backend(const string& serial):
        m_serial(serial) {
        auto port = port::find(serial);
        VCML_ERROR_ON(!port, "serial port not found: %s", serial.c_str());
        port->attach(this);
    }

    backend::~backend() {
        auto port = port::find(m_serial);
        if (port)
            port->detach(this);
    }

    typedef function<backend*(const string&, const string&)> construct;
    static const unordered_map<string, construct> g_backends = {
        { "file", backend_file::create },
        { "tcp", backend_tcp::create },
        { "stderr", backend_fd::create },
        { "stdout", backend_fd::create },
        { "term", backend_term::create },
    };

    backend* backend::create(const string& port, const string& type) {
        string kind = type.substr(0, type.find(":"));

        auto it = g_backends.find(kind);
        if (it == g_backends.end()) {
            stringstream ss;
            log_warn("unknown serial backend '%s'", type.c_str());
            log_warn("the following backend types are known:");
            for (auto avail : g_backends)
                log_warn("  %s", avail.first.c_str());
            return nullptr;
        }

        try {
            return it->second(port, type);
        } catch (std::exception& ex) {
            log_warn("%s: %s", type.c_str(), ex.what());
            return nullptr;
        } catch (...) {
            return nullptr;
        }
    }

}}
