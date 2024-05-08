/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/serial/uart_base.h"

namespace vcml {
namespace serial {

static serial_bits uart8250_data_bits(u8 lcr) {
    return (serial_bits)(SERIAL_5_BITS + (lcr & 3));
}

static serial_stop uart8250_stop_bits(u8 lcr) {
    if (lcr & uart_base::LCR_SPB)
        return (lcr & 3) ? SERIAL_STOP_2 : SERIAL_STOP_1_5;
    return SERIAL_STOP_1;
}

static serial_parity uart8250_parity(u8 lcr) {
    if (!(lcr & uart_base::LCR_PEN))
        return SERIAL_PARITY_NONE;

    if (!(lcr & uart_base::LCR_SPB)) {
        if (lcr & uart_base::LCR_EPS)
            return SERIAL_PARITY_EVEN;
        return SERIAL_PARITY_ODD;
    }

    if (lcr & uart_base::LCR_EPS)
        return SERIAL_PARITY_MARK;
    return SERIAL_PARITY_SPACE;
}

void uart_base::calibrate() {
    if (m_divisor == 0) {
        log_warn("zero baud divisor specified, reverting to default");
        m_divisor = clock_hz() / (16 * DEFAULT_BAUD);
    }

    baud_t baud = clock_hz() / (m_divisor * 16);
    log_debug("setup divisor %hu (%zu baud)", m_divisor, baud);
    serial_tx.set_baud(baud);
    serial_tx.set_data_width(uart8250_data_bits(lcr));
    serial_tx.set_stop_bits(uart8250_stop_bits(lcr));
    serial_tx.set_parity(uart8250_parity(lcr));
}

void uart_base::update() {
    // update status
    lsr.set_bit<LSR_TEMT>(m_tx_fifo.empty());
    lsr.set_bit<LSR_THRE>(m_tx_fifo.size() < m_tx_size);
    lsr.set_bit<LSR_DR>(!m_rx_fifo.empty());

    // update interrupt
    iir.set_bit<IRQ_RLS>(lsr & (LSR_OE | LSR_PE));
    iir.set_bit<IRQ_RDA>(!m_rx_fifo.empty());
    irq = iir & ier;
}

u8 uart_base::read_rbr() {
    if (lcr & LCR_DLAB)
        return m_divisor & 0xff;

    if (m_rx_fifo.empty())
        return 0;

    u8 val = m_rx_fifo.front();
    m_rx_fifo.pop();
    update();
    return val;
}

u8 uart_base::read_ier() {
    if (lcr & LCR_DLAB)
        return m_divisor >> 8;
    return ier;
}

u8 uart_base::read_iir() {
    if (iir & IRQ_RLS)
        return IIR_RLS;

    if (iir & IRQ_RDA)
        return IIR_RDA;

    if (iir & IRQ_THRE) {
        iir &= ~IRQ_THRE;
        update();
        return IIR_THRE;
    }

    if (iir & IRQ_MST)
        return IIR_MST;

    return IIR_NOIP;
}

u8 uart_base::read_lsr() {
    u8 val = lsr;
    lsr &= ~(LSR_OE | LSR_PE);
    update();
    return val;
}

void uart_base::write_thr(u8 val) {
    if (lcr & LCR_DLAB) {
        m_divisor = deposit(m_divisor, 0, 8, val);
        calibrate();
        return;
    }

    thr = val;
    m_tx_fifo.push(thr);
    iir &= ~IRQ_THRE;
    update();

    m_tx_fifo.pop();
    serial_tx.send(thr);
    iir.set_bit<IRQ_THRE>(m_tx_fifo.empty());
    update();
}

void uart_base::write_ier(u8 val) {
    if (lcr & LCR_DLAB) {
        m_divisor = deposit(m_divisor, 8, 8, val);
        calibrate();
        return;
    }

    VCML_LOG_REG_BIT_CHANGE(IRQ_RDA, ier, val);
    VCML_LOG_REG_BIT_CHANGE(IRQ_THRE, ier, val);
    VCML_LOG_REG_BIT_CHANGE(IRQ_RLS, ier, val);
    VCML_LOG_REG_BIT_CHANGE(IRQ_MST, ier, val);

    ier = val & 0x0f;

    update();
}

void uart_base::write_lcr(u8 val) {
    size_t oldbits = SERIAL_5_BITS + (lcr & 0x3);
    size_t newbits = SERIAL_5_BITS + (val & 0x3);
    if (newbits != oldbits)
        log_debug("word length %zu bits", newbits);

    VCML_LOG_REG_BIT_CHANGE(LCR_STP, lcr, val);
    VCML_LOG_REG_BIT_CHANGE(LCR_PEN, lcr, val);
    VCML_LOG_REG_BIT_CHANGE(LCR_EPS, lcr, val);
    VCML_LOG_REG_BIT_CHANGE(LCR_SPB, lcr, val);
    VCML_LOG_REG_BIT_CHANGE(LCR_BCB, lcr, val);
    VCML_LOG_REG_BIT_CHANGE(LCR_DLAB, lcr, val);

    lcr = val;

    calibrate();
}

void uart_base::write_fcr(u8 val) {
    log_debug("FIFOs %sabled", val & FCR_FE ? "en" : "dis");

    if (val & FCR_CRF) {
        while (!m_rx_fifo.empty())
            m_rx_fifo.pop();
        log_debug("receiver FIFO cleared");
    }

    if (val & FCR_CTF) {
        while (!m_tx_fifo.empty())
            m_tx_fifo.pop();
        log_debug("transmitter FIFO cleared");
    }

    if (val & FCR_DMA)
        log_debug("FCR_DMA bit set");

    switch (val & 0b11000000) {
    case FCR_IT1:
        log_debug("interrupt threshold 1 byte");
        break;
    case FCR_IT4:
        log_debug("interrupt threshold 4 bytes");
        break;
    case FCR_IT8:
        log_debug("interrupt threshold 8 bytes");
        break;
    case FCR_IT14:
        log_debug("interrupt threshold 14 bytes");
        break;
    default:
        break;
    }
}

void uart_base::serial_receive(const serial_target_socket& socket,
                               serial_payload& tx) {
    if (m_rx_fifo.size() < m_rx_size) {
        m_rx_fifo.push(tx.data & tx.mask);
        if (!serial_test_parity(tx)) {
            log_warn("parity error detected");
            lsr |= LSR_PE;
        }
    } else {
        log_warn("rx fifo overflow");
        lsr |= LSR_OE;
    }

    update();
}

uart_base::uart_base(const sc_module_name& nm, size_t rx_size, size_t tx_size):
    peripheral(nm),
    serial_host(),
    m_rx_size(rx_size),
    m_tx_size(tx_size),
    m_rx_fifo(),
    m_tx_fifo(),
    m_divisor(1),
    thr("thr", 0x0, 0x00),
    ier("ier", 0x1, 0x00),
    iir("iir", 0x2, 0x00),
    lcr("lcr", 0x3, LCR_WL8), // 8bits, no parity
    mcr("mcr", 0x4, 0x00),
    lsr("lsr", 0x5, LSR_THRE | LSR_TEMT),
    msr("msr", 0x6, 0x00),
    scr("scr", 0x7, 0x00),
    serial_tx("serial_tx"),
    serial_rx("serial_rx"),
    irq("irq"),
    in("in") {
    thr.sync_always();
    thr.allow_read_write();
    thr.on_read(&uart_base::read_rbr);
    thr.on_write(&uart_base::write_thr);

    thr.sync_always();
    ier.allow_read_write();
    ier.on_read(&uart_base::read_ier);
    ier.on_write(&uart_base::write_ier);

    iir.sync_always();
    iir.no_writeback();
    iir.allow_read_write();
    iir.on_read(&uart_base::read_iir);
    iir.on_write(&uart_base::write_fcr);

    lcr.sync_always();
    lcr.allow_read_write();
    lcr.on_write(&uart_base::write_lcr);

    lsr.sync_always();
    lsr.allow_read_only();
    lsr.on_read(&uart_base::read_lsr);

    mcr.allow_read_write();
    msr.allow_read_write();
    scr.allow_read_write();

    serial_tx.set_baud(DEFAULT_BAUD);
    serial_tx.set_data_width(uart8250_data_bits(lcr));
    serial_tx.set_stop_bits(uart8250_stop_bits(lcr));
    serial_tx.set_parity(uart8250_parity(lcr));
}

uart_base::~uart_base() {
    // nothing to do
}

void uart_base::reset() {
    peripheral::reset();
    m_divisor = clock_hz() / (16 * DEFAULT_BAUD);
    calibrate();
}

VCML_EXPORT_MODEL(vcml::serial::uart8250, name, args) {
    return new uart8250(name);
}

VCML_EXPORT_MODEL(vcml::serial::uart16550, name, args) {
    return new uart16550(name);
}

} // namespace serial
} // namespace vcml
