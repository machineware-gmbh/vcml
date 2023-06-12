/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/logging/log_file.h"

namespace vcml {

log_file::log_file(const string& filename):
    publisher(LOG_ERROR, LOG_DEBUG),
    m_file(filename.c_str(), std::ios_base::out) {
    // nothing to do
}

log_file::~log_file() {
    // nothing to do
}

void log_file::publish(const logmsg& msg) {
    m_file << msg << std::endl;
}

} // namespace vcml
