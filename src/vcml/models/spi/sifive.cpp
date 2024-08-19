/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/spi/sifive.h"

namespace vcml {
namespace spi {

using SCKDIV = field<0, 12, u32>;

enum sckmode_bits : u32 {
    SCKMODE_PHA = bit(0),
    SCKMODE_POL = bit(1),
    SCKMODE_MASK = SCKMODE_PHA | SCKMODE_POL,
};

using CSMODE = field<0, 2, u32>;

enum csmode : u32 {
    CSMODE_AUTO = 0,
    CSMODE_HOLD = 2,
    CSMODE_OFF = 3,
};

using DELAY0_CSSCK = field<0, 8, u32>;
using DELAY0_SCKCS = field<16, 8, u32>;
using DELAY1_INTERCS = field<0, 8, u32>;
using DELAY1_INTERXFR = field<16, 8, u32>;

enum delay_bits : u32 {
    DELAY0_MASK = DELAY0_CSSCK::MASK | DELAY0_SCKCS::MASK,
    DELAY1_MASK = DELAY1_INTERCS::MASK | DELAY1_INTERXFR::MASK,
};

using FMT_PROTO = field<0, 2, u32>;
using FMT_LEN = field<16, 4, u32>;

enum fmt_bits : u32 {
    FMT_LSB = bit(2),
    FMT_DIR = bit(3),
    FMT_MASK = FMT_PROTO::MASK | FMT_LSB | FMT_DIR | FMT_LEN::MASK,
};

enum csproto : u32 {
    CSPROTO_SINGLE = 0,
    CSPROTO_DUAL = 1,
    CSPROTO_QUAD = 2,
};

enum rxtx_data_bits : u32 {
    TXFIFO_FULL = bit(31),
    RXFIFO_EMPTY = bit(31),
};

constexpr size_t FIFO_CAPACITY = 8;

using TXMARK = field<0, 3, u32>;
using RXMARK = field<0, 3, u32>;

enum irq_bits : u32 {
    IRQ_TXWM = bit(0),
    IRQ_RXWM = bit(1),
};

enum fctrl_bits : u32 {
    FCTRL_EN = bit(0),
};

using FFMT_ADDR_LEN = field<1, 3, u32>;
using FFMT_PAD_CNT = field<4, 4, u32>;
using FFMT_CMD_PROTO = field<8, 2, u32>;
using FFMT_ADDR_PROTO = field<10, 2, u32>;
using FFMT_DATA_PROTO = field<12, 2, u32>;
using FFMT_CMD_CODE = field<16, 8, u32>;
using FFMT_PAD_CODE = field<24, 8, u32>;

enum ffmt_bits : u32 {
    FFMT_CMD_EN = bit(0),
    FFMT_MASK = FFMT_CMD_EN | FFMT_ADDR_LEN::MASK | FFMT_PAD_CNT::MASK |
                FFMT_CMD_PROTO::MASK | FFMT_ADDR_PROTO::MASK |
                FFMT_DATA_PROTO::MASK | FFMT_CMD_CODE::MASK |
                FFMT_PAD_CODE::MASK,
};

void sifive::write_sckdiv(u32 val) {
    sckdiv = val & SCKDIV::MASK;
    m_ev.notify();
}

constexpr u64 csid_mask(size_t numcs) {
    if (numcs == 0)
        return 0;
    return bitmask(64 - clz(numcs - 1));
}

void sifive::write_csid(u32 val) {
    u32 newcsid = val & csid_mask(numcs);
    if (newcsid >= numcs) {
        log_warn("invalid csid ignored: %u", val);
        return;
    }

    if (newcsid != csid) {
        csid = newcsid;
        update_cs(false);
    }
}

void sifive::write_csdef(u32 val) {
    if (csdef != val) {
        csdef = val;
        update_cs(false);
    }
}

void sifive::write_csmode(u32 val) {
    val &= CSMODE::MASK;
    if (csmode != val) {
        csmode = val;
        update_cs(false);
    }
}

static string mode_string(u32 fmt) {
    static const char* proto[] = { "single", "dual", "quad", "unknown" };
    const char* lsb = fmt & FMT_LSB ? "lsb" : "msb";
    const char* dir = fmt & FMT_DIR ? "tx" : "tx+rx";
    return mkstr("%s-mode, %s-first, %s, %u-bits-per-word",
                 proto[get_field<FMT_PROTO>(fmt)], lsb, dir,
                 get_field<FMT_LEN>(fmt));
}

void sifive::write_fmt(u32 val) {
    val = val & FMT_MASK;
    if (fmt != val) {
        log_debug("detected operation mode change");
        log_debug("%s (old)", mode_string(fmt).c_str());
        log_debug("%s (new)", mode_string(val).c_str());
        fmt = val;
    }
}

void sifive::write_txdata(u32 val) {
    if (m_txff.full()) {
        log_warn("txfifo full, data dropped");
        return;
    }

    m_txff.push(val);
    m_ev.notify(SC_ZERO_TIME);
    update_irq();
}

void sifive::write_txmark(u32 val) {
    txmark = val & TXMARK::MASK;
    update_irq();
}

void sifive::write_rxmark(u32 val) {
    rxmark = val & RXMARK::MASK;
    update_irq();
}

void sifive::write_ie(u32 val) {
    ie = val & (IRQ_TXWM | IRQ_RXWM);
    update_irq();
}

u32 sifive::read_txdata() {
    if (m_txff.full())
        return TXFIFO_FULL;
    return 0;
}

u32 sifive::read_rxdata() {
    if (m_rxff.empty())
        return RXFIFO_EMPTY;

    u32 val = m_rxff.pop();
    update_irq();
    return val;
}

void sifive::update_cs(bool set) {
    for (size_t i = 0; i < numcs; i++) {
        if (cs.exists(i)) {
            u32 mask = bit(i);
            cs[i] = !!(csdef & mask) ^ (i == csid && set);
        }
    }
}

void sifive::update_sclk() {
    u32 div = sckdiv.get_field<SCKDIV>();
    sclk = clk / (2 * (div + 1));
}

void sifive::update_irq() {
    if (m_txff.num_used() < txmark.get_field<TXMARK>())
        ip |= IRQ_TXWM;
    else
        ip &= ~IRQ_TXWM;

    if (m_rxff.num_used() > rxmark.get_field<RXMARK>())
        ip |= IRQ_RXWM;
    else
        ip &= ~IRQ_RXWM;

    irq = ie & ip;
}

constexpr u8 format_data(u8 data, u32 fmt) {
    size_t len = get_field<FMT_LEN>(fmt);
    if (len > 8)
        len = 8;
    return data & bitmask(len);
}

void sifive::transmit() {
    while (true) {
        wait(m_ev);

        update_sclk();

        while (!m_txff.empty()) {
            u32 mode = csmode.get_field<CSMODE>();
            if (mode != CSMODE_OFF)
                update_cs(true);

            u8 data = format_data(m_txff.pop(), fmt);
            spi_payload tx(data);
            spi_out.transport(tx);

            if (!m_rxff.full() && !(fmt & FMT_DIR))
                m_rxff.push(tx.miso);

            if (mode == CSMODE_AUTO)
                update_cs(false);
        }

        update_irq();
    }
}

sifive::sifive(const sc_module_name& nm):
    peripheral(nm),
    m_txff(FIFO_CAPACITY),
    m_rxff(FIFO_CAPACITY),
    numcs("numcs", 4),
    sckdiv("sckdiv", 0x00, 3),
    sckmode("sckmode", 0x04, 0),
    csid("csid", 0x10, 0),
    csdef("csdef", 0x14, 0xffffffff),
    csmode("csmode", 0x18, 0),
    delay0("delay0", 0x28, 0x00010001),
    delay1("delay1", 0x2c, 0000000001),
    fmt("fmt", 0x40, 0x00080008),
    txdata("txdata", 0x48, 0),
    rxdata("rxdata", 0x4c, 0),
    txmark("txmark", 0x50, 0),
    rxmark("rxmark", 0x54, 0),
    fctrl("fctrl", 0x60, FCTRL_EN),
    ffmt("ffmt", 0x64, 0x00030007),
    ie("ie", 0x70, 0),
    ip("ip", 0x74, 0),
    sclk("sclk"),
    cs("cs", numcs),
    irq("irq"),
    spi_out("spi_out"),
    in("in") {
    sckdiv.sync_always();
    sckdiv.allow_read_write();
    sckdiv.on_write(&sifive::write_sckdiv);

    sckmode.sync_never();
    sckmode.allow_read_write();
    sckmode.on_write_mask(SCKMODE_MASK);

    csid.sync_always();
    csid.allow_read_write();
    csid.on_write(&sifive::write_csid);

    csdef.sync_always();
    csdef.allow_read_write();
    csdef.on_write(&sifive::write_csdef);

    csmode.sync_always();
    csmode.allow_read_write();
    csmode.on_write(&sifive::write_csmode);

    delay0.sync_never();
    delay0.allow_read_write();
    delay0.on_write_mask(DELAY0_MASK);

    delay1.sync_never();
    delay1.allow_read_write();
    delay1.on_write_mask(DELAY1_MASK);

    fmt.sync_never();
    fmt.allow_read_write();
    fmt.on_write(&sifive::write_fmt);

    txdata.sync_always();
    txdata.allow_read_write();
    txdata.on_read(&sifive::read_txdata);
    txdata.on_write(&sifive::write_txdata);

    rxdata.sync_always();
    rxdata.allow_read_only();
    rxdata.on_read(&sifive::read_rxdata);

    txmark.sync_always();
    txmark.allow_read_write();
    txmark.on_write(&sifive::write_txmark);

    rxmark.sync_always();
    rxmark.allow_read_write();
    rxmark.on_write(&sifive::write_rxmark);

    fctrl.sync_never();
    fctrl.allow_read_write();
    fctrl.on_write_mask(FCTRL_EN);

    ffmt.sync_never();
    ffmt.allow_read_write();
    ffmt.on_write_mask(FFMT_MASK);

    ie.sync_always();
    ie.allow_read_write();
    ie.on_write(&sifive::write_ie);

    ip.sync_always();
    ip.allow_read_only();

    SC_HAS_PROCESS(sifive);
    SC_THREAD(transmit);
}

sifive::~sifive() {
    // nothing to do
}

void sifive::reset() {
    peripheral::reset();

    m_txff.reset();
    m_rxff.reset();

    update_cs(false);
    update_sclk();
    update_irq();
}

void sifive::handle_clock_update(hz_t oldclk, hz_t newclk) {
    peripheral::handle_clock_update(oldclk, newclk);
    m_ev.notify(SC_ZERO_TIME);
}

VCML_EXPORT_MODEL(vcml::spi::sifive, name, args) {
    return new sifive(name);
}

} // namespace spi
} // namespace vcml
