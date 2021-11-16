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

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"
#include "vcml/common/range.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/irq.h"
#include "vcml/serial/port.h"

#include "vcml/ports.h"
#include "vcml/peripheral.h"

namespace vcml { namespace generic {

    class uart8250: public peripheral,
                    public serial::port
    {
    private:
        const size_t m_rx_size;
        const size_t m_tx_size;

        queue<u8> m_rx_fifo;
        queue<u8> m_tx_fifo;

        u16 m_divisor;

        void calibrate();
        void update();
        void poll();

        u8 read_RBR();
        u8 read_IER();
        u8 read_IIR();

        u8 write_THR(u8 val);
        u8 write_IER(u8 val);
        u8 write_LCR(u8 val);
        u8 write_FCR(u8 val);

        // disabled
        uart8250();
        uart8250(const uart8250&);

    public:
        enum : u32 {
            DEFAULT_BAUD = 9600
        };

        enum lsr_status {
            LSR_DR   = 1 << 0, // line status data ready
            LSR_THRE = 1 << 5, // line status transmitter hold empty
            LSR_TEMT = 1 << 6, // line status transmitter empty
        };

        enum ier_status {
            IER_RDA  = 1 << 0, // enable receiver data available irq
            IER_THRE = 1 << 1, // enable transmitter hold empty irq
            IER_RLS  = 1 << 2, // enable receiver line status irq
            IER_MST  = 1 << 3, // enable modem status irq
        };

        enum iir_status {
            IIR_NOIP = 1 << 0, // no interrupt pending
            IIR_MST  = 0 << 1, // irq modem status
            IIR_THRE = 1 << 1, // irq transmitter hold empty
            IIR_RDA  = 2 << 1, // irq received data available
            IIR_RLS  = 3 << 1, // irq receiver line status
        };

        enum lcr_status {
            LCR_WL5  = 0 << 0, // word length 5 bit
            LCR_WL6  = 1 << 0, // word length 6 bit
            LCR_WL7  = 2 << 0, // word length 7 bit
            LCR_WL8  = 3 << 0, // word length 8 bit
            LCR_STP  = 1 << 2, // stop bit control
            LCR_PEN  = 1 << 3, // parity bit enable
            LCR_EPS  = 1 << 4, // even parity select
            LCR_SPB  = 1 << 5, // stick parity bit
            LCR_BCB  = 1 << 6, // break control bit
            LCR_DLAB = 1 << 7, // divisor latch access bit
        };

        enum fcr_status {
            FCR_FE   = 1 << 0, // FIFO enable
            FCR_CRF  = 1 << 1, // Clear receiver FIFO
            FCR_CTF  = 1 << 2, // Clear transmit FIFO
            FCR_DMA  = 1 << 3, // DMA mode control
            FCR_IT1  = 0 << 6, // IRQ trigger threshold at 1 byte
            FCR_IT4  = 1 << 6, // IRQ trigger threshold at 4 bytes
            FCR_IT8  = 2 << 6, // IRQ trigger threshold at 8 bytes
            FCR_IT14 = 3 << 6, // IRQ trigger threshold at 14 bytes
        };

        reg<u8> THR; // transmit hold / receive buffer
        reg<u8> IER; // interrupt enable register
        reg<u8> IIR; // interrupt identify register
        reg<u8> LCR; // line control register
        reg<u8> MCR; // modem control register
        reg<u8> LSR; // line status register
        reg<u8> MSR; // modem status register
        reg<u8> SCR; // scratch register

        irq_initiator_socket IRQ;
        tlm_target_socket IN;

        uart8250(const sc_module_name& name);
        virtual ~uart8250();

        VCML_KIND(uart8250);

        virtual void reset() override;
    };

}}

#endif
