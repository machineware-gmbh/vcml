/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/can/bridge.h"
#include "vcml/models/can/backend.h"
#include "vcml/models/can/backend_file.h"

#ifdef HAVE_SOCKETCAN
#include "vcml/models/can/backend_socket.h"
#endif

namespace vcml {
namespace can {

backend::backend(bridge* br): m_parent(br), m_type("unknown"), log(br->log) {
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
#ifdef HAVE_SOCKETCAN
        { "socket", backend_socket::create },
#endif
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
