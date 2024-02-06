/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/serial/sifive_uart.h"

namespace vcml {
namespace serial {

enum txdata_bits : u32 {
    TXDATA_FULL = bit(31),
};

typedef field<0, 8, u32> TXDATA_DATA;

enum rxdata_bits : u32 {
    RXDATA_EMPTY = bit(31),
};

typedef field<0, 8, u32> RXDATA_DATA;

enum txctrl_bits : u32 {
    TXCTRL_TXEN = bit(0),
    TXCTRL_NSTOP = bit(1),
};

typedef field<16, 3, u32> TXCTRL_TXCNT;

enum rxctrl_bits : u32 {
    RXCTRL_RXEN = bit(0),
    RXCTRL_NSTOP = bit(1),
};

typedef field<16, 3, u32> RXCTRL_RXCNT;

enum ie_bits : u32 {
    IE_TXWM = bit(0),
    IE_RXWM = bit(1),
};

enum ip_bits : u32 {
    IP_TXWM = bit(0),
    IP_RXWM = bit(1),
};

bool sifive_uart::is_tx_full() const {
    return txdata & TXDATA_FULL;
}

bool sifive_uart::is_rx_empty() const {
    return rxdata & RXDATA_EMPTY;
}

bool sifive_uart::is_tx_enabled() const {
    return txctrl & TXCTRL_TXEN;
}

bool sifive_uart::is_rx_enabled() const {
    return rxctrl & RXCTRL_RXEN;
}

u8 sifive_uart::num_stop_bits() const {
    return !!(txctrl & TXCTRL_NSTOP) + 1;
}

u8 sifive_uart::get_tx_watermark() const {
    return get_field<TXCTRL_TXCNT>(txctrl);
}

u8 sifive_uart::get_rx_watermark() const {
    return get_field<RXCTRL_RXCNT>(rxctrl);
}

void sifive_uart::serial_receive(u8 data) {
    if (is_rx_enabled()) {
        if (m_rx_fifo.size() < m_rx_fifo_size)
            m_rx_fifo.push(data);
        else {
            log_warn("FIFO buffer overflow, data dropped");
        }

        update_rx();
    }
}

void sifive_uart::update_tx() {
    bool tx_full = m_tx_fifo.size() == m_tx_fifo_size;
    txdata.set_bit<TXDATA_FULL>(tx_full);

    bool tx_under_watermark = m_tx_fifo.size() < get_tx_watermark();
    ip.set_bit<IP_TXWM>(tx_under_watermark);

    bool should_raise_tx_irq = ip & IP_TXWM && ie & IE_TXWM;
    tx_irq = should_raise_tx_irq;
}

void sifive_uart::update_rx() {
    bool rx_empty = m_rx_fifo.empty();
    rxdata.set_bit<RXDATA_EMPTY>(rx_empty);

    bool rx_over_watermark = m_rx_fifo.size() > get_rx_watermark();
    ip.set_bit<IP_RXWM>(rx_over_watermark);

    bool should_raise_rx_irq = ip & IP_RXWM && ie & IE_RXWM;
    rx_irq = should_raise_rx_irq;
}

void sifive_uart::send_tx() {
    while (!m_tx_fifo.empty()) {
        serial_tx.send(m_tx_fifo.front());
        m_tx_fifo.pop();
        update_tx();
    }
}

void sifive_uart::write_txdata(u32 val) {
    if (!is_tx_full()) {
        m_tx_fifo.push(val & 0xff);
    }
    update_tx();

    if (is_tx_enabled())
        send_tx();
}

u32 sifive_uart::read_txdata() {
    return is_tx_full() ? TXDATA_FULL : 0;
}

u32 sifive_uart::read_rxdata() {
    u32 val = RXDATA_EMPTY;
    if (is_rx_enabled() && !is_rx_empty()) {
        val = m_rx_fifo.front();
        m_rx_fifo.pop();
        update_rx();
    }
    return val;
}

void sifive_uart::write_txctrl(u32 val) {
    const u32 mask = (TXCTRL_TXEN | TXCTRL_NSTOP | TXCTRL_TXCNT());
    txctrl = val & mask;
    if (is_tx_enabled())
        send_tx();
    update_tx();
}

void sifive_uart::write_rxctrl(u32 val) {
    rxctrl = val & (RXCTRL_RXEN | RXCTRL_NSTOP | RXCTRL_RXCNT());
    update_tx();
}

void sifive_uart::write_ie(u32 val) {
    ie = val & 0b11;
    update_tx();
    update_rx();
}

void sifive_uart::write_div(u32 val) {
    div = val & 0xffff;
    serial_tx.set_baud(clock_hz() / val);
}

sifive_uart::sifive_uart(const sc_module_name& nm):
    peripheral(nm),
    serial_host(),
    m_tx_fifo_size("tx_fifo_size", 8),
    m_tx_fifo(),
    m_rx_fifo_size("rx_fifo_size", 8),
    m_rx_fifo(),
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
    txdata.on_write(&sifive_uart::write_txdata);
    txdata.on_read(&sifive_uart::read_txdata);

    rxdata.sync_always();
    rxdata.allow_read_write();
    rxdata.on_read(&sifive_uart::read_rxdata);
    rxdata.on_write(
        [&](u32 val) {}); // writes to rxdata are allowed but ignored

    txctrl.sync_never();
    txctrl.allow_read_write();
    txctrl.on_write(&sifive_uart::write_txctrl);

    rxctrl.sync_never();
    rxctrl.allow_read_write();
    rxctrl.on_write(&sifive_uart::write_rxctrl);

    ie.sync_always();
    ie.allow_read_write();
    ie.on_write(&sifive_uart::write_ie);

    ip.sync_never();
    ip.allow_read_only();

    div.sync_never();
    div.allow_read_write();
    div.on_write(&sifive_uart::write_div);

    serial_tx.set_baud(SERIAL_115200BD);

    update_tx();
    update_rx();
}

sifive_uart::~sifive_uart() {
    // nothing to do
}

void sifive_uart::reset() {
    peripheral::reset();
    if (!m_tx_fifo.empty())
        m_tx_fifo = {};
    if (!m_rx_fifo.empty())
        m_rx_fifo = {};
    tx_irq = false;
    rx_irq = false;
    div = clock_hz() / SERIAL_115200BD;
}

VCML_EXPORT_MODEL(vcml::serial::sifive_uart, name, args) {
    return new sifive_uart(name);
}

} // namespace serial
} // namespace vcml
