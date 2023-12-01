/******************************************************************************
 *                                                                            *
 * Copyright (C) 2023 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_SERIAL_SIFIVE_UART_H
#define VCML_SERIAL_SIFIVE_UART_H

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

class sifive_uart : public peripheral, public serial_host
{
private:
    static const u32 SIFIVE_UART_TXDATA_FULL   = 1U << 31U;
    static const u32 SIFIVE_UART_RXDATA_EMPTY  = 1U << 31U;
    static const u32 SIFIVE_UART_TXCTRL_TXEN   = 1U;
    static const u32 SIFIVE_UART_TXCTRL_NSTOP  = 1U << 1U;
    static const u32 SIFIVE_UART_RXCTRL_RXEN   = 1U;
    static const u32 SIFIVE_UART_WM_OFFSET     = 16U;
    static const u32 SIFIVE_UART_DIV_MASK      = 0xFFFFU;

    static const u32 SIFIVE_UART_IE_TXWM       = 1; /* Transmit watermark interrupt enable */
    static const u32 SIFIVE_UART_IE_RXWM       = 2; /* Receive watermark interrupt enable */

    static const u32 SIFIVE_UART_IP_TXWM       = 1; /* Transmit watermark interrupt pending */
    static const u32 SIFIVE_UART_IP_RXWM       = 2; /* Receive watermark interrupt pending */

    queue<u8> m_tx_fifo;
    queue<u8> m_rx_fifo;

    u32 get_tx_cnt();
    u32 get_rx_cnt();
    u32 get_ip();
    void update_irq();
    void flush_tx_fifo();

    u32 read_txdata();
    void write_txdata(u32 val);

    u32 read_rxdata();

    u32 read_txctrl();
    void write_txctrl(u32 val);

    u32 read_rxctrl();
    void write_rxctrl(u32 val);

    u32 read_ie();
    void write_ie(u32 val);

    u32 read_ip();

    u32 read_div();
    void write_div(u32 val);

    // serial_host
    void serial_receive(const serial_target_socket& socket,
                        serial_payload& tx) override;
public:
    property<u64> tx_fifo_size;
    property<u64> rx_fifo_size;

    enum : baud_t { DEFAULT_BAUD = SERIAL_115200BD };

    reg<u32> txdata; // Transmit data register
    reg<u32> rxdata; // Receive data register
    reg<u32> txctrl; // Transmit control register
    reg<u32> rxctrl; // Receive control register
    reg<u32> ie;     // UART interrupt enable
    reg<u32> ip;     // UART interrupt pending
    reg<u32> div;    // Baud rate divisor

    serial_initiator_socket serial_tx;
    serial_target_socket serial_rx;

    gpio_initiator_socket irq;
    tlm_target_socket in;

    sifive_uart(const sc_module_name& name);
    virtual ~sifive_uart();
    VCML_KIND(serial::sifive_uart);
    virtual void reset() override;

    sifive_uart() = delete;
    sifive_uart(const sifive_uart&) = delete;
};

} // namespace serial
} // namespace vcml

#endif
