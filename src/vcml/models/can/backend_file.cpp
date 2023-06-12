/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
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
