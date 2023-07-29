/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/meta/simdev.h"

namespace vcml {
namespace meta {

void simdev::write_stop(u32 val) {
    log_info("stopping simulation upon request (%u)", val);
    request_stop();
}

void simdev::write_exit(u32 val) {
    log_info("exiting simulation upon request (%u)", val);
    ::exit(val);
}

void simdev::write_abrt(u32 val) {
    log_info("aborting simulation upon request (%u)", val);
    ::abort();
}

u64 simdev::read_sclk() {
    const sc_time& now = sc_time_stamp();
    return (u64)now.value();
}

u64 simdev::read_hclk() {
    return mwr::timestamp_us();
}

void simdev::write_sout(u32 val) {
    fputc(val, stdout);
    fflush(stdout);
}

void simdev::write_serr(u32 val) {
    fputc(val, stderr);
    fflush(stderr);
}

u32 simdev::read_prng() {
    return random();
}

simdev::simdev(const sc_module_name& nm):
    peripheral(nm),
    stop("stop", 0x00, 0),
    exit("exit", 0x08, 0),
    abrt("abrt", 0x10, 0),
    sclk("sclk", 0x18, 0),
    hclk("hclk", 0x20, 0),
    sout("sout", 0x28, 0),
    serr("serr", 0x30, 0),
    prng("prng", 0x38, 0),
    in("in") {
    stop.allow_read_write();
    stop.on_write(&simdev::write_stop);

    exit.allow_read_write();
    exit.on_write(&simdev::write_exit);

    abrt.allow_read_write();
    abrt.on_write(&simdev::write_abrt);

    sclk.allow_read_write();
    sclk.on_read(&simdev::read_sclk);

    hclk.allow_read_write();
    hclk.on_read(&simdev::read_hclk);

    sout.allow_read_write();
    sout.on_write(&simdev::write_sout);

    serr.allow_read_write();
    serr.on_write(&simdev::write_serr);

    prng.allow_read_write();
    prng.on_read(&simdev::read_prng);
}

simdev::~simdev() {
    // nothing to do
}

void simdev::reset() {
    // nothing to do
}

VCML_EXPORT_MODEL(vcml::meta::simdev, name, args) {
    return new simdev(name);
}

} // namespace meta
} // namespace vcml
