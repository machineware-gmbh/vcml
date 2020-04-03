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

#ifndef VCML_OPENCORES_OCSPI_H
#define VCML_OPENCORES_OCSPI_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"

#include "vcml/spi.h"
#include "vcml/ports.h"
#include "vcml/register.h"
#include "vcml/peripheral.h"
#include "vcml/slave_socket.h"

#include "vcml/properties/property.h"

namespace vcml { namespace opencores {

    class ocspi: public peripheral, public spi_bw_transport_if
    {
    private:
        u8 m_shift_reg;
        bool m_txe_irq;
        bool m_txr_irq;

        u8 read_RXDATA();
        u8 read_TXDATA();

        u8 write_TXDATA(u8 val);
        u8 write_STATUS(u8 val);
        u32 write_CONTROL(u32 val);
        u32 write_BAUDDIV(u32 val);

        // disabled
        ocspi();
        ocspi(const ocspi&);
    public:
        enum status_bis {
            STATUS_TXE = 1 << 0, /* transfer ended */
            STATUS_TXR = 1 << 1, /* transfer ready */
        };

        reg<ocspi, u8> RXDATA;
        reg<ocspi, u8> TXDATA;
        reg<ocspi, u8> STATUS;
        reg<ocspi, u32> CONTROL;
        reg<ocspi, u32> BAUDDIV;

        out_port<bool> IRQ;

        slave_socket IN;
        spi_initiator_socket SPI_OUT;

        property<clock_t> clock;

        ocspi(const sc_module_name& name);
        virtual ~ocspi();
        VCML_KIND(ocspi);
        virtual void reset();

        void trace_in(u8 val) const;
        void trace_out(u8 val) const;
    };

    inline void ocspi::trace_in(u8 val) const {
        if (logger::would_log(LOG_TRACE) && loglvl >= LOG_TRACE)
            logger::log(LOG_TRACE, name(), mkstr("<< 0x%02x", val));
    }

    inline void ocspi::trace_out(u8 val) const {
        if (logger::would_log(LOG_TRACE) && loglvl >= LOG_TRACE)
            logger::log(LOG_TRACE, name(), mkstr(">> 0x%02x", val));
    }

}}

#endif
