/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/arm/arch_timer.h"

namespace vcml {
namespace arm {

enum cnttidr_bits : u32 {
    CNTTIDR_PRESENT = bit(0),
    CNTTIDR_VIRTUAL = bit(1),
};

enum cntx_ctl_bits : u32 {
    CNTX_CTL_ENABLE = bit(0),
    CNTX_CTL_IMASK = bit(1),
    CNTX_CTL_ISTATUS = bit(2),
    CNTX_CTL_RWMASK = CNTX_CTL_ENABLE | CNTX_CTL_IMASK,
};

constexpr u32 cnttidr_default(size_t num_timers) {
    u32 cnttidr = 0;
    for (size_t i = 0; i < arch_timer::MAX_FRAMES; i++)
        cnttidr |= (CNTTIDR_PRESENT | CNTTIDR_VIRTUAL) << (4 * i);
    return cnttidr;
}

u32 arch_timer::cntframe::read_cntp_tval() {
    if (!(cntp_ctl & CNTX_CTL_ENABLE))
        return 0;

    u64 cnt = m_parent.get_phys_counter();
    u64 cmp = cntp_cval;

    return cnt <= cmp ? cmp - cnt : 0;
}

u32 arch_timer::cntframe::read_cntv_tval() {
    if (!(cntv_ctl & CNTX_CTL_ENABLE))
        return 0;

    u64 cnt = m_parent.get_virt_counter(m_idx);
    u64 cmp = cntv_cval;

    return cnt <= cmp ? cmp - cnt : 0;
}

void arch_timer::cntframe::write_cntp_cval(u64 val) {
    cntp_cval = val;
    update();
}

void arch_timer::cntframe::write_cntp_tval(u32 val) {
    cntp_tval = val;
    cntp_cval = m_parent.get_phys_counter() + val;
    update();
}

void arch_timer::cntframe::write_cntp_ctl(u32 val) {
    cntp_ctl = val & CNTX_CTL_RWMASK;
    update();
}

void arch_timer::cntframe::write_cntv_cval(u64 val) {
    cntv_cval = val;
    update();
}

void arch_timer::cntframe::write_cntv_tval(u32 val) {
    cntv_tval = val;
    cntv_cval = m_parent.get_virt_counter(m_idx) + val;
    update();
}

void arch_timer::cntframe::write_cntv_ctl(u32 val) {
    cntv_ctl = val & CNTX_CTL_RWMASK;
    update();
}

arch_timer::cntframe::cntframe(const sc_module_name& nm, arch_timer& parent,
                               size_t idx):
    peripheral(nm),
    m_idx(idx),
    m_parent(parent),
    m_trigger("trigger"),
    cntpct("cntpct", 0x00),
    cntvct("cntvct", 0x08),
    cntfrq("cntfrq", 0x10),
    cntel0acr("cntel0acr", 0x14),
    cntvoff("cntvoff", 0x18),
    cntp_cval("cntp_cval", 0x20),
    cntp_tval("cntp_tval", 0x28),
    cntp_ctl("cntp_ctl", 0x2c),
    cntv_cval("cntv_cval", 0x30),
    cntv_tval("cntv_tval", 0x38),
    cntv_ctl("cntv_ctl", 0x3c),
    in("in"),
    irq_phys("irq_phys"),
    irq_virt("irq_virt") {
    cntpct.sync_always();
    cntpct.allow_read_only();
    cntpct.on_read([&]() -> u64 { return m_parent.get_phys_counter(); });

    cntvct.sync_always();
    cntvct.allow_read_only();
    cntvct.on_read([&]() -> u64 { return m_parent.get_virt_counter(m_idx); });

    cntfrq.sync_always();
    cntfrq.allow_read_only();
    cntfrq.on_read([&]() -> u64 { return m_parent.cntfrq; });

    cntvoff.sync_always();
    cntvoff.allow_read_only();
    cntvoff.on_read([&]() -> u64 { return m_parent.cntvoff[m_idx]; });

    cntp_cval.sync_always();
    cntp_cval.allow_read_write();
    cntp_cval.on_write(&cntframe::write_cntp_cval);

    cntp_tval.sync_always();
    cntp_tval.allow_read_write();
    cntp_tval.on_read(&cntframe::read_cntp_tval);
    cntp_tval.on_write(&cntframe::write_cntp_tval);

    cntp_ctl.sync_always();
    cntp_ctl.allow_read_write();
    cntp_ctl.on_write(&cntframe::write_cntp_ctl);

    cntv_cval.sync_always();
    cntv_cval.allow_read_write();
    cntv_cval.on_write(&cntframe::write_cntv_cval);

    cntv_tval.sync_always();
    cntv_tval.allow_read_write();
    cntv_tval.on_read(&cntframe::read_cntv_tval);
    cntv_tval.on_write(&cntframe::write_cntv_tval);

    cntv_ctl.sync_always();
    cntv_ctl.allow_read_write();
    cntv_ctl.on_write(&cntframe::write_cntv_ctl);

    SC_HAS_PROCESS(arch_timer::cntframe);
    SC_METHOD(update);
    sensitive << m_trigger;
}

void arch_timer::cntframe::update() {
    cntp_ctl &= ~CNTX_CTL_ISTATUS;
    cntv_ctl &= ~CNTX_CTL_ISTATUS;

    u64 pct = m_parent.get_phys_counter();
    u64 vct = m_parent.get_virt_counter(m_idx);

    if (cntp_ctl & CNTX_CTL_ENABLE) {
        if (pct >= cntp_cval)
            cntp_ctl |= CNTX_CTL_ISTATUS;
        else
            m_trigger.notify(clock_cycles(cntp_cval - pct));
    }

    if (cntv_ctl & CNTX_CTL_ENABLE) {
        if (vct >= cntv_cval)
            cntv_ctl |= CNTX_CTL_ISTATUS;
        else
            m_trigger.notify(clock_cycles(cntv_cval - vct));
    }

    irq_phys = (cntp_ctl & CNTX_CTL_ISTATUS) && !(cntp_ctl & CNTX_CTL_IMASK);
    irq_virt = (cntv_ctl & CNTX_CTL_ISTATUS) && !(cntv_ctl & CNTX_CTL_IMASK);
}

u64 arch_timer::get_phys_counter() {
    return sc_time_stamp().to_seconds() * clk;
}

u64 arch_timer::get_virt_counter(size_t idx) {
    return get_phys_counter() - cntvoff[idx];
}

void arch_timer::write_cntvoff(u64 val, size_t idx) {
    cntvoff[idx] = val;
    if (idx < frames.size())
        frames[idx].update();
}

arch_timer::arch_timer(const sc_module_name& nm, size_t n):
    peripheral(nm),
    nframes("nframes", n),
    frames("frames", nframes,
           [&](const char* n, size_t idx) -> arch_timer::cntframe* {
               string nm = mkstr("frame%zu", idx);
               return new arch_timer::cntframe(nm.c_str(), *this, idx);
           }),
    cntfrq("cntfrq", 0x0),
    cntnsar("cntnsar", 0x4),
    cnttidr("cnttidr", 0x8, cnttidr_default(nframes)),
    cntacr("cntacr", 0x40),
    cntvoff("cntvoff", 0x80),
    timer_in("timer_in"),
    frame_in("frame_in"),
    irq_phys("irq_phys"),
    irq_virt("irq_virt") {
    VCML_ERROR_ON(nframes > MAX_FRAMES, "too many frames: %zu", nframes.get());

    cntfrq.sync_always();
    cntfrq.allow_read_write();

    cntnsar.sync_always();
    cntnsar.allow_read_write();

    cnttidr.sync_never();
    cnttidr.allow_read_only();

    cntacr.sync_always();
    cntacr.allow_read_write();

    cntvoff.sync_always();
    cntvoff.allow_read_write();
    cntvoff.on_write(&arch_timer::write_cntvoff);

    for (size_t i = 0; i < nframes; i++) {
        rst.bind(frames[i].rst);
        clk.bind(frames[i].clk);
        frame_in[i].bind(frames[i].in);
        frames[i].irq_phys.bind(irq_phys[i]);
        frames[i].irq_virt.bind(irq_virt[i]);
    }
}

} // namespace arm
} // namespace vcml
