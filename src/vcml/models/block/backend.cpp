/******************************************************************************
 *                                                                            *
 * Copyright 2022 MachineWare GmbH                                            *
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

#include "vcml/models/block/backend.h"
#include "vcml/models/block/backend_ram.h"
#include "vcml/models/block/backend_file.h"

namespace vcml {
namespace block {

backend::backend(const string& type, bool readonly):
    m_type(type), m_readonly(readonly) {
    // nothing to do
}

size_t backend::remaining() {
    return capacity() - pos();
}

void backend::wzero(size_t size, bool may_unmap) {
    static const u8 zero[512] = {};
    while (size > 0) {
        size_t n = min(size, sizeof(zero));
        write(zero, n);
        size -= n;
    }
}

void backend::discard(size_t size) {
    size_t cur = pos();
    seek(cur + size);
}

void backend::flush() {
    // nothing to do
}

static size_t parse_capacity(const string& desc) {
    string s = to_lower(desc);
    char* endptr = nullptr;
    size_t cap = strtoull(s.c_str(), &endptr, 0);

    if (endptr == s.c_str() + s.length())
        return cap;
    if (strcmp(endptr, "kib") == 0)
        return cap * KiB;
    if (strcmp(endptr, "mib") == 0)
        return cap * MiB;
    if (strcmp(endptr, "gib") == 0)
        return cap * GiB;

    VCML_REPORT("invalid ramdisk capacity: %s", s.c_str());
}

backend* backend::create(const string& image, bool readonly) {
    if (image.empty()) // default ramdisk if nothing else was specified
        return new backend_ram(2 * GiB, readonly);

    if (starts_with(image, "ramdisk:")) {
        size_t cap = parse_capacity(image.substr(8));
        return new backend_ram(cap, readonly);
    }

    // if no image specification is given we test if its just a path
    if (!mwr::file_exists(image))
        VCML_REPORT("cannot access '%s'", image.c_str());
    return new backend_file(image, readonly);
}

} // namespace block
} // namespace vcml
