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

#ifndef VCML_UTILS_H
#define VCML_UTILS_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/strings.h"
#include "vcml/common/thctl.h"

namespace vcml {

#define VCML_KIND(name)                \
    virtual const char* kind() const { \
        return "vcml::" #name;         \
    }

    string dirname(const string& filename);
    string tempdir();

    string progname();
    string username();

    double realtime();

    bool file_exists(const string& filename);

    size_t fd_peek(int fd, time_t timeout_ms = 0ull);
    size_t fd_read(int fd, void* buffer, size_t buflen);
    size_t fd_write(int fd, const void* buffer, size_t buflen);

    string tlm_response_to_str(tlm_response_status status);
    string tlm_transaction_to_str(const tlm_generic_payload& tx);

    u64 time_to_ns(const sc_time& t);
    u64 time_to_us(const sc_time& t);
    u64 time_to_ms(const sc_time& t);

    bool is_thread(sc_process_b* proc = nullptr);
    bool is_method(sc_process_b* proc = nullptr);

    sc_process_b* current_thread();
    sc_process_b* current_method();

    sc_object*    find_object(const string& name);
    sc_attr_base* find_attribute(const string& name);

    string call_origin();

    vector<string> backtrace(unsigned int frames = 63, unsigned int skip = 1);

    bool is_debug_build();

    class stream_state_guard
    {
    private:
        std::ostream& m_os;
        std::ios::fmtflags m_flags;
        stream_state_guard() = delete;
    public:
        stream_state_guard(ostream& os): m_os(os), m_flags(os.flags()) {}
        ~stream_state_guard() { m_os.flags(m_flags); }
    };

}

#if (SYSTEMC_VERSION < 20140408)
inline sc_core::sc_time operator % (const sc_core::sc_time& t1,
                                    const sc_core::sc_time& t2 ) {
    sc_core::sc_time tmp(t1.value() % t2.value(), false);
    return tmp;
}
#endif

#if (SYSTEMC_VERSION < 20171012)
namespace sc_core {
    typedef std::type_index sc_type_index;
}
#endif

std::istream& operator >> (std::istream& is, sc_core::sc_time& t);

#endif
