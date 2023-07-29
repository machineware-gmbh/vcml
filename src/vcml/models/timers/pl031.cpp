/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/timers/pl031.h"

namespace vcml {
namespace timers {

enum cr_bits : u32 {
    CR_ENABLE = bit(0),
};

u32 pl031::read_dr() {
    return cr & CR_ENABLE ? m_offset + time_to_sec(sc_time_stamp()) : 0;
}

void pl031::write_mr(u32 val) {
    mr = val;
    update();
}

void pl031::write_lr(u32 val) {
    m_offset += val - read_dr();
    lr = val;
    update();
}

void pl031::write_cr(u32 val) {
    VCML_LOG_REG_BIT_CHANGE(CR_ENABLE, cr, val);
    cr = val & CR_ENABLE;
    if (!(cr & CR_ENABLE))
        m_offset = 0;
    update();
}

void pl031::write_imsc(u32 val) {
    imsc = !!val;
    update();
}

void pl031::write_icr(u32 val) {
    if (val & 1)
        ris = 0;
    update();
}

void pl031::update() {
    m_notify.cancel();

    u32 next = mr - read_dr();
    if (next == 0) {
        ris = 1;
    } else {
        m_notify.notify(next, SC_SEC);
    }

    irq = (ris & imsc) && (cr & CR_ENABLE);
}

pl031::pl031(const sc_module_name& nm):
    peripheral(nm),
    m_offset(time(NULL)),
    m_notify("notify"),
    dr("dr", 0x0),
    mr("mr", 0x4),
    lr("lr", 0x8),
    cr("cr", 0xc, CR_ENABLE),
    imsc("imsc", 0x10),
    ris("ris", 0x14),
    mis("mis", 0x18),
    icr("icr", 0x1c),
    pid("pid", 0xfe0),
    cid("cid", 0xff0),
    in("in"),
    irq("irq") {
    dr.sync_always();
    dr.allow_read_only();
    dr.on_read(&pl031::read_dr);

    mr.sync_always();
    mr.allow_read_write();
    mr.on_write(&pl031::write_mr);

    lr.sync_always();
    lr.allow_read_write();
    lr.on_write(&pl031::write_lr);

    cr.sync_always();
    cr.allow_read_write();
    cr.on_write(&pl031::write_cr);

    imsc.sync_always();
    imsc.allow_read_write();
    imsc.on_write(&pl031::write_imsc);

    ris.sync_always();
    ris.allow_read_only();

    mis.sync_always();
    mis.allow_read_only();
    mis.on_read([&]() -> u32 { return ris & imsc; });

    icr.sync_always();
    icr.allow_write_only();
    icr.on_write(&pl031::write_icr);

    SC_HAS_PROCESS(pl031);
    SC_METHOD(update);
    sensitive << m_notify;
    dont_initialize();
}

pl031::~pl031() {
    // nothing to do
}

void pl031::reset() {
    peripheral::reset();

    for (size_t i = 0; i < pid.count(); i++)
        pid[i] = (AMBA_PID >> (i * 8)) & 0xff;

    for (size_t i = 0; i < cid.count(); i++)
        cid[i] = (AMBA_CID >> (i * 8)) & 0xff;

    update();
};

VCML_EXPORT_MODEL(vcml::timers::pl031, name, args) {
    return new pl031(name);
}

} // namespace timers
} // namespace vcml
