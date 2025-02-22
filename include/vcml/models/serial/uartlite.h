/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_SERIAL_XILINX_UARTLITE_H
#define VCML_SERIAL_XILINX_UARTLITE_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/peripheral.h"
#include "vcml/core/model.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"
#include "vcml/protocols/serial.h"

namespace vcml {
namespace serial {

class uartlite : public peripheral, public serial_host
{
private:
    queue<u8> m_tx_fifo;
    queue<u8> m_rx_fifo;
    sc_event m_txev;

    void tx_thread();

    u32 read_rx_fifo();
    u32 read_status();

    void write_tx_fifo(u32 val);
    void write_control(u32 val);

    // serial host
    virtual void serial_receive(u8 data) override;

public:
    property<int> baudrate;
    property<int> databits;
    property<bool> use_parity;
    property<bool> odd_parity;

    reg<u32> rx_fifo;
    reg<u32> tx_fifo;
    reg<u32> status;
    reg<u32> control;

    tlm_target_socket in;
    gpio_initiator_socket irq;

    serial_initiator_socket serial_tx;
    serial_target_socket serial_rx;

    uartlite(const sc_module_name& name);
    virtual ~uartlite();
    VCML_KIND(serial::uartlite);

    virtual void reset() override;
};

} // namespace serial
} // namespace vcml

#endif
