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

#include "vcml/serial/backend_file.h"

namespace vcml {
namespace serial {

backend_file::backend_file(const string& port, const string& rx,
                           const string& tx):
    backend(port), m_rx(), m_tx() {
    if (!rx.empty()) {
        m_rx.open(rx.c_str(), ifstream::binary | ifstream::in);
        if (!m_rx.good())
            log_warn("failed to open file '%s'", rx.c_str());
    }

    if (!tx.empty()) {
        auto mode = ofstream::binary | ofstream::app | ofstream::out;
        m_tx.open(tx.c_str(), mode);
        if (!m_tx.good())
            log_warn("failed to open file '%s'", tx.c_str());
    }

    m_type = mkstr("file:%s:%s", rx.c_str(), tx.c_str());
}

backend_file::~backend_file() {
    // nothing to do
}

bool backend_file::read(u8& val) {
    if (!m_rx.is_open() || !m_rx.good())
        return false;

    m_rx.read(reinterpret_cast<char*>(&val), sizeof(val));
    return m_rx.gcount() > 0u;
}

void backend_file::write(u8 val) {
    if (m_tx.is_open() && m_tx.good()) {
        m_tx.write(reinterpret_cast<const char*>(&val), sizeof(val));
        m_tx.flush();
    }
}

backend* backend_file::create(const string& port, const string& type) {
    string rx = port + ".rx";
    string tx = port + ".tx";

    vector<string> args = split(type, ':');

    if (args.size() == 2) {
        rx = args[1] + ".rx";
        tx = args[1] + ".tx";
    }

    if (args.size() >= 3) {
        rx = args[1];
        tx = args[2];
    }

    return new backend_file(port, rx, tx);
}

} // namespace serial
} // namespace vcml
