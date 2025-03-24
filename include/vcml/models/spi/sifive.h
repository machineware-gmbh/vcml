/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_SPI_SIFIVE_H
#define VCML_SPI_SIFIVE_H

#include "vcml/core/types.h"
#include "vcml/core/fifo.h"
#include "vcml/core/systemc.h"
#include "vcml/core/peripheral.h"
#include "vcml/core/model.h"

#include "vcml/properties/property.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"
#include "vcml/protocols/spi.h"
#include "vcml/protocols/clk.h"

namespace vcml {
namespace spi {

class sifive : public peripheral
{
private:
    sc_event m_ev;

    fifo<u8> m_txff;
    fifo<u8> m_rxff;

    void write_sckdiv(u32 val);
    void write_csid(u32 val);
    void write_csdef(u32 val);
    void write_csmode(u32 val);
    void write_fmt(u32 val);
    void write_txdata(u32 val);
    void write_txmark(u32 val);
    void write_rxmark(u32 val);
    void write_ie(u32 val);

    u32 read_txdata();
    u32 read_rxdata();

    void update_cs(bool set);
    void update_sclk();
    void update_irq();

    void transmit();

public:
    property<size_t> numcs;

    reg<u32> sckdiv;
    reg<u32> sckmode;
    reg<u32> csid;
    reg<u32> csdef;
    reg<u32> csmode;
    reg<u32> delay0;
    reg<u32> delay1;
    reg<u32> fmt;
    reg<u32> txdata;
    reg<u32> rxdata;
    reg<u32> txmark;
    reg<u32> rxmark;
    reg<u32> fctrl;
    reg<u32> ffmt;
    reg<u32> ie;
    reg<u32> ip;

    clk_initiator_socket sclk;
    gpio_initiator_array<> cs;
    gpio_initiator_socket irq;
    spi_initiator_socket spi_out;
    tlm_target_socket in;

    sifive(const sc_module_name& name);
    virtual ~sifive();
    VCML_KIND(spi::sifive);

    virtual void reset() override;

protected:
    virtual void handle_clock_update(hz_t oldclk, hz_t newclk) override;
};

} // namespace spi
} // namespace vcml

#endif
