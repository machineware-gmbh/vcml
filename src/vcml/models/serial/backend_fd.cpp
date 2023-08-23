/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/serial/backend_fd.h"

namespace vcml {
namespace serial {

backend_fd::backend_fd(terminal* term, int fd):
    backend(term, fd == STDOUT_FDNO ? "stdout" : "stderr"), m_fd(fd) {
}

backend_fd::~backend_fd() {
    // nothing to do
}

bool backend_fd::read(u8& val) {
    return false;
}

void backend_fd::write(u8 val) {
    mwr::fd_write(m_fd, &val, sizeof(val));
}

backend* backend_fd::create(terminal* term, const string& type) {
    if (starts_with(type, "stdout"))
        return new backend_fd(term, STDOUT_FDNO);
    if (starts_with(type, "stderr"))
        return new backend_fd(term, STDERR_FDNO);

    VCML_REPORT("unknown type: %s", type.c_str());
}

} // namespace serial
} // namespace vcml
