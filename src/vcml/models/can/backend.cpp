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
#include "vcml/models/can/backend_tcp.h"

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

static unordered_map<string, backend::create_fn>& all_backends() {
    static unordered_map<string, backend::create_fn> instance;
    return instance;
}

void backend::define(const string& type, create_fn fn) {
    auto& backends = all_backends();
    if (stl_contains(backends, type))
        VCML_ERROR("can backend '%s' already defined", type.c_str());
    backends[type] = std::move(fn);
}

VCML_DEFINE_CAN_BACKEND(file, backend_file::create)
VCML_DEFINE_CAN_BACKEND(tcp, backend_tcp::create)
#ifdef HAVE_SOCKETCAN
VCML_DEFINE_CAN_BACKEND(socket, backend_socket::create)
#endif

backend* backend::create(bridge* br, const string& desc) {
    size_t pos = desc.find(':');
    string type = desc.substr(0, pos);
    vector<string> args;
    if (pos != string::npos)
        args = split(desc.substr(pos + 1), ',');

    auto& backends = all_backends();
    auto it = backends.find(type);
    if (it == backends.end()) {
        stringstream ss;
        ss << "unknown network backend '" << desc << "'" << std::endl
           << "the following can backends are known:" << std::endl;
        for (const auto& avail : backends)
            ss << "  " << avail.first;
        VCML_REPORT("%s", ss.str().c_str());
    }

    try {
        return it->second(br, args);
    } catch (std::exception& ex) {
        VCML_REPORT("%s: %s", desc.c_str(), ex.what());
    } catch (...) {
        VCML_REPORT("%s: unknown error", desc.c_str());
    }
}

} // namespace can
} // namespace vcml
