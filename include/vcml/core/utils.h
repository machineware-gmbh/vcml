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

#include "vcml/core/types.h"
#include "vcml/core/strings.h"

namespace vcml {

template <typename V, typename T>
inline void stl_remove(V& v, const T& t) {
    v.erase(std::remove(v.begin(), v.end(), t), v.end());
}

template <typename V, class PRED>
inline void stl_remove_if(V& v, PRED p) {
    v.erase(std::remove_if(v.begin(), v.end(), p), v.end());
}

template <typename T1, typename T2, class PRED>
inline void stl_remove_if(std::map<T1, T2>& m, PRED p) {
    for (auto it = std::begin(m); it != std::end(m);)
        it = p(it) ? m.erase(it) : ++it;
}

template <typename T1, typename T2, class PRED>
inline void stl_remove_if(unordered_map<T1, T2>& m, PRED p) {
    for (auto it = std::begin(m); it != std::end(m);)
        it = p(it) ? m.erase(it) : ++it;
}

template <typename V, typename T>
inline bool stl_contains(const V& v, const T& t) {
    return std::find(v.begin(), v.end(), t) != v.end();
}

template <typename T1, typename T2>
inline bool stl_contains(const std::map<T1, T2>& m, const T1& t) {
    return m.find(t) != m.end();
}

template <typename T1, typename T2>
inline bool stl_contains(const std::unordered_map<T1, T2>& m, const T1& t) {
    return m.find(t) != m.end();
}

template <typename V, class PRED>
inline bool stl_contains_if(const V& v, PRED p) {
    return std::find_if(v.begin(), v.end(), p) != v.end();
}

template <typename T>
inline void stl_add_unique(vector<T>& v, const T& t) {
    if (!stl_contains(v, t))
        v.push_back(t);
}

string dirname(const string& path);
string filename(const string& path);
string filename_noext(const string& path);

string curr_dir();
string temp_dir();

string progname();
string username();

bool file_exists(const string& filename);

double realtime();

u64 realtime_us();
u64 timestamp_us();

size_t fd_peek(int fd, time_t timeout_ms = 0ull);
size_t fd_read(int fd, void* buffer, size_t buflen);
size_t fd_write(int fd, const void* buffer, size_t buflen);

string call_origin();
vector<string> backtrace(unsigned int frames = 63, unsigned int skip = 1);

string get_thread_name(const thread& t = std::thread());
bool set_thread_name(thread& t, const string& name);

bool is_debug_build();

class stream_guard
{
private:
    std::ostream& m_os;
    std::ios::fmtflags m_flags;
    stream_guard() = delete;

public:
    stream_guard(ostream& os): m_os(os), m_flags(os.flags()) {}
    ~stream_guard() { m_os.flags(m_flags); }
};

struct termcolors {
    static const char* const CLEAR;
    static const char* const BLACK;
    static const char* const RED;
    static const char* const GREEN;
    static const char* const YELLOW;
    static const char* const BLUE;
    static const char* const MAGENTA;
    static const char* const CYAN;
    static const char* const WHITE;
    static const char* const BRIGHT_BLACK;
    static const char* const BRIGHT_RED;
    static const char* const BRIGHT_GREEN;
    static const char* const BRIGHT_YELLOW;
    static const char* const BRIGHT_BLUE;
    static const char* const BRIGHT_MAGENTA;
    static const char* const BRIGHT_CYAN;
    static const char* const BRIGHT_WHITE;
};

} // namespace vcml

#endif
