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

#include "vcml/logging/logger.h"

namespace vcml {

    vector<logger*> logger::loggers[SEVERITY_MAX];

    void logger::register_logger() {
        for (int l = m_min; l <= m_max; l++)
            stl_add_unique(logger::loggers[l], this);
    }

    void logger::unregister_logger() {
        for (int l = m_min; l <= m_max; l++)
            stl_remove_erase(logger::loggers[l], this);
    }

    void logger::set_level(severity min, severity max) {
        unregister_logger();
        m_min = min;
        m_max = max;
        register_logger();
    }

    logger::logger(severity min, severity max):
        m_min(min),
        m_max(max) {
        register_logger();
    }

    logger::~logger() {
        unregister_logger();
    }

    void logger::log(const report& message) {
        for (auto l: loggers[message.get_severity()])
            l->write_log(message);
    }

    void log(const severity lvl, const string& message) {
        report msg(lvl, message);
        logger::log(msg);
    }

}
