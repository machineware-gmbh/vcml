/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/logging/inscight.h"

namespace vcml {
namespace publishers {

inscight::inscight(): publisher(LOG_ERROR, LOG_DEBUG) {
#if !defined(HAVE_INSCIGHT) || !defined(INSCIGHT_LOG_MESSAGE)
    mwr::log_warn("InSCight logging not available");
#endif
}

inscight::~inscight() {
    // nothing to do
}

void inscight::publish(const mwr::logmsg& msg) {
#if defined(HAVE_INSCIGHT) && defined(INSCIGHT_LOG_MESSAGE)
    for (const string& line : msg.lines) {
        // NOLINTNEXTLINE(clang-analyzer-unix.Malloc)
        INSCIGHT_LOG_MESSAGE(msg.level, msg.sender.c_str(), line.c_str());
    }
#endif
}

} // namespace publishers
} // namespace vcml
