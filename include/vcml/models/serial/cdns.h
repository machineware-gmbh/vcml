/******************************************************************************
 *                                                                            *
 * Copyright (C) 2023 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_SERIAL_CDNS_H
#define VCML_SERIAL_CDNS_H

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

class cdns : public peripheral, public serial_host
{
private:
    queue<u8> m_rxff;
    queue<u8> m_txff;

    sc_event m_txev;

    void push_rxff(u8 val);
    void push_txff(u8 val);

    void write_cr(u32 val);
    void write_mr(u32 val);
    void write_ier(u32 val);
    void write_idr(u32 val);
    void write_isr(u32 val);
    void write_txrx(u32 val);

    u32 read_txrx();

    void tx_thread();
    void update_irq();

    virtual void serial_receive(u8 data) override;

public:
    property<size_t> rxff_size;
    property<size_t> txff_size;

    reg<u32> cr;
    reg<u32> mr;
    reg<u32> ier;
    reg<u32> idr;
    reg<u32> imr;
    reg<u32> isr;
    reg<u32> brgr;
    reg<u32> rtor;
    reg<u32> rtrig;
    reg<u32> mcr;
    reg<u32> msr;
    reg<u32> sr;
    reg<u32> txrx;
    reg<u32> bdiv;
    reg<u32> fdel;
    reg<u32> pmin;
    reg<u32> pwid;
    reg<u32> ttrig;

    tlm_target_socket in;
    gpio_initiator_socket irq;

    serial_initiator_socket serial_tx;
    serial_target_socket serial_rx;

    cdns(const sc_module_name& name);
    virtual ~cdns();
    VCML_KIND(serial::cdns);

    virtual void reset() override;
};

} // namespace serial
} // namespace vcml

#endif
