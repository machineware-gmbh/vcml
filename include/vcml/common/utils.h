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

#include "vcml/common/types.h"
#include "vcml/common/strings.h"

namespace vcml {

    string dirname(const string& filename);
    string tempdir();

    string progname();
    string username();

    double realtime();

    bool file_exists(const string& filename);

    size_t fd_peek(int fd, time_t timeout_ms = 0ull);
    size_t fd_read(int fd, void* buffer, size_t buflen);
    size_t fd_write(int fd, const void* buffer, size_t buflen);

    string call_origin();
    vector<string> backtrace(unsigned int frames = 63, unsigned int skip = 1);

    bool set_thread_name(thread& t, const string& name);

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

#endif
