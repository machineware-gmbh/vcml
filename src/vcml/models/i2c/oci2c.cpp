/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/i2c/oci2c.h"

namespace vcml {
namespace i2c {

void oci2c::write_ctr(u8 val) {
    if ((val & CTR_EN) && !(ctr & CTR_EN)) {
        u32 pre = (u32)prerhi << 8 | (u32)prerlo;

        m_hz = clk.read() / (pre + 1) / 5;
        log_debug("enabling device, prescale=0x%x, clock=%luhz", pre, m_hz);
    }

    ctr = val & CTR_MASK;
    update();
}

void oci2c::write_cmd(u8 val) {
    if (!(ctr & CTR_EN))
        return;

    if (val & CMD_IACK) {
        sr &= ~SR_IF;
        update();
    }

    sr |= SR_TIP;

    i2c_response ack = I2C_ACK;

    if ((val & (CMD_STA | CMD_WR)) == (CMD_STA | CMD_WR)) {
        ack = i2c.start(m_tx);
        sr |= SR_IF;
    } else if (val & CMD_STO) {
        ack = i2c.stop();
        sr |= SR_IF;
    } else if (val & CMD_RD) {
        ack = i2c.transport(m_rx);
        sr |= SR_IF;
    } else if (val & CMD_WR) {
        ack = i2c.transport(m_tx);
        sr |= SR_IF;
    }

    if (success(ack))
        sr &= ~SR_NACK;
    else
        sr |= SR_NACK;

    sr &= ~SR_TIP;
    update();
}

void oci2c::update() {
    irq = (ctr & CTR_EN) && (ctr & CTR_IEN) && (sr & SR_IF);
}

oci2c::oci2c(const sc_module_name& nm, u8 reg_shift):
    peripheral(nm),
    m_hz(0),
    m_tx(0xff),
    m_rx(0xff),
    prerlo("prerlo", 0u << reg_shift, 0xff),
    prerhi("prerhi", 1u << reg_shift, 0xff),
    ctr("ctr", 2u << reg_shift, 0x00),
    rxr("rxr", 3u << reg_shift, 0x00),
    sr("sr", 4u << reg_shift, 0x00),
    in("in"),
    irq("irq"),
    i2c("i2c") {
    ctr.sync_always();
    ctr.allow_read_write();
    ctr.on_write(&oci2c::write_ctr);

    rxr.sync_always();
    rxr.allow_read_write();
    rxr.on_read([&]() -> u8 { return m_rx; });
    rxr.on_write([&](u8 val) -> void { m_tx = val; });

    sr.sync_always();
    sr.allow_read_write();
    sr.on_write(&oci2c::write_cmd);
}

oci2c::~oci2c() {
    // nothing to do
}

void oci2c::reset() {
    m_hz = 0;
    m_tx = 0xff;
    m_rx = 0xff;
    peripheral::reset();
}

VCML_EXPORT_MODEL(vcml::i2c::oci2c, name, args) {
    if (args.empty())
        return new oci2c(name);

    u8 shift = from_string<u8>(args[0]);
    return new oci2c(name, shift);
}

} // namespace i2c
} // namespace vcml
