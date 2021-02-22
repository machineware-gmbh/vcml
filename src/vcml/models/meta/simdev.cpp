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

namespace vcml { namespace meta {

    u32 simdev::write_STOP(u32 val) {
        log_info("stopping simulation upon request (%u)", val);
        sc_stop();
        return 0;
    }

    u32 simdev::write_EXIT(u32 val) {
        log_info("exiting simulation upon request (%u)", val);
        exit(val);
        return 0;
    }

    u32 simdev::write_ABRT(u32 val) {
        log_info("aborting simulation upon request (%u)", val);
        abort();
        return 0;
    }

    u64 simdev::read_SCLK() {
        sc_time now = sc_time_stamp();
        return (u64)now.value();
    }

    u64 simdev::read_HCLK() {
        return realtime_us();
    }

    u32 simdev::write_SOUT(u32 val) {
        fputc(val, stdout);
        fflush(stdout);
        return 0;
    }

    u32 simdev::write_SERR(u32 val) {
        fputc(val, stderr);
        fflush(stderr);
        return 0;
    }

    u32 simdev::read_PRNG() {
        return (u32)random();
    }

    simdev::simdev(const sc_module_name& nm):
        peripheral(nm),
        STOP("STOP", 0x00, 0),
        EXIT("EXIT", 0x08, 0),
        ABRT("ABRT", 0x10, 0),
        SCLK("SCLK", 0x18, 0),
        HCLK("HCLK", 0x20, 0),
        SOUT("SOUT", 0x28, 0),
        SERR("SERR", 0x30, 0),
        PRNG("PRNG", 0x38, 0),
        IN("IN") {

        STOP.allow_read_write();
        STOP.write = &simdev::write_STOP;

        EXIT.allow_read_write();
        EXIT.write = &simdev::write_EXIT;

        ABRT.allow_read_write();
        ABRT.write = &simdev::write_ABRT;

        SCLK.allow_read_write();
        SCLK.read = &simdev::read_SCLK;

        HCLK.allow_read_write();
        HCLK.read = &simdev::read_HCLK;

        SOUT.allow_read_write();
        SOUT.write = &simdev::write_SOUT;

        SERR.allow_read_write();
        SERR.write = &simdev::write_SERR;

        PRNG.allow_read_write();
        PRNG.read = &simdev::read_PRNG;
    }

    simdev::~simdev() {
        // nothing to do
    }

    void simdev::reset() {
        // nothing to do
    }

}}
