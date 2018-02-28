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

#include "vcml/logging/log_term.h"

namespace vcml {

    log_term::log_term(bool use_cerr):
        logger(SEVERITY_ERROR, SEVERITY_INFO),
        m_use_colors(isatty(use_cerr ? STDERR_FILENO : STDIN_FILENO)),
        m_os(use_cerr ? std::cerr : std::cout) {
        // nothing to do
    }

    log_term::~log_term() {
        // nothing to do
    }

    void log_term::write_log(const report& msg) {
        if (m_use_colors)
            m_os << colors[msg.get_severity()];
        m_os << msg;
        if (m_use_colors)
            m_os << reset;
        m_os << std::endl;
    }

    const char* log_term::colors[SEVERITY_MAX] = {
        [SEVERITY_ERROR]   = "\x1B[31m", // red
        [SEVERITY_WARNING] = "\x1B[33m", // yellow
        [SEVERITY_INFO]    = "\x1B[32m", // green
        [SEVERITY_DEBUG]   = "\x1B[34m"  // blue
    };

    const char* log_term::reset = "\x1B[0m";

}

