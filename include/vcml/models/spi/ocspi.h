/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_SPI_OCSPI_H
#define VCML_SPI_OCSPI_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/peripheral.h"
#include "vcml/core/model.h"

#include "vcml/properties/property.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"
#include "vcml/protocols/spi.h"

namespace vcml {
namespace spi {

class ocspi : public peripheral
{
private:
    bool m_txe_irq;
    bool m_txr_irq;

    void write_txdata(u8 val);
    void write_status(u8 val);
    void write_control(u32 val);
    void write_bauddiv(u32 val);

    // disabled
    ocspi();
    ocspi(const ocspi&);

public:
    enum status_bis {
        STATUS_TXE = 1 << 0, // transfer ended
        STATUS_TXR = 1 << 1, // transfer ready
    };

    reg<u8> rxdata;
    reg<u8> txdata;
    reg<u8> status;
    reg<u32> control;
    reg<u32> bauddiv;

    gpio_initiator_socket irq;
    tlm_target_socket in;
    spi_initiator_socket spi_out;

    property<hz_t> clock;

    ocspi(const sc_module_name& name);
    virtual ~ocspi();
    VCML_KIND(spi::ocspi);

    virtual void reset() override;
};

} // namespace spi
} // namespace vcml

#endif
