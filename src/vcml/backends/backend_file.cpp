/******************************************************************************
 *                                                                            *
 * Copyright 2018 Jan Henrik Weinstock                                        *
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

#include "vcml/backends/backend_file.h"

namespace vcml {

    backend_file::backend_file(const sc_module_name& nm, const char* rx_file,
                               const char* tx_file):
        backend(nm),
        m_rx(),
        m_tx(),
        rx("rx", rx_file ? rx_file : string(name()) + ".rx"),
        tx("tx", tx_file ? tx_file : string(name()) + ".tx") {
        if (!rx.get().empty()) {
            m_rx.open(rx.get().c_str(), ifstream::binary | ifstream::in);
            if (!m_rx.good())
                log_warn("failed to open file '%s'", rx.get().c_str());
        }

        if (!tx.get().empty()) {
            m_tx.open(tx.get().c_str(),
                      ofstream::binary | ofstream::app | ofstream::out);
            if (!m_tx.good())
                log_warn("failed to open file '%s'", tx.get().c_str());
        }
    }

    backend_file::~backend_file() {
        /* nothing to do */
    }

    size_t backend_file::peek() {
        if (!m_rx.is_open() || !m_rx.good())
            return 0;

        size_t pos = m_rx.tellg();
        m_rx.seekg(0, m_rx.end);
        size_t end = m_rx.tellg();
        m_rx.seekg(pos, m_rx.beg);

        return end - pos;
    }

    size_t backend_file::read(void* buf, size_t len) {
        if (!m_rx.is_open() || !m_rx.good())
            return 0;
        m_rx.read(reinterpret_cast<char*>(buf), len);
        return m_rx.gcount();
    }

    size_t backend_file::write(const void* buf, size_t len) {
        if (!m_tx.is_open() || !m_tx.good())
            return 0;
        m_tx.write(reinterpret_cast<const char*>(buf), len);
        m_tx.flush();
        return len;
    }

    backend* backend_file::create(const string& nm) {
        return new backend_file(nm.c_str());
    }

}
