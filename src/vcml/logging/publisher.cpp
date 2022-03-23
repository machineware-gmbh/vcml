/******************************************************************************
 *                                                                            *
 * Copyright 2021 Jan Henrik Weinstock                                        *
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

#include "vcml/logging/publisher.h"
#include "vcml/component.h"

namespace vcml {

vector<publisher*> publisher::publishers[NUM_LOG_LEVELS];

bool publisher::print_time_stamp  = true;
bool publisher::print_delta_cycle = false;
bool publisher::print_sender      = true;
bool publisher::print_source      = false;
bool publisher::print_backtrace   = true;

const char* publisher::prefix[NUM_LOG_LEVELS] = {
    /* [LOG_ERROR] = */ "E",
    /* [LOG_WARN]  = */ "W",
    /* [LOG_INFO]  = */ "I",
    /* [LOG_DEBUG] = */ "D",
};

const char* publisher::desc[NUM_LOG_LEVELS] = {
    /* [LOG_ERROR] = */ "error",
    /* [LOG_WARN]  = */ "warning",
    /* [LOG_INFO]  = */ "info",
    /* [LOG_DEBUG] = */ "debug",
};

ostream& operator<<(ostream& os, const log_level& lvl) {
    if (lvl < LOG_ERROR || lvl > LOG_DEBUG)
        return os << "unknown log level";
    return os << publisher::desc[lvl];
}

istream& operator>>(istream& is, log_level& lvl) {
    string s;
    is >> s;
    lvl = NUM_LOG_LEVELS;
    for (int i = LOG_ERROR; i < NUM_LOG_LEVELS; i++)
        if (s == publisher::desc[i])
            lvl = (log_level)i;
    if (lvl == vcml::NUM_LOG_LEVELS)
        is.setstate(std::ios::failbit);
    return is;
}

ostream& operator<<(ostream& os, const logmsg& msg) {
    publisher::print_logmsg(os, msg);
    return os;
}

logmsg::logmsg(log_level lvl, const string& s):
    level(lvl),
    time(sc_time_stamp()),
    time_offset(SC_ZERO_TIME),
    cycle(sc_delta_count()),
    sender(s),
    source({ "", -1 }),
    lines() {
    if (sender.empty())
        sender = call_origin();
    sc_object* obj  = find_object(sender.c_str());
    component* comp = dynamic_cast<component*>(obj);
    if (comp && is_thread())
        time_offset = comp->local_time();
}

void publisher::register_publisher() {
    for (int l = m_min; l <= m_max; l++)
        stl_add_unique(publisher::publishers[l], this);
}

void publisher::unregister_publisher() {
    for (int l = m_min; l <= m_max; l++)
        stl_remove_erase(publisher::publishers[l], this);
}

bool publisher::check_filters(const logmsg& msg) const {
    if (m_filters.empty())
        return true;

    for (auto& filter : m_filters)
        if (filter(msg))
            return true;

    return false;
}

void publisher::set_level(log_level min, log_level max) {
    unregister_publisher();
    m_min = min;
    m_max = max;
    register_publisher();
}

publisher::publisher(): m_min(LOG_ERROR), m_max(LOG_DEBUG) {
    register_publisher();
}

publisher::publisher(log_level max): m_min(LOG_ERROR), m_max(max) {
    register_publisher();
}

publisher::publisher(log_level min, log_level max): m_min(min), m_max(max) {
    register_publisher();
}

publisher::~publisher() {
    unregister_publisher();
}

void publisher::publish(log_level level, const string& sender,
                        const string& str, const char* file, int line) {
    logmsg msg(level, sender);
    msg.lines = split(str, '\n');

    if (file != nullptr) {
        msg.source.file = file;
        msg.source.line = line;
    }

    for (auto& logger : publishers[msg.level])
        if (logger->check_filters(msg))
            logger->publish(msg);
}

void publisher::publish(log_level level, const string& sender,
                        const report& rep) {
    stringstream ss;
    if (print_backtrace) {
        vector<string> bt = rep.backtrace();
        ss << "Backtrace(" << bt.size() << ")" << std::endl;
        for (unsigned int i = bt.size() - 1; i < bt.size(); i--)
            ss << "#" << i << ": " << bt[i] << std::endl;
    }

    ss << rep.message();

    string origin = sender;
    if (origin.empty())
        origin = rep.origin();

    // always force printing of source locations of reports
    bool print   = print_source;
    print_source = true;
    publish(level, origin, ss.str(), rep.file(), rep.line());
    print_source = print;
}

void publisher::publish(log_level level, const string& sender,
                        const std::exception& ex) {
    string msg = mkstr("exception: %s", ex.what());
    publish(level, sender, msg);
}

void publisher::print_timing(ostream& os, const sc_time& time, u64 delta) {
    if (print_time_stamp) {
        u64 seconds = time_to_ns(time) / 1000000000ull;
        u64 nanosec = time_to_ns(time) % 1000000000ull;
        os << " " << std::dec << seconds << "." << std::setw(9)
           << std::setfill('0') << nanosec;
    }

    if (print_delta_cycle)
        os << " <" << delta << ">";
}

void publisher::print_prefix(ostream& os, const logmsg& msg) {
    os << "[" << publisher::prefix[msg.level];
    print_timing(os, msg.time + msg.time_offset, msg.cycle);
    os << "]";

    if (print_sender && !msg.sender.empty())
        os << " " << msg.sender << ":";
}

void publisher::print_logmsg(ostream& os, const logmsg& msg) {
    stringstream prefix;
    publisher::print_prefix(prefix, msg);

    if (!msg.lines.empty()) {
        for (size_t i = 0; i < msg.lines.size() - 1; i++)
            os << prefix.str() << " " << msg.lines[i] << std::endl;
        os << prefix.str() << " " << msg.lines.back();
    }

    if (print_source) {
        os << " (from ";
        if (msg.source.file && strlen(msg.source.file))
            os << msg.source.file;
        else
            os << "<unknown>";
        if (msg.source.line > -1)
            os << ":" << msg.source.line;
        os << ")";
    }
}

} // namespace vcml
