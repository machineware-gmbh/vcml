/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_LOGGER_H
#define VCML_LOGGER_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"

#include "vcml/logging/publisher.h"

namespace vcml {

class module;

class logger
{
private:
    string m_name;
    module* m_parent;

    void vlog(log_level lvl, const char* file, int line, const char* format,
              va_list args) const;

public:
    const char* name() const { return m_name.c_str(); }
    log_level level() const;

    bool would_log(log_level lvl) const;

    logger();
    logger(sc_object* parent);
    logger(const string& name);

    logger(logger&&) = default;
    logger(const logger&) = default;

    void log(log_level lvl, const char* format, ...) const
        MWR_DECL_PRINTF(3, 4);
    void log(log_level lvl, const char* file, int line, const char* format,
             ...) const MWR_DECL_PRINTF(5, 6);

    void error(const char* format, ...) const MWR_DECL_PRINTF(2, 3);
    void warn(const char* format, ...) const MWR_DECL_PRINTF(2, 3);
    void info(const char* format, ...) const MWR_DECL_PRINTF(2, 3);
    void debug(const char* format, ...) const MWR_DECL_PRINTF(2, 3);

    void error(const char* file, int line, const char* format, ...) const
        MWR_DECL_PRINTF(4, 5);
    void warn(const char* file, int line, const char* format, ...) const
        MWR_DECL_PRINTF(4, 5);
    void info(const char* file, int line, const char* format, ...) const
        MWR_DECL_PRINTF(4, 5);
    void debug(const char* file, int line, const char* format, ...) const
        MWR_DECL_PRINTF(4, 5);

    void error(const std::exception& ex) const;
    void warn(const std::exception& ex) const;
    void info(const std::exception& ex) const;
    void debug(const std::exception& ex) const;

    void error(const report& rep) const;
    void warn(const report& rep) const;
    void info(const report& rep) const;
    void debug(const report& rep) const;
};

inline bool logger::would_log(log_level lvl) const {
    return lvl <= level() && publisher::would_publish(lvl);
}

extern logger log;

#define log_error(...) log.error(__FILE__, __LINE__, __VA_ARGS__) // NOLINT
#define log_warn(...)  log.warn(__FILE__, __LINE__, __VA_ARGS__)  // NOLINT
#define log_info(...)  log.info(__FILE__, __LINE__, __VA_ARGS__)  // NOLINT
#define log_debug(...) log.debug(__FILE__, __LINE__, __VA_ARGS__) // NOLINT

// NOLINTNEXTLINE(readability-identifier-naming)
#define log_error_once(...)                             \
    do {                                                \
        static int once = 1;                            \
        if (once) {                                     \
            log.error(__FILE__, __LINE__, __VA_ARGS__); \
            once = 0;                                   \
        }                                               \
    } while (0)

// NOLINTNEXTLINE(readability-identifier-naming)
#define log_warn_once(...)                             \
    do {                                               \
        static int once = 1;                           \
        if (once) {                                    \
            log.warn(__FILE__, __LINE__, __VA_ARGS__); \
            once = 0;                                  \
        }                                              \
    } while (0)

// NOLINTNEXTLINE(readability-identifier-naming)
#define log_info_once(...)                             \
    do {                                               \
        static int once = 1;                           \
        if (once) {                                    \
            log.info(__FILE__, __LINE__, __VA_ARGS__); \
            once = 0;                                  \
        }                                              \
    } while (0) // NOLINT

// NOLINTNEXTLINE(readability-identifier-naming)
#define log_debug_once(...)                             \
    do {                                                \
        static int once = 1;                            \
        if (once) {                                     \
            log.debug(__FILE__, __LINE__, __VA_ARGS__); \
            once = 0;                                   \
        }                                               \
    } while (0)

} // namespace vcml

#endif
