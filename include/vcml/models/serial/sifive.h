/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_SERIAL_SIVIVE_UART_H
#define VCML_SERIAL_SIVIVE_UART_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/peripheral.h"
#include "vcml/core/model.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"
#include "vcml/protocols/serial.h"

namespace vcml {
namespace serial {

class sifive : public peripheral, public serial_host
{
private:
    queue<u8> m_tx_fifo;
    queue<u8> m_rx_fifo;
    sc_event m_txev;

    void update_tx();
    void update_rx();
    void tx_thread();

    u32 read_txdata();
    u32 read_rxdata();

    void write_txdata(u32 val);
    void write_txctrl(u32 val);
    void write_rxctrl(u32 val);
    void write_ie(u32 val);
    void write_div(u32 val);

    // serial host
    virtual void serial_receive(u8 data) override;

public:
    property<size_t> tx_fifo_size;
    property<size_t> rx_fifo_size;

    reg<u32> txdata;
    reg<u32> rxdata;
    reg<u32> txctrl;
    reg<u32> rxctrl;
    reg<u32> ie;
    reg<u32> ip;
    reg<u32> div;

    tlm_target_socket in;
    gpio_initiator_socket tx_irq;
    gpio_initiator_socket rx_irq;

    serial_initiator_socket serial_tx;
    serial_target_socket serial_rx;

    bool is_tx_full() const;
    bool is_rx_empty() const;
    bool is_tx_enabled() const;
    bool is_rx_enabled() const;

    serial_stop num_stop_bits() const;

    size_t tx_watermark() const;
    size_t rx_watermark() const;

    sifive(const sc_module_name& name);
    virtual ~sifive();
    VCML_KIND(serial::sifive);

    virtual void reset() override;
};

} // namespace serial
} // namespace vcml

#endif
