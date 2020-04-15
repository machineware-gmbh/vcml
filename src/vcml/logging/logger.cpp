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

    size_t logger::trace_name_length = 20;

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

    void logger::print_prefix(ostream& os, log_level lvl, const sc_time& t) {
        os << "[" << vcml::logger::prefix[lvl];

        if (print_time_stamp) {
            u64 seconds = time_to_ns(t) / 1000000000ull;
            u64 nanosec = time_to_ns(t) % 1000000000ull;
            os << " " << std::dec << seconds
               << "." << std::setw(9) << std::setfill('0') << nanosec;
        }

        if (print_delta_cycle)
            os << " <" << sc_delta_count() << ">";

        os << "]";
    }

    void logger::print_trace(bool forward, const string& org,
                             const tlm_generic_payload& tx,
                             const sc_time& dt) {
        if (!would_log(LOG_TRACE))
            return;

        stringstream ss;
        print_prefix(ss, LOG_TRACE, sc_time_stamp() + dt);

        if (!org.empty()) {
            if (trace_name_length < org.length())
                trace_name_length = org.length();

            ss << " " << org << ":";
            for (size_t i = org.length(); i < trace_name_length; i++)
                ss << " ";
        }

        ss << (forward ? ">> " : "<< ");
        ss << tlm_transaction_to_str(tx);

        for (auto out: loggers[LOG_TRACE])
            out->log_line(LOG_TRACE, ss.str().c_str());
    }

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
        if (!would_log(lvl))
            return;

        sc_object* obj = find_object(org.c_str());
        component* comp = dynamic_cast<component*>(obj);
        sc_time now = sc_time_stamp();
        if (comp && is_thread())
            now += comp->local_time();

        stringstream ss;
        print_prefix(ss, lvl, now);

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

    void logger::trace_fw(const string& org, const tlm_generic_payload& tx,
                          const sc_time& dt) {
        print_trace(true, org, tx, dt);
    }

    void logger::trace_bw(const string& org, const tlm_generic_payload& tx,
                          const sc_time& dt) {
        print_trace(false, org, tx, dt);
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
