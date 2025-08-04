/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_SPI_MCP3208_H
#define VCML_SPI_MCP3208_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/range.h"
#include "vcml/core/component.h"
#include "vcml/core/model.h"

#include "vcml/protocols/spi.h"
#include "vcml/protocols/gpio.h"

namespace vcml {
namespace spi {

class mcp3208 : public module, public spi_host, public gpio_host
{
private:
    bool m_single;
    int m_bitidx;
    u16 m_chanid;
    i16 m_buffer;

    u16 read_voltage();
    bool sample_bit(bool in);

    virtual void gpio_transport(const gpio_target_socket& socket,
                                gpio_payload& tx) override;
    virtual void spi_transport(const spi_target_socket& socket,
                               spi_payload& spi) override;

    bool cmd_get_voltage(const vector<string>& args, ostream& os);
    bool cmd_set_voltage(const vector<string>& args, ostream& os);

public:
    property<bool> csmode;

    property<double> vref;
    property<double> v0;
    property<double> v1;
    property<double> v2;
    property<double> v3;
    property<double> v4;
    property<double> v5;
    property<double> v6;
    property<double> v7;

    spi_target_socket spi_in;
    gpio_target_socket spi_cs;

    mcp3208(const sc_module_name& nm);
    virtual ~mcp3208();
    VCML_KIND(spi::mcp3208);
};

} // namespace spi
} // namespace vcml

#endif
