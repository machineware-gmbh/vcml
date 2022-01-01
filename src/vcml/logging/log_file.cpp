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

}
