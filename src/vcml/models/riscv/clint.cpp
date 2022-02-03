/******************************************************************************
 *                                                                            *
 * Copyright 2020 Jan Henrik Weinstock                                        *
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

#include "vcml/models/riscv/clint.h"

namespace vcml { namespace riscv {

    u64 clint::get_cycles() const {
        sc_time delta = sc_time_stamp() - m_time_reset;
        return delta / clock_cycle();
    }

    u32 clint::read_MSIP(size_t hart) {
        if (!IRQ_SW.exists(hart))
            return 0;

        return IRQ_SW[hart].read() ? 1 : 0;
    }

    void clint::write_MSIP(u32 val, size_t hart) {
        if (!IRQ_SW.exists(hart))
            return;

        const u32 mask = 1u << 0;
        val &= mask;

        log_debug("%sing interrupt on hart %zu", val ? "sett" : "clear", hart);

        MSIP[hart] = val;
        IRQ_SW[hart].write(val != 0);
    }

    void clint::write_MTIMECMP(u64 val, size_t hart) {
        if (!IRQ_TIMER.exists(hart))
            return;

        MTIMECMP[hart] = val;
        update_timer();
    }

    u64 clint::read_MTIME() {
        return get_cycles();
    }

    void clint::update_timer() {
        u64 mtime = get_cycles();

        for (auto it : IRQ_TIMER) {
            auto hart = it.first;
            auto port = it.second;

            u64 mcomp = MTIMECMP.get(hart);
            port->write(mtime >= mcomp);

            if (mtime >= mcomp)
                log_debug("triggering hart %zu timer interrupt", it.first);
            else
                m_trigger.notify(clock_cycles(mcomp - mtime));
        }
    }

    clint::clint(const sc_module_name& nm):
        peripheral(nm),
        m_time_reset(),
        m_trigger("triggerev"),
        MSIP("MSIP", 0x0000, 0),
        MTIMECMP("MTIMECMP", 0x4000, 0),
        MTIME("MTIME", 0xbff8, 0),
        IRQ_SW("IRQ_SW"),
        IRQ_TIMER("IRQ_TIMER"),
        IN("IN") {

        MSIP.sync_always();
        MSIP.allow_read_write();
        MSIP.on_read(&clint::read_MSIP);
        MSIP.on_write(&clint::write_MSIP);

        MTIMECMP.sync_on_write();
        MTIMECMP.allow_read_write();
        MTIMECMP.on_write(&clint::write_MTIMECMP);

        MTIME.sync_on_read();
        MTIME.allow_read_only();
        MTIME.on_read(&clint::read_MTIME);

        SC_METHOD(update_timer);
        sensitive << m_trigger;
        dont_initialize();
    }

    clint::~clint() {
        // nothing to do
    }

    void clint::reset() {
        peripheral::reset();

        m_time_reset = sc_time_stamp();
    }

}}
