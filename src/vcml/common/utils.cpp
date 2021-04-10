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

#include <limits.h>
#include <cxxabi.h>
#include <execinfo.h>
#include <unistd.h>

#include "vcml/common/utils.h"
#include "vcml/common/thctl.h"
#include "vcml/common/systemc.h"
#include "vcml/common/version.h"

namespace vcml {

    string dirname(const string& path) {
#ifdef _WIN32
        const char separator = '\\';
#else
        const char separator = '/';
#endif
        size_t i = path.rfind(separator, path.length());
        return (i == string::npos) ? "." : path.substr(0, i);
    }

    string filename(const string& path) {
#ifdef _WIN32
        const char separator = '\\';
#else
        const char separator = '/';
#endif
        size_t i = path.rfind(separator, path.length());
        return (i == string::npos) ? path : path.substr(i + 1);
    }

    string filename_noext(const string& path) {
        const string name = filename(path);
        size_t i = name.rfind('.', path.length());
        return (i == string::npos) ? name : name.substr(0, i);
    }

    string curr_dir() {
        char path[PATH_MAX];
        if (getcwd(path, sizeof(path)) != path)
            VCML_ERROR("cannot read current directory: %s", strerror(errno));
        return string(path);
    }

    string temp_dir() {
#ifdef _WIN32
        // ToDo: implement tempdir for windows
#else
        return "/tmp/";
#endif
    }

    string progname() {
        char path[PATH_MAX];
        ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);

        if (len == -1)
            return "unknown";

        path[len] = '\0';
        return path;
    }

    string username() {
        char uname[255];
        if (getlogin_r(uname, sizeof(uname)))
            return "unknown";
        return uname;
    }

    bool file_exists(const string& filename) {
        return access(filename.c_str(), F_OK) != -1;
    }

    double realtime() {
        struct timespec tp;
        clock_gettime(CLOCK_REALTIME, &tp);
        return tp.tv_sec + tp.tv_nsec * 1e-9;
    }

    u64 realtime_us() {
        struct timespec tp = {};
        if (clock_gettime(CLOCK_REALTIME, &tp))
            VCML_ERROR("cannot read clock: %s (%d)", strerror(errno), errno);
        return tp.tv_sec * 1000000ul + tp.tv_nsec / 1000ul;
    }

    u64 timestamp_us() {
        struct timespec tp = {};
        if (clock_gettime(CLOCK_MONOTONIC, &tp))
            VCML_ERROR("cannot read clock: %s (%d)", strerror(errno), errno);
        return tp.tv_sec * 1000000ul + tp.tv_nsec / 1000ul;
    }

    size_t fd_peek(int fd, time_t timeoutms) {
        if (fd < 0)
            return 0;

        fd_set in, out, err;
        struct timeval timeout;

        FD_ZERO(&in);
        FD_SET(fd, &in);
        FD_ZERO(&out);
        FD_ZERO(&err);

        timeout.tv_sec  = (timeoutms / 1000ull);
        timeout.tv_usec = (timeoutms % 1000ull) * 1000ull;

        struct timeval* ptimeout = ~timeoutms ? &timeout : nullptr;
        int ret = select(fd + 1, &in, &out, &err, ptimeout);
        return ret > 0 ? 1 : 0;
    }

    size_t fd_read(int fd, void* buffer, size_t buflen) {
        if (fd < 0 || buffer == nullptr || buflen == 0)
            return 0;

        u8* ptr = reinterpret_cast<u8*>(buffer);

        ssize_t len;
        size_t numread = 0;

        while (numread < buflen) {
            do {
                len = ::read(fd, ptr + numread, buflen - numread);
            } while (len < 0 && errno == EINTR);

            if (len <= 0)
                return numread;

            numread += len;
        }

        return numread;
    }

    size_t fd_write(int fd, const void* buffer, size_t buflen) {
        if (fd < 0 || buffer == nullptr || buflen == 0)
            return false;

        const u8* ptr = reinterpret_cast<const u8*>(buffer);

        ssize_t len;
        size_t written = 0;

        while (written < buflen) {
            do {
                len = ::write(fd, ptr + written, buflen - written);
            } while (len < 0 && errno == EINTR);

            if (len <= 0)
                return written;

            written += len;
        }

        return written;
    }

    string call_origin() {
        if (!thctl_is_sysc_thread()) {
            string name = get_thread_name();
            return mkstr("thread '%s'", name.c_str());
        }

        sc_core::sc_simcontext* simc = sc_core::sc_get_curr_simcontext();
        if (simc) {
            sc_process_b* proc = sc_get_current_process_b();
            if (proc) {
                sc_object* parent = proc->get_parent_object();
                return parent ? parent->name() : proc->name();
            }

            sc_module* module = simc->hierarchy_curr();
            if (module)
                return module->name();
        }

        return "";
    }

    vector<string> backtrace(unsigned int frames, unsigned int skip) {
        vector<string> sv;

        void* symbols[frames + skip];
        unsigned int size = (unsigned int)::backtrace(symbols, frames + skip);
        if (size <= skip)
            return sv;

        sv.resize(size - skip);

        size_t dmbufsz = 256;
        char* dmbuf = (char*)malloc(dmbufsz);
        char** names = ::backtrace_symbols(symbols, size);
        for (unsigned int i = skip; i < size; i++) {
            char* func = nullptr, *offset = nullptr, *end = nullptr;
            for (char* ptr = names[i]; *ptr != '\0'; ptr++) {
                if (*ptr == '(')
                    func = ptr++;
                else if (*ptr == '+')
                    offset = ptr++;
                else if (*ptr == ')') {
                    end = ptr++;
                    break;
                }
            }

            if (!func || !offset || !end) {
                sv[i-skip] = mkstr("<unknown> [%p]", symbols[i]);
                continue;
            }

            *func++ = '\0';
            *offset++ = '\0';
            *end = '\0';

            sv[i-skip] = string(func) + "+" + string(offset);

            int status = 0;
            char* res = abi::__cxa_demangle(func, dmbuf, &dmbufsz, &status);
            if (status == 0) {
                sv[i-skip] = string(dmbuf) + "+" + string(offset);
                dmbuf = res; // dmbuf might get reallocated
            }
        }

        free(names);
        free(dmbuf);

        return sv;
    }

    string get_thread_name(const thread& t) {
#ifdef __linux__
        std::thread::native_handle_type handle =
            const_cast<std::thread&>(t).native_handle();
        if (!t.joinable())
            handle = (std::thread::native_handle_type)pthread_self();

        char buffer[256] = {};
        if (pthread_getname_np(handle, buffer, sizeof(buffer)) != 0)
            return "unknown";

        return buffer;
#else
        return "unknown";
#endif
    }

    bool set_thread_name(thread& t, const string& name) {
#ifdef __linux__
        return pthread_setname_np(t.native_handle(), name.c_str()) == 0;
#else
        return false;
#endif
    }

    bool is_debug_build() {
#ifdef VCML_DEBUG
        return true;
#else
        return false;
#endif
    }

}
