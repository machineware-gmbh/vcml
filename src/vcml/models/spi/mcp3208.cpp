/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/spi/mcp3208.h"

namespace vcml {
namespace spi {

static i16 mcp3208_convert(double vref, double v1, double v2) {
    return 4096.0 * (v1 - v2) / vref;
}

u16 mcp3208::read_voltage() {
    if (m_single) {
        switch (m_chanid) {
        case 0:
            return mcp3208_convert(vref, v0, 0.0);
        case 1:
            return mcp3208_convert(vref, v1, 0.0);
        case 2:
            return mcp3208_convert(vref, v2, 0.0);
        case 3:
            return mcp3208_convert(vref, v3, 0.0);
        case 4:
            return mcp3208_convert(vref, v4, 0.0);
        case 5:
            return mcp3208_convert(vref, v5, 0.0);
        case 6:
            return mcp3208_convert(vref, v6, 0.0);
        case 7:
        default:
            return mcp3208_convert(vref, v7, 0.0);
        }
    } else {
        switch (m_chanid) {
        case 0:
            return mcp3208_convert(vref, v0, v1);
        case 1:
            return mcp3208_convert(vref, v1, v0);
        case 2:
            return mcp3208_convert(vref, v2, v3);
        case 3:
            return mcp3208_convert(vref, v3, v2);
        case 4:
            return mcp3208_convert(vref, v4, v5);
        case 5:
            return mcp3208_convert(vref, v5, v4);
        case 6:
            return mcp3208_convert(vref, v6, v7);
        case 7:
        default:
            return mcp3208_convert(vref, v7, v6);
        }
    }
}

bool mcp3208::sample_bit(bool inbit) {
    switch (m_bitidx) {
    case 0:
        if (inbit)
            m_bitidx++;
        return false;

    case 1:
        m_single = inbit;
        m_chanid = 0;
        m_bitidx++;
        return false;

    case 2:
    case 3:
    case 4:
        m_chanid |= (u8)inbit << (4 - m_bitidx);
        m_bitidx++;
        return false;

    case 5:
        m_bitidx++;
        return false;

    case 6:
        m_buffer = read_voltage();
        log_debug("read voltage%hu 0x%04hx", m_chanid, m_buffer);
        m_bitidx++;
        return false;

    default:
        break;
    }

    if (m_bitidx >= 7 && m_bitidx <= 18)
        return m_buffer & bit(18 - m_bitidx++);

    return false;
}

void mcp3208::gpio_notify(const gpio_target_socket& socket) {
    m_bitidx = 0;
}

void mcp3208::spi_transport(const spi_target_socket& socket,
                            spi_payload& spi) {
    if (spi_cs == csmode) {
        u8 input = spi.mosi;
        u8 output = 0;

        for (int i = 7; i >= 0; i--) {
            u8 mask = 1u << i;
            if (sample_bit(input & mask))
                output |= mask;
        }

        spi.miso = output;
    }
}

mcp3208::mcp3208(const sc_module_name& nm):
    component(nm),
    spi_host(),
    m_bitidx(),
    m_chanid(),
    m_buffer(),
    csmode("csmode", false), // cs active low
    vref("vref", 5.0),
    v0("v0", 0.0),
    v1("v1", 1.0),
    v2("v2", 2.5),
    v3("v3", 3.0),
    v4("v4", 3.3),
    v5("v5", 4.0),
    v6("v6", 4.5),
    v7("v7", 5.0),
    spi_in("spi_in"),
    spi_cs("spi_cs") {
    clk.stub();
    rst.stub();
}

mcp3208::~mcp3208() {
    // nothing to do
}

VCML_EXPORT_MODEL(vcml::spi::mcp3208, name, args) {
    return new mcp3208(name);
}

} // namespace spi
} // namespace vcml
