/******************************************************************************
 *                                                                            *
 * Copyright 2021 Jan Henrik Weinstock                                        *
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

#include "vcml/serial/backend_fd.h"

namespace vcml { namespace serial {

    backend_fd::backend_fd(const string& port, int fd):
        backend(port),
        m_fd(fd) {
    }

    backend_fd::~backend_fd() {
        // nothing to do
    }

    bool backend_fd::peek() {
        return false;
    }

    bool backend_fd::read(u8& val) {
        return false;
    }

    void backend_fd::write(u8 val) {
        fd_write(m_fd, &val, sizeof(val));
    }

    backend* backend_fd::create(const string& serial, const string& type) {
        if (starts_with(type, "stdout"))
            return new backend_fd(serial, STDOUT_FILENO);
        if (starts_with(type, "stderr"))
            return new backend_fd(serial, STDERR_FILENO);

        return nullptr;
    }

}}
