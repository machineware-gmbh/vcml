/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/serial/uartlite.h"

namespace vcml {
namespace serial {

enum status_bits : u32 {
    STATUS_RX_FIFO_DATA = bit(0),
    STATUS_RX_FIFO_FULL = bit(1),
    STATUS_TX_FIFO_EMPTY = bit(2),
    STATUS_TX_FIFO_FULL = bit(3),
    STATUS_INTR_ENABLED = bit(4),
    STATUS_OVERRUN_ERROR = bit(5),
    STATUS_FRAME_ERROR = bit(6),
    STATUS_PARITY_ERROR = bit(7),
    STATUS_ERROR = STATUS_OVERRUN_ERROR | STATUS_FRAME_ERROR |
                   STATUS_PARITY_ERROR,
};

enum control_bits : u32 {
    CONTROL_RST_TX_FIFO = bit(0),
    CONTROL_RST_RX_FIFO = bit(1),
    CONTROL_ENABLE_INTR = bit(4),
};

void uartlite::tx_thread() {
    while (true) {
        while (m_tx_fifo.empty())
            wait(m_txev);

        while (!m_tx_fifo.empty()) {
            serial_tx.send(m_tx_fifo.front());
            wait(serial_tx.cycle());
            m_tx_fifo.pop();
        }

        if (status & STATUS_INTR_ENABLED)
            irq.pulse();
    }
}

u32 uartlite::read_rx_fifo() {
    if (m_rx_fifo.empty())
        return 0;

    u32 val = m_rx_fifo.front();
    m_rx_fifo.pop();
    return val;
}

u32 uartlite::read_status() {
    status.set_bit<STATUS_RX_FIFO_DATA>(!m_rx_fifo.empty());
    status.set_bit<STATUS_RX_FIFO_FULL>(m_rx_fifo.size() >= 8);
    status.set_bit<STATUS_TX_FIFO_EMPTY>(m_tx_fifo.empty());
    status.set_bit<STATUS_TX_FIFO_FULL>(m_tx_fifo.size() >= 8);

    u32 val = status;
    status &= ~STATUS_ERROR;
    return val;
}

void uartlite::write_tx_fifo(u32 val) {
    if (m_tx_fifo.size() >= 8) {
        log_warn("tx fifo full, data dropped");
        return;
    }

    m_tx_fifo.push(val);
    m_txev.notify(clock_cycle());
}

static void flush_fifo(queue<u8>& fifo) {
    queue<u8> empty;
    fifo.swap(empty);
}

void uartlite::write_control(u32 val) {
    if (val & CONTROL_RST_RX_FIFO)
        flush_fifo(m_rx_fifo);
    if (val & CONTROL_RST_TX_FIFO)
        flush_fifo(m_tx_fifo);

    status.set_bit<STATUS_INTR_ENABLED>(val & CONTROL_ENABLE_INTR);
}

void uartlite::serial_receive(u8 data) {
    if (m_rx_fifo.size() >= 8) {
        log_warn("rx fifo full, data dropped");
        return;
    }

    m_rx_fifo.push(data);
    if ((status & STATUS_INTR_ENABLED) && (m_rx_fifo.size() == 1))
        irq.pulse();
}

uartlite::uartlite(const sc_module_name& nm):
    peripheral(nm),
    serial_host(),
    m_tx_fifo(),
    m_rx_fifo(),
    m_txev("txev"),
    baudrate("baudrate", SERIAL_9600BD),
    databits("databits", 8),
    use_parity("use_parity", false),
    odd_parity("odd_parity", false),
    rx_fifo("rx_fifo", 0x0),
    tx_fifo("tx_fifo", 0x4),
    status("status", 0x8),
    control("control", 0xc),
    in("in"),
    irq("irq"),
    serial_tx("serial_tx"),
    serial_rx("serial_rx") {
    rx_fifo.sync_always();
    rx_fifo.allow_read_ignore_write();
    rx_fifo.on_read(&uartlite::read_rx_fifo);

    tx_fifo.sync_always();
    tx_fifo.allow_read_write();
    tx_fifo.on_write(&uartlite::write_tx_fifo);

    status.sync_always();
    status.allow_read_ignore_write();
    status.on_read(&uartlite::read_status);

    control.sync_always();
    control.allow_read_write();
    control.on_write(&uartlite::write_control);

    serial_tx.set_stop_bits(SERIAL_STOP_1);

    if (baudrate > 0) {
        serial_tx.set_baud(baudrate);
    } else {
        log_warn("invalid baud rate, using default");
        serial_tx.set_baud(SERIAL_9600BD);
    }

    if (databits >= 5 && databits <= 8) {
        serial_bits bits = (serial_bits)databits.get();
        serial_tx.set_data_width(bits);
    } else {
        log_warn("databits must be >=5 and <=8, using default");
        serial_tx.set_data_width(SERIAL_8_BITS);
    }

    if (use_parity) {
        serial_tx.set_parity(odd_parity ? SERIAL_PARITY_ODD
                                        : SERIAL_PARITY_EVEN);
    } else {
        serial_tx.set_parity(SERIAL_PARITY_NONE);
    }

    SC_HAS_PROCESS(uartlite);
    SC_THREAD(tx_thread);
}

uartlite::~uartlite() {
    // nothing to do
}

void uartlite::reset() {
    peripheral::reset();
    flush_fifo(m_rx_fifo);
    flush_fifo(m_tx_fifo);
}

VCML_EXPORT_MODEL(vcml::serial::uartlite, name, args) {
    return new uartlite(name);
}

} // namespace serial
} // namespace vcml
