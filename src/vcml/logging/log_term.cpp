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
    log_term(use_cerr, isatty(use_cerr ? STDERR_FILENO : STDIN_FILENO)) {
    // nothing to do
}

log_term::log_term(bool use_cerr, bool use_colors):
    publisher(), m_colors(use_colors), m_os(use_cerr ? std::cerr : std::cout) {
    // nothing to do
}

log_term::~log_term() {
    // nothing to do
}

void log_term::publish(const logmsg& msg) {
    VCML_ERROR_ON(!m_os.good(), "log stream broken");

    stringstream ss;
    if (m_colors)
        ss << colors[msg.level];
    ss << msg;
    if (m_colors)
        ss << mwr::termcolors::CLEAR;
    ss << std::endl;

    m_os << ss.rdbuf() << std::flush;
}

const char* log_term::colors[NUM_LOG_LEVELS] = {
    /* [LOG_ERROR] = */ mwr::termcolors::RED,
    /* [LOG_WARN]  = */ mwr::termcolors::YELLOW,
    /* [LOG_INFO]  = */ mwr::termcolors::GREEN,
    /* [LOG_DEBUG] = */ mwr::termcolors::BLUE,
};

} // namespace vcml
