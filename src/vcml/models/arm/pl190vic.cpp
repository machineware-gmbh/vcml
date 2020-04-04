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

#include "vcml/models/arm/pl190vic.h"

namespace vcml { namespace arm {

    void pl190vic::update() {
        RISR = SINT | m_ext_irq;    // update raw interrupt status
        FIQS = RISR & INTE &  INTS; // update FIQ status
        IRQS = RISR & INTE & ~INTS; // update IRQ status

        for (auto irq : IRQ_OUT)
            irq.second->write(IRQS != 0u);

        for (auto fiq : FIQ_OUT)
            fiq.second->write(FIQS != 0u);

        for (unsigned int l = 0; l < VCTRL.count(); l++) {
            if (VCTRL[l] & VCTRL_ENABLED) {
                u32 source = VCTRL[l] & VCTRL_SOURCE_M;
                u32 srcmsk = 1 << source;
                if (IRQS & srcmsk) {
                    ADDR = VADDR[l];
                    m_current_irq = source;
                    m_vect_int = true;
                }
            }
        }
    }

    void pl190vic::irq_handler(unsigned int irq) {
        if (IRQ_IN[irq].read()) {
            m_ext_irq = m_ext_irq | (1 << irq);
            log_debug("setting IRQ %d", irq);
        } else {
            m_ext_irq = m_ext_irq & ~(1 << irq);
            log_debug("cleared IRQ %d", irq);
        }

        update();
    }

    u32 pl190vic::write_INTE(u32 val) {
        INTE |= val; // set hardware interrupt
        update();
        return INTE;
    }


    u32 pl190vic::write_IECR(u32 val) {
        INTE &= ~val; // clear hardware interrupt
        update();
        return INTE;
    }

    u32 pl190vic::write_SINT(u32 val) {
        SINT |= val; // set software interrupt
        update();
        return SINT;
    }

    u32 pl190vic::write_SICR(u32 val) {
        SINT &= ~val; // clear software interrupt
        update();
        return SINT;
    }

    u32 pl190vic::write_ADDR(u32 val) {
        if (m_vect_int) { // write indicates EOI, value not important
            INTE &= ~(1 << m_current_irq);
            m_vect_int = false;
            update();
        }

        return VADDR;
    }

    u32 pl190vic::write_VCTRL(u32 val, unsigned int idx) {
        return val & VCTRL_M;
    }

    pl190vic::pl190vic(const sc_module_name& nm):
        peripheral(nm),
        m_ext_irq(0),
        m_current_irq(0xFF),
        m_vect_int(false),
        IRQS  ("IRQS",  0x000),
        FIQS  ("FIQS",  0x004),
        RISR  ("RISR",  0x008),
        INTS  ("INTS",  0x00C),
        INTE  ("INTE",  0x010),
        IECR  ("IECR",  0x014),
        SINT  ("SINT",  0x018),
        SICR  ("SICR",  0x01C),
        PROT  ("PROT",  0x020),
        ADDR  ("ADDR",  0x030),
        DEFA  ("DEFA",  0x034),
        VADDR ("VADDR", 0x100),
        VCTRL ("VCTRL", 0x200),
        PID   ("PID",   0xFE0),
        CID   ("CID",   0xFF0),
        IN("IN"),
        IRQ_IN("IRQ_IN"),
        IRQ_OUT("IRQ_OUT"),
        FIQ_OUT("FIQ_OUT") {

        IRQS.allow_read();
        FIQS.allow_read();
        RISR.allow_read();

        INTS.allow_read_write();

        INTE.allow_read_write();
        INTE.write = &pl190vic::write_INTE;

        IECR.allow_read_write();
        IECR.write = &pl190vic::write_IECR;

        SINT.allow_read_write();
        SINT.write = &pl190vic::write_SINT;

        SICR.allow_write();
        SICR.write = &pl190vic::write_SICR;

        PROT.allow_read_write(); // not implemented

        ADDR.allow_read_write();
        ADDR.write = &pl190vic::write_ADDR;

        DEFA.allow_read_write();

        VADDR.allow_read_write();

        VCTRL.allow_read_write();
        VCTRL.tagged_write = &pl190vic::write_VCTRL;

        PID.allow_read();
        CID.allow_read();

        reset();
    }

    pl190vic::~pl190vic() {
        // nothing to do
    }

    void pl190vic::reset() {
        for (unsigned int i = 0; i < PID.count(); i++)
            PID[i] = (VCML_ARM_PL190VIC_PID >> (i * 8)) & 0xFF;

        for (unsigned int i = 0; i < CID.count(); i++)
            CID[i] = (VCML_ARM_PL190VIC_CID >> (i * 8)) & 0xFF;
    }

    void pl190vic::end_of_elaboration() {
        for (auto irq : IRQ_IN) {
            if (irq.first >= VCML_ARM_PL190VIC_NIRQ) {
                VCML_ERROR("max number of connected IRQs (%d) exceeded",
                           VCML_ARM_PL190VIC_NIRQ);
            }

            stringstream ss;
            ss << "irq_handler_" << irq.first;

            sc_spawn_options opts;
            opts.spawn_method();
            opts.set_sensitivity(irq.second);
            opts.dont_initialize();

            sc_spawn(sc_bind(&pl190vic::irq_handler, this, irq.first),
                     sc_gen_unique_name(ss.str().c_str()), &opts);
        }
    }

}}
