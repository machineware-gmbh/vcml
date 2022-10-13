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

#ifndef VCML_LOGGING_PUBLISHER_H
#define VCML_LOGGING_PUBLISHER_H

#include "vcml/core/types.h"
#include "vcml/core/report.h"
#include "vcml/core/systemc.h"

namespace vcml {

enum log_level {
    LOG_ERROR = 0,
    LOG_WARN,
    LOG_INFO,
    LOG_DEBUG,
    NUM_LOG_LEVELS
};

VCML_TYPEINFO(log_level);

ostream& operator<<(ostream& os, const log_level& lvl);
istream& operator>>(istream& is, log_level& lvl);

struct logmsg {
    log_level level;
    sc_time time;
    sc_time time_offset;
    u64 cycle;
    string sender;

    struct {
        const char* file;
        int line;

    } source;

    vector<string> lines;

    logmsg(log_level level, const string& sender);
};

ostream& operator<<(ostream& os, const logmsg& msg);

typedef function<bool(const logmsg& msg)> log_filter;

class publisher
{
private:
    mutable mutex m_mtx;

    log_level m_min;
    log_level m_max;

    vector<log_filter> m_filters;

    void register_publisher();
    void unregister_publisher();

    bool check_filters(const logmsg& msg) const;
    void do_publish(const logmsg& msg);

    static vector<publisher*> publishers[NUM_LOG_LEVELS];

public:
    void set_level(log_level max);
    void set_level(log_level min, log_level max);

    void filter(log_filter filter);
    void filter_time(const sc_time& t0, const sc_time& t1);
    void filter_cycle(u64 start, u64 end);
    void filter_source(const string& file, int line = -1);

    publisher();
    publisher(log_level max);
    publisher(log_level min, log_level max);
    virtual ~publisher();

    publisher(const publisher&) = delete;
    publisher& operator=(const publisher&) = delete;

    static bool would_publish(log_level lvl);

    static void publish(log_level level, const string& sender,
                        const string& message, const char* file = nullptr,
                        int line = -1);

    static void publish(log_level level, const string& sender,
                        const std::exception& ex);

    static void publish(log_level level, const string& sender,
                        const report& rep);

    virtual void publish(const logmsg& msg) = 0;

    static bool print_time_stamp;
    static bool print_delta_cycle;
    static bool print_sender;
    static bool print_source;
    static bool print_backtrace;

    static void print_timing(ostream& os, const sc_time& time, u64 delta);
    static void print_prefix(ostream& os, const logmsg& msg);
    static void print_logmsg(ostream& os, const logmsg& msg);

    static const char* prefix[NUM_LOG_LEVELS];
    static const char* desc[NUM_LOG_LEVELS];
};

inline void publisher::set_level(log_level max) {
    set_level(LOG_ERROR, max);
}

inline bool publisher::would_publish(log_level lvl) {
    VCML_ERROR_ON(lvl >= NUM_LOG_LEVELS, "illegal log level %u", lvl);
    return !publishers[lvl].empty();
}

inline void publisher::filter(log_filter filter) {
    m_filters.push_back(std::move(filter));
}

inline void publisher::filter_time(const sc_time& t0, const sc_time& t1) {
    filter([t0, t1](const logmsg& msg) -> bool {
        return msg.time >= t0 && msg.time < t1;
    });
}

inline void publisher::filter_cycle(u64 start, u64 end) {
    filter([start, end](const logmsg& msg) -> bool {
        return msg.cycle >= start && msg.cycle < end;
    });
}

inline void publisher::filter_source(const string& file, int line) {
    filter([file, line](const logmsg& msg) -> bool {
        if (!ends_with(msg.source.file, file))
            return false;
        if (line != -1 && msg.source.line != line)
            return false;
        return true;
    });
}

} // namespace vcml

#endif
