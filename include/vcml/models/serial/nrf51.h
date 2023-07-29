/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_SERIAL_NRF51_H
#define VCML_SERIAL_NRF51_H

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

class nrf51 : public peripheral, public serial_host
{
private:
    static constexpr size_t FIFO_SIZE = 6;

    queue<u8> m_fifo;

    bool m_enabled;
    bool m_rx_enabled;
    bool m_tx_enabled;

    bool is_enabled() const { return m_enabled; }
    bool is_rx_enabled() const { return is_enabled() && m_rx_enabled; }
    bool is_tx_enabled() const { return is_enabled() && m_tx_enabled; }

    u32 read_rxd();

    void write_startrx(u32 val);
    void write_stoprx(u32 val);
    void write_starttx(u32 val);
    void write_stoptx(u32 val);
    void write_suspend(u32 val);
    void write_enable(u32 val);
    void write_inten(u32 val);
    void write_intenset(u32 val);
    void write_intenclr(u32 val);
    void write_errsrc(u32 val);
    void write_txd(u32 val);
    void write_baudrate(u32 val);
    void write_config(u32 val);

    void update();

    // serial_host
    void serial_receive(u8 data) override;

public:
    reg<u32> startrx;
    reg<u32> stoprx;
    reg<u32> starttx;
    reg<u32> stoptx;
    reg<u32> suspend;

    reg<u32> cts;
    reg<u32> ncts;
    reg<u32> rxdrdy;
    reg<u32> txdrdy;
    reg<u32> error;
    reg<u32> rxto;

    reg<u32> inten;
    reg<u32> intenset;
    reg<u32> intenclr;
    reg<u32> errsrc;
    reg<u32> enable;
    reg<u32> pselrts;
    reg<u32> pseltxd;
    reg<u32> pselcts;
    reg<u32> pselrxd;
    reg<u32> rxd;
    reg<u32> txd;
    reg<u32> baudrate;
    reg<u32> config;

    serial_initiator_socket serial_tx;
    serial_target_socket serial_rx;

    gpio_initiator_socket irq;
    tlm_target_socket in;

    nrf51(const sc_module_name& name);
    virtual ~nrf51();
    VCML_KIND(serial::nrf51);
    virtual void reset() override;
};

} // namespace serial
} // namespace vcml

#endif
