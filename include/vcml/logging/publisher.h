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

#include "vcml/common/types.h"
#include "vcml/common/strings.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"

namespace vcml {

    enum log_level {
        LOG_ERROR = 0,
        LOG_WARN,
        LOG_INFO,
        LOG_DEBUG,
        LOG_TRACE,
        NUM_LOG_LEVELS
    };

    enum trace_direction : int {
        TRACE_FW = 1,
        TRACE_FW_NOINDENT = 2,
        TRACE_BW = -1,
        TRACE_BW_NOINDENT = -2,
    };

    VCML_TYPEINFO(log_level);

    ostream& operator << (ostream& os, const log_level& lvl);
    istream& operator >> (istream& is, log_level& lvl);

    struct logmsg {
        log_level level;
        sc_time   time;
        sc_time   time_offset;
        u64       cycle;
        string    sender;

        struct {
            const char* file;
            int         line;

        } source;

        vector<string> lines;

        logmsg(log_level _level, const string& _sender);
    };

    template <typename PAYLOAD>
    struct trace_msg : public logmsg {
        trace_direction direction;
        const PAYLOAD& payload;

        trace_msg(const string& _sender, trace_direction _direction,
                  const PAYLOAD& _payload):
            logmsg(LOG_TRACE, _sender),
            direction(_direction),
            payload(_payload) {
        }
    };

    ostream& operator << (ostream& os, const logmsg& msg);

    typedef function<bool(const logmsg& msg)> log_filter;

    class publisher
    {
    private:
        log_level m_min;
        log_level m_max;

        vector<log_filter> m_filters;

        void register_publisher();
        void unregister_publisher();

        bool check_filters(const logmsg& msg) const;

        // disabled
        publisher(const publisher&);
        publisher& operator = (const publisher&);

        static size_t trace_curr_indent;
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

        static bool would_publish(log_level lvl);

        static void publish(log_level level,
                            const string& sender,
                            const string& message,
                            const char* file = nullptr,
                            int line = -1);

        static void publish(log_level level,
                            const string& sender,
                            const std::exception& ex);

        static void publish(log_level level,
                            const string& sender,
                            const report& rep);

        template <typename PAYLOAD>
        void publish(const trace_msg<PAYLOAD>& msg);

        virtual void publish(const logmsg& msg) = 0;

        template <typename SENDER, typename PAYLOAD>
        static void trace(trace_direction dir, const SENDER& sender,
                          const PAYLOAD& tx, const sc_time& dt = SC_ZERO_TIME);

        template <typename SENDER, typename PAYLOAD>
        static void trace_fw(const SENDER& sender, const PAYLOAD& tx,
                             const sc_time& dt = SC_ZERO_TIME);

        template <typename SENDER, typename PAYLOAD>
        static void trace_bw(const SENDER& sender, const PAYLOAD& tx,
                             const sc_time& dt = SC_ZERO_TIME);

        static bool print_time_stamp;
        static bool print_delta_cycle;
        static bool print_sender;
        static bool print_source;
        static bool print_backtrace;

        static size_t trace_name_length;
        static size_t trace_indent_incr;

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

    template <typename PAYLOAD>
    inline void publisher::publish(const trace_msg<PAYLOAD>& msg) {
        publish((logmsg)msg);
    }

    inline void publisher::filter(log_filter filter) {
        m_filters.push_back(filter);
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

    template <typename SENDER, typename PAYLOAD>
    inline void publisher::trace(trace_direction dir, const SENDER& sender,
        const PAYLOAD& tx, const sc_time& dt) {
        if (!would_publish(LOG_TRACE))
            return;

        trace_msg<PAYLOAD> msg(sender.name(), dir, tx);
        msg.time_offset = dt;

        stringstream ss;
        if (dir == TRACE_FW)
            trace_curr_indent += trace_indent_incr;
        if (dir >= TRACE_FW)
            ss << string(trace_curr_indent, ' ') << ">> ";
        if (dir <= TRACE_BW)
            ss << string(trace_curr_indent, ' ') << "<< ";
        if (dir == TRACE_BW) {
            if (trace_curr_indent >= trace_indent_incr)
                trace_curr_indent -= trace_indent_incr;
            else
                trace_curr_indent = 0;
        }

        vector<string> lines = split(to_string(tx), '\n');
        for (auto line : lines)
            msg.lines.push_back(ss.str() + line);

        for (auto pub : publishers[LOG_TRACE])
            pub->publish(msg);
    }

    template <typename SENDER, typename PAYLOAD>
    inline void publisher::trace_fw(const SENDER& sender, const PAYLOAD& tx,
        const sc_time& dt) {
        trace(TRACE_FW, sender, tx, dt);
    }

    template <typename SENDER, typename PAYLOAD>
    inline void publisher::trace_bw(const SENDER& sender, const PAYLOAD& tx,
        const sc_time& dt) {
        trace(TRACE_BW, sender, tx, dt);
    }

}

#endif
