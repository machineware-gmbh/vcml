/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_SERIAL_UART8250_H
#define VCML_SERIAL_UART8250_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/range.h"
#include "vcml/core/peripheral.h"
#include "vcml/core/model.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"
#include "vcml/protocols/serial.h"

namespace vcml {
namespace serial {

class uart8250 : public peripheral, public serial_host
{
private:
    const size_t m_rx_size;
    const size_t m_tx_size;

    queue<u8> m_rx_fifo;
    queue<u8> m_tx_fifo;

    u16 m_divisor;

    void calibrate();
    void update();

    u8 read_rbr();
    u8 read_ier();
    u8 read_iir();
    u8 read_lsr();

    void write_thr(u8 val);
    void write_ier(u8 val);
    void write_lcr(u8 val);
    void write_fcr(u8 val);

    // serial_host
    void serial_receive(const serial_target_socket& socket,
                        serial_payload& tx) override;

public:
    enum : baud_t { DEFAULT_BAUD = SERIAL_9600BD };

    // clang-format off
    enum lsr_status : u8 {
        LSR_DR   = bit(0), // line status data ready
        LSR_OE   = bit(1), // line status overrun error
        LSR_PE   = bit(2), // line status parity error
        LSR_THRE = bit(5), // line status transmitter hold empty
        LSR_TEMT = bit(6), // line status transmitter empty
    };

    enum irq_status : u8 {
        IRQ_RDA  = bit(0), // enable receiver data available irq
        IRQ_THRE = bit(1), // enable transmitter hold empty irq
        IRQ_RLS  = bit(2), // enable receiver line status irq
        IRQ_MST  = bit(3), // enable modem status irq
    };

    enum iir_status : u8 {
        IIR_NOIP = bit(0), // no interrupt pending
        IIR_MST  = 0 << 1, // irq modem status
        IIR_THRE = 1 << 1, // irq transmitter hold empty
        IIR_RDA  = 2 << 1, // irq received data available
        IIR_RLS  = 3 << 1, // irq receiver line status
    };

    enum lcr_status : u8 {
        LCR_WL5  = 0 << 0, // word length 5 bit
        LCR_WL6  = 1 << 0, // word length 6 bit
        LCR_WL7  = 2 << 0, // word length 7 bit
        LCR_WL8  = 3 << 0, // word length 8 bit
        LCR_STP  = bit(2), // stop bit control
        LCR_PEN  = bit(3), // parity bit enable
        LCR_EPS  = bit(4), // even parity select
        LCR_SPB  = bit(5), // stick parity bit
        LCR_BCB  = bit(6), // break control bit
        LCR_DLAB = bit(7), // divisor latch access bit
    };

    enum fcr_status : u8 {
        FCR_FE   = bit(0), // FIFO enable
        FCR_CRF  = bit(1), // Clear receiver FIFO
        FCR_CTF  = bit(2), // Clear transmit FIFO
        FCR_DMA  = bit(3), // DMA mode control
        FCR_IT1  = 0 << 6, // IRQ trigger threshold at 1 byte
        FCR_IT4  = 1 << 6, // IRQ trigger threshold at 4 bytes
        FCR_IT8  = 2 << 6, // IRQ trigger threshold at 8 bytes
        FCR_IT14 = 3 << 6, // IRQ trigger threshold at 14 bytes
    };

    // clang-format on

    reg<u8> thr; // transmit hold / receive buffer
    reg<u8> ier; // interrupt enable register
    reg<u8> iir; // interrupt identify register
    reg<u8> lcr; // line control register
    reg<u8> mcr; // modem control register
    reg<u8> lsr; // line status register
    reg<u8> msr; // modem status register
    reg<u8> scr; // scratch register

    serial_initiator_socket serial_tx;
    serial_target_socket serial_rx;

    gpio_initiator_socket irq;
    tlm_target_socket in;

    uart8250(const sc_module_name& name);
    virtual ~uart8250();
    VCML_KIND(serial::uart8250);
    virtual void reset() override;

    uart8250() = delete;
    uart8250(const uart8250&) = delete;
};

} // namespace serial
} // namespace vcml

#endif
