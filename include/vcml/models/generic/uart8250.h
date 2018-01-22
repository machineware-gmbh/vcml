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

#ifndef VCML_GENERIC_UART8250_H
#define VCML_GENERIC_UART8250_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"

#include "vcml/range.h"
#include "vcml/ports.h"
#include "vcml/command.h"
#include "vcml/register.h"
#include "vcml/peripheral.h"
#include "vcml/slave_socket.h"

namespace vcml { namespace generic {

    class uart8250: public peripheral
    {
    private:
        u8 m_divisor_msb;
        u8 m_divisor_lsb;

        bool m_rx_ready;
        bool m_tx_empty;

        void update();
        void poll();

        u8 read_RBR();
        u8 read_IER();
        u8 read_IIR();
        u8 read_LSR();

        u8 write_THR(u8 val);
        u8 write_IER(u8 val);
        u8 write_FCR(u8 val);

        // disabled
        uart8250();
        uart8250(const uart8250&);

    public:
        enum lsr_status {
            LSR_DR   = 1 << 0, /* line status data ready */
            LSR_THRE = 1 << 5, /* line status transmitter hold empty */
            LSR_TEMT = 1 << 6, /* line status transmitter empty */
        };

        enum ier_status {
            IER_RDA  = 1 << 0, /* enable receiver data available irq */
            IER_THRE = 1 << 1, /* enable transmitter hold empty irq */
            IER_RLS  = 1 << 2, /* enable receiver line status irq */
        };

        enum iir_status {
            IIR_NOIP = 0x1,    /* no interrupt pending */
            IIR_THRE = 0x2,    /* irq transmitter hold empty */
            IIR_RDA  = 0x4,    /* irq received data available */
            IIR_RLS  = 0x6,    /* irq receiver line status */
        };

        enum lcr_status {
            LCR_DLAB = 1 << 7, /* divisor latch access bit */
        };

        reg<uart8250, u8> THR; /* transmit hold / receive buffer */
        reg<uart8250, u8> IER; /* interrupt enable register */
        reg<uart8250, u8> IIR; /* interrupt identify register */
        reg<uart8250, u8> LCR; /* line control register */
        reg<uart8250, u8> MCR; /* modem control register */
        reg<uart8250, u8> LSR; /* line status register */
        reg<uart8250, u8> MSR; /* modem status register */
        reg<uart8250, u8> SCR; /* scratch register */

        out_port IRQ;
        slave_socket IN;

        property<clock_t> clock;

        uart8250(const sc_module_name& name);
        virtual ~uart8250();

        VCML_KIND(uart8250);
    };

}}

#endif
