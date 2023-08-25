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

backend* backend::create(bridge* br, const string& type) {
    string kind = type.substr(0, type.find(':'));
    typedef function<backend*(bridge*, const string&)> construct;
    static const unordered_map<string, construct> backends = {
        { "file", backend_file::create },
#ifdef HAVE_TAP
        { "tap", backend_tap::create },
#endif
#ifdef HAVE_LIBSLIRP
        { "slirp", backend_slirp::create },
#endif
    };

    auto it = backends.find(kind);
    if (it == backends.end()) {
        stringstream ss;
        ss << "unknown network backend '" << type << "'" << std::endl
           << "the following network backends are known:" << std::endl;
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

} // namespace ethernet
} // namespace vcml
