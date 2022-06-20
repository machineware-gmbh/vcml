/******************************************************************************
 *                                                                            *
 * Copyright 2022 Jan Henrik Weinstock                                        *
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

#include "vcml/models/can/bridge.h"
#include "vcml/models/can/backend.h"
#include "vcml/models/can/backend_file.h"
#include "vcml/models/can/backend_socket.h"

namespace vcml {
namespace can {

backend::backend(bridge* br): m_parent(br), m_type("unknown") {
    m_parent->attach(this);
}

backend::~backend() {
    if (m_parent != nullptr)
        m_parent->detach(this);
}

void backend::send_to_guest(can_frame frame) {
    m_parent->send_to_guest(frame);
}

backend* backend::create(bridge* br, const string& type) {
    string kind = type.substr(0, type.find(':'));
    typedef function<backend*(bridge*, const string&)> construct;
    static const unordered_map<string, construct> backends = {
        { "file", backend_file::create },
        { "socket", backend_socket::create },
    };

    auto it = backends.find(kind);
    if (it == backends.end()) {
        stringstream ss;
        ss << "unknown network backend '" << type << "'" << std::endl
           << "the following can backends are known:" << std::endl;
        for (const auto& avail : backends)
            ss << "  " << avail.first;
        VCML_REPORT("%s", ss.str().c_str());
    }

    try {
        return it->second(br, type);
    } catch (std::exception& ex) {
        VCML_REPORT("%s: %s", type.c_str(), ex.what());
    } catch (...) {
        VCML_REPORT("%s: unknown error", type.c_str());
    }
}

} // namespace can
} // namespace vcml
