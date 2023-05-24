/******************************************************************************
 *                                                                            *
 * Copyright 2018 Jan Henrik Weinstock                                        *
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

#ifndef VCML_SPI_OCSPI_H
#define VCML_SPI_OCSPI_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/peripheral.h"

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

    property<clock_t> clock;

    ocspi(const sc_module_name& name);
    virtual ~ocspi();
    VCML_KIND(spi::ocspi);

    virtual void reset() override;
};

} // namespace spi
} // namespace vcml

#endif
