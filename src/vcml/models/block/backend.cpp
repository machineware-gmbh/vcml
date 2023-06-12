/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
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
    return new backend_file(image, readonly);
}

} // namespace block
} // namespace vcml
