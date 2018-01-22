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

#include "vcml/backends/backend_null.h"

namespace vcml {

    backend_null::backend_null(const sc_module_name& nm):
        backend(nm) {
        /* nothing to do */
    }

    backend_null::~backend_null() {
        /* nothing to do */
    }

    size_t backend_null::peek() {
        return 0;
    }

    size_t backend_null::read(void* buf, size_t len) {
        return 0;
    }

    size_t backend_null::write(const void* buf, size_t len) {
        return len;
    }

    backend* backend_null::create(const string& nm) {
        return new backend_null(nm.c_str());
    }

    VCML_REGISTER_BACKEND_TYPE(null, &backend_null::create);

}
