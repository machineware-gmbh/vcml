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

#include "vcml/models/arm/gicv2.h"

namespace vcml { namespace arm {

    static unsigned int ctz32(u32 value) {
        if (value & 0x00000001)
            return 0; // special case for odd value

        unsigned int count = 1;

        // binary search for the trailing one bit
        if (!(value & 0x0000FFFF)) {
            count += 16;
            value >>= 16;
        }

        if (!(value & 0x000000FF)) {
            count += 8;
            value >>= 8;
        }

        if (!(value & 0x0000000F)) {
            count += 4;
            value >>= 4;
        }

        if (!(value & 0x00000003)) {
            count += 2;
            value >>= 2;
        }

        return count - (value & 0x00000001);
    }

    static inline bool is_software_interrupt(unsigned int irq) {
        return irq < VCML_ARM_GICv2_NSGI;
    }

    gicv2::irq_state::irq_state():
        enabled(0),
        pending(0),
        active(0),
        level(0),
        model(N_N),
        trigger(EDGE) {
        // nothing to do
    }

    u32 gicv2::distif::int_pending_mask(int cpu) {
        u32 value = 0;
        unsigned int mask = 1 << cpu;
        for (unsigned int irq = 0; irq < VCML_ARM_GICv2_PRIV; irq++)
            if (m_parent->test_pending(irq, mask))
                    value |= (1 << irq);
        return value;
    }

    u32 gicv2::distif::spi_pending_mask(int cpu) {
        u32 value = 0;
        unsigned int offset = VCML_ARM_GICv2_PRIV + cpu * 32;
        for (unsigned int irq = 0; irq < 32; irq++)
            if (m_parent->test_pending(offset + irq, gicv2::ALL_CPU))
                value |= (1 << irq);
        return value;
    }

    u16 gicv2::distif::ppi_enabled_mask(int cpu) {
        u16 value = 0;
        unsigned int mask = 1 << cpu;
        for (unsigned int irq = 0; irq < VCML_ARM_GICv2_NPPI; irq++)
            if (m_parent->is_irq_enabled(irq + VCML_ARM_GICv2_NSGI, mask))
                value |= (1 << irq);
        return value;
    }

    u32 gicv2::distif::write_CTLR(u32 val) {
        if ((val & CTLR_ENABLE) && !(CTLR & CTLR_ENABLE))
            log_debug("(CTLR) irq forwarding enabled");
        if (!(val & CTLR_ENABLE) && (CTLR & CTLR_ENABLE))
            log_debug("(CTLR) irq forwarding disabled");
        CTLR = val & CTLR_ENABLE;
        m_parent->update();
        return CTLR;
    }

