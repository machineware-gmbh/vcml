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

#include "vcml/net/client_file.h"

namespace vcml { namespace net {

    client_file::client_file(const string& adapter, const string& tx):
        client(adapter),
        m_count(0),
        m_tx(tx) {
        if (!m_tx.good())
            log_warn("failed to open file '%s'", tx.c_str());
    }

    client_file::~client_file() {
        // nothing to do
    }

    bool client_file::recv_packet(vector<u8>& packet) {
        return false;
    }

    void client_file::send_packet(const vector<u8>& packet) {
        m_tx << "[" << sc_time_stamp() << "] packet #" << ++m_count << ", "
             << packet.size() << " bytes";

        for (size_t i = 0; i < packet.size(); i++) {
            m_tx << (i % 25 ? " " : "\n")
                 << std::hex << std::setw(2) << std::setfill('0')
                 << (int)packet[i] << std::dec;
        }

        m_tx << std::endl << std::endl;
    }

    client* client_file::create(const string& adapter, const string& type) {
        string tx = adapter + ".tx";
        vector<string> args = split(type, ':');
        if (args.size() > 1)
            tx = args[1];

        return new client_file(adapter, tx);
    }

}}
