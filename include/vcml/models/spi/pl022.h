/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_SPI_PL022_H
#define VCML_SPI_PL022_H

#include "vcml/core/types.h"
#include "vcml/core/fifo.h"
#include "vcml/core/systemc.h"
#include "vcml/core/peripheral.h"
#include "vcml/core/model.h"

#include "vcml/properties/property.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/spi.h"
#include "vcml/protocols/clk.h"

namespace vcml {
namespace spi {

class pl022 : public peripheral
{
private:
    sc_event m_ev;

    fifo<u16> m_txff;
    fifo<u16> m_rxff;

    void update_cs(bool active);
    void update_irq();
    void update_sclk();
    void transmit();

    u16 read_dr(bool debug);
    u16 read_mis(bool debug);

    void write_cr0(u16 val, bool debug);
    void write_cr1(u16 val, bool debug);
    void write_dr(u16 val, bool debug);
    void write_cpsr(u16 val, bool debug);
    void write_imsc(u16 val, bool debug);
    void write_icr(u16 val, bool debug);
    void write_dmacr(u16 val, bool debug);

public:
    reg<u16> cr0;
    reg<u16> cr1;
    reg<u16> dr;
    reg<u16> sr;
    reg<u16> cpsr;
    reg<u16> imsc;
    reg<u16> ris;
    reg<u16> mis;
    reg<u16> icr;
    reg<u16> dmacr;

    reg<u32, 4> pid;
    reg<u32, 4> cid;

    gpio_initiator_socket txintr;
    gpio_initiator_socket rxintr;
    gpio_initiator_socket rorintr;
    gpio_initiator_socket rtintr;
    gpio_initiator_socket intr;

    gpio_initiator_socket rxdmasreq;
    gpio_initiator_socket rxdmabreq;
    gpio_target_socket rxdmaclr;

    gpio_initiator_socket txdmasreq;
    gpio_initiator_socket txdmabreq;
    gpio_target_socket txdmaclr;

    clk_initiator_socket sclk;
    spi_initiator_socket spi_out;
    gpio_initiator_socket spi_cs;

    tlm_target_socket in;

    pl022(const sc_module_name& name);
    virtual ~pl022();
    VCML_KIND(spi::pl022);

    virtual void reset() override;

protected:
    virtual void handle_clock_update(hz_t oldclk, hz_t newclk) override;

    virtual void before_end_of_elaboration() override;
};

} // namespace spi
} // namespace vcml

#endif
