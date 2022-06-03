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

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"
#include "vcml/common/range.h"

#include "vcml/protocols/spi.h"
#include "vcml/protocols/irq.h"

#include "vcml/ports.h"
#include "vcml/module.h"

namespace vcml {
namespace generic {

class max31855 : public module, public spi_host
{
private:
    u16 fp_temp_thermalcouple;
    u16 fp_temp_internal;

    in_port<bool> cs;
    bool cs_mode;

    inline double fp_max(const int decimal_pos, const int size_bits) {
        return (double)(((u16)pow(2, size_bits - 1)) - 1) /
               (double)(1 << decimal_pos);
    };

    inline double fp_min(const int decimal_pos, const int size_bits) {
        return (double)(-(u16)pow(2, size_bits - 1)) /
               (double)(1 << decimal_pos);
    };

    u16 to_fix_point(const double in, const int decimal_pos,
                     const int size_bits);
    void sample_temps();
    u8 do_spi_transport(u8 val);
    void cs_edge();

    enum spi_states : u8 {
        BYTE0 = 0,
        BYTE1 = 1,
        BYTE2 = 2,
        BYTE3 = 3,
    };
    u8 state;

public:
    property<double> temp_thermalcouple;
    property<double> temp_internal;

    property<bool> fault;
    property<bool> scv; // Short circuit to VCC
    property<bool> scg; // Short circuit to GND
    property<bool> oc;  // Open clamps

    spi_target_socket spi_in;

    max31855(const sc_module_name& nm);
    virtual ~max31855();
    VCML_KIND(generic::max31855);

    virtual void spi_transport(const spi_target_socket& socket,
                               spi_payload& spi) override;

    void bind(sc_signal<bool>& select, bool cs_active_high = true);
};

} // namespace generic
} // namespace vcml

#endif
