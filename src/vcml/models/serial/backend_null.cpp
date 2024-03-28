/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/serial/backend_null.h"

namespace vcml {
namespace serial {

backend_null::backend_null(terminal* term): backend(term, "null") {
    // nothing to do
}

backend_null::~backend_null() {
    // nothing to do
}

bool backend_null::read(u8& val) {
    return false;
}

void backend_null::write(u8 val) {
    // nothing to do
}

backend* backend_null::create(terminal* term, const string& type) {
    if (starts_with(type, "null"))
        return new backend_null(term);

    VCML_REPORT("unknown type: %s", type.c_str());
}

} // namespace serial
} // namespace vcml
