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

#include "vcml/models/generic/max31855.h"

namespace vcml {
namespace generic {

u16 max31855::to_fix_point(const double in, const int decimal_pos,
                           const int size_bits) {
    double val = 0.0;
    if (in > fp_max(decimal_pos, size_bits))
        val = fp_max(decimal_pos, size_bits);
    else if (in < fp_min(decimal_pos, size_bits))
        val = fp_min(decimal_pos, size_bits);
    else
        val = in;
    return (u16)round(val * (1 << decimal_pos)) & (u16)(pow(2, size_bits) - 1);
}

void max31855::sample_temps() {
    fp_temp_thermalcouple = to_fix_point(temp_thermalcouple, 2, 14);
    fp_temp_internal = to_fix_point(temp_internal, 4, 12);
}

u8 max31855::do_spi_transport(u8 mosi) {
    u8 miso = 0;

    switch (state) {
    case BYTE0:
        miso = (u8)(fp_temp_thermalcouple >> 6);
        state = BYTE1;
        break;
    case BYTE1:
        miso = (u8)((fp_temp_thermalcouple << 2) | (fault == true));
        state = BYTE2;
        break;
    case BYTE2:
        miso = (u8)(fp_temp_internal >> 4);
        state = BYTE3;
        break;
    case BYTE3:
        miso = (u8)((fp_temp_internal << 4) | ((scv == true) << 2) |
                    ((scg == true) << 1) | (oc == true));
        state = BYTE0;
        break;
    default:
        VCML_ERROR("Unknown spi transfer state");
        break;
    }

    return miso;
}

void max31855::cs_edge() {
    while (true) {
        if (cs == cs_mode) {
            state = BYTE0;
            sample_temps();
        }
        wait();
    }
}

void max31855::spi_transport(const spi_target_socket& socket,
                             spi_payload& spi) {
    spi.miso = do_spi_transport(spi.mosi);
}

void max31855::bind(sc_signal<bool>& select, bool cs_active_high) {
    cs.bind(select);
    cs_mode = cs_active_high;
}

max31855::max31855(const sc_module_name& nm):
    module(nm),
    spi_host(),
    fp_temp_thermalcouple(0),
    fp_temp_internal(0),
    cs("cs"),
    cs_mode(true),
    state(BYTE0),
    temp_thermalcouple("temp_thermalcouple", 25),
    temp_internal("temp_internal", 10),
    fault("fault", false),
    scv("scv", false),
    scg("scg", false),
    oc("oc", false),
    spi_in("spi_in") {
    SC_HAS_PROCESS(max31855);
    SC_THREAD(cs_edge);
    sensitive << cs;
}

max31855::~max31855() {
    // nothing to do
}

} // namespace generic
} // namespace vcml
