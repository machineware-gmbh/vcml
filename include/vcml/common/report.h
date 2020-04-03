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

#ifndef VCML_REPORT_H
#define VCML_REPORT_H

#include <exception>

#include "vcml/common/types.h"
#include "vcml/common/strings.h"

namespace vcml {

    class report: public std::exception
    {
    private:
        string         m_message;
        string         m_origin;
        double         m_time;
        string         m_file;
        int            m_line;
        vector<string> m_backtrace;
        string         m_desc;

    public:
        const char*  message()  const { return m_message.c_str(); }
        const char*  origin()   const { return m_origin.c_str(); }
        const double time()     const { return m_time; }
        const char*  file()     const { return m_file.c_str(); }
        int          line()     const { return m_line; }

        const vector<string> backtrace() const;

        report() = delete;
        report(const string& msg, const char* file, int line);
        virtual ~report() throw();

        virtual const char* what() const throw();

        static void report_segfaults();
        static unsigned int max_backtrace_length;
    };

    inline const vector<string> report::backtrace() const {
        return m_backtrace;
    }

#define VCML_REPORT(...)                                                      \
    throw ::vcml::report(::vcml::mkstr(__VA_ARGS__), __FILE__, __LINE__)

#define VCML_REPORT_ON(condition, ...)                                        \
    do {                                                                      \
        if (condition)  {                                                     \
            VCML_REPORT(__VA_ARGS__) ;                                        \
        }                                                                     \
    } while (0)

#define VCML_REPORT_ONCE(...)                                                 \
    do {                                                                      \
        static bool report_done = false;                                      \
        if (!report_done) {                                                   \
            report_done = true;                                               \
            VCML_REPORT(__VA_ARGS__);                                         \
        }                                                                     \
    } while (0)

#define VCML_ERROR(...)                                                       \
    do {                                                                      \
        fprintf(stderr, "%s:%d ", __FILE__, __LINE__);                        \
        fprintf(stderr, __VA_ARGS__);                                         \
        fprintf(stderr, "\n");                                                \
        abort();                                                              \
    } while (0)

#define VCML_ERROR_ON(condition, ...)                                         \
    do {                                                                      \
        if (condition) {                                                      \
            VCML_ERROR(__VA_ARGS__);                                          \
        }                                                                     \
    } while (0)

}

std::ostream& operator << (std::ostream& os, const vcml::report& rep);

#endif
