/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
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
