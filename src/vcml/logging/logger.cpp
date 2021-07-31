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
    bool logger::print_sender = true;
    bool logger::print_backtrace = true;

    size_t logger::trace_name_length = 20;

    const char* logger::prefix[NUM_LOG_LEVELS] = {
            /* [LOG_ERROR] = */ "E",
            /* [LOG_WARN]  = */ "W",
            /* [LOG_INFO]  = */ "I",
            /* [LOG_DEBUG] = */ "D",
            /* [LOG_TRACE] = */ "T"
    };

    const char* logger::desc[NUM_LOG_LEVELS] = {
            /* [LOG_ERROR] = */ "error",
            /* [LOG_WARN]  = */ "warning",
            /* [LOG_INFO]  = */ "info",
            /* [LOG_DEBUG] = */ "debug",
            /* [LOG_TRACE] = */ "trace"
    };

    ostream& operator << (ostream& os, const log_level& lvl) {
        if (lvl < LOG_ERROR || lvl > LOG_TRACE)
            return os << "unknown log level";
        return os << logger::desc[lvl];
    }

    istream& operator >> (istream& is, log_level& lvl) {
        string s; is >> s;
        lvl = NUM_LOG_LEVELS;
        for (int i = LOG_ERROR; i < NUM_LOG_LEVELS; i++)
            if (s == logger::desc[i])
                lvl = (log_level)i;
        if (lvl == vcml::NUM_LOG_LEVELS)
            is.setstate(std::ios::failbit);
        return is;
    }

    ostream& operator << (ostream& os, const logmsg& msg) {
        stringstream prefix;
        logger::print_prefix(prefix, msg);

        if (!msg.lines.empty()) {
            for (size_t i = 0; i < msg.lines.size() - 1; i++)
                os << prefix.str() << " " << msg.lines[i] << std::endl;
            os << prefix.str() << " " << msg.lines.back();
        }

        return os;
    }

    void logger::register_logger() {
        for (int l = m_min; l <= m_max; l++)
            stl_add_unique(logger::loggers[l], this);
    }

    void logger::unregister_logger() {
        for (int l = m_min; l <= m_max; l++)
            stl_remove_erase(logger::loggers[l], this);
    }

    bool logger::check_filters(const logmsg& msg) const {
        if (m_filters.empty())
            return true;

        for (auto& filter : m_filters)
            if (filter(msg))
                return true;

        return false;
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

    void logger::publish(log_level level, const string& sender,
                         const string& message, const char* file, int line) {
        logmsg msg;
        msg.level = level;
        msg.time = sc_time_stamp();
        msg.cycle = sc_delta_count();
        msg.source.file = file ? file : "";
        msg.source.line = line;
        msg.sender = sender;
        msg.lines = split(message, '\n');

        sc_object* obj = find_object(sender.c_str());
        component* comp = dynamic_cast<component*>(obj);
        if (comp && is_thread())
            msg.time += comp->local_time();

        publish(msg);
    }

    void logger::publish(const logmsg& msg) {
        for (auto& logger : loggers[msg.level])
            if (logger->check_filters(msg))
                logger->write_log(msg);
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

        publish(LOG_ERROR, rep.origin(), ss.str(), rep.file(), rep.line());
    }

    void logger::trace(const string& sender, const string& trace_message,
                       const sc_time& dt) {
        logmsg msg;
        msg.level = LOG_TRACE;
        msg.time = sc_time_stamp() + dt;
        msg.cycle = sc_delta_count();
        msg.source.file = "";
        msg.source.line = -1;
        msg.sender = sender;

        stringstream ss;
        if (!msg.sender.empty()) {
            if (trace_name_length < msg.sender.length())
                trace_name_length = msg.sender.length();
            for (size_t i = msg.sender.length(); i < trace_name_length; i++)
                ss << " ";
        }

        ss << trace_message;
        msg.lines.push_back(ss.str());
        publish(msg);
    }

    void logger::trace_fw(const string& org, const tlm_generic_payload& tx,
                          const sc_time& dt) {
        if (would_log(LOG_TRACE))
            trace(org, ">> " + tlm_transaction_to_str(tx), dt);
    }

    void logger::trace_bw(const string& org, const tlm_generic_payload& tx,
                          const sc_time& dt) {
        if (would_log(LOG_TRACE))
            trace(org, "<< " + tlm_transaction_to_str(tx), dt);
    }

    void logger::print_prefix(ostream& os, const logmsg& msg) {
        os << "[" << vcml::logger::prefix[msg.level];

        if (print_time_stamp) {
            u64 seconds = time_to_ns(msg.time) / 1000000000ull;
            u64 nanosec = time_to_ns(msg.time) % 1000000000ull;
            os << " " << std::dec << seconds
               << "." << std::setw(9) << std::setfill('0') << nanosec;
        }

        if (print_delta_cycle)
            os << " <" << msg.cycle << ">";

        os << "]";

        if (print_sender && !msg.sender.empty())
            os << " " << msg.sender << ":";
    }

}

