/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/logging/log_stream.h"

namespace vcml {

log_stream::log_stream(ostream& o): publisher(LOG_ERROR, LOG_DEBUG), os(o) {
    // nothing to do
}

log_stream::~log_stream() {
    // nothing to do
}

void log_stream::publish(const logmsg& msg) {
    os << msg << std::endl;
}

} // namespace vcml
