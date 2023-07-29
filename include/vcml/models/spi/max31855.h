/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_SPI_MAX31855_H
#define VCML_SPI_MAX31855_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/range.h"
#include "vcml/core/component.h"
#include "vcml/core/model.h"

#include "vcml/protocols/spi.h"
#include "vcml/protocols/gpio.h"

namespace vcml {
namespace spi {

class max31855 : public component, public spi_host
{
private:
    u16 m_fp_temp_thermocouple;
    u16 m_fp_temp_internal;

    bool m_cs_mode;

    void sample_temps();
    u8 do_spi_transport(u8 val);

    enum spi_states : u8 {
        BYTE0 = 0,
        BYTE1 = 1,
        BYTE2 = 2,
        BYTE3 = 3,
    };
    u8 m_state;

public:
    property<double> temp_thermocouple;
    property<double> temp_internal;

    property<bool> fault;
    property<bool> scv; // Short circuit to VCC
    property<bool> scg; // Short circuit to GND
    property<bool> oc;  // Open clamps

    spi_target_socket spi_in;
    gpio_target_socket cs;

    max31855(const sc_module_name& nm);
    virtual ~max31855();
    VCML_KIND(spi::max31855);

    virtual void gpio_notify(const gpio_target_socket& socket) override;

    virtual void spi_transport(const spi_target_socket& socket,
                               spi_payload& spi) override;

    void bind(gpio_initiator_socket& s, bool cs_active_high = true);
};

} // namespace spi
} // namespace vcml

#endif
