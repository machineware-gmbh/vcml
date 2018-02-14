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

#include "vcml/backends/backend.h"

#include "vcml/backends/backend_null.h"
#include "vcml/backends/backend_file.h"
#include "vcml/backends/backend_stdout.h"
#include "vcml/backends/backend_term.h"
#include "vcml/backends/backend_tcp.h"
#include "vcml/backends/backend_tap.h"

namespace vcml {

    std::map<string, backend::backend_cfn> backend::types = {
            { "null", &backend_null::create },
            { "file", &backend_file::create },
            { "stdout", &backend_stdout::create },
            { "term", &backend_term::create },
            { "tcp", &backend_tcp::create },
            { "tap", &backend_tap::create }
    };

    backend::backend(const sc_module_name& nm):
        sc_module(nm) {
        /* nothing to do */
    }

    backend::~backend() {
        /* nothing to do */
    }

    int backend::register_backend_type(const string& type, backend_cfn fn) {
        if (stl_contains(types, type))
            VCML_ERROR("backend type '%s' already registered", type.c_str());
        types[type] = fn;
        static int count = 0;
        return count++;
    }

    backend* backend::create(const string& type, const string& name) {
        if (!stl_contains(types, type)) {
            stringstream ss;
            ss << "unknown backend type '" << type << "', "
               << "the following types are known:"  << std::endl;
            std::map<string, backend_cfn>::const_iterator it;
            for (it = types.begin(); it != types.end(); it++)
                ss << " " << it->first << std::endl;
            log_warning(ss.str().c_str());
            return NULL;
        }

        backend_cfn fn = types[type];
        backend* be = NULL;

        try {
            be = (*fn)(name);
            return be;
        } catch (std::exception& ex) {
            log_warning(ex.what());
            log_warning("error creating %s backend", type.c_str());
            return NULL;
        }
    }

    size_t backend::peek(int fd) {
        if (fd < 0)
            return 0;

        fd_set in, out, err;
        struct timeval timeout;

        FD_ZERO(&in);
        FD_SET(fd, &in);
        FD_ZERO(&out);
        FD_ZERO(&err);

        timeout.tv_sec = 0;
        timeout.tv_usec = 0;

        int ret = select(fd + 1, &in, &out, &err, &timeout);
        return ret > 0 ? 1 : 0;
    }

    size_t backend::full_read(int fd, void* buffer, size_t len) {
        char* ptr = reinterpret_cast<char*>(buffer);

        size_t numread = 0;
        while (numread < len) {
            ssize_t res = ::read(fd, ptr + numread, len - numread);
            if (res == 0)
                return numread;
            if (res < 0)
                VCML_ERROR(strerror(errno));
            numread += res;
        }

        return numread;
    }

    size_t backend::full_write(int fd, const void* buffer, size_t len) {
        const char* ptr = reinterpret_cast<const char*>(buffer);

        size_t written = 0;
        while (written < len) {
            ssize_t res = ::write(fd, ptr + written, len - written);
            if (res == 0)
                return written;
            if (res < 0)
                VCML_ERROR(strerror(errno));
            written += res;
        }

        return written;
    }

}
