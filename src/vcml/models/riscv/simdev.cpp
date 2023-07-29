/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/riscv/simdev.h"

namespace vcml {
namespace riscv {

void simdev::write_finish(u32 val) {
    finish = val;

    u32 status = val & 0xffff;
    u32 code = val >> 16;

    switch (status) {
    case FINISH_FAIL:
        log_info("simulation abort requested by cpu %d", current_cpu());
        exit(code);
        break;

    case FINISH_PASS:
        log_info("simulation exit requested by cpu %d", current_cpu());
        request_stop();
        break;

    case FINISH_RESET:
        log_info("simulation reset requested by cpu %d", current_cpu());
        request_stop();
        break;

    default:
        log_warn("illegal exit request 0x%08x", val);
        break;
    }
}

simdev::simdev(const sc_module_name& nm):
    peripheral(nm), finish("finish", 0x0, 0), in("in") {
    finish.sync_always();
    finish.allow_read_write();
    finish.on_write(&simdev::write_finish);
}

simdev::~simdev() {
    // nothing to do
}

VCML_EXPORT_MODEL(vcml::riscv::simdev, name, args) {
    return new simdev(name);
}

} // namespace riscv
} // namespace vcml
