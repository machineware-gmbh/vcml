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
        logger(LOG_ERROR, LOG_DEBUG),
        m_use_colors(isatty(use_cerr ? STDERR_FILENO : STDIN_FILENO)),
        m_os(use_cerr ? std::cerr : std::cout) {
        // nothing to do
    }

    log_term::~log_term() {
        // nothing to do
    }

    void log_term::log_line(log_level lvl, const char* line) {
        if (m_use_colors)
            m_os << colors[lvl];
        m_os << line;
        if (m_use_colors)
            m_os << reset;
        m_os << std::endl;
    }

    const char* log_term::colors[NUM_LOG_LEVELS] = {
        [LOG_ERROR] = "\x1B[31m", // red
        [LOG_WARN]  = "\x1B[33m", // yellow
        [LOG_INFO]  = "\x1B[32m", // green
        [LOG_DEBUG] = "\x1B[36m", // blue
        [LOG_TRACE] = "\x1B[35m"  // magenta
    };

    const char* log_term::reset = "\x1B[0m";

}