    u32 gicv2::distif::read_ISER() {
        int cpu = ISER.current_bank();
        if (cpu < 0) {
            log_warning("(ISER) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        u32 mask = ppi_enabled_mask(cpu);
        return (mask << 16) | 0xFFFF; // SGIs are always enabled
    }

    u32 gicv2::distif::write_ISER(u32 val) {
        int cpu = ISER.current_bank();
        if (cpu < 0) {
            log_warning("(ISER) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        unsigned int irq = VCML_ARM_GICv2_NSGI;
        unsigned int mask = 1 << cpu;

        for (; irq < VCML_ARM_GICv2_PRIV; irq++) {
            if (val & (1 << irq)) {
                m_parent->enable_irq(irq, mask);
                if (m_parent->get_irq_level(irq, mask) &&
                    m_parent->get_irq_trigger(irq) == gicv2::LEVEL) {
                    m_parent->set_irq_pending(irq, true, mask);
                }
            }
        }

        m_parent->update();
        return ISER;
    }

    u32 gicv2::distif::read_SSER(unsigned int idx) {
        u32 value = 0;
        unsigned int irq = VCML_ARM_GICv2_PRIV + idx * 32;
        for (unsigned int i = 0; i < 32; i++) {
            if (m_parent->is_irq_enabled(irq + i, gicv2::ALL_CPU))
                value |= (1 << i);
        }

        return value;
    }

    u32 gicv2::distif::write_SSER(u32 val, unsigned int idx) {
        unsigned int irq = VCML_ARM_GICv2_PRIV + idx * 32;
        for (unsigned int i = 0; i < 32; i++) {
            if (val & (1 << i)) {
                m_parent->enable_irq(irq + i, gicv2::ALL_CPU);
                if ((m_parent->get_irq_level(irq + i, gicv2::ALL_CPU)) &&
                    (m_parent->get_irq_trigger(irq + i) == gicv2::LEVEL)) {
                    m_parent->set_irq_pending(irq + i, true, gicv2::ALL_CPU);
                }
            }
        }

        m_parent->update();
        return SSER;
    }

    u32 gicv2::distif::read_ICER() {
        int cpu = ICER.current_bank();
        if (cpu < 0) {
            log_warning("(ICER) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        u32 mask = ppi_enabled_mask(cpu);
        return (mask << 16) | 0xFFFF; // SGIs are always enabled
    }

    u32 gicv2::distif::write_ICER(u32 val) {
        int cpu = ICER.current_bank();
        if (cpu < 0) {
            log_warning("(ICER) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        unsigned int irq = VCML_ARM_GICv2_NSGI;
        unsigned int mask = 1 << cpu;

        for (; irq < VCML_ARM_GICv2_PRIV; irq++) {
            if (val & (1 << irq))
                m_parent->disable_irq(irq, mask);
        }

        m_parent->update();
        return ICER;
    }

    u32 gicv2::distif::read_SCER(unsigned int idx) {
        u32 value = 0;
        unsigned int irq = VCML_ARM_GICv2_PRIV + idx * 32;
        for (unsigned int i = 0; i < 32; i++) {
            if (m_parent->is_irq_enabled(irq + i, gicv2::ALL_CPU))
                value |= (1 << i);
        }

        return value;
    }

    u32 gicv2::distif::write_SCER(u32 val, unsigned int idx) {
        unsigned int irq = VCML_ARM_GICv2_PRIV + idx * 32;
        for (unsigned int i = 0; i < 32; i++) {
            if (val & (1 << i))
                m_parent->disable_irq(irq + i, gicv2::ALL_CPU);
        }

        m_parent->update();
        return SCER;
    }

    u32 gicv2::distif::read_ISPR() {
        int cpu = ISPR.current_bank();
        if (cpu < 0) {
            log_warning("(ISPR) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        return int_pending_mask(cpu);
    }

    u32 gicv2::distif::write_ISPR(u32 value) {
        int cpu = ISPR.current_bank();
        if (cpu < 0) {
            log_warning("(ISPR) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        unsigned int irq = VCML_ARM_GICv2_NSGI;
        unsigned int mask = 1 << cpu;

        for (; irq < VCML_ARM_GICv2_PRIV; irq++) {
            if (value & (1 << irq))
                m_parent->set_irq_pending(irq, true, mask);
        }

        m_parent->update();
        return ISPR;
    }

    u32 gicv2::distif::read_SSPR(unsigned int idx) {
        return spi_pending_mask(idx);
    }

    u32 gicv2::distif::write_SSPR(u32 value, unsigned int idx) {
        unsigned int irq = VCML_ARM_GICv2_PRIV + idx * 32;
        for (unsigned int i = 0; i < 32; i++) {
            if (value & (1 << i))
                m_parent->set_irq_pending(irq + i, true, SPIT[i]);
        }

        m_parent->update();
        return SSPR;
    }

    u32 gicv2::distif::read_ICPR() {
        int cpu = ICPR.current_bank();
        if (cpu < 0) {
            log_warning("(ICPR) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        return int_pending_mask(cpu);
    }

    u32 gicv2::distif::write_ICPR(u32 value) {
        int cpu = ICPR.current_bank();
        if (cpu < 0) {
            log_warning("(ICPR) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        unsigned int irq = VCML_ARM_GICv2_NSGI;
        unsigned int mask = 1 << cpu;

        for (; irq < VCML_ARM_GICv2_PRIV; irq++) {
            if (value & (1 << irq))
                m_parent->set_irq_pending(irq, false, mask);
        }

        m_parent->update();
        return SCPR;
    }

    u32 gicv2::distif::read_SCPR(unsigned int idx) {
        return spi_pending_mask(idx);
    }

    u32 gicv2::distif::write_SCPR(u32 val, unsigned int idx) {
        unsigned int irq = VCML_ARM_GICv2_PRIV + idx * 32;
        for (unsigned int i = 0; i < 32; i++) {
            if (val & (1 << i))
                m_parent->set_irq_pending(irq + i, false, gicv2::ALL_CPU);
        }

        m_parent->update();
        return SCPR;
    }

    u32 gicv2::distif::read_IACR() {
        int cpu = ICPR.current_bank();
        if (cpu < 0) {
            log_warning("(IACR) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        u32 value = 0;
        unsigned int mask = 1 << cpu;
        for (unsigned int l = 0; l < VCML_ARM_GICv2_PRIV; l++) {
            if (m_parent->is_irq_active(l, mask))
                value |= (1 << l);
        }

        return value;
    }

    u32 gicv2::distif::read_SACR(unsigned int idx) {
        u32 value = 0;
        unsigned int irq = VCML_ARM_GICv2_PRIV + idx * 32;
        for (unsigned int i = 0; i < 32; i++) {
            if (m_parent->is_irq_active(irq + i, gicv2::ALL_CPU))
                value |= (1 << i);
        }

        return value;
    }

    u32 gicv2::distif::read_INTT(unsigned int idx) {
        int cpu = INTT.current_bank();
        if (cpu < 0) {
            log_warning("(INTT) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        // local cpu is always target for its own SGIs and PPIs
        return 0x01010101 << cpu;
    }

    u32 gicv2::distif::write_CPPI(u32 value) {
        CPPI = value & 0xAAAAAAAA; // odd bits are reserved, zero them out

        unsigned int irq = VCML_ARM_GICv2_NSGI;
        for (unsigned int i = 0; i < VCML_ARM_GICv2_NPPI; i++) {
            if (value & (2 << (i * 2))) {
                m_parent->set_irq_trigger(irq + i, gicv2::EDGE);
                log_debug("irq %u configured to be edge sensitive", irq + i);
            } else {
                m_parent->set_irq_trigger(irq + i, gicv2::LEVEL);
                log_debug("irq %u configured to be level sensitive", irq + i);
            }
        }

        m_parent->update();
        return CPPI;
    }

    u32 gicv2::distif::write_CSPI(u32 value, unsigned int idx) {
        CSPI[idx] = value & 0xAAAAAAAA; // odd bits are reserved, zero them out

        unsigned int irq = VCML_ARM_GICv2_PRIV + idx * 16;
        for (unsigned int i = 0; i < 16; i++) {
            if (value & (2 << (i * 2))) {
                m_parent->set_irq_trigger(irq + i, gicv2::EDGE);
                log_debug("irq %u configured to be edge sensitive", irq + i);
            } else {
                m_parent->set_irq_trigger(irq + i, gicv2::LEVEL);
                log_debug("irq %u configured to be level sensitive", irq + i);
            }
        }

        m_parent->update();
        return CSPI;
    }

    u32 gicv2::distif::write_SCTL(u32 value) {
        int cpu = SCTL.current_bank();
        if (cpu < 0) {
            log_warning("(SCTL) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        unsigned int src_cpu = 1 << cpu;
        unsigned int sgi_num = (value >>  0) & 0x0F;
        unsigned int targets = (value >> 16) & 0xFF;
        unsigned int filters = (value >> 24) & 0x03;

        switch (filters) {
        case 0x0:
            // forward interrupt to CPUs specified in CPU target list
            break;
        case 0x1:
            // forward interrupt to all CPUs except writing CPU
            targets = gicv2::ALL_CPU ^ src_cpu;
            break;
        case 0x2:
            // forward interrupt only to writing CPU
            targets = 1 << cpu;
            break;
        default:
            log_warning("bad SGI target filter");
            break;
        }

        m_parent->set_irq_pending(sgi_num, true, targets);
        for (int target = 0; target < VCML_ARM_GICv2_NCPU; target++) {
            if (targets & (1 << target))
                set_sgi_pending(1 << cpu, sgi_num, target, true);
        }

        m_parent->update();
        return SCTL;
    }


    u8 gicv2::distif::write_SGIS(u8 value, unsigned int idx) {
        int cpu = SGIS.current_bank();
        if (cpu < 0) {
            log_warning("(SGIS) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        unsigned int mask = 1 << cpu;
        unsigned int irq = idx;

        set_sgi_pending(value, irq, cpu, true);
        m_parent->set_irq_pending(irq, true, mask);
        m_parent->update();

        return SGIS;
    }

    u8 gicv2::distif::write_SGIC(u8 value, unsigned int idx) {
        int cpu = SGIC.current_bank();
        if (cpu < 0) {
            log_warning("(SGIC) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        unsigned int mask = 1 << cpu;
        unsigned int irq = idx;

        set_sgi_pending(value, irq, cpu, false);
        if (SGIC.bank(cpu, idx) == 0) // clear SGI if no sources remain
            m_parent->set_irq_pending(irq, false, mask);
        m_parent->update();
        return SGIC;
    }

    gicv2::distif::distif(const sc_module_name& nm):
        peripheral(nm),
        m_parent(dynamic_cast<gicv2*>(get_parent_object())),
        CTLR("CTLR", 0x000, 0x00000000),
        ICTR("ICTR", 0x004, 0x00000000),
        ISER("ISER", 0x100, 0x0000FFFF),
        SSER("SSER", 0x104, 0x00000000),
        ICER("ICER", 0x180, 0x0000FFFF),
        SCER("SCER", 0x184, 0x00000000),
        ISPR("ISPR", 0x200, 0x00000000),
        SSPR("SSPR", 0x204, 0x00000000),
        ICPR("ICPR", 0x280, 0x00000000),
        SCPR("SCPR", 0x284, 0x00000000),
        IACR("IACR", 0x300),
        SACR("SACR", 0x304),
        SGIP("SGIP", 0x400, 0x00),
        PPIP("PPIP", 0x410, 0x00),
        SPIP("SPIP", 0x420, 0x00),
        INTT("INTT", 0x800),
        SPIT("SPIT", 0x820),
        CSGI("CSGI", 0xC00, 0xAAAAAAAA),
        CPPI("CPPI", 0xC04, 0xAAAAAAAA),
        CSPI("CSPI", 0xC08),
        SCTL("SCTL", 0xF00),
        SGIS("SGIS", 0xF20),
        SGIC("SGIC", 0xF10),
        CIDR("CIDR", 0xFF0),
        IN("IN") {
        VCML_ERROR_ON(!m_parent, "gicv2 parent module not found");

        CTLR.allow_read_write();
        CTLR.write = &distif::write_CTLR;
        ICTR.allow_read();

        ISER.set_banked();
        ISER.allow_read_write();
        ISER.read = &distif::read_ISER;
        ISER.write = &distif::write_ISER;

        SSER.allow_read_write();
        SSER.tagged_read = &distif::read_SSER;
        SSER.tagged_write = &distif::write_SSER;

        ICER.set_banked();
        ICER.allow_read_write();
        ICER.read = &distif::read_ICER;
        ICER.write = &distif::write_ICER;

        SCER.allow_read_write();
        SCER.tagged_read = &distif::read_SCER;
        SCER.tagged_write = &distif::write_SCER;

        ISPR.set_banked();
        ISPR.allow_read_write();
        ISPR.read = &distif::read_ISPR;
        ISPR.write = &distif::write_ISPR;

        SSPR.allow_read_write();
        SSPR.tagged_read = &distif::read_SSPR;
        SSPR.tagged_write = &distif::write_SSPR;

        ICPR.set_banked();
        ICPR.allow_read_write();
        ICPR.read = &distif::read_ICPR;
        ICPR.write = &distif::write_ICPR;

        SCPR.allow_read_write();
        SCPR.tagged_read = &distif::read_SCPR;
        SCPR.tagged_write = &distif::write_SCPR;

        IACR.set_banked();
        IACR.allow_read();
        IACR.read = &distif::read_IACR;

        SACR.allow_read();
        SACR.tagged_read = &distif::read_SACR;

        SGIP.set_banked();
        SGIP.allow_read_write();

        PPIP.set_banked();
        PPIP.allow_read_write();

        SPIP.allow_read_write();

        INTT.set_banked();
        INTT.allow_read();
        INTT.tagged_read = &distif::read_INTT;

        SPIT.allow_read_write();

        CSGI.allow_read();

        CPPI.allow_read_write();
        CPPI.write = &distif::write_CPPI;

        CSPI.allow_read_write();
        CSPI.tagged_write = &distif::write_CSPI;

        SCTL.set_banked();
        SCTL.allow_write();
        SCTL.write = &distif::write_SCTL;

        SGIS.set_banked();
        SGIS.allow_read_write();
        SGIS.tagged_write = &distif::write_SGIS;

        SGIC.set_banked();
        SGIC.allow_read_write();
        SGIC.tagged_write = &distif::write_SGIC;

        CIDR.allow_read();

        reset();
    }

    gicv2::distif::~distif() {
        // nothing to do
    }

    void gicv2::distif::reset() {
        for (unsigned int i = 0; i < CIDR.num(); i++)
            CIDR[i] = (VCML_ARM_GICv2_CID >> (i * 8)) & 0xFF;
    }

    void gicv2::distif::setup(unsigned int num_cpu, unsigned int num_irq) {
        ICTR = ((num_cpu & 0x7) << 5) | (0xF & ((num_irq / 32) - 1));
    }

    void gicv2::distif::set_sgi_pending(u8 value, unsigned int sgi,
                                        unsigned int cpu, bool set) {
        if (set) {
            SGIS.bank(cpu, sgi) |= value;
            SGIC.bank(cpu, sgi) |= value;
        } else {
            SGIS.bank(cpu, sgi) &= ~value;
            SGIC.bank(cpu, sgi) &= ~value;
        }
    }

    void gicv2::distif::end_of_elaboration() {
        // SGIs are enabled per default and cannot be disabled
        for (unsigned int irq = 0; irq < VCML_ARM_GICv2_NSGI; irq++)
            m_parent->enable_irq(irq, gicv2::ALL_CPU);
    }

    void gicv2::cpuif::set_current_irq(unsigned int cpu, unsigned int irq) {
        m_curr_irq[cpu] = irq;
        PRIO.bank(cpu) = (irq == VCML_ARM_GICv2_SPURIOUS_IRQ) ? 0x100 :
                                  (m_parent->get_irq_priority(cpu, irq));
        m_parent->update();
    }

    u32 gicv2::cpuif::write_CTLR(u32 val) {
        if ((val & CTLR_ENABLE) && !(CTLR & CTLR_ENABLE))
            log_debug("(CTLR) enabling cpu %d", CTLR.current_bank());
        if (!(val & CTLR_ENABLE) && (CTLR & CTLR_ENABLE))
            log_debug("(CTLR) disabling cpu %d", CTLR.current_bank());
        return val & CTLR_ENABLE;
    }

    u32 gicv2::cpuif::write_IPMR(u32 val) {
        return val & 0x000000FF; // read only first 8 bits
    }

    u32 gicv2::cpuif::write_BIPR(u32 val) {
        ABPR = val & 0x7; // read only first 3 bits, store copy in ABPR
        return ABPR;
    }

    u32 gicv2::cpuif::write_EOIR(u32 val) {
        int cpu = EOIR.current_bank();
        if (cpu < 0) {
            log_warning("(EOIR) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        if (m_curr_irq[cpu] == VCML_ARM_GICv2_SPURIOUS_IRQ)
            return 0; // no active IRQ

        unsigned int irq = val & 0x3FF; // interrupt id stored in bits [9..0]
        if (irq >= m_parent->get_irq_num()) {
            log_warning("(EOI) invalid irq %d ignored", irq);
            return 0;
        }

        if (irq == m_curr_irq[cpu]) {
            set_current_irq(cpu, m_prev_irq[irq][cpu]);
            return 0;
        }

        // handle IRQ that is not currently running
        int iter = m_curr_irq[cpu];
        while (m_prev_irq[iter][cpu] != VCML_ARM_GICv2_SPURIOUS_IRQ) {
            if (m_prev_irq[iter][cpu] == irq) {
                m_prev_irq[iter][cpu] = m_prev_irq[irq][cpu];
                break;
            }

            iter = m_prev_irq[iter][cpu];
        }

        return 0;
    }

    u32 gicv2::cpuif::read_IACK() {
        int cpu = IACK.current_bank();
        if (cpu < 0) {
            log_warning("(IACK) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        int irq = PEND.bank(cpu);
        int cpu_mask = (m_parent->get_irq_model(irq) == gicv2::N_1) ?
                       (gicv2::ALL_CPU) : (1 << cpu);

        log_debug("(IACK) cpu %d acknowledges irq %d", cpu, irq);

        // check if CPU is acknowledging a not pending interrupt
        if (irq == VCML_ARM_GICv2_SPURIOUS_IRQ ||
            m_parent->get_irq_priority(cpu, irq) >= PRIO.bank(cpu)) {
            return VCML_ARM_GICv2_SPURIOUS_IRQ;
        }

        if (is_software_interrupt(irq)) {
            u8 pending = m_parent->DISTIF.SGIS.bank(cpu, irq);
            unsigned int src_cpu = ctz32(pending);
            m_parent->DISTIF.set_sgi_pending(1 << src_cpu, irq, cpu, false);

            // check if SGI is not pending from any CPUs
            if (m_parent->DISTIF.SGIS.bank(cpu, irq) == 0)
                m_parent->set_irq_pending(irq, false, cpu_mask);
            IACK = (src_cpu & 0x7) << 10 | irq;
        } else {
            // clear pending state for interrupt 'irq'
            m_parent->set_irq_pending(irq, false, cpu_mask);
            IACK = irq;
        }

        m_prev_irq[irq][cpu] = m_curr_irq[cpu];
        set_current_irq(cpu, irq); // set the acknowledged IRQ to running

        return IACK;
    }

    gicv2::cpuif::cpuif(const sc_module_name& nm):
        peripheral(nm),
        m_parent(dynamic_cast<gicv2*>(get_parent_object())),
        CTLR("CTLR", 0x00, 0x0),
        IPMR("IPMR", 0x04, 0x0),
        BIPR("BIPR", 0x08, 0x0),
        IACK("IACK", 0x0C, 0x0),
        EOIR("EOIR", 0x10, 0x0),
        PRIO("PRIO", 0x14, VCML_ARM_GICv2_IDLE_PRIO),
        PEND("PEND", 0x18, VCML_ARM_GICv2_SPURIOUS_IRQ),
        ABPR("ABPR", 0x1C, 0x0),
        CIDR("CIDR", 0xFF0),
        IN("IN") {
        VCML_ERROR_ON(!m_parent, "gicv2 parent module not found");

        CTLR.set_banked();
        CTLR.allow_read_write();
        CTLR.write = &cpuif::write_CTLR;

        IPMR.set_banked();
        IPMR.allow_read_write();
        IPMR.write = &cpuif::write_IPMR;

        BIPR.set_banked();
        BIPR.allow_read_write();
        BIPR.write = &cpuif::write_BIPR;

        IACK.set_banked();
        IACK.allow_read();
        IACK.read = &cpuif::read_IACK;

        EOIR.set_banked();
        EOIR.allow_write();
        EOIR.write = &cpuif::write_EOIR;

        PRIO.set_banked();
        PRIO.allow_read();

        PEND.set_banked();
        PEND.allow_read();

        ABPR.set_banked();
        ABPR.allow_read_write();

        CIDR.allow_read();

        reset();
    }

    gicv2::cpuif::~cpuif() {
        // nothing to do
    }

    void gicv2::cpuif::reset() {
        PRIO = PRIO.get_default();
        PEND = PEND.get_default();

        for (unsigned int i = 0; i < CIDR.num(); i++)
            CIDR[i] = (VCML_ARM_GICv2_CID >> (i * 8)) & 0xFF;

        for (unsigned int irq = 0; irq < VCML_ARM_GICv2_NIRQ; irq++)
            for (unsigned int cpu = 0; cpu < VCML_ARM_GICv2_NCPU; cpu++)
                m_prev_irq[irq][cpu] = VCML_ARM_GICv2_SPURIOUS_IRQ;

        for (unsigned int cpu = 0; cpu < VCML_ARM_GICv2_NCPU; cpu++)
            m_curr_irq[cpu] = VCML_ARM_GICv2_SPURIOUS_IRQ;
    }

    gicv2::gicv2(const sc_module_name& nm):
        peripheral(nm),
        DISTIF("distif"),
        CPUIF("cpuif"),
        PPI_IN("PPI_IN"),
        SPI_IN("SPI_IN"),
        FIQ_OUT("FIQ_OUT"),
        IRQ_OUT("IRQ_OUT"),
        m_irq_num(VCML_ARM_GICv2_PRIV),
        m_cpu_num(0),
        m_irq_state() {
        // nothing to do
    }

    gicv2::~gicv2() {
        // nothing to do
    }

    void gicv2::update() {
        for (unsigned int cpu = 0; cpu < m_cpu_num; cpu++) {
            unsigned int irq;
            unsigned int mask = 1 << cpu;
            unsigned int best_irq = VCML_ARM_GICv2_SPURIOUS_IRQ;
            unsigned int best_prio = VCML_ARM_GICv2_IDLE_PRIO;
            CPUIF.PEND.bank(cpu) = VCML_ARM_GICv2_SPURIOUS_IRQ;

            if (DISTIF.CTLR == 0u || CPUIF.CTLR.bank(cpu) == 0u) {
                IRQ_OUT[cpu].write(false);
                continue; // TODO check if continue or return
            }

            // check SGIs
            for (irq = 0; irq < VCML_ARM_GICv2_NSGI; irq++) {
                if (is_irq_enabled(irq, mask) && test_pending(irq, mask)) {
                    if (DISTIF.SGIP.bank(cpu, irq) < best_prio) {
                        best_prio = DISTIF.SGIP.bank(cpu, irq);
                        best_irq = irq;
                    }
                }
            }

            // check PPIs
            for (irq = VCML_ARM_GICv2_NSGI; irq < VCML_ARM_GICv2_PRIV; irq++) {
                if (is_irq_enabled(irq, mask) && test_pending(irq, mask)) {
                    int idx = irq - VCML_ARM_GICv2_NSGI;
                    if (DISTIF.PPIP.bank(cpu, idx) < best_prio) {
                        best_prio = DISTIF.PPIP.bank(cpu, idx);
                        best_irq = irq;
                    }
                }
            }

            // check SPIs
            for (irq = VCML_ARM_GICv2_PRIV; irq < m_irq_num; irq++) {
                int idx = irq - VCML_ARM_GICv2_PRIV;
                if (is_irq_enabled(irq, mask) && test_pending(irq, mask) &&
                    (DISTIF.SPIT[idx] & mask)) {
                    if (DISTIF.SPIP[idx] < best_prio) {
                        best_prio = DISTIF.SPIP[idx];
                        best_irq = irq;
                    }
                }
            }

            // signal IRQ to processor if priority is higher
            bool level = false;
            if (best_prio < CPUIF.IPMR.bank(cpu)) {
                CPUIF.PEND.bank(cpu) = best_irq;
                if (best_prio < CPUIF.PRIO.bank(cpu))
                    level = true;
            }

            if (!IRQ_OUT[cpu].read() && level)
                log_debug("raising irq %u for cpu %d", best_irq, cpu);

            if (IRQ_OUT[cpu].read() && !level)
                log_debug("clearing irq %u for cpu %d", best_irq, cpu);

            IRQ_OUT[cpu].write(level); // FIRQ or NIRQ?
        }
    }

    u8 gicv2::get_irq_priority(unsigned int cpu, unsigned int irq)
    {
        if (irq < VCML_ARM_GICv2_NSGI)
            return DISTIF.SGIP.bank(cpu, irq);
        else if (irq < VCML_ARM_GICv2_PRIV)
            return DISTIF.PPIP.bank(cpu, irq - VCML_ARM_GICv2_NSGI);
        else if (irq < VCML_ARM_GICv2_NIRQ)
            return DISTIF.SPIP[irq - VCML_ARM_GICv2_PRIV];

        log_error("tried to get IRQ priority of invalid IRQ ID (%d)", irq);
        return 0;
    }

    void gicv2::ppi_handler(unsigned int cpu, unsigned int irq) {
        unsigned int idx = irq - VCML_ARM_GICv2_NSGI + cpu * VCML_ARM_GICv2_NPPI;
        unsigned int mask = 1 << cpu;

        bool irq_level = PPI_IN[idx].read();
        set_irq_level(irq, irq_level, mask);
        if (get_irq_trigger(irq) == EDGE)
            set_irq_pending(irq, true, mask);

        update();
    }

    void gicv2::spi_handler(unsigned int irq) {
        unsigned int idx = irq - VCML_ARM_GICv2_PRIV;
        unsigned int target_cpu = DISTIF.SPIT[idx];

        bool irq_level = SPI_IN[idx].read();
        set_irq_level(irq, irq_level, gicv2::ALL_CPU);
        if (get_irq_trigger(irq) == EDGE)
            set_irq_pending(irq, true, target_cpu);

        update();
    }

    void gicv2::end_of_elaboration() {
        m_cpu_num = 0;
        m_irq_num = VCML_ARM_GICv2_PRIV;

        // determine the number of processors from the connected IRQs
        for (auto cpu : IRQ_OUT)
            m_cpu_num = max(m_cpu_num, cpu.first + 1);

        // register handlers for each private peripheral interrupt
        for (auto ppi : PPI_IN) {
            unsigned int cpu = ppi.first / VCML_ARM_GICv2_NPPI;
            unsigned int irq = ppi.first % VCML_ARM_GICv2_NPPI
                               + VCML_ARM_GICv2_NSGI;
            stringstream ss;
            ss << "cpu_" << cpu << "_ppi_" << irq << "_handler";

            sc_spawn_options opts;
            opts.spawn_method();
            opts.set_sensitivity(ppi.second);
            opts.dont_initialize();

            sc_spawn(sc_bind(&gicv2::ppi_handler, this, cpu, irq),
                     ss.str().c_str(), &opts);
        }

        // register handlers for each SPI
        for (auto spi : SPI_IN) {
            unsigned int irq = spi.first + VCML_ARM_GICv2_PRIV;

            if (irq >= VCML_ARM_GICv2_NIRQ)
                VCML_ERROR("too many interrupts (%u)", irq);

            if (irq >= m_irq_num)
                m_irq_num = irq + 1;

            stringstream ss;
            ss << "spi_" << irq << "_handler";


            sc_spawn_options opts;
            opts.spawn_method();
            opts.set_sensitivity(spi.second);
            opts.dont_initialize();

            sc_spawn(sc_bind(&gicv2::spi_handler, this, irq),
                     ss.str().c_str(), &opts);
        }

        log_debug("found %u cpus with %u irqs in total", m_cpu_num, m_irq_num);
        DISTIF.setup(m_cpu_num, m_irq_num);
    }

}}
