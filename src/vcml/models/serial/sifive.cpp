/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/serial/sifive.h"

namespace vcml {
namespace serial {

enum txdata_bits : u32 {
    TXDATA_FULL = bit(31),
};

using TXDATA_DATA = field<0, 8, u32>;

enum rxdata_bits : u32 {
    RXDATA_EMPTY = bit(31),
};

using RXDATA_DATA = field<0, 8, u32>;

enum txctrl_bits : u32 {
    TXCTRL_TXEN = bit(0),
    TXCTRL_NSTOP = bit(1),
};

using TXCTRL_TXCNT = field<16, 3, u32>;

enum rxctrl_bits : u32 {
    RXCTRL_RXEN = bit(0),
    RXCTRL_NSTOP = bit(1),
};

using RXCTRL_RXCNT = field<16, 3, u32>;

enum ie_bits : u32 {
    IE_TXWM = bit(0),
    IE_RXWM = bit(1),
};

enum ip_bits : u32 {
    IP_TXWM = bit(0),
    IP_RXWM = bit(1),
};

static void ignore_write(u32 val) {
    // ignore value
}

bool sifive::is_tx_full() const {
    return txdata & TXDATA_FULL;
}

bool sifive::is_rx_empty() const {
    return rxdata & RXDATA_EMPTY;
}

bool sifive::is_tx_enabled() const {
    return txctrl & TXCTRL_TXEN;
}

bool sifive::is_rx_enabled() const {
    return rxctrl & RXCTRL_RXEN;
}

serial_stop sifive::num_stop_bits() const {
    if (txctrl & TXCTRL_NSTOP)
        return SERIAL_STOP_2;
    else
        return SERIAL_STOP_1;
}

size_t sifive::tx_watermark() const {
    return get_field<TXCTRL_TXCNT>(txctrl);
}

size_t sifive::rx_watermark() const {
    return get_field<RXCTRL_RXCNT>(rxctrl);
}

void sifive::update_tx() {
    bool tx_full = m_tx_fifo.size() == tx_fifo_size;
    txdata.set_bit<TXDATA_FULL>(tx_full);

    bool tx_under_watermark = m_tx_fifo.size() < tx_watermark();
    ip.set_bit<IP_TXWM>(tx_under_watermark);

    bool should_raise_tx_irq = ip & IP_TXWM && ie & IE_TXWM;
    tx_irq = should_raise_tx_irq;
}

void sifive::update_rx() {
    bool rx_empty = m_rx_fifo.empty();
    rxdata.set_bit<RXDATA_EMPTY>(rx_empty);

    bool rx_over_watermark = m_rx_fifo.size() > rx_watermark();
    ip.set_bit<IP_RXWM>(rx_over_watermark);

    bool should_raise_rx_irq = ip & IP_RXWM && ie & IE_RXWM;
    rx_irq = should_raise_rx_irq;
}

void sifive::tx_thread() {
    while (true) {
        wait(m_txev);
        while (!m_tx_fifo.empty()) {
            serial_tx.send(m_tx_fifo.front());
            m_tx_fifo.pop();
            update_tx();
        }
    }
}

u32 sifive::read_txdata() {
    return is_tx_full() ? TXDATA_FULL : 0;
}

u32 sifive::read_rxdata() {
    if (is_rx_empty())
        return RXDATA_EMPTY;

    u32 val = m_rx_fifo.front();
    m_rx_fifo.pop();
    update_rx();
    return val;
}

void sifive::write_txdata(u32 val) {
    if (!is_tx_full())
        m_tx_fifo.push(val & 0xff);

    update_tx();
    if (is_tx_enabled())
        m_txev.notify(SC_ZERO_TIME);
}

void sifive::write_txctrl(u32 val) {
    txctrl = val & (TXCTRL_TXEN | TXCTRL_NSTOP | TXCTRL_TXCNT());
    serial_tx.set_stop_bits(num_stop_bits());

    if (is_tx_enabled())
        m_txev.notify(SC_ZERO_TIME);
    update_tx();
}

void sifive::write_rxctrl(u32 val) {
    rxctrl = val & (RXCTRL_RXEN | RXCTRL_NSTOP | RXCTRL_RXCNT());
    update_rx();
}

void sifive::write_ie(u32 val) {
    ie = val & (IE_TXWM | IE_RXWM);
    update_tx();
    update_rx();
}

void sifive::write_div(u32 val) {
    div = val & 0xffff;
    serial_tx.set_baud(clock_hz() / val);
}

void sifive::serial_receive(u8 data) {
    if (is_rx_enabled()) {
        if (m_rx_fifo.size() < rx_fifo_size)
            m_rx_fifo.push(data);
        else
            log_warn("FIFO buffer overflow, data dropped");

        update_rx();
    }
}

sifive::sifive(const sc_module_name& nm):
    peripheral(nm),
    serial_host(),
    m_tx_fifo(),
    m_rx_fifo(),
    m_txev("txev"),
    tx_fifo_size("tx_fifo_size", 8),
    rx_fifo_size("rx_fifo_size", 8),
    txdata("txdata", 0x00, 0x0),
    rxdata("rxdata", 0x04, 0x0),
    txctrl("txctrl", 0x08, 0x0),
    rxctrl("rxctrl", 0x0c, 0x0),
    ie("ie", 0x10, 0x0),
    ip("ip", 0x14, 0x0),
    div("div", 0x18, 0x0),
    in("in"),
    tx_irq("tx_irq"),
    rx_irq("rx_irq"),
    serial_tx("serial_tx"),
    serial_rx("serial_rx") {
    txdata.sync_always();
    txdata.allow_read_write();
    txdata.on_write(&sifive::write_txdata);
    txdata.on_read(&sifive::read_txdata);

    rxdata.sync_always();
    rxdata.allow_read_write();
    rxdata.on_read(&sifive::read_rxdata);
    rxdata.on_write(ignore_write);

    txctrl.sync_never();
    txctrl.allow_read_write();
    txctrl.on_write(&sifive::write_txctrl);

    rxctrl.sync_never();
    rxctrl.allow_read_write();
    rxctrl.on_write(&sifive::write_rxctrl);

    ie.sync_always();
    ie.allow_read_write();
    ie.on_write(&sifive::write_ie);

    ip.sync_never();
    ip.allow_read_only();

    div.sync_never();
    div.allow_read_write();
    div.on_write(&sifive::write_div);

    serial_tx.set_baud(SERIAL_115200BD);
    serial_tx.set_data_width(SERIAL_8_BITS);
    serial_tx.set_parity(SERIAL_PARITY_NONE);
    serial_tx.set_stop_bits(SERIAL_STOP_1);

    update_tx();
    update_rx();

    SC_HAS_PROCESS(sifive);
    SC_THREAD(tx_thread);
}

sifive::~sifive() {
    // nothing to do
}

void sifive::reset() {
    peripheral::reset();

    m_tx_fifo = {};
    m_rx_fifo = {};

    tx_irq = false;
    rx_irq = false;

    div = clock_hz() / serial_tx.baud();
}

VCML_EXPORT_MODEL(vcml::serial::sifive, name, args) {
    return new sifive(name);
}

} // namespace serial
} // namespace vcml
