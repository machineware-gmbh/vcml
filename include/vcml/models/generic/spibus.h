/******************************************************************************
 *                                                                            *
 * Copyright 2019 Jan Henrik Weinstock                                        *
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

#ifndef VCML_GENERIC_SPIBUS_H
#define VCML_GENERIC_SPIBUS_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"

#include "vcml/logging/logger.h"
#include "vcml/properties/property.h"

#include "vcml/protocols/spi.h"
#include "vcml/protocols/gpio.h"

#include "vcml/ports.h"
#include "vcml/component.h"

namespace vcml {
namespace generic {

class spibus : public component, public spi_host
{
private:
    unordered_map<unsigned int, bool> m_csmode;

    // disabled
    spibus();
    spibus(const spibus&);

public:
    spi_target_socket spi_in;
    spi_initiator_socket_array<> spi_out;
    gpio_target_socket_array<> cs;

    spibus(const sc_module_name& nm);
    virtual ~spibus();
    VCML_KIND(spibus);
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

inline void spibus::set_active_high(unsigned int port, bool set) {
    m_csmode[port] = set;
}

inline void spibus::set_active_low(unsigned int port, bool set) {
    m_csmode[port] = !set;
}

} // namespace generic
} // namespace vcml

#endif
