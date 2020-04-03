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
#include "vcml/component.h"

namespace vcml {

    vector<logger*> logger::loggers[NUM_LOG_LEVELS];

    bool logger::print_time_stamp = true;
    bool logger::print_delta_cycle = false;
    bool logger::print_origin = true;
    bool logger::print_backtrace = true;

    const char* logger::prefix[NUM_LOG_LEVELS] = {
            [LOG_ERROR] = "E",
            [LOG_WARN]  = "W",
            [LOG_INFO]  = "I",
            [LOG_DEBUG] = "D",
            [LOG_TRACE] = "T"
    };

    const char* logger::desc[NUM_LOG_LEVELS] = {
            [LOG_ERROR] = "error",
            [LOG_WARN]  = "warning",
            [LOG_INFO]  = "info",
            [LOG_DEBUG] = "debug",
            [LOG_TRACE] = "trace"
    };

    void logger::register_logger() {
        for (int l = m_min; l <= m_max; l++)
            stl_add_unique(logger::loggers[l], this);
    }

    void logger::unregister_logger() {
        for (int l = m_min; l <= m_max; l++)
            stl_remove_erase(logger::loggers[l], this);
    }

    void logger::set_level(log_level min, log_level max) {
        unregister_logger();
        m_min = min;
        m_max = max;
        register_logger();
    }

    logger::logger():
        m_min(LOG_ERROR),
        m_max(LOG_DEBUG) {
        register_logger();
    }

    logger::logger(log_level max):
        m_min(LOG_ERROR),
        m_max(max) {
        register_logger();
    }

    logger::logger(log_level min, log_level max):
        m_min(min),
        m_max(max) {
        register_logger();
    }

    logger::~logger() {
        unregister_logger();
    }

    void logger::log(log_level lvl, const string& org, const string& msg) {
        if (loggers[lvl].empty())
            return;

        sc_object* obj = find_object(org.c_str());
        component* comp = dynamic_cast<component*>(obj);
        sc_time now = sc_time_stamp();
        if (comp && is_thread())
            now += comp->local_time();

        stringstream ss;
        ss << "[" << vcml::logger::prefix[lvl];
        if (print_time_stamp) {
            ss << " " << std::fixed << std::setprecision(9)
               << now.to_seconds() << "s";
        }

        if (print_delta_cycle)
            ss << " <" << sc_delta_count() << ">";
        ss << "]";

        if (print_origin && !org.empty())
            ss << " " << org << ":";

        vector<string> lines = split(msg, '\n');
        for (auto line : lines) {
            string full = ss.str() + " " + line;
            for (auto out: loggers[lvl])
                out->log_line(lvl, full.c_str());
        }
    }

    void logger::log(const report& rep) {
        stringstream ss;
        if (print_backtrace) {
            vector<string> bt = rep.backtrace();
            ss << "Backtrace(" << bt.size() << ")" << std::endl;
            for (unsigned int i = bt.size() - 1; i < bt.size(); i--)
                ss << "#" << i << ": " << bt[i] << std::endl;
        }

        ss << rep.message() << std::endl;
        ss << "(from " << rep.file() << ":" << rep.line() << ")";

        log(LOG_ERROR, rep.origin(), ss.str());
    }

}

std::ostream& operator << (std::ostream& os, const vcml::log_level& lvl) {
    if (lvl < vcml::LOG_ERROR || lvl > vcml::LOG_TRACE)
        return os << "unknown log level";
    return os << vcml::logger::desc[lvl];
}

std::istream& operator >> (std::istream& is, vcml::log_level& lvl) {
    std::string s; is >> s;
    lvl = vcml::NUM_LOG_LEVELS;
    for (int i = vcml::LOG_ERROR; i < vcml::NUM_LOG_LEVELS; i++)
        if (s == vcml::logger::desc[i])
            lvl = (vcml::log_level)i;
    if (lvl == vcml::NUM_LOG_LEVELS)
        is.setstate(std::ios::failbit);
    return is;
}
