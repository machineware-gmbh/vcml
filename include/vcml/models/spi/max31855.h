/******************************************************************************
 *                                                                            *
 * Copyright 2022 Simon Winther, Jan Henrik Weinstock                         *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License");            *
 * you may not use this file except in compliance with the License.           *
 * You may obtain a copy of the License at                                    *
 *                                                                            *
 *     http://www.apache.org/licenses/LICENSE-2.0                             *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_MODELS_MAX31855_H
#define VCML_MODELS_MAX31855_H

#include "vcml/core/types.h"
#include "vcml/core/report.h"
#include "vcml/core/systemc.h"
#include "vcml/core/range.h"
#include "vcml/core/component.h"

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

    static u16 to_fp_14_2(const double t);
    static u16 to_fp_12_4(const double t);

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
