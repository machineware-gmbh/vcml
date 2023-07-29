/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_SPI_BUS_H
#define VCML_SPI_BUS_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/component.h"
#include "vcml/core/model.h"

#include "vcml/logging/logger.h"
#include "vcml/properties/property.h"

#include "vcml/protocols/spi.h"
#include "vcml/protocols/gpio.h"

namespace vcml {
namespace spi {

class bus : public component, public spi_host
{
private:
    unordered_map<unsigned int, bool> m_csmode;

    // disabled
    bus();
    bus(const bus&);

public:
    spi_target_socket spi_in;
    spi_initiator_array spi_out;
    gpio_target_array cs;

    bus(const sc_module_name& nm);
    virtual ~bus();
    VCML_KIND(spi::bus);
    virtual void reset() override;

    bool is_valid(unsigned int port) const;
    bool is_active(unsigned int port) const;
    bool is_active_high(unsigned int port) const;
    bool is_active_low(unsigned int port) const;

    void set_active_high(unsigned int port, bool set = true);
    void set_active_low(unsigned int port, bool set = true);

    virtual void spi_transport(const spi_target_socket& socket,
                               spi_payload& spi) override;

    unsigned int next_free() const;

    void bind(spi_initiator_socket& initiator);
    unsigned int bind(spi_target_socket& target, gpio_initiator_socket& cs,
                      bool cs_active_high = true);
};

inline void bus::set_active_high(unsigned int port, bool set) {
    m_csmode[port] = set;
}

inline void bus::set_active_low(unsigned int port, bool set) {
    m_csmode[port] = !set;
}

} // namespace spi
} // namespace vcml

#endif
