/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/ethernet/bridge.h"
#include "vcml/models/ethernet/backend.h"
#include "vcml/models/ethernet/backend_file.h"

#ifdef HAVE_TAP
#include "vcml/models/ethernet/backend_tap.h"
#endif

#ifdef HAVE_LIBSLIRP
#include "vcml/models/ethernet/backend_slirp.h"
#endif

namespace vcml {
namespace ethernet {

backend::backend(bridge* br): m_parent(br), m_type("unknown"), log(br->log) {
    m_parent->attach(this);
}

backend::~backend() {
    if (m_parent != nullptr)
        m_parent->detach(this);
}

void backend::send_to_guest(eth_frame frame) {
    m_parent->send_to_guest(std::move(frame));
}

static unordered_map<string, backend::create_fn>& all_backends() {
    static unordered_map<string, backend::create_fn> instance;
    return instance;
}

void backend::define(const string& type, create_fn create) {
    auto& backends = all_backends();
    if (stl_contains(backends, type))
        VCML_ERROR("ethernet backend '%s' already registered", type.c_str());
    backends[type] = std::move(create);
}

VCML_DEFINE_ETHERNET_BACKEND(file, backend_file::create)

#ifdef HAVE_TAP
VCML_DEFINE_ETHERNET_BACKEND(tap, backend_tap::create)
#endif

#ifdef HAVE_LIBSLIRP
VCML_DEFINE_ETHERNET_BACKEND(slirp, backend_slirp::create)
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
        ss << "unknown network backend '" << type << "'" << std::endl
           << "the following network backends are known:" << std::endl;
        for (const auto& avail : backends)
            ss << "  " << avail.first;
        VCML_REPORT("%s", ss.str().c_str());
    }

    try {
        return it->second(br, args);
    } catch (std::exception& ex) {
        VCML_REPORT("%s: %s", type.c_str(), ex.what());
    } catch (...) {
        VCML_REPORT("%s: unknown error", type.c_str());
    }
}

} // namespace ethernet
} // namespace vcml
