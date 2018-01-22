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

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"

namespace vcml {

    class logger
    {
    private:
        severity m_min;
        severity m_max;

        void register_logger();
        void unregister_logger();

        // disabled
        logger();
        logger(const logger&);
        logger& operator = (const logger&);

        static vector<logger*> loggers[SEVERITY_MAX];

    public:
        inline void set_level(severity lvl);
        void set_level(severity min, severity max);

        logger(severity min, severity max);
        virtual ~logger();

        virtual void write_log(const report& message) = 0;

        inline static bool would_log(severity sev) {
            return !loggers[sev].empty();
        }

        static void log(const report& message);
    };

    inline void logger::set_level(severity lvl) {
        set_level(lvl, lvl);
    }

    void log(const severity lvl, const string& msg);

#define VCML_DEFINE_LOG(name, level)            \
    inline void name(const char* format, ...) { \
        if (!logger::would_log(level))          \
            return;                             \
        va_list args;                           \
        va_start(args, format);                 \
        log(level, vmkstr(format, args));       \
        va_end(args);                           \
    }

    VCML_DEFINE_LOG(log_error, SEVERITY_ERROR);
    VCML_DEFINE_LOG(log_warning, SEVERITY_WARNING);
    VCML_DEFINE_LOG(log_info, SEVERITY_INFO);
    VCML_DEFINE_LOG(log_debug, SEVERITY_DEBUG);
#undef VCML_DEFINE_LOG

#define VCML_WARNING(...)                                          \
    ::vcml::logger::log(::vcml::report(::vcml::SEVERITY_WARNING,   \
                                       ::vcml::mkstr(__VA_ARGS__), \
                                       __FILE__, __LINE__))

#define VCML_INFO(...)                                             \
    ::vcml::logger::log(::vcml::report(::vcml::SEVERITY_INFO,      \
                                       ::vcml::mkstr(__VA_ARGS__), \
                                       __FILE__, __LINE__))

#define VCML_DEBUG(...)                                            \
    ::vcml::logger::log(::vcml::report(::vcml::SEVERITY_WAR,       \
                                       ::vcml::mkstr(__VA_ARGS__), \
                                       __FILE__, __LINE__))

#define VCML_WARNING_ONCE(...)           \
    do {                                 \
        static bool report_done = false; \
        if (!report_done) {              \
            report_done = true;          \
            VCML_WARNING(__VA_ARGS__);   \
        }                                \
    } while (0)

#define VCML_INFO_ONCE(...)              \
    do {                                 \
        static bool report_done = false; \
        if (!report_done) {              \
            report_done = true;          \
            VCML_INFO(__VA_ARGS__);      \
        }                                \
    } while (0)

#define VCML_DEBUG_ONCE(...)             \
    do {                                 \
        static bool report_done = false; \
        if (!report_done) {              \
            report_done = true;          \
            VCML_DEBUG(__VA_ARGS__);     \
        }                                \
    } while (0)

}

#endif
