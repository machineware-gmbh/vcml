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

#include "vcml/spi.h"
#include "vcml/ports.h"
#include "vcml/component.h"

namespace vcml { namespace generic {

    typedef port_list<spi_initiator_socket> spi_initiator_list;

    class spibus: public component,
                  public spi_fw_transport_if,
                  public spi_bw_transport_if {
    private:
        std::map<unsigned int, bool> m_csmode;

        u8 do_spi_transport(u8 val);

        // disabled
        spibus();
        spibus(const spibus&);

    public:
        spi_target_socket  SPI_IN;
        spi_initiator_list SPI_OUT;

        in_port_list<bool> CS;

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

        virtual u8 spi_transport(u8 val) override;

        unsigned int next_free() const;

        void bind(spi_initiator_socket& initiator);
        unsigned int bind(spi_target_socket& target, sc_signal<bool>& cs,
                          bool cs_active_high = true);

        void trace_in(u8 val) const;
        void trace_out(u8 val) const;
    };

    inline void spibus::set_active_high(unsigned int port, bool set) {
        m_csmode[port] = set;
    }

    inline void spibus::set_active_low(unsigned int port, bool set) {
        m_csmode[port] = !set;
    }

    inline void spibus::trace_in(u8 val) const {
        if (logger::would_log(LOG_TRACE) && loglvl >= LOG_TRACE)
            logger::trace(name(), mkstr(">> 0x%02x", val));
    }

    inline void spibus::trace_out(u8 val) const {
        if (logger::would_log(LOG_TRACE) && loglvl >= LOG_TRACE)
            logger::trace(name(), mkstr("<< 0x%02x", val));
    }

}}

#endif
