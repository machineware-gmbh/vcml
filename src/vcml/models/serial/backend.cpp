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
#include "vcml/models/serial/backend_null.h"
#include "vcml/models/serial/backend_file.h"
#include "vcml/models/serial/backend_tcp.h"
#include "vcml/models/serial/backend_fd.h"
#include "vcml/models/serial/backend_term.h"
#include "vcml/models/serial/backend_tui.h"

namespace vcml {
namespace serial {

backend::backend(terminal* term, const string& type):
    m_term(term), m_type(type), log(term->log) {
    m_term->attach(this);
}

backend::~backend() {
    if (m_term)
        m_term->detach(this);
}

static backend* stdin_owner = nullptr;

void backend::capture_stdin() {
    VCML_ERROR_ON(stdin_owner, "stdin already owned by %s on %s",
                  stdin_owner->type(), stdin_owner->term()->name());
    stdin_owner = this;
}

void backend::release_stdin() {
    if (stdin_owner == this)
        stdin_owner = nullptr;
}

static unordered_map<string, backend::create_fn>& all_backends() {
    static unordered_map<string, backend::create_fn> instance;
    return instance;
};

VCML_DEFINE_SERIAL_BACKEND(null, backend_null::create);
VCML_DEFINE_SERIAL_BACKEND(file, backend_file::create);
VCML_DEFINE_SERIAL_BACKEND(tcp, backend_tcp::create);
VCML_DEFINE_SERIAL_BACKEND(stderr, backend_fd::create_stderr);
VCML_DEFINE_SERIAL_BACKEND(stdout, backend_fd::create_stdout);
VCML_DEFINE_SERIAL_BACKEND(term, backend_term::create);
VCML_DEFINE_SERIAL_BACKEND(tui, backend_tui::create);

void backend::define(const string& type, create_fn fn) {
    auto& backends = all_backends();
    if (mwr::stl_contains(backends, type))
        VCML_ERROR("serial backend '%s' already registered", type.c_str());
    backends[type] = std::move(fn);
}

backend* backend::create(terminal* term, const string& desc) {
    size_t pos = desc.find(':');
    string type = desc.substr(0, pos);
    vector<string> args;
    if (pos != string::npos)
        args = split(desc.substr(pos + 1), ',');

    auto& backends = all_backends();
    auto it = backends.find(type);
    if (it == backends.end()) {
        stringstream ss;
        ss << "unknown serial backend '" << type << "'" << std::endl
           << "the following backends are known:" << std::endl;
        for (const auto& avail : backends)
            ss << "  " << avail.first;
        VCML_REPORT("%s", ss.str().c_str());
    }

    try {
        return it->second(term, args);
    } catch (std::exception& ex) {
        VCML_REPORT("%s: %s", type.c_str(), ex.what());
    } catch (...) {
        VCML_REPORT("%s: unknown error", type.c_str());
    }
}

} // namespace serial
} // namespace vcml
