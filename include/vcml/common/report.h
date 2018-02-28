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

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"

namespace vcml {

    enum severity {
        SEVERITY_ERROR = 0,
        SEVERITY_WARNING,
        SEVERITY_INFO,
        SEVERITY_DEBUG,
        SEVERITY_MAX
    };

    class report: public std::exception
    {
    private:
        severity       m_severity;
        string         m_message;
        string         m_source;
        sc_time        m_time;
        string         m_file;
        int            m_line;
        vector<string> m_backtrace;
        string         m_desc;

        void init();

        // disabled
        report();

    public:
        severity       get_severity() const { return m_severity; }
        const char*    get_message()  const { return m_message.c_str(); }
        const char*    get_source()   const { return m_source.c_str(); }
        const sc_time& get_time()     const { return m_time; }
        const char*    get_file()     const { return m_file.c_str(); }
        int            get_line()     const { return m_line; }

        const vector<string> get_backtrace() const {
            return m_backtrace;
        }

        report(const string& msg);
        report(severity sev, const string& msg);
        report(severity sev, const string& msg, const char* file, int line);
        report(const sc_report& rep);
        virtual ~report() throw();

        void write(ostream& os) const;
        void write_severity_and_time(ostream& os) const;
        void write_backtrace(ostream& os) const;

        virtual const char* what() const throw();

        static const char* prefix[SEVERITY_MAX];
        static const char* desc[SEVERITY_MAX];
    };

#define VCML_ERROR(...)                                                      \
    throw ::vcml::report(::vcml::SEVERITY_ERROR, ::vcml::mkstr(__VA_ARGS__), \
                         __FILE__, __LINE__)

#define VCML_ERROR_ON(condition, ...)                                        \
    do {                                                                     \
        if (condition)  {                                                    \
            VCML_ERROR(__VA_ARGS__) ;                                        \
        }                                                                    \
    } while (0)

#define VCML_ERROR_ONCE(...) do {                                            \
    static bool report_done = false;                                         \
    if (!report_done) {                                                      \
        report_done = true;                                                  \
        VCML_ERROR(__VA_ARGS__);                                             \
    }                                                                        \
} while (0)


#define VCML_DOMAIN              "vcml"
#define VCML_REPORT_INFO(msg)    SC_REPORT_INFO(VCML_DOMAIN, msg)
#define VCML_REPORT_WARNING(msg) SC_REPORT_WARNING(VCML_DOMAIN, msg)
#define VCML_REPORT_ERROR(msg)   SC_REPORT_ERROR(VCML_DOMAIN, msg)

    void initialize_reporting();
}

std::ostream& operator << (std::ostream& os, const vcml::severity sev);
std::istream& operator >> (std::istream& is, vcml::severity& sev);

std::ostream& operator << (std::ostream& os, const vcml::report& rep);

#endif
