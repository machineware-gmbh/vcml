/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/spi/pl022.h"

namespace vcml {
namespace spi {

using CR0_DSS = field<0, 4, u16>;
using CR0_FRF = field<4, 2, u16>;
using CR0_SCR = field<8, 8, u16>;

enum cr0_bits : u16 {
    CR0_SPO = bit(6),
    CR0_SPH = bit(7),
};

static constexpr u64 pl022_data_mask(u16 cr0) {
    return bitmask(get_field<CR0_DSS>(cr0) + 1);
}

static const char* pl022_frame_format(u16 frf) {
    switch (frf) {
    case 0b00:
        return "Motorola SPI";
    case 0b01:
        return "TI synchronous";
    case 0b10:
        return "National Microwire";
    case 0b11:
    default:
        return "undefined";
    }
}

static constexpr bool pl022_frame_is_tiformat(u16 cr0) {
    return get_field<CR0_FRF>(cr0) == 1;
}

enum cr1_bits : u16 {
    CR1_LBM = bit(0),
    CR1_SSE = bit(1),
    CR1_MS = bit(2),
    CR1_SOD = bit(3),
    CR1_MASK = CR1_LBM | CR1_SSE | CR1_MS | CR1_SOD,
};

enum sr_bits : u16 {
    SR_TFE = bit(0),
    SR_TNF = bit(1),
    SR_RNE = bit(2),
    SR_RFF = bit(3),
    SR_BSY = bit(4),
    SR_MASK = SR_TFE | SR_TNF | SR_RNE | SR_RFF | SR_BSY,
    SR_RESET = SR_TFE | SR_TNF,
};

using CPSR_CPSDVSR = field<0, 8, u16>;

enum irq_bits : u16 {
    IRQ_ROR = bit(0),
    IRQ_RT = bit(1),
    IRQ_RX = bit(2),
    IRQ_TX = bit(3),
    IRQ_MASK = IRQ_ROR | IRQ_RT | IRQ_RX | IRQ_TX,
};

enum dma_bits : u16 {
    DMACR_RXDMAE = bit(0),
    DMACR_TXDMAE = bit(1),
    DMACR_MASK = DMACR_RXDMAE | DMACR_TXDMAE,
};

void pl022::update_cs(bool active) {
    if (pl022_frame_is_tiformat(cr0)) {
        if (active)
            spi_cs.pulse();
    } else {
        spi_cs = !active; // pl022 is active low
    }
}

void pl022::update_irq() {
    sr.set_bit<SR_TFE>(m_txff.empty());
    sr.set_bit<SR_TNF>(!m_txff.full());
    sr.set_bit<SR_RNE>(!m_rxff.empty());
    sr.set_bit<SR_RFF>(m_rxff.full());

    // IRQ_ROR is handled in transmit() thread
    // IRQ_RT is not modelled
    ris.set_bit<IRQ_RX>(sr & SR_RNE);
    ris.set_bit<IRQ_TX>(sr & SR_TNF);

    mis = ris & imsc;
    intr = mis & IRQ_MASK;
    txintr = mis & IRQ_TX;
    rxintr = mis & IRQ_RX;
    rorintr = mis & IRQ_ROR;
    rtintr = mis & IRQ_RT;

    rxdmasreq = (cr1 & CR1_SSE) && (dmacr & DMACR_RXDMAE) &&
                (m_rxff.num_used() >= 1) && !rxdmasreq;
    rxdmasreq = (cr1 & CR1_SSE) && (dmacr & DMACR_RXDMAE) &&
                (m_rxff.num_used() >= 4) && !rxdmasreq;
    txdmasreq = (cr1 & CR1_SSE) && (dmacr & DMACR_TXDMAE) &&
                (m_txff.num_free() >= 1) && !txdmasreq;
    txdmasreq = (cr1 & CR1_SSE) && (dmacr & DMACR_TXDMAE) &&
                (m_txff.num_free() >= 4) && !txdmasreq;
}

void pl022::update_sclk() {
    size_t div = cpsr.get_field<CPSR_CPSDVSR>() & ~1u;
    size_t scr = cr0.get_field<CR0_SCR>();
    sclk = clk / (div * (1 + scr));
}

void pl022::transmit() {
    while (true) {
        wait(m_ev);

        update_sclk();
        update_cs(true);
        sr |= SR_BSY;

        while (!m_txff.empty() && (cr1 & CR1_SSE)) {
            spi_payload tx;
            tx.mosi = m_txff.pop();
            tx.mask = pl022_data_mask(cr0);
            wait(sc_time(1.0 / sclk, SC_SEC));

            if (cr1 & CR1_LBM)
                tx.miso = tx.mosi;
            else
                spi_out->spi_transport(tx);

            if (!m_rxff.full())
                m_rxff.push(tx.miso);
            else
                ris |= IRQ_ROR;
        }

        sr &= ~SR_BSY;
        update_cs(false);
        update_irq();
    }
}

u16 pl022::read_dr(bool debug) {
    if (m_rxff.empty())
        return 0;

    if (debug)
        return m_rxff.top();

    u16 val = m_rxff.pop();
    update_irq();
    return val;
}

u16 pl022::read_mis(bool debug) {
    if (!debug)
        update_irq();
    return ris & imsc;
}

void pl022::write_cr0(u16 val, bool debug) {
    u32 dss = cr0.get_field<CR0_DSS>();
    u32 frf = cr0.get_field<CR0_FRF>();
    u32 scr = cr0.get_field<CR0_SCR>();
    bool spo = cr0 & CR0_SPO;
    bool sph = cr0 & CR0_SPH;

    cr0 = val;

    if (cr0.get_field<CR0_DSS>() != dss) {
        u32 bits = cr0.get_field<CR0_DSS>() + 1;
        log_debug("data size updated: %u bits", bits);
    }

    if (cr0.get_field<CR0_FRF>() != frf) {
        u32 format = cr0.get_field<CR0_FRF>();
        log_debug("frame formate updated: %s", pl022_frame_format(format));
        if (!debug)
            update_cs(false);
    }

    if (!!(cr0 & CR0_SPO) != spo)
        log_debug("sclock polarity %s", spo ? "low" : "high");
    if (!!(cr0 & CR0_SPH) != sph)
        log_debug("sclock phase %s", sph ? "low" : "high");

    if (cr0.get_field<CR0_SCR>() != scr) {
        u32 div = cr0.get_field<CR0_SCR>();
        if (!debug)
            update_sclk();
        log_debug("serial clock updated: %u (%zuHz)", div, sclk.get());
    }
}

void pl022::write_cr1(u16 val, bool debug) {
    bool lbm = cr1 & CR1_LBM;
    bool sse = cr1 & CR1_SSE;
    bool ms = cr1 & CR1_MS;
    bool sod = cr1 & CR1_SOD;

    cr1 = val & CR1_MASK;

    if (!!(cr1 & CR1_SSE) != sse)
        log_debug("controller %sabled", sse ? "dis" : "en");
    if (!!(cr1 & CR1_LBM) != lbm)
        log_debug("loopback mode %sabled", lbm ? "dis" : "en");
    if (!!(cr1 & CR1_MS) != ms)
        log_debug("%s mode selected", ms ? "master" : "slave");
    if (!!(cr1 & CR1_SOD) != sod)
        log_debug("slave output %sabled", sod ? "en" : "dis");

    if (cr1 & CR1_MS) {
        log_warn("spi slave mode not implemented");
        cr1 &= ~CR1_MS;
    }
}

void pl022::write_dr(u16 val, bool debug) {
    if (m_txff.full()) {
        log_warn("transmission FIFO full, data dropped");
        return;
    }

    m_txff.push(val);

    if (!debug) {
        update_irq();
        m_ev.notify(SC_ZERO_TIME);
    }
}

void pl022::write_cpsr(u16 val, bool debug) {
    u16 newdiv = get_field<CPSR_CPSDVSR>(val);
    u16 olddiv = get_field<CPSR_CPSDVSR>(cpsr);

    if (newdiv < 2)
        newdiv = 2;
    if (newdiv > 254)
        newdiv = 254;

    if (newdiv != olddiv) {
        sr.set_field<CPSR_CPSDVSR>(newdiv);
        if (!debug)
            m_ev.notify(SC_ZERO_TIME);
    }
}

void pl022::write_imsc(u16 val, bool debug) {
    imsc = val & IRQ_MASK;
    if (!debug)
        update_irq();
}

void pl022::write_icr(u16 val, bool debug) {
    ris &= ~(val & (IRQ_ROR | IRQ_RT));

    if (!debug)
        update_irq();
}

void pl022::write_dmacr(u16 val, bool debug) {
    dmacr = val & DMACR_MASK;

    if (!debug)
        update_irq();
}

pl022::pl022(const sc_module_name& nm):
    peripheral(nm),
    m_ev("ev"),
    m_txff(8),
    m_rxff(8),
    cr0("cr0", 0x00, 0x0000),
    cr1("cr1", 0x04, 0x0000),
    dr("dr", 0x08, 0x0000),
    sr("sr", 0x0c, SR_RESET),
    cpsr("cpsr", 0x10, 0x0002),
    imsc("imsc", 0x14, 0x0000),
    ris("ris", 0x18, IRQ_RX),
    mis("mis", 0x1c, 0x0000),
    icr("icr", 0x20, 0x0000),
    dmacr("dmacr", 0x24, 0x0000),
    pid("pid", 0xfe0, { 0x22, 0x10, 0x24, 0x00 }),
    cid("cid", 0xff0, { 0x0d, 0xf0, 0x05, 0xb1 }),
    txintr("txintr"),
    rxintr("rxintr"),
    rorintr("rorintr"),
    rtintr("rtintr"),
    intr("intr"),
    rxdmasreq("rxdmasreq"),
    rxdmabreq("rxdmabreq"),
    rxdmaclr("rxdmaclr"),
    txdmasreq("txdmasreq"),
    txdmabreq("txdmabreq"),
    txdmaclr("txdmaclr"),
    sclk("sclk"),
    spi_out("spi_out"),
    spi_cs("spi_cs"),
    in("in") {
    cr0.sync_always();
    cr0.allow_read_write();
    cr0.on_write(&pl022::write_cr0);

    cr1.sync_always();
    cr1.allow_read_write();
    cr1.on_write(&pl022::write_cr1);

    dr.sync_always();
    dr.allow_read_write();
    dr.on_read(&pl022::read_dr);
    dr.on_write(&pl022::write_dr);

    sr.sync_always();
    sr.allow_read_only();

    cpsr.sync_always();
    cpsr.allow_read_write();
    cpsr.on_write(&pl022::write_cpsr);

    imsc.sync_always();
    imsc.allow_read_write();
    imsc.on_write(&pl022::write_imsc);

    ris.sync_always();
    ris.allow_read_only();

    mis.sync_always();
    mis.allow_read_only();
    mis.on_read(&pl022::read_mis);

    icr.sync_always();
    icr.allow_read_write();
    icr.on_write(&pl022::write_icr);

    dmacr.sync_always();
    dmacr.allow_read_write();
    dmacr.on_write(&pl022::write_dmacr);

    pid.sync_never();
    pid.allow_read_only();

    cid.sync_never();
    cid.allow_read_only();

    SC_HAS_PROCESS(pl022);
    SC_THREAD(transmit);
}

pl022::~pl022() {
    // nothing to do
}

void pl022::reset() {
    peripheral::reset();

    m_txff.reset();
    m_rxff.reset();

    update_sclk();
    update_irq();
}

void pl022::handle_clock_update(hz_t oldclk, hz_t newclk) {
    peripheral::handle_clock_update(oldclk, newclk);
    m_ev.notify(SC_ZERO_TIME);
}

void pl022::before_end_of_elaboration() {
    if (!txintr.is_bound())
        txintr.stub();
    if (!rxintr.is_bound())
        rxintr.stub();
    if (!rorintr.is_bound())
        rorintr.stub();
    if (!rtintr.is_bound())
        rtintr.stub();

    if (!rxdmasreq.is_bound() && !rxdmabreq && !rxdmaclr.is_bound()) {
        rxdmasreq.stub();
        rxdmabreq.stub();
        rxdmaclr.stub();
    }

    if (!txdmasreq.is_bound() && !txdmabreq && !txdmaclr.is_bound()) {
        txdmasreq.stub();
        txdmabreq.stub();
        txdmaclr.stub();
    }
}

VCML_EXPORT_MODEL(vcml::spi::pl022, name, args) {
    return new pl022(name);
}

} // namespace spi
} // namespace vcml
