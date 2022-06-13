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

#include "vcml/models/serial/terminal.h"

#include "vcml/models/serial/backend.h"
#include "vcml/models/serial/backend_file.h"
#include "vcml/models/serial/backend_tcp.h"
#include "vcml/models/serial/backend_fd.h"
#include "vcml/models/serial/backend_term.h"

namespace vcml {
namespace serial {

backend::backend(terminal* term, const string& type):
    m_term(term), m_type(type) {
    VCML_ERROR_ON(!term, "backend created without terminal");
    m_term->attach(this);
}

backend::~backend() {
    if (m_term)
        m_term->detach(this);
}

backend* backend::create(terminal* term, const string& type) {
    string kind = type.substr(0, type.find(':'));
    typedef function<backend*(terminal*, const string&)> construct;
    static const unordered_map<string, construct> backends = {
        { "file", backend_file::create }, { "tcp", backend_tcp::create },
        { "stderr", backend_fd::create }, { "stdout", backend_fd::create },
        { "term", backend_term::create },
    };

    auto it = backends.find(kind);
    if (it == backends.end()) {
        stringstream ss;
        ss << "unknown serial backend '" << type << "'" << std::endl
           << "the following backends are known:" << std::endl;
        for (const auto& avail : backends)
            ss << "  " << avail.first;
        VCML_REPORT("%s", ss.str().c_str());
    }

    try {
        return it->second(term, type);
    } catch (std::exception& ex) {
        VCML_REPORT("%s: %s", type.c_str(), ex.what());
    } catch (...) {
        VCML_REPORT("%s: unknown error", type.c_str());
    }
}

} // namespace serial
} // namespace vcml
