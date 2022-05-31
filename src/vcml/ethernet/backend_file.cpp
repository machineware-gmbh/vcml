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

#include "vcml/ethernet/backend_file.h"

namespace vcml {
namespace ethernet {

backend_file::backend_file(gateway* gw, const string& tx):
    backend(gw), m_count(0), m_tx(tx) {
    if (!m_tx.good())
        log_warn("failed to open file '%s'", tx.c_str());
    m_type = mkstr("file:%s", tx.c_str());
}

backend_file::~backend_file() {
    // nothing to do
}

void backend_file::send_to_host(const eth_frame& frame) {
    m_tx << "[" << sc_time_stamp() << "] packet #" << ++m_count << " " << frame
         << std::endl;

    for (size_t i = 0; i < frame.size(); i++) {
        m_tx << (i % 25 ? " " : "\n\t") << std::hex << std::setw(2)
             << std::setfill('0') << (int)frame[i] << std::dec;
    }

    m_tx << std::endl << std::endl;
}

backend* backend_file::create(gateway* gw, const string& type) {
    string tx = mkstr("%s.tx", gw->name());
    vector<string> args = split(type, ':');
    if (args.size() > 1)
        tx = args[1];

    return new backend_file(gw, tx);
}

} // namespace ethernet
} // namespace vcml
