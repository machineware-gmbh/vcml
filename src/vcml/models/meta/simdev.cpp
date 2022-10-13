/******************************************************************************
 *                                                                            *
 * Copyright 2019 Jan Henrik Weinstock                                        *
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

#include "vcml/models/meta/simdev.h"

namespace vcml {
namespace meta {

void simdev::write_stop(u32 val) {
    log_info("stopping simulation upon request (%u)", val);
    sc_stop();
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
    return timestamp_us();
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

} // namespace meta
} // namespace vcml
