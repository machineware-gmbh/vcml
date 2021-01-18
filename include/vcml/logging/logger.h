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

#ifndef VCML_LOGGER_H
#define VCML_LOGGER_H

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

    VCML_TYPEINFO(log_level);

    class logger
    {
    private:
        log_level m_min;
        log_level m_max;

        void register_logger();
        void unregister_logger();

        // disabled
        logger(const logger&);
        logger& operator = (const logger&);

        static vector<logger*> loggers[NUM_LOG_LEVELS];

        static void print_prefix(ostream& os, log_level lvl, const sc_time& t);
        static void print_trace(bool forward, const string& org,
                                const tlm_generic_payload& tx,
                                const sc_time& dt);

    public:
        inline void set_level(log_level max);
        void set_level(log_level min, log_level max);

        logger();
        logger(log_level max);
        logger(log_level min, log_level max);
        virtual ~logger();

        virtual void log_line(log_level lvl, const char* line) = 0;

        static bool would_log(log_level lvl);

        static void log(log_level lvl, const string& org, const string& msg);
        static void log(const report& rep);

        static void trace_fw(const string& org, const tlm_generic_payload& tx,
                             const sc_time& dt);
        static void trace_bw(const string& org, const tlm_generic_payload& tx,
                             const sc_time& dt);

        static bool print_time_stamp;
        static bool print_delta_cycle;
        static bool print_origin;
        static bool print_backtrace;

        static size_t trace_name_length;

        static const char* prefix[NUM_LOG_LEVELS];
        static const char* desc[NUM_LOG_LEVELS];
    };

    inline void logger::set_level(log_level max) {
        set_level(LOG_ERROR, max);
    }

    inline bool logger::would_log(log_level lvl) {
        return !loggers[lvl].empty();
    }

#define VCML_DEFINE_LOG(name, level)                             \
    inline void name(const char* format, ...) {                  \
        if (!logger::would_log(level))                           \
            return;                                              \
        va_list args;                                            \
        va_start(args, format);                                  \
        logger::log(level, call_origin(), vmkstr(format, args)); \
        va_end(args);                                            \
    }

    VCML_DEFINE_LOG(log_error, LOG_ERROR)
    VCML_DEFINE_LOG(log_warn, LOG_WARN)
    VCML_DEFINE_LOG(log_warning, LOG_WARN)
    VCML_DEFINE_LOG(log_info, LOG_INFO)
    VCML_DEFINE_LOG(log_debug, LOG_DEBUG)
#undef VCML_DEFINE_LOG

    inline void trace(const tlm_generic_payload& tx) {
        if (!logger::would_log(LOG_TRACE))
            return;
        logger::log(LOG_TRACE, call_origin(), tlm_transaction_to_str(tx));
    }

    inline void trace_errors(const tlm_generic_payload& tx) {
        if (!logger::would_log(LOG_TRACE) || tx.is_response_ok())
            return;
        logger::log(LOG_TRACE, call_origin(), tlm_transaction_to_str(tx));
    }

}

std::ostream& operator << (std::ostream& os, const vcml::log_level& lvl);
std::istream& operator >> (std::istream& is, vcml::log_level& lvl);

#endif
