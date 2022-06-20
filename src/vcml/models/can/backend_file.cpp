/******************************************************************************
 *                                                                            *
 * Copyright 2022 Jan Henrik Weinstock                                        *
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

#include "vcml/models/can/backend_file.h"

namespace vcml {
namespace can {

backend_file::backend_file(bridge* br, const string& tx):
    backend(br), m_count(0), m_tx(tx) {
    if (!m_tx.good())
        log_warn("failed to open file '%s'", tx.c_str());
    m_type = mkstr("file:%s", tx.c_str());
}

backend_file::~backend_file() {
    // nothing to do
}

void backend_file::send_to_host(const can_frame& frame) {
    m_tx << "[" << sc_time_stamp() << "] frame #" << ++m_count << " " << frame
         << std::endl;
}

backend* backend_file::create(bridge* br, const string& type) {
    string tx = mkstr("%s.tx", br->name());
    vector<string> args = split(type, ':');
    if (args.size() > 1)
        tx = args[1];

    return new backend_file(br, tx);
}

} // namespace can
} // namespace vcml
