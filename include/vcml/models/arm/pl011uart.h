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

#ifndef VCML_ARM_PL011UART_H
#define VCML_ARM_PL011UART_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"
#include "vcml/common/range.h"

#include "vcml/ports.h"
#include "vcml/command.h"
#include "vcml/register.h"
#include "vcml/slave_socket.h"
#include "vcml/uart.h"

namespace vcml { namespace arm {

    class pl011uart: public uart
    {
    private:
        unsigned int m_fifo_size;
        queue<u16>   m_fifo;
        sc_event     m_enable;

        void poll();
        void update();

        u16 read_DR();
        u16 write_DR(u16 val);
        u8  write_RSR(u8 val);
        u16 write_IBRD(u16 val);
        u16 write_FBRD(u16 val);
        u8  write_LCR(u8 val);
        u16 write_CR(u16 val);
        u16 write_IFLS(u16 val);
        u16 write_IMSC(u16 val);
        u16 write_ICR(u16 val);

        // disabled
        pl011uart();
        pl011uart(const pl011uart&);

    public:
        enum amba_ids: u32 {
            AMBA_PID = 0x00141011, // Peripheral ID
            AMBA_CID = 0xB105F00D, // PrimeCell ID
        };

        enum : u32 {
            FIFOSIZE = 16, // FIFO size
        };

        enum dr_bits: u16 {
            DR_FE = 1 <<  8, // Framing Error
            DR_PE = 1 <<  9, // Parity Error
            DR_BE = 1 << 10, // Break Error
            DR_OE = 1 << 11, // Overrun Error
        };

        enum rsr_bits: u32 {
            RSR_O = 0x8, // RSR Flags Offset
            RSR_M = 0xF, // RSR Flags Mask
        };

        enum fr_bits: u16 {
            FR_CTS    = 1 << 0,  // Clear To Send
            FR_DSR    = 1 << 1,  // Data Set Ready
            FR_DCD    = 1 << 2,  // Data Carrier Detect
            FR_BUSY   = 1 << 3,  // Busy/Transmitting
            FR_RXFE   = 1 << 4,  // Receive FIFO Empty
            FR_TXFF   = 1 << 5,  // Transmit FIFO FULL
            FR_RXFF   = 1 << 6,  // Receive FIFO Full
            FR_TXFE   = 1 << 7,  // Transmit FIFO Empty
            FR_RI     = 1 << 8   // Ring Indicator
        };

        enum ris_bits: u32 {
            RIS_RX = 1 <<  4, // Receive Interrupt Status
            RIS_TX = 1 <<  5, // Transmit Interrupt Status
            RIS_RT = 1 <<  6, // Receive Timeout Interrupt Status
            RIS_FE = 1 <<  7, // Framing Error Interrupt Status
            RIS_PE = 1 <<  8, // Parity Error Interrupt Status
            RIS_BE = 1 <<  9, // Break Error Interrupt Status
            RIS_OE = 1 << 10, // Overrun Error Interrupt Status
            RIS_M  = 0x7F     // Raw Interrupt Status Mask
        };

        enum lcr_bits: u32 {
            LCR_BRK    = 1 <<  0, // Send Break
            LCR_PEN    = 1 <<  1, // Parity Enable
            LCR_EPS    = 1 <<  2, // Even Parity Select
            LCR_STP2   = 1 <<  3, // Two Stop Bits Select
            LCR_FEN    = 1 <<  4, // FIFO Enable
            LCR_WLEN   = 3 <<  5, // Word Length
            LCR_SPS    = 1 <<  7, // Stick Parity Select
            LCR_IBRD_M = 0xFFFF,  // Integer Baud Rate Divider Mask
            LCR_FBRD_M = 0x003F,  // Fraction Baud Raid Divider Mask
            LCR_H_M    = 0xFF     // LCR Header Mask
        };

        enum cr_bits {
            CR_UARTEN = 1 <<  0, // UART Enable
            CR_TXE    = 1 <<  8, // Transmit Enable
            CR_RXE    = 1 <<  9  // Receive Enable
        };

        reg<pl011uart, u16> DR;   // Data Register
        reg<pl011uart,  u8> RSR;  // Receive Status Register
        reg<pl011uart, u16> FR;   // Flag Register
        reg<pl011uart,  u8> ILPR; // IrDA Low-Power Counter Register
        reg<pl011uart, u16> IBRD; // Integer Baud Rate Register
        reg<pl011uart, u16> FBRD; // Fractional Baud Rate Register
        reg<pl011uart,  u8> LCR;  // Line Control Register
        reg<pl011uart, u16> CR;   // Control Register
        reg<pl011uart, u16> IFLS; // Interrupt FIFO Level select
        reg<pl011uart, u16> IMSC; // Interrupt Mask Set/Clear Register
        reg<pl011uart, u16> RIS;  // Raw Interrupt Status
        reg<pl011uart, u16> MIS;  // Masked Interrupt Status
        reg<pl011uart, u16> ICR;  // Interrupt Clear Register
        reg<pl011uart, u16> DMAC; // DMA Control

        reg<pl011uart, u32, 4> PID; // Peripheral ID Register
        reg<pl011uart, u32, 4> CID; // Cell ID Register

        slave_socket   IN;
        out_port<bool> IRQ;

        bool is_enabled()    const { return CR & CR_UARTEN; }
        bool is_rx_enabled() const { return CR & CR_RXE; }
        bool is_tx_enabled() const { return CR & CR_TXE; }

        pl011uart(const sc_module_name& name);
        virtual ~pl011uart();
        VCML_KIND(arm::pl011uart);

        virtual void reset();
    };

}}

#endif
