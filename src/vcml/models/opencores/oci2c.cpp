/******************************************************************************
 *                                                                            *
 * Copyright 2022 Jan Henrik Weinstock                                        *
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

#include "vcml/models/opencores/oci2c.h"

namespace vcml {
namespace opencores {

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

} // namespace opencores
} // namespace vcml
