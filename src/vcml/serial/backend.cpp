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

    backend::backend(const string& port):
        m_port(port),
        m_type("unknown") {
        auto serial = port::find(port);
        VCML_ERROR_ON(!serial, "serial port not found: %s", port.c_str());
        serial->attach(this);
    }

    backend::~backend() {
        auto port = port::find(m_port);
        if (port)
            port->detach(this);
    }

    backend* backend::create(const string& port, const string& type) {
        string kind = type.substr(0, type.find(":"));
        typedef function<backend*(const string&, const string&)> construct;
        static const unordered_map<string, construct> backends = {
            { "file", backend_file::create },
            { "tcp", backend_tcp::create },
            { "stderr", backend_fd::create },
            { "stdout", backend_fd::create },
            { "term", backend_term::create },
        };

        auto it = backends.find(kind);
        if (it == backends.end()) {
            stringstream ss;
            ss << "unknown serial backend '" << type << "'" << std::endl
               << "the following backend types are known:";
            for (auto avail : backends)
                ss << " " << avail.first;
            ss << std::endl;
            VCML_REPORT("%s", ss.str().c_str());
        }

        try {
            return it->second(port, type);
        } catch (std::exception& ex) {
            VCML_REPORT("%s: %s", type.c_str(), ex.what());
        } catch (...) {
            VCML_REPORT("%s: unknown error", type.c_str());
        }
    }

}}
