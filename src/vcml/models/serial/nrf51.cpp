/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/serial/nrf51.h"

namespace vcml {
namespace serial {

enum inten_bits : u32 {
    INTEN_CT = bit(0),
    INTEN_NCTS = bit(1),
    INTEN_RXDRDY = bit(2),
    INTEN_TXDRDY = bit(7),
    INTEN_ERROR = bit(9),
    INTEN_RXTO = bit(17),
    INTEN_MASK = INTEN_CT | INTEN_NCTS | INTEN_RXDRDY | INTEN_TXDRDY |
                 INTEN_ERROR | INTEN_RXTO,
};

enum errorsrc_bits : u32 {
    ERRORSRC_OVERRUN = bit(0),
    ERRORSRC_PARITY = bit(1),
    ERRORSRC_FRAMING = bit(2),
    ERRORSRC_BREAK = bit(3),
};

enum config_bits : u32 {
    CONFIG_HWFC = bit(1),
    CONFIG_PARITY = bitmask(3, 1),
};

static baud_t nrf51_baud(u32 val) {
    switch (val) {
    case 0x0004f000:
        return 1200;
    case 0x0009d000:
        return 2400;
    case 0x0013b000:
        return 4800;
    case 0x00275000:
        return 9600;
    case 0x003b0000:
        return 14400;
    case 0x004ea000:
        return 19200;
    case 0x0075f000:
        return 28800;
    case 0x009d5000:
        return 38400;
    case 0x00ebf000:
        return 57600;
    case 0x013a9000:
        return 76800;
    case 0x01d7e000:
        return 115200;
    case 0x03afb000:
        return 230400;
    case 0x04000000:
        return 250000;
    case 0x075F7000:
        return 460800;
    case 0x0ebedfa4:
        return 921600;
    case 0x10000000:
        return 1000000;
    default:
        return 0;
    }
}

u32 nrf51::read_rxd() {
    if (!is_rx_enabled() || m_fifo.empty())
        return 0;

    u32 data = m_fifo.front();
    m_fifo.pop();
    rxdrdy = !m_fifo.empty();
    update();
    return data;
}

void nrf51::write_startrx(u32 val) {
    m_rx_enabled = (val == 1u);
    startrx = val;
    update();
}

void nrf51::write_stoprx(u32 val) {
    m_rx_enabled = !(val == 1u);
    stoprx = val;
    update();
}

void nrf51::write_starttx(u32 val) {
    m_tx_enabled = (val == 1u);
    starttx = val;
    update();
}

void nrf51::write_stoptx(u32 val) {
    m_tx_enabled = !(val == 1u);
    stoptx = val;
    update();
}

void nrf51::write_suspend(u32 val) {
    m_rx_enabled = !(val == 1u);
    m_tx_enabled = !(val == 1u);
    suspend = val;
    update();
}

void nrf51::write_enable(u32 val) {
    m_enabled = (val == 4u);
    enable = val;
    update();
}

void nrf51::write_inten(u32 val) {
    if (is_enabled())
        inten = val;
    update();
}

void nrf51::write_intenset(u32 val) {
    if (is_enabled())
        inten |= val;
    update();
}

void nrf51::write_intenclr(u32 val) {
    if (is_enabled())
        inten &= ~val;
    update();
}

void nrf51::write_errsrc(u32 val) {
    if (is_enabled())
        errsrc &= ~val;
    update();
}

void nrf51::write_txd(u32 val) {
    if (is_tx_enabled())
        serial_tx.send(val);
    update();
}

void nrf51::write_baudrate(u32 val) {
    if (is_enabled())
        log_warn("changing baud rate while UART active");

    baudrate = val;
    baud_t bd = nrf51_baud(val);
    if (bd)
        log_debug("setting baud rate to %zuu", bd);
    else
        log_warn("invalid baud rate: 0x%08x", val);
    serial_tx.set_baud(bd);
}

void nrf51::write_config(u32 val) {
    if (is_enabled())
        log_warn("changing configuration while UART active");

    if ((val & CONFIG_PARITY) == CONFIG_PARITY)
        serial_tx.set_parity(SERIAL_PARITY_MARK);
    else
        serial_tx.set_parity(SERIAL_PARITY_NONE);
}

void nrf51::update() {
    rxdrdy = is_rx_enabled() && !m_fifo.empty();
    txdrdy = is_tx_enabled();

    error = is_enabled() && errsrc != 0u;

    bool irq_status = false;
    if (is_enabled()) {
        irq_status |= rxdrdy && (inten & INTEN_RXDRDY);
        irq_status |= txdrdy && (inten & INTEN_TXDRDY);
        irq_status |= error && (inten & INTEN_ERROR);
        irq_status |= rxto && (inten & INTEN_RXTO);
    }

    irq = irq_status;
}

void nrf51::serial_receive(u8 data) {
    if (!is_rx_enabled())
        return;

    if (m_fifo.size() >= FIFO_SIZE) {
        log_warn("FIFO buffer overflow, data dropped");
        m_fifo.pop();
    }

    m_fifo.push(data);
    rxdrdy = 1;
    update();
}

nrf51::nrf51(const sc_module_name& nm):
    peripheral(nm),
    serial_host(),
    m_fifo(),
    m_enabled(),
    m_rx_enabled(),
    m_tx_enabled(),
    startrx("startrx", 0x0),
    stoprx("stoprx", 0x4),
    starttx("starttx", 0x8),
    stoptx("stoptx", 0xc),
    suspend("suspend", 0x1c),
    cts("cts", 0x100),
    ncts("ncts", 0x104),
    rxdrdy("rxdrdy", 0x108),
    txdrdy("txdrdy", 0x11c),
    error("error", 0x124),
    rxto("rxto", 0x144),
    inten("inten", 0x300),
    intenset("intenset", 0x304),
    intenclr("intenclr", 0x308),
    errsrc("errsrc", 0x480),
    enable("enable", 0x500),
    pselrts("pselrts", 0x508, 0xffffffff),
    pseltxd("pseltxd", 0x50c, 0xffffffff),
    pselcts("pselcts", 0x510, 0xffffffff),
    pselrxd("pselrxd", 0x514, 0xffffffff),
    rxd("rxd", 0x518),
    txd("txd", 0x51c),
    baudrate("baudrate", 0x524, 0x04000000),
    config("config", 0x56c, 0x0),
    serial_tx("serial_tx"),
    serial_rx("serial_rx"),
    irq("irq"),
    in("in") {
    startrx.sync_always();
    startrx.allow_read_write();
    startrx.on_write(&nrf51::write_startrx);

    stoprx.sync_always();
    stoprx.allow_read_write();
    stoprx.on_write(&nrf51::write_stoprx);

    starttx.sync_always();
    starttx.allow_read_write();
    starttx.on_write(&nrf51::write_starttx);

    stoptx.sync_always();
    stoptx.allow_read_write();
    stoptx.on_write(&nrf51::write_stoptx);

    suspend.sync_always();
    suspend.allow_read_write();
    suspend.on_write(&nrf51::write_suspend);

    inten.sync_always();
    inten.allow_read_write();
    inten.on_write(&nrf51::write_inten);

    intenset.sync_always();
    intenset.allow_read_write();
    intenset.on_read([&]() -> u32 { return inten; });
    intenset.on_write(&nrf51::write_intenset);

    intenclr.sync_always();
    intenclr.allow_read_write();
    intenclr.on_read([&]() -> u32 { return inten; });
    intenclr.on_write(&nrf51::write_intenclr);

    errsrc.sync_always();
    errsrc.allow_read_write();
    errsrc.on_write(&nrf51::write_errsrc);

    enable.sync_always();
    enable.allow_read_write();
    enable.on_write(&nrf51::write_enable);

    rxd.sync_always();
    rxd.allow_read_write();
    rxd.on_read(&nrf51::read_rxd);

    txd.sync_always();
    txd.allow_read_write();
    txd.on_write(&nrf51::write_txd);

    baudrate.sync_always();
    baudrate.allow_read_write();
    baudrate.on_write(&nrf51::write_baudrate);

    config.sync_always();
    config.allow_read_write();
    config.on_write(&nrf51::write_config);

    serial_tx.set_data_width(SERIAL_8_BITS);
    serial_tx.set_baud(nrf51_baud(baudrate));
    serial_tx.set_parity(SERIAL_PARITY_NONE);
}

nrf51::~nrf51() {
    // nothing to do
}

void nrf51::reset() {
    peripheral::reset();

    while (!m_fifo.empty())
        m_fifo.pop();

    m_enabled = false;
    m_rx_enabled = false;
    m_tx_enabled = false;

    update();
}

VCML_EXPORT_MODEL(vcml::serial::nrf51, name, args) {
    return new nrf51(name);
}

} // namespace serial
} // namespace vcml
