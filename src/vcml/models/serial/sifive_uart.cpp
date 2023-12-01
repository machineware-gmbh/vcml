/******************************************************************************
 *                                                                            *
 * Copyright (C) 2023 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/serial/sifive_uart.h"

namespace vcml {
namespace serial {

u32 sifive_uart::get_tx_cnt() {
    return (txctrl >> SIFIVE_UART_WM_OFFSET) & (m_tx_fifo.size() - 1);
}

u32 sifive_uart::get_rx_cnt() {
    return (rxctrl >> SIFIVE_UART_WM_OFFSET) & (m_rx_fifo.size() - 1);
}

u32 sifive_uart::get_ip() {
    u32 ret = 0;

    u32 txcnt = get_tx_cnt();
    u32 rxcnt = get_rx_cnt();

    if ((u32)m_tx_fifo.size() < txcnt) {
        ret |= SIFIVE_UART_IP_TXWM;
    }
    
    if ((u32)m_rx_fifo.size() > rxcnt) {
        ret |= SIFIVE_UART_IP_RXWM;
    }

    return ret;
}

void sifive_uart::update_irq() {
    bool cond = false;

    u32 uip = get_ip();
    if (((uip & SIFIVE_UART_IP_TXWM) && (ie & SIFIVE_UART_IE_TXWM)) ||
        ((uip & SIFIVE_UART_IP_RXWM) && (ie & SIFIVE_UART_IE_RXWM))) {
        cond = true;
    }

    if (cond) {
        irq = true;
    } else {
        irq = false;
    }
}

void sifive_uart::flush_tx_fifo() {
    while(!m_tx_fifo.empty()) {
        serial_tx.send(m_tx_fifo.front());
        m_tx_fifo.pop();
    }
}

u32 sifive_uart::read_txdata() {
    return m_tx_fifo.size() == tx_fifo_size ? SIFIVE_UART_TXDATA_FULL : 0;
}

void sifive_uart::write_txdata(u32 val) {
    u8 ch = val;
    if (m_tx_fifo.size() < tx_fifo_size) {
        m_tx_fifo.push(ch);
    }

    if (!(txctrl & SIFIVE_UART_TXCTRL_TXEN)) {
        return;
    }

    if (!m_tx_fifo.empty()) {
        flush_tx_fifo(); 
    }
    update_irq();
}

u32 sifive_uart::read_rxdata() {
    u32 ret = SIFIVE_UART_RXDATA_EMPTY;
    if (!m_rx_fifo.empty()) {
        ret = m_rx_fifo.front();  
        m_rx_fifo.pop();
        update_irq();
    }

    return ret;
}

u32 sifive_uart::read_txctrl() {
    return txctrl;
}

void sifive_uart::write_txctrl(u32 val) {
    txctrl = val &
             (((tx_fifo_size - 1U) << SIFIVE_UART_WM_OFFSET) |
              SIFIVE_UART_TXCTRL_TXEN | SIFIVE_UART_TXCTRL_NSTOP);

    if ((txctrl & SIFIVE_UART_TXCTRL_TXEN) && !m_tx_fifo.empty()) {
        flush_tx_fifo();
    }

    update_irq();
}

u32 sifive_uart::read_rxctrl() {
    return rxctrl;
}

void sifive_uart::write_rxctrl(u32 val) {
    rxctrl = val &
             (((rx_fifo_size - 1U) << SIFIVE_UART_WM_OFFSET) |
              SIFIVE_UART_RXCTRL_RXEN);

    update_irq();
}

u32 sifive_uart::read_ie() {
    return ie;
}

void sifive_uart::write_ie(u32 val) {
    ie = val & (SIFIVE_UART_IE_TXWM | SIFIVE_UART_IE_RXWM);
    update_irq();
}

u32 sifive_uart::read_ip() {
    return get_ip();
}

u32 sifive_uart::read_div() {
    return div;
}

void sifive_uart::write_div(u32 val) {
    div = val & SIFIVE_UART_DIV_MASK;
}

void sifive_uart::serial_receive(const serial_target_socket& socket,
                                 serial_payload& tx) {
    if (!(rxctrl & SIFIVE_UART_RXCTRL_RXEN)) {
        return;
    }

    if (m_rx_fifo.size() >= rx_fifo_size) {
        log_warn("FIFO buffer overflow, data dropped");
        return;
    }

    m_rx_fifo.push(tx.data & tx.mask);
    update_irq();
}

sifive_uart::sifive_uart(const sc_module_name& nm):
    peripheral(nm),
    serial_host(),
    m_tx_fifo(),
    m_rx_fifo(),
    tx_fifo_size("tx_fifo_size", 8),
    rx_fifo_size("rx_fifo_size", 8),
    txdata("txdata", 0x0, 0x0),
    rxdata("rxdata", 0x4, 0x0),
    txctrl("txctrl", 0x8, 0x0),
    rxctrl("rxctrl", 0xc, 0x0),
    ie("ie", 0x10, 0x0),
    ip("ip", 0x14, 0x0),
    div("div", 0x18, 0x0),
    serial_tx("serial_tx"),
    serial_rx("serial_rx"),
    irq("irq"),
    in("in") {

    txdata.allow_read_write();
    txdata.on_read(&sifive_uart::read_txdata);
    txdata.on_write(&sifive_uart::write_txdata);

    rxdata.allow_read_only();
    rxdata.on_read(&sifive_uart::read_rxdata);

    txctrl.allow_read_write();
    txctrl.on_read(&sifive_uart::read_txctrl);
    txctrl.on_write(&sifive_uart::write_txctrl);

    rxctrl.allow_read_write();
    rxctrl.on_read(&sifive_uart::read_rxctrl);
    rxctrl.on_write(&sifive_uart::write_rxctrl);

    ie.allow_read_write();
    ie.on_read(&sifive_uart::read_ie);
    ie.on_write(&sifive_uart::write_ie);

    ip.allow_read_only();
    ip.on_read(&sifive_uart::read_ip);

    div.allow_read_write();
    div.on_read(&sifive_uart::read_div);
    div.on_write(&sifive_uart::write_div);

    serial_tx.set_baud(DEFAULT_BAUD);

    div = clock_hz() / DEFAULT_BAUD;
}

sifive_uart::~sifive_uart() {
    // nothing to do
}

void sifive_uart::reset() {
    peripheral::reset();

    txdata = 0;
    rxdata = 0;
    txctrl = 0;
    rxctrl = 0;
    ie = 0;
    ip = 0;
    div = clock_hz() / DEFAULT_BAUD;
    while (!m_tx_fifo.empty()) {
        m_tx_fifo.pop();
    }
    while (!m_rx_fifo.empty()) {
        m_rx_fifo.pop();
    }
}

VCML_EXPORT_MODEL(vcml::serial::sifive_uart, name, args) {
    return new sifive_uart(name);
}

} // namespace serial
} // namespace vcml
