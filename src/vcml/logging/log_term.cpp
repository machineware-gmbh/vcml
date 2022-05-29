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
    publisher(LOG_ERROR, LOG_DEBUG),
    m_use_colors(isatty(use_cerr ? STDERR_FILENO : STDIN_FILENO)),
    m_os(use_cerr ? std::cerr : std::cout) {
    // nothing to do
}

log_term::~log_term() {
    // nothing to do
}

void log_term::publish(const logmsg& msg) {
    if (m_use_colors)
        m_os << colors[msg.level];
    m_os << msg;
    if (m_use_colors)
        m_os << termcolors::CLEAR;
    m_os << std::endl;
}

const char* log_term::colors[NUM_LOG_LEVELS] = {
    /* [LOG_ERROR] = */ termcolors::RED,
    /* [LOG_WARN]  = */ termcolors::YELLOW,
    /* [LOG_INFO]  = */ termcolors::GREEN,
    /* [LOG_DEBUG] = */ termcolors::BLUE,
};

} // namespace vcml
