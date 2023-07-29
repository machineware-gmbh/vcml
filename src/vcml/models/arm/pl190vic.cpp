/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/arm/pl190vic.h"

namespace vcml {
namespace arm {

void pl190vic::update() {
    risr = sint | m_ext_irq;    // update raw interrupt status
    fiqs = risr & inte & ints;  // update FIQ status
    irqs = risr & inte & ~ints; // update IRQ status

    for (auto irq : irq_out)
        irq.second->write(irqs != 0u);

    for (auto fiq : fiq_out)
        fiq.second->write(fiqs != 0u);

    for (unsigned int l = 0; l < vctrl.count(); l++) {
        if (vctrl[l] & VCTRL_ENABLED) {
            u32 source = vctrl[l] & VCTRL_SOURCE_M;
            u32 srcmsk = 1 << source;
            if (irqs & srcmsk) {
                addr = vaddr[l];
                m_current_irq = source;
                m_vect_int = true;
            }
        }
    }
}

void pl190vic::write_inte(u32 val) {
    inte |= val; // set hardware interrupt
    update();
}

void pl190vic::write_iecr(u32 val) {
    inte &= ~val; // clear hardware interrupt
    update();
}

void pl190vic::write_sint(u32 val) {
    sint |= val; // set software interrupt
    update();
}

void pl190vic::write_sicr(u32 val) {
    sint &= ~val; // clear software interrupt
    update();
}

void pl190vic::write_addr(u32 val) {
    if (m_vect_int) { // write indicates EOI, value not important
        inte &= ~(1 << m_current_irq);
        m_vect_int = false;
        update();
    }

    vaddr = val;
}

void pl190vic::write_vctrl(u32 val, size_t idx) {
    vctrl = val & VCTRL_M;
}

pl190vic::pl190vic(const sc_module_name& nm):
    peripheral(nm),
    m_ext_irq(0),
    m_current_irq(0xff),
    m_vect_int(false),
    irqs("irqs", 0x000),
    fiqs("fiqs", 0x004),
    risr("risr", 0x008),
    ints("ints", 0x00c),
    inte("inte", 0x010),
    iecr("iecr", 0x014),
    sint("sint", 0x018),
    sicr("sicr", 0x01c),
    prot("prot", 0x020),
    addr("addr", 0x030),
    defa("defa", 0x034),
    vaddr("vaddr", 0x100),
    vctrl("vctrl", 0x200),
    pid("pid", 0xfe0),
    cid("cid", 0xff0),
    in("in"),
    irq_in("irq_in", NIRQ),
    irq_out("irq_out"),
    fiq_out("fiq_out") {
    irqs.allow_read_only();
    fiqs.allow_read_only();
    risr.allow_read_only();

    ints.allow_read_write();

    inte.allow_read_write();
    inte.on_write(&pl190vic::write_inte);

    iecr.allow_read_write();
    iecr.on_write(&pl190vic::write_iecr);

    sint.allow_read_write();
    sint.on_write(&pl190vic::write_sint);

    sicr.allow_write_only();
    sicr.on_write(&pl190vic::write_sicr);

    prot.allow_read_write(); // not implemented

    addr.allow_read_write();
    addr.on_write(&pl190vic::write_addr);

    defa.allow_read_write();

    vaddr.allow_read_write();

    vctrl.allow_read_write();
    vctrl.on_write(&pl190vic::write_vctrl);

    pid.allow_read_only();
    cid.allow_read_only();
}

pl190vic::~pl190vic() {
    // nothing to do
}

void pl190vic::reset() {
    for (unsigned int i = 0; i < pid.count(); i++)
        pid[i] = (AMBA_PID >> (i * 8)) & 0xff;

    for (unsigned int i = 0; i < cid.count(); i++)
        cid[i] = (AMBA_CID >> (i * 8)) & 0xff;
}

void pl190vic::gpio_notify(const gpio_target_socket& socket) {
    unsigned int nirq = irq_in.index_of(socket);
    const u32 mask = 1 << nirq;

    if (socket.read()) {
        m_ext_irq |= mask;
        log_debug("setting IRQ %u", nirq);
    } else {
        m_ext_irq &= ~mask;
        log_debug("cleared IRQ %u", nirq);
    }

    update();
}

VCML_EXPORT_MODEL(vcml::arm::pl190, name, args) {
    return new pl190vic(name);
}

} // namespace arm
} // namespace vcml
