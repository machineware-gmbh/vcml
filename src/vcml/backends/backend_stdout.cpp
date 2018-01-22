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

#include "vcml/backends/backend_stdout.h"

namespace vcml {

    backend_stdout::backend_stdout(const sc_module_name& nm):
        backend(nm) {
        /* nothing to do */
    }

    backend_stdout::~backend_stdout() {
        /* nothing to do */
    }

    size_t backend_stdout::peek() {
        return 0;
    }

    size_t backend_stdout::read(void* buf, size_t len) {
        return 0;
    }

    size_t backend_stdout::write(const void* buf, size_t len) {
        return full_write(STDOUT_FILENO, buf, len);
    }

    backend* backend_stdout::create(const string& nm) {
        return new backend_stdout(nm.c_str());
    }

    VCML_REGISTER_BACKEND_TYPE(stdout, &backend_stdout::create);

}
