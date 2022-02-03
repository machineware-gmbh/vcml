/******************************************************************************
 *                                                                            *
 * Copyright 2020 Jan Henrik Weinstock, Lukas Juenger                         *
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

#include "vcml/models/arm/gic400.h"

namespace vcml { namespace arm {

    static inline bool is_software_interrupt(unsigned int irq) {
        return irq < gic400::NSGI;
    }

    gic400::irq_state::irq_state():
        enabled(0),
        pending(0),
        active(0),
        level(0),
        signaled(0),
        model(N_N),
        trigger(EDGE) {
        // nothing to do
    }

    gic400::lr::lr():
        pending(false),
        active(false),
        hw(0),
        prio(0),
        virtual_id(0),
        physical_id(0),
        cpu_id(0) {
        // nothing to do
    }

    u32 gic400::distif::int_pending_mask(int cpu) {
        u32 value = 0;
        unsigned int mask = 1 << cpu;
        for (unsigned int irq = 0; irq < NPRIV; irq++)
            if (m_parent->test_pending(irq, mask))
                    value |= (1 << irq);
        return value;
    }

    u32 gic400::distif::spi_pending_mask(int cpu) {
        u32 value = 0;
        unsigned int offset = NPRIV + cpu * 32;
        for (unsigned int irq = 0; irq < 32; irq++)
            if (m_parent->test_pending(offset + irq, gic400::ALL_CPU))
                value |= (1 << irq);
        return value;
    }

    u16 gic400::distif::ppi_enabled_mask(int cpu) {
        u16 value = 0;
        unsigned int mask = 1 << cpu;
        for (unsigned int irq = 0; irq < NPPI; irq++)
            if (m_parent->is_irq_enabled(irq + NSGI, mask))
                value |= (1 << irq);
        return value;
    }

    void gic400::distif::write_CTLR(u32 val) {
        if ((val & CTLR_ENABLE) && !(CTLR & CTLR_ENABLE))
            log_debug("(CTLR) irq forwarding enabled");
        if (!(val & CTLR_ENABLE) && (CTLR & CTLR_ENABLE))
            log_debug("(CTLR) irq forwarding disabled");
        CTLR = val & CTLR_ENABLE;
        m_parent->update();
    }

    u32 gic400::distif::read_TYPER() {
        return 0xff;
    }

    u32 gic400::distif::read_ISENABLER_PPI() {
        int cpu = current_cpu();
        if (cpu < 0) {
            log_warn("(ISER) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        u32 mask = ppi_enabled_mask(cpu);
        return (mask << 16) | 0xffff; // SGIs are always enabled
    }

    void gic400::distif::write_ISENABLER_PPI(u32 val) {
        int cpu = current_cpu();
        if (cpu < 0) {
            log_warn("(ISER) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        unsigned int irq = NSGI;
        unsigned int mask = 1 << cpu;

        for (; irq < NPRIV; irq++) {
            if (val & (1 << irq)) {
                m_parent->enable_irq(irq, mask);
                if (m_parent->get_irq_level(irq, mask) &&
                    m_parent->get_irq_trigger(irq) == gic400::LEVEL) {
                    m_parent->set_irq_pending(irq, true, mask);
                }
            }
        }

        m_parent->update();
    }

    u32 gic400::distif::read_ISENABLER_SPI(size_t idx) {
        u32 value = 0;
        unsigned int irq = NPRIV + idx * 32;
        for (unsigned int i = 0; i < 32; i++) {
            if (m_parent->is_irq_enabled(irq + i, gic400::ALL_CPU))
                value |= (1 << i);
        }

        return value;
    }

    void gic400::distif::write_ISENABLER_SPI(u32 val, size_t idx) {
        unsigned int irq = NPRIV + idx * 32;
        for (unsigned int i = 0; i < 32; i++) {
            if (val & (1 << i)) {
                m_parent->enable_irq(irq + i, gic400::ALL_CPU);
                if ((m_parent->get_irq_level(irq + i, gic400::ALL_CPU)) &&
                    (m_parent->get_irq_trigger(irq + i) == gic400::LEVEL)) {
                    m_parent->set_irq_pending(irq + i, true, gic400::ALL_CPU);
                }
            }
        }

        m_parent->update();
    }

    u32 gic400::distif::read_ICENABLER_PPI() {
        int cpu = current_cpu();
        if (cpu < 0) {
            log_warn("(ICER) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        u32 mask = ppi_enabled_mask(cpu);
        return (mask << 16) | 0xFFFF; // SGIs are always enabled
    }

    void gic400::distif::write_ICENABLER_PPI(u32 val) {
        int cpu = current_cpu();
        if (cpu < 0) {
            log_warn("(ICER) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        unsigned int irq = NSGI;
        unsigned int mask = 1 << cpu;

        for (; irq < NPRIV; irq++) {
            if (val & (1 << irq))
                m_parent->disable_irq(irq, mask);
        }

        m_parent->update();
    }

    u32 gic400::distif::read_ICENABLER_SPI(size_t idx) {
        u32 value = 0;
        unsigned int irq = NPRIV + idx * 32;
        for (unsigned int i = 0; i < 32; i++) {
            if (m_parent->is_irq_enabled(irq + i, gic400::ALL_CPU))
                value |= (1 << i);
        }

        return value;
    }

    void gic400::distif::write_ICENABLER_SPI(u32 val, size_t idx) {
        unsigned int irq = NPRIV + idx * 32;
        for (unsigned int i = 0; i < 32; i++) {
            if (val & (1 << i))
                m_parent->disable_irq(irq + i, gic400::ALL_CPU);
        }

        m_parent->update();
    }

    u32 gic400::distif::read_ISPENDR_PPI() {
        int cpu = current_cpu();
        if (cpu < 0) {
            log_warn("(ISPR) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        return int_pending_mask(cpu);
    }

    void gic400::distif::write_ISPENDR_PPI(u32 value) {
        int cpu = current_cpu();
        if (cpu < 0) {
            log_warn("(ISPR) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        unsigned int irq = NSGI;
        unsigned int mask = 1 << cpu;

        for (; irq < NPRIV; irq++) {
            if (value & (1 << irq))
                m_parent->set_irq_pending(irq, true, mask);
        }

        m_parent->update();
    }

    u32 gic400::distif::read_SSPR(size_t idx) {
        return spi_pending_mask(idx);
    }

    void gic400::distif::write_SSPR(u32 value, size_t idx) {
        unsigned int irq = NPRIV + idx * 32;
        for (unsigned int i = 0; i < 32; i++) {
            if (value & (1 << i))
                m_parent->set_irq_pending(irq + i, true, ITARGETS_SPI[i]);
        }

        m_parent->update();
    }

    u32 gic400::distif::read_ICPENDR_PPI() {
        int cpu = current_cpu();
        if (cpu < 0) {
            log_warn("(ICPENDR0) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        return int_pending_mask(cpu);
    }

    void gic400::distif::write_ICPENDR_PPI(u32 value) {
        int cpu = current_cpu();
        if (cpu < 0) {
            log_warn("(ICPENDR0) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        unsigned int irq = NSGI;
        unsigned int mask = 1 << cpu;

        for (; irq < NPRIV; irq++) {
            if (value & (1 << irq))
                m_parent->set_irq_pending(irq, false, mask);
        }

        m_parent->update();
    }

    u32 gic400::distif::read_ICPENDR_SPI(size_t idx) {
        return spi_pending_mask(idx);
    }

    void gic400::distif::write_ICPENDR_SPI(u32 val, size_t idx) {
        unsigned int irq = NPRIV + idx * 32;
        for (unsigned int i = 0; i < 32; i++) {
            if (val & (1 << i))
                m_parent->set_irq_pending(irq + i, false, gic400::ALL_CPU);
        }

        m_parent->update();
    }

    u32 gic400::distif::read_ISACTIVER_PPI() {
        int cpu = current_cpu();
        if (cpu < 0) {
            log_warn("(ISACTIVER0) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        u32 value = 0;
        unsigned int mask = 1 << cpu;
        for (unsigned int l = 0; l < NPRIV; l++) {
            if (m_parent->is_irq_active(l, mask))
                value |= (1 << l);
        }

        return value;
    }

    u32 gic400::distif::read_ISACTIVER_SPI(size_t idx) {
        u32 value = 0;
        unsigned int irq = NPRIV + idx * 32;
        for (unsigned int i = 0; i < 32; i++) {
            if (m_parent->is_irq_active(irq + i, gic400::ALL_CPU))
                value |= (1 << i);
        }

        return value;
    }

    void gic400::distif::write_ICACTIVER_PPI(u32 val) {
        int cpu = current_cpu();
        if (cpu < 0) {
            log_warn("(ICAR) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        unsigned int mask = 1 << cpu;
        for (unsigned int irq = 0; irq < 32; irq++) {
            if (val & (1 << irq))
                m_parent->set_irq_active(irq, false, mask);
        }
    }

    void gic400::distif::write_ICACTIVER_SPI(u32 val, size_t idx) {
        unsigned int irq = NPRIV + idx * 32;
        for (unsigned int i = 0; i < 32; i++) {
            if (val & (1 << i))
                m_parent->set_irq_active(irq + i, false, gic400::ALL_CPU);
        }
    }

    u32 gic400::distif::read_ITARGETS_PPI(size_t idx) {
        int cpu = current_cpu();
        if (cpu < 0) {
            log_warn("(INTT) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        // local cpu is always target for its own SGIs and PPIs
        return 0x01010101 << cpu;
    }

    void gic400::distif::write_ICFGR(u32 value) {
        ICFGR_PPI = value & 0xaaaaaaaa; // odd bits are reserved, zero them out

        unsigned int irq = NSGI;
        for (unsigned int i = 0; i < NPPI; i++) {
            if (value & (2 << (i * 2))) {
                m_parent->set_irq_trigger(irq + i, gic400::EDGE);
                log_debug("irq %u configured to be edge sensitive", irq + i);
            } else {
                m_parent->set_irq_trigger(irq + i, gic400::LEVEL);
                log_debug("irq %u configured to be level sensitive", irq + i);
            }
        }

        m_parent->update();
    }

    void gic400::distif::write_ICFGR_SPI(u32 value, size_t idx) {
        ICFGR_SPI[idx] = value & 0xaaaaaaaa; // odd bits are reserved

        unsigned int irq = NPRIV + idx * 16;
        for (unsigned int i = 0; i < 16; i++) {
            if (value & (2 << (i * 2))) {
                m_parent->set_irq_trigger(irq + i, gic400::EDGE);
                log_debug("irq %u configured to be edge sensitive", irq + i);
            } else {
                m_parent->set_irq_trigger(irq + i, gic400::LEVEL);
                log_debug("irq %u configured to be level sensitive", irq + i);
            }
        }

        m_parent->update();
    }

    void gic400::distif::write_SGIR(u32 value) {
        int cpu = current_cpu();
        if (cpu < 0) {
            log_warn("(SCTL) invalid cpu %d, assuming 0", cpu);
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
            targets = gic400::ALL_CPU ^ src_cpu;
            break;
        case 0x2:
            // forward interrupt only to writing CPU
            targets = 1 << cpu;
            break;
        default:
            log_warn("bad SGI target filter");
            break;
        }

        m_parent->set_irq_pending(sgi_num, true, targets);
        for (unsigned int target = 0; target < NCPU; target++) {
            if (targets & (1 << target))
                set_sgi_pending(1 << cpu, sgi_num, target, true);
        }

        m_parent->set_irq_signaled(sgi_num, false, targets);
        m_parent->update();
    }


    void gic400::distif::write_SPENDSGIR(u8 value, size_t idx) {
        int cpu = current_cpu();
        if (cpu < 0) {
            log_warn("(SGIS) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        unsigned int mask = 1 << cpu;
        unsigned int irq = idx;

        set_sgi_pending(value, irq, cpu, true);
        m_parent->set_irq_pending(irq, true, mask);
        m_parent->set_irq_signaled(irq, false, mask);
        m_parent->update();
    }

    void gic400::distif::write_CPENDSGIR(u8 value, size_t idx) {
        int cpu = current_cpu();
        if (cpu < 0) {
            log_warn("(SGIC) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        unsigned int mask = 1 << cpu;
        unsigned int irq = idx;

        set_sgi_pending(value, irq, cpu, false);
        if (CPENDSGIR.bank(cpu, idx) == 0) // clear SGI if no sources remain
            m_parent->set_irq_pending(irq, false, mask);
        m_parent->update();
    }

    gic400::distif::distif(const sc_module_name& nm):
        peripheral(nm),
        m_parent(dynamic_cast<gic400*>(get_parent_object())),
        CTLR("CTLR", 0x000, 0x00000000),
        TYPER("TYPER", 0x004, 0x00000000),
        IIDR("IIDR", 0x008, 0x00000000),
        ISENABLER_PPI("ISENABLER_PPI", 0x100, 0x0000FFFF),
        ISENABLER_SPI("ISENABLER_SPI", 0x104, 0x00000000),
        ICENABLER_PPI("ICENABLER_PPI", 0x180, 0x0000FFFF),
        ICENABLER_SPI("ICENABLER_SPI", 0x184, 0x00000000),
        ISPENDR_PPI("ISPENDR_PPI", 0x200, 0x00000000),
        ISPENDR_SPI("ISPENDR_SPI", 0x204, 0x00000000),
        ICPENDR_PPI("ICPENDR_PPI", 0x280, 0x00000000),
        ICPENDR_SPI("ICPENDR_SPI", 0x284, 0x00000000),
        ISACTIVER_PPI("ISACTIVER_PPI", 0x300),
        ISACTIVER_SPI("ISACTIVER_SPI", 0x304),
        ICACTIVER_PPI("ICACTIVER_PPI", 0x380, 0x00000000),
        ICACTIVER_SPI("ICACTIVER_SPI", 0x384, 0x00000000),
        IPRIORITY_SGI("IPRIORITY_SGI", 0x400, 0x00),
        IPRIORITY_PPI("IPRIORITY_PPI", 0x410, 0x00),
        IPRIORITY_SPI("IPRIORITY_SPI", 0x420, 0x00),
        ITARGETS_PPI("ITARGETS_PPI", 0x800),
        ITARGETS_SPI("ITARGETS_SPI", 0x820),
        ICFGR_SGI("ICFGR_SGI", 0xC00, 0xAAAAAAAA),
        ICFGR_PPI("ICFGR_PPI", 0xC04, 0xAAAAAAAA),
        ICFGR_SPI("ICFGR_SPI", 0xC08),
        SGIR("SGIR", 0xF00),
        CPENDSGIR("SGIC", 0xF10),
        SPENDSGIR("SPENDSGIR", 0xF20),
        CIDR("CIDR", 0xFF0),
        IN("IN") {
        VCML_ERROR_ON(!m_parent, "gic400 parent module not found");

        CTLR.sync_on_write();
        CTLR.allow_read_write();
        CTLR.on_write(&distif::write_CTLR);

        TYPER.sync_never();
        TYPER.allow_read_only();
        TYPER.on_read(&distif::read_TYPER);

        IIDR.sync_never();
        IIDR.allow_read_only();

        ISENABLER_PPI.set_banked();
        ISENABLER_PPI.sync_always();
        ISENABLER_PPI.allow_read_write();
        ISENABLER_PPI.on_read(&distif::read_ISENABLER_PPI);
        ISENABLER_PPI.on_write(&distif::write_ISENABLER_PPI);

        ISENABLER_SPI.sync_always();
        ISENABLER_SPI.allow_read_write();
        ISENABLER_SPI.on_read(&distif::read_ISENABLER_SPI);
        ISENABLER_SPI.on_write(&distif::write_ISENABLER_SPI);

        ICENABLER_PPI.set_banked();
        ICENABLER_PPI.sync_always();
        ICENABLER_PPI.allow_read_write();
        ICENABLER_PPI.on_read(&distif::read_ICENABLER_PPI);
        ICENABLER_PPI.on_write(&distif::write_ICENABLER_PPI);

        ICENABLER_SPI.sync_always();
        ICENABLER_SPI.allow_read_write();
        ICENABLER_SPI.on_read(&distif::read_ICENABLER_SPI);
        ICENABLER_SPI.on_write(&distif::write_ICENABLER_SPI);

        ISPENDR_PPI.set_banked();
        ISPENDR_PPI.sync_always();
        ISPENDR_PPI.allow_read_write();
        ISPENDR_PPI.on_read(&distif::read_ISPENDR_PPI);
        ISPENDR_PPI.on_write(&distif::write_ISPENDR_PPI);

        ISPENDR_SPI.sync_always();
        ISPENDR_SPI.allow_read_write();
        ISPENDR_SPI.on_read(&distif::read_SSPR);
        ISPENDR_SPI.on_write(&distif::write_SSPR);

        ICPENDR_PPI.set_banked();
        ICPENDR_PPI.sync_always();
        ICPENDR_PPI.allow_read_write();
        ICPENDR_PPI.on_read(&distif::read_ICPENDR_PPI);
        ICPENDR_PPI.on_write(&distif::write_ICPENDR_PPI);

        ICPENDR_SPI.sync_always();
        ICPENDR_SPI.allow_read_write();
        ICPENDR_SPI.on_read(&distif::read_ICPENDR_SPI);
        ICPENDR_SPI.on_write(&distif::write_ICPENDR_SPI);

        ISACTIVER_PPI.set_banked();
        ISACTIVER_PPI.allow_read_only();
        ISACTIVER_PPI.sync_on_read();
        ISACTIVER_PPI.on_read(&distif::read_ISACTIVER_PPI);

        ISACTIVER_SPI.allow_read_only();
        ISACTIVER_SPI.sync_on_read();
        ISACTIVER_SPI.on_read(&distif::read_ISACTIVER_SPI);

        ICACTIVER_PPI.set_banked();
        ICACTIVER_PPI.sync_on_write();
        ICACTIVER_PPI.allow_read_write();
        ICACTIVER_PPI.on_write(&distif::write_ICACTIVER_PPI);

        ICACTIVER_SPI.sync_on_write();
        ICACTIVER_SPI.allow_read_write();
        ICACTIVER_SPI.on_write(&distif::write_ICACTIVER_SPI);

        IPRIORITY_SGI.set_banked();
        IPRIORITY_SGI.sync_never();
        IPRIORITY_SGI.allow_read_write();

        IPRIORITY_PPI.set_banked();
        IPRIORITY_PPI.sync_never();
        IPRIORITY_PPI.allow_read_write();

        IPRIORITY_SGI.sync_never();
        IPRIORITY_SGI.allow_read_write();

        ITARGETS_PPI.set_banked();
        ITARGETS_PPI.sync_always();
        ITARGETS_PPI.allow_read_write();
        ITARGETS_PPI.on_read(&distif::read_ITARGETS_PPI);

        ITARGETS_SPI.sync_always();
        ITARGETS_SPI.allow_read_write();

        ICFGR_SGI.allow_read_only();
        ICFGR_SGI.sync_on_read();

        ICFGR_PPI.sync_on_write();
        ICFGR_PPI.allow_read_write();
        ICFGR_PPI.on_write(&distif::write_ICFGR);

        ICFGR_SPI.sync_on_write();
        ICFGR_SPI.allow_read_write();
        ICFGR_SPI.on_write(&distif::write_ICFGR_SPI);

        SGIR.set_banked();
        SGIR.allow_write_only();
        SGIR.sync_on_write();
        SGIR.on_write(&distif::write_SGIR);

        SPENDSGIR.set_banked();
        SPENDSGIR.sync_always();
        SPENDSGIR.allow_read_write();
        SPENDSGIR.on_write(&distif::write_SPENDSGIR);

        CPENDSGIR.set_banked();
        CPENDSGIR.sync_always();
        CPENDSGIR.allow_read_write();
        CPENDSGIR.on_write(&distif::write_CPENDSGIR);

        CIDR.allow_read_only();
        CIDR.sync_never();

        reset();
    }

    gic400::distif::~distif() {
        // nothing to do
    }

    void gic400::distif::reset() {
        peripheral::reset();

        for (unsigned int i = 0; i < CIDR.count(); i++)
            CIDR[i] = (PCID >> (i * 8)) & 0xFF;
    }

    void gic400::distif::setup(unsigned int num_cpu, unsigned int num_irq) {
        TYPER = ((num_cpu & 0x7) << 5) | (0xF & ((num_irq / 32) - 1));
    }

    void gic400::distif::set_sgi_pending(u8 value, unsigned int sgi,
                                        unsigned int cpu, bool set) {
        if (set) {
            SPENDSGIR.bank(cpu, sgi) |= value;
            CPENDSGIR.bank(cpu, sgi) |= value;
        } else {
            SPENDSGIR.bank(cpu, sgi) &= ~value;
            CPENDSGIR.bank(cpu, sgi) &= ~value;
        }
    }

    void gic400::distif::end_of_elaboration() {
        // SGIs are enabled per default and cannot be disabled
        for (unsigned int irq = 0; irq < NSGI; irq++)
            m_parent->enable_irq(irq, gic400::ALL_CPU);
    }

    void gic400::cpuif::set_current_irq(unsigned int cpu, unsigned int irq) {
        m_curr_irq[cpu] = irq;
        RPR.bank(cpu) = (irq == SPURIOUS_IRQ) ? 0x100 :
                         (m_parent->get_irq_priority(cpu, irq));
        m_parent->update();
    }

    void gic400::cpuif::write_CTLR(u32 val) {
        if ((val & CTLR_ENABLE) && !(CTLR & CTLR_ENABLE))
            log_debug("(CTLR) enabling cpu %d", current_cpu());
        if (!(val & CTLR_ENABLE) && (CTLR & CTLR_ENABLE))
            log_debug("(CTLR) disabling cpu %d", current_cpu());
        CTLR = val & CTLR_ENABLE;
    }

    void gic400::cpuif::write_PMR(u32 val) {
        PMR = val & 0x000000ff; // read only first 8 bits
    }

    void gic400::cpuif::write_BPR(u32 val) {
        ABPR = val & 0x7; // read only first 3 bits, store copy in ABPR
        BPR = ABPR;
    }

    void gic400::cpuif::write_EOIR(u32 val) {
        int cpu = current_cpu();
        if (cpu < 0) {
            log_warn("(EOIR) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        if (m_curr_irq[cpu] == SPURIOUS_IRQ)
            return; // no active IRQ

        unsigned int irq = val & 0x3ff; // interrupt id stored in bits [9..0]
        if (irq >= m_parent->get_irq_num()) {
            log_warn("(EOI) invalid irq %d ignored", irq);
            return;
        }

        if (irq == m_curr_irq[cpu]) {
            log_debug("(EOI) cpu %d eois irq %d", cpu, irq);
            set_current_irq(cpu, m_prev_irq[irq][cpu]);
            m_parent->set_irq_active(irq, false, 1 << cpu);
            m_parent->update();
            return;
        }

        // handle IRQ that is not currently running
        int iter = m_curr_irq[cpu];
        while (m_prev_irq[iter][cpu] != SPURIOUS_IRQ) {
            if (m_prev_irq[iter][cpu] == irq) {
                m_prev_irq[iter][cpu] = m_prev_irq[irq][cpu];
                break;
            }

            iter = m_prev_irq[iter][cpu];
        }

        return;
    }

    u32 gic400::cpuif::read_IAR() {
        int cpu = current_cpu();
        if (cpu < 0) {
            log_warn("(IACK) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        int irq = HPPIR.bank(cpu);
        int cpu_mask = (m_parent->get_irq_model(irq) == gic400::N_1) ?
                       (gic400::ALL_CPU) : (1 << cpu);

        log_debug("(IACK) cpu %d acknowledges irq %d", cpu, irq);

        // check if CPU is acknowledging a not pending interrupt
        if (irq == SPURIOUS_IRQ ||
            m_parent->get_irq_priority(cpu, irq) >= RPR.bank(cpu)) {
            return SPURIOUS_IRQ;
        }

        if (is_software_interrupt(irq)) {
            u32 pending = m_parent->DISTIF.SPENDSGIR.bank(cpu, irq);
            unsigned int src_cpu = ctz(pending);
            m_parent->DISTIF.set_sgi_pending(1 << src_cpu, irq, cpu, false);

            // check if SGI is not pending from any CPUs
            if (m_parent->DISTIF.SPENDSGIR.bank(cpu, irq) == 0)
                m_parent->set_irq_pending(irq, false, cpu_mask);
            IAR = (src_cpu & 0x7) << 10 | irq;
        } else {
            // clear pending state for interrupt 'irq'
            m_parent->set_irq_pending(irq, false, cpu_mask);
            IAR = irq;
        }

        m_prev_irq[irq][cpu] = m_curr_irq[cpu];
        set_current_irq(cpu, irq); // set the acknowledged IRQ to running
        m_parent->set_irq_active(irq, true, cpu_mask);
        m_parent->set_irq_signaled(irq, true, cpu_mask);
        return IAR;
    }

    gic400::cpuif::cpuif(const sc_module_name& nm):
        peripheral(nm),
        m_parent(dynamic_cast<gic400*>(get_parent_object())),
        CTLR("CTLR", 0x00, 0x0),
        PMR("PMR", 0x04, 0x0),
        BPR("BPR", 0x08, 0x0),
        IAR("IAR", 0x0C, 0x0),
        EOIR("EOIR", 0x10, 0x0),
        RPR("RPR", 0x14, IDLE_PRIO),
        HPPIR("HPPIR", 0x18, SPURIOUS_IRQ),
        ABPR("ABPR", 0x1C, 0x0),
        APR("APR", 0xD0, 0x00000000),
        IIDR("IIDR", 0xFC, IFID),
        CIDR("CIDR", 0xFF0),
        DIR("DIR", 0x1000),
        IN("IN") {
        VCML_ERROR_ON(!m_parent, "gic400 parent module not found");

        CTLR.set_banked();
        CTLR.sync_always();
        CTLR.allow_read_write();
        CTLR.on_write(&cpuif::write_CTLR);

        PMR.set_banked();
        PMR.sync_always();
        PMR.allow_read_write();
        PMR.on_write(&cpuif::write_PMR);

        BPR.set_banked();
        BPR.sync_always();
        BPR.allow_read_write();
        BPR.on_write(&cpuif::write_BPR);

        IAR.set_banked();
        IAR.allow_read_only();
        IAR.sync_on_read();
        IAR.on_read(&cpuif::read_IAR);

        EOIR.set_banked();
        EOIR.allow_write_only();
        EOIR.sync_on_write();
        EOIR.on_write(&cpuif::write_EOIR);

        RPR.set_banked();
        RPR.sync_never();
        RPR.allow_read_only();

        HPPIR.set_banked();
        HPPIR.sync_never();
        HPPIR.allow_read_only();

        ABPR.set_banked();
        ABPR.sync_always();
        ABPR.allow_read_write();

        APR.sync_always();
        APR.allow_read_write();

        IIDR.sync_never();
        IIDR.allow_read_only();

        CIDR.sync_never();
        CIDR.allow_read_only();

        DIR.set_banked();
        DIR.sync_always();
        DIR.allow_read_write();

        reset();
    }

    gic400::cpuif::~cpuif() {
        // nothing to do
    }

    void gic400::cpuif::reset() {
        peripheral::reset();

        for (unsigned int i = 0; i < CIDR.count(); i++)
            CIDR[i] = (PCID >> (i * 8)) & 0xFF;

        for (unsigned int irq = 0; irq < NIRQ; irq++)
            for (unsigned int cpu = 0; cpu < NCPU; cpu++)
                m_prev_irq[irq][cpu] = SPURIOUS_IRQ;

        for (unsigned int cpu = 0; cpu < NCPU; cpu++)
            m_curr_irq[cpu] = SPURIOUS_IRQ;
    }

    void gic400::vifctrl::write_HCR(u32 val) {
        u8 cpu = current_cpu();
        HCR.bank(cpu) = val;
        m_parent->update(true);
    }

    u32 gic400::vifctrl::read_VTR() {
        return 0x90000000 | (NLR - 1);
    }

    void gic400::vifctrl::write_LR(u32 val, size_t idx) {
        u8 cpu = current_cpu();
        u8 state = (val >> 28) & 0b11;
        u8 hw = (val >> 31) & 0b1;

        if (hw == 0) {
            u8 eoi = (val >> 19) & 0b1;
            if (eoi == 1)
                log_error("(LR) maintenance IRQ not implemented");
            u8 cpu_id = (val >> 10) & 0b111;
            set_lr_cpuid(idx, cpu, cpu_id);
            set_lr_hw(idx, cpu, false);
            set_lr_physid(idx, cpu, 0);
        } else {
            set_lr_cpuid(idx, cpu, 0);
            set_lr_hw(idx, cpu, true);
            u16 physid = (val >> 10) & 0x1ff;
            set_lr_physid(idx, cpu, physid);
        }

        if (state & 1)
            set_lr_pending(idx, cpu, true);
        if (state & 2)
            set_lr_active(idx, cpu, true);
        if (state == 0) {
            set_lr_pending(idx, cpu, false);
            set_lr_active(idx, cpu, false);
        }

        u32 prio = (val >> 23) & 0x1f;
        u32 irq = val & 0x1ff;

        set_lr_prio(idx, cpu, prio);
        set_lr_vid(idx, cpu, irq);

        LR[idx] = val;
        m_parent->update(true);
    }

    u32 gic400::vifctrl::read_LR(size_t idx) {
        u8 cpu = current_cpu();

        // update pending and active bit
        if (is_lr_pending(idx, cpu))
            LR[idx] = LR[idx] | LR_PENDING_MASK;
        else
            LR[idx] = LR[idx] & ~LR_PENDING_MASK;

        if (is_lr_active(idx, cpu))
            LR[idx] = LR[idx] | LR_ACTIVE_MASK;
        else
            LR[idx] = LR[idx] & ~LR_ACTIVE_MASK;

        return LR[idx];
    }

    void gic400::vifctrl::write_VMCR(u32 val) {
        int cpu = current_cpu();
        u8 prio_mask = (val >> 27) & 0x1f;
        u8 bpr = (val >> 21) & 0x03;
        u32 ctlr = val & 0x1ff;

        m_parent->VCPUIF.PMR.bank(cpu) = prio_mask << 3;
        m_parent->VCPUIF.BPR.bank(cpu) = bpr;
        m_parent->VCPUIF.CTLR.bank(cpu) = ctlr;
    }

    u32 gic400::vifctrl::read_VMCR() {
        int cpu = current_cpu();
        u8 prio_mask = (m_parent->VCPUIF.PMR.bank(cpu) >> 3) & 0x1f;
        u8 bpr = m_parent->VCPUIF.BPR.bank(cpu) & 0x03;
        u32 ctlr = m_parent->VCPUIF.CTLR.bank(cpu) & 0x1ff;

        return (prio_mask << 27 | bpr << 21 | ctlr);
    }

    void gic400::vifctrl::write_APR(u32 val) {
       int cpu = current_cpu();
       u8 prio = IDLE_PRIO;
       if (val != 0)
           prio = fls(val) << (VIRT_MIN_BPR + 1);
       m_parent->VCPUIF.RPR.bank(cpu) = prio;
       APR = val;
    }

    u8 gic400::vifctrl::get_irq_priority(unsigned int cpu, unsigned int irq) {
        for(unsigned int i = 0; i < NLR; i++) {
            if (m_lr_state[cpu][i].virtual_id == irq &&
                    (m_lr_state[cpu][i].active ||
                    m_lr_state[cpu][i].pending)) {
                return m_lr_state[cpu][i].prio;
            }
        }

        log_error("failed getting LR prio for irq %d on cpu %d", irq, cpu);
        return 0;
    }

    u8 gic400::vifctrl::get_lr(unsigned int irq, u8 cpu) {
        for(unsigned int i = 0; i < NLR; i++) {
            if (m_lr_state[cpu][i].virtual_id == irq  &&
                (m_lr_state[cpu][i].active ||
                m_lr_state[cpu][i].pending)) {
                return i;
            }
        }

        log_error("failed getting LR for irq %d on cpu%d", irq, cpu);
        return 0;
    }

    gic400::vifctrl::vifctrl(const sc_module_name& nm):
        peripheral(nm),
        m_parent(dynamic_cast<gic400*>(get_parent_object())),
        m_lr_state(),
        HCR("HCR", 0x0),
        VTR("VTR", 0x04, 0x0),
        VMCR("VMCR", 0x08),
        APR("APR", 0xF0, 0x0),
        LR("LR", 0x100, 0x0),
        IN("IN") {

        HCR.set_banked();
        HCR.allow_read_write();
        HCR.on_write(&vifctrl::write_HCR);

        VTR.allow_read_only();
        VTR.on_read(&vifctrl::read_VTR);

        LR.set_banked();
        LR.allow_read_write();
        LR.on_write(&vifctrl::write_LR);
        LR.on_read(&vifctrl::read_LR);

        VMCR.allow_read_write();
        VMCR.on_read(&vifctrl::read_VMCR);
        VMCR.on_write(&vifctrl::write_VMCR);

        APR.set_banked();
        APR.allow_read_write();
        APR.on_write(&vifctrl::write_APR);
    }

    gic400::vifctrl::~vifctrl() {
        // nothing to do
    }

    void gic400::vcpuif::write_CTLR(u32 val) {
        if (val > 1)
            log_error("(vCTLR) using unimplemented features");
        CTLR = val;
    }

    void gic400::vcpuif::write_BPR(u32 val) {
        BPR = std::max(val & 0x07, (u32)VIRT_MIN_BPR);
    }

    u32 gic400::vcpuif::read_IAR() {
        u8 cpu = current_cpu();

        u32 irq = HPPIR.bank(cpu);
        if (irq == SPURIOUS_IRQ ||
            m_vifctrl->get_irq_priority(cpu, irq) >= RPR.bank(cpu))
            return SPURIOUS_IRQ;

        u32 prio = m_vifctrl->get_irq_priority(cpu, irq) << 3;
        u32 mask = ~0ul << ((BPR.bank(cpu) & 0x07) + 1);
        RPR.bank(cpu) = prio & mask;
        u32 preemption_level = prio >> (VIRT_MIN_BPR + 1);
        u32 bitno = preemption_level % 32;
        m_parent->VIFCTRL.APR.bank(cpu) |= (1 << bitno);

        u8 lr = m_vifctrl->get_lr(irq, cpu);
        m_vifctrl->set_lr_active(lr, cpu, true);

        m_vifctrl->set_lr_pending(lr, cpu, false);

        log_debug("(vIACK) cpu %d acknowledges virq %d", cpu, irq);
        m_parent->update(true);
        u8 cpu_id = m_vifctrl->get_lr_cpuid(lr, cpu);
        return ( (cpu_id & 0x111) << 10 | irq);
    }

    void gic400::vcpuif::write_EOIR(u32 val) {
        u8 cpu = current_cpu();
        u32 irq = val & 0x1FF;

        if (irq >= m_parent->get_irq_num()) {
            log_warn("(EOI) invalid irq %d ignored", irq);
            return;
        }

        log_debug("(vEOIR) cpu%d eois virq %d", cpu, irq);

        // drop priority and update APR
        m_vifctrl->APR.bank(cpu) &= m_parent->VIFCTRL.APR.bank(cpu) - 1;
        u32 apr = m_parent->VIFCTRL.APR.bank(cpu);

        if (apr)
            RPR.bank(cpu) = fls(apr) << (VIRT_MIN_BPR + 1);
        else
            RPR.bank(cpu) = IDLE_PRIO;

        // deactivate interrupt
        u8 lr = m_vifctrl->get_lr(irq, cpu);
        m_vifctrl->set_lr_active(lr, cpu, false);
        if (m_vifctrl->is_lr_hw(lr, cpu)) {
            u16 physid = m_vifctrl->get_lr_physid(lr, cpu);
            if (!(physid < NSGI || physid > NIRQ)) {
                m_parent->set_irq_active(physid, false, 1 << cpu);
            } else {
                log_error("unexpected physical id %d for cpu %d in LR %d",
                          physid, cpu, lr);
            }
        }

        m_parent->update(true);
        EOIR = val;
    }

    gic400::vcpuif::vcpuif(const sc_module_name& nm, vifctrl *vifctrl):
        peripheral(nm),
        m_parent(dynamic_cast<gic400*>(get_parent_object())),
        m_vifctrl(vifctrl),
        CTLR("CTLR", 0x00, 0x0),
        PMR("PMR", 0x04, 0x0),
        BPR("BPR", 0x08, 2),
        IAR("IAR", 0x0C, 0x0),
        EOIR("EOIR", 0x10, 0x0),
        RPR("RPR", 0x14, IDLE_PRIO),
        HPPIR("HPPIR", 0x18, SPURIOUS_IRQ),
        APR("APR", 0xD0, 0x00000000),
        IIDR("IIDR", 0xFC, IFID),
        IN("IN") {

        CTLR.set_banked();
        CTLR.allow_read_write();
        CTLR.on_write(&vcpuif::write_CTLR);

        PMR.set_banked();
        PMR.allow_read_write();

        BPR.set_banked();
        BPR.allow_read_write();
        BPR.on_write(&vcpuif::write_BPR);

        IAR.set_banked();
        IAR.allow_read_only();
        IAR.on_read(&vcpuif::read_IAR);

        EOIR.set_banked();
        EOIR.allow_write_only();
        EOIR.on_write(&vcpuif::write_EOIR);

        RPR.set_banked();

        HPPIR.set_banked();
        HPPIR.allow_read_write();

        APR.set_banked();
        APR.allow_read_write();

        IIDR.allow_read_only();

        reset();
    }

    gic400::vcpuif::~vcpuif() {
        // nothing to do
    }

    void gic400::vcpuif::reset() {
        RPR = RPR.get_default();
        HPPIR = HPPIR.get_default();
    }

    gic400::gic400(const sc_module_name& nm):
        peripheral(nm),
        DISTIF("distif"),
        CPUIF("cpuif"),
        VIFCTRL("vifctrl"),
        VCPUIF("vcpuif", &VIFCTRL),
        PPI_IN("PPI_IN", IRQ_AS_PPI),
        SPI_IN("SPI_IN", IRQ_AS_SPI),
        FIQ_OUT("FIQ_OUT"),
        IRQ_OUT("IRQ_OUT"),
        VFIQ_OUT("VFIQ_OUT"),
        VIRQ_OUT("VIRQ_OUT"),
        m_irq_num(NPRIV),
        m_cpu_num(0),
        m_irq_state() {
        DISTIF.CLOCK.bind(CLOCK);
        DISTIF.RESET.bind(RESET);
        CPUIF.CLOCK.bind(CLOCK);
        CPUIF.RESET.bind(RESET);
        VIFCTRL.CLOCK.bind(CLOCK);
        VIFCTRL.RESET.bind(RESET);
        VCPUIF.CLOCK.bind(CLOCK);
        VCPUIF.RESET.bind(RESET);
    }

    gic400::~gic400() {
        // nothing to do
    }

    void gic400::update(bool virt) {
        for (unsigned int cpu = 0; cpu < m_cpu_num; cpu++) {
            unsigned int irq;
            unsigned int mask = 1 << cpu;
            unsigned int best_irq = SPURIOUS_IRQ;
            unsigned int best_prio = IDLE_PRIO;

            if (!virt)
                CPUIF.HPPIR.bank(cpu) = SPURIOUS_IRQ;
            else
                VCPUIF.HPPIR.bank(cpu) = SPURIOUS_IRQ;

            if (!virt && (DISTIF.CTLR == 0u || CPUIF.CTLR.bank(cpu) == 0u)) {
                log_debug("Disabling IRQ[%d]", cpu);
                IRQ_OUT[cpu].write(false);
                continue; // TODO check if continue or return
            }

            if (virt && (VIFCTRL.HCR.bank(cpu) == 0u)) {
                log_debug("Disabling VIRQ[%d]", cpu);
                VIRQ_OUT[cpu].write(false);
                continue;
            }

            if (!virt) {
                // check SGIs
                for (irq = 0; irq < NSGI; irq++) {
                    if (is_irq_enabled(irq, mask) && test_pending(irq, mask) &&
                        !is_irq_active(irq, mask)) {
                        if (DISTIF.IPRIORITY_SGI.bank(cpu, irq) < best_prio) {
                            best_prio = DISTIF.IPRIORITY_SGI.bank(cpu, irq);
                            best_irq = irq;
                        }
                    }
                }

                // check PPIs
                for (irq = NSGI; irq < NPRIV; irq++) {
                    if (is_irq_enabled(irq, mask) && test_pending(irq, mask) &&
                        !is_irq_active(irq, mask)) {
                        int idx = irq - NSGI;
                        if (DISTIF.IPRIORITY_PPI.bank(cpu, idx) < best_prio) {
                            best_prio = DISTIF.IPRIORITY_PPI.bank(cpu, idx);
                            best_irq = irq;
                        }
                    }
                }

                // check SPIs
                for (irq = NPRIV; irq < m_irq_num; irq++) {
                    int idx = irq - NPRIV;
                    if (is_irq_enabled(irq, mask) && test_pending(irq, mask) &&
                        (DISTIF.ITARGETS_SPI[idx] & mask) && !is_irq_active(irq, mask)) {
                        if (DISTIF.IPRIORITY_SPI[idx] < best_prio) {
                            best_prio = DISTIF.IPRIORITY_SPI[idx];
                            best_irq = irq;
                        }
                    }
                }

            } else {

                for (unsigned int lr_idx = 0; lr_idx < NLR; lr_idx++) {
                    if (VIFCTRL.is_lr_pending(lr_idx, cpu)) {
                        u8 prio = (VIFCTRL.LR.bank(cpu, lr_idx) & (0x1F << 23)) >> 23;
                        if (prio < best_prio) {
                            best_prio = prio;
                            best_irq = (VIFCTRL.LR.bank(cpu, lr_idx) & 0x1FF);
                        }
                    }
                }
            }

            // signal IRQ to processor if priority is higher
            bool level = false;
            if (!virt) {
                if (best_prio < CPUIF.PMR.bank(cpu)) {
                    log_debug("setting irq %u pending on cpuif %u", best_irq, cpu);
                    CPUIF.HPPIR.bank(cpu) = best_irq;
                    if (best_prio < CPUIF.RPR.bank(cpu))
                        level = true;
                }
            } else {
                if (best_prio < VCPUIF.PMR.bank(cpu)) {
                    VCPUIF.HPPIR.bank(cpu) = best_irq;
                    if (best_prio < VCPUIF.RPR.bank(cpu))
                        level = true;
                 }
            }

            if (!virt) {
                if (IRQ_OUT[cpu].read() != level)
                    log_debug("%sing %s[%u] for irq %u",
                              level ? "sett" : "clear", "IRQ", cpu, best_irq);
                IRQ_OUT[cpu].write(level); // FIRQ or NIRQ?
            } else {
                if(VIRQ_OUT[cpu].read() != level)
                    log_debug("%sing %s[%u] for irq %u",
                              level ? "sett" : "clear", "VIRQ", cpu, best_irq);
                VIRQ_OUT[cpu].write(level);
            }
        }
    }

    u8 gic400::get_irq_priority(unsigned int cpu, unsigned int irq) {
        if (irq < NSGI)
            return DISTIF.IPRIORITY_SGI.bank(cpu, irq);
        else if (irq < NPRIV)
            return DISTIF.IPRIORITY_PPI.bank(cpu, irq - NSGI);
        else if (irq < NIRQ)
            return DISTIF.IPRIORITY_SPI[irq - NPRIV];

        log_error("tried to get IRQ priority of invalid IRQ ID (%d)", irq);
        return 0;
    }

    void gic400::end_of_elaboration() {
        m_cpu_num = 0;
        m_irq_num = NPRIV;

        // determine the number of processors from the connected IRQs
        for (auto cpu : IRQ_OUT)
            m_cpu_num = max<unsigned int>(m_cpu_num, cpu.first + 1);

        for (auto spi : SPI_IN) {
            unsigned int irq = spi.first + NPRIV;

            if (irq >= NIRQ)
                VCML_ERROR("too many interrupts (%u)", irq);

            if (irq >= m_irq_num)
                m_irq_num = irq + 1;
        }

        log_debug("found %u cpus with %u irqs in total", m_cpu_num, m_irq_num);
        DISTIF.setup(m_cpu_num, m_irq_num);
    }

    void gic400::irq_transport(const irq_target_socket& socket,
                               irq_payload& irq) {
        switch (socket.as) {
        case IRQ_AS_PPI: {
            unsigned int cpu = PPI_IN.index_of(socket) / NPPI;
            unsigned int idx = PPI_IN.index_of(socket) % NPPI;
            handle_ppi(cpu, idx, irq);
            break;
        }

        case IRQ_AS_SPI: {
            unsigned int idx = SPI_IN.index_of(socket);
            handle_spi(idx, irq);
            break;
        }

        default:
            VCML_ERROR("illegal interrupt space: %d", (int)socket.as);
        }
    }

    void gic400::handle_ppi(unsigned int cpu, unsigned int idx,
                            irq_payload& tx) {
        unsigned int irq = NSGI + idx;
        unsigned int mask = 1 << cpu;

        set_irq_level(irq, tx.active, mask);
        set_irq_signaled(irq, false, gic400::ALL_CPU);
        if (get_irq_trigger(irq) == EDGE && tx.active)
            set_irq_pending(irq, true, mask);

        update();
    }

    void gic400::handle_spi(unsigned int idx, irq_payload& tx) {
        unsigned int irq = NPRIV + idx;
        unsigned int target_cpu = DISTIF.ITARGETS_SPI[idx];

        set_irq_level(irq, tx.active, gic400::ALL_CPU);
        set_irq_signaled(irq, false, gic400::ALL_CPU);
        if (get_irq_trigger(irq) == EDGE && tx.active)
            set_irq_pending(irq, true, target_cpu);

        update();
    }
}}
