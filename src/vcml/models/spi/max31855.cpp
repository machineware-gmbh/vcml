/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/spi/max31855.h"

namespace vcml {
namespace spi {

static u16 to_fp_14_2(double t) {
    if (t > 2047.5)
        return 0x1fff;
    if (t < -2048.0)
        return 0x2000;
    return (i16)round(t * 4) & 0x3fff;
}

static u16 to_fp_12_4(double t) {
    if (t > 127.9375)
        return 0x7ff;
    if (t < -128)
        return 0x800;
    return (i16)round(t * 16) & 0xfff;
}

void max31855::sample_temps() {
    m_fp_temp_thermocouple = to_fp_14_2(temp_thermocouple);
    m_fp_temp_internal = to_fp_12_4(temp_internal);
}

u8 max31855::do_spi_transport(u8 mosi) {
    u8 miso = 0;

    switch (m_state) {
    case BYTE0:
        miso = (u8)(m_fp_temp_thermocouple >> 6);
        m_state = BYTE1;
        break;
    case BYTE1:
        miso = (u8)((m_fp_temp_thermocouple << 2) | (fault == true));
        m_state = BYTE2;
        break;
    case BYTE2:
        miso = (u8)(m_fp_temp_internal >> 4);
        m_state = BYTE3;
        break;
    case BYTE3:
        miso = (u8)((m_fp_temp_internal << 4) | ((scv == true) << 2) |
                    ((scg == true) << 1) | (oc == true));
        m_state = BYTE0;
        break;
    default:
        VCML_ERROR("Unknown spi transfer state");
        break;
    }

    return miso;
}

void max31855::gpio_notify(const gpio_target_socket& socket) {
    if (socket == cs && cs == m_cs_mode) {
        m_state = BYTE0;
        sample_temps();
    }
}

void max31855::spi_transport(const spi_target_socket& socket,
                             spi_payload& spi) {
    spi.miso = do_spi_transport(spi.mosi);
}

void max31855::bind(gpio_initiator_socket& s, bool cs_active_high) {
    s.bind(cs);
    m_cs_mode = cs_active_high;
}

max31855::max31855(const sc_module_name& nm):
    component(nm),
    spi_host(),
    m_fp_temp_thermocouple(25.0),
    m_fp_temp_internal(10.0),
    m_cs_mode(true),
    m_state(BYTE0),
    temp_thermocouple("temp_thermocouple", 25),
    temp_internal("temp_internal", 10),
    fault("fault", false),
    scv("scv", false),
    scg("scg", false),
    oc("oc", false),
    spi_in("spi_in"),
    cs("cs") {
    clk.stub();
    rst.stub();
}

max31855::~max31855() {
    // nothing to do
}

VCML_EXPORT_MODEL(vcml::spi::max31855, name, args) {
    return new max31855(name);
}

} // namespace spi
} // namespace vcml
