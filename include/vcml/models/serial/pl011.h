/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_SERIAL_PL011_H
#define VCML_SERIAL_PL011_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/range.h"
#include "vcml/core/peripheral.h"
#include "vcml/core/model.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"
#include "vcml/protocols/serial.h"

namespace vcml {
namespace serial {

class pl011 : public peripheral, public serial_host
{
private:
    unsigned int m_fifo_size;
    queue<u16> m_fifo;

    // serial host
    virtual void serial_receive(u8 data) override;

    void update();

    u16 read_dr();
    void write_dr(u16 val);
    void write_rsr(u8 val);
    void write_ibrd(u16 val);
    void write_fbrd(u16 val);
    void write_lcr(u8 val);
    void write_cr(u16 val);
    void write_ifls(u16 val);
    void write_imsc(u16 val);
    void write_icr(u16 val);

    // disabled
    pl011();
    pl011(const pl011&);

public:
    enum amba_ids : u32 {
        AMBA_PID = 0x00141011, // Peripheral ID
        AMBA_CID = 0xb105f00d, // PrimeCell ID
    };

    enum : u32 {
        FIFOSIZE = 16, // FIFO size
    };

    enum dr_bits : u16 {
        DR_FE = 1 << 8,  // Framing Error
        DR_PE = 1 << 9,  // Parity Error
        DR_BE = 1 << 10, // Break Error
        DR_OE = 1 << 11, // Overrun Error
    };

    enum rsr_bits : u32 {
        RSR_O = 0x8, // RSR Flags Offset
        RSR_M = 0xf, // RSR Flags Mask
    };

    enum fr_bits : u16 {
        FR_CTS = 1 << 0,  // Clear To Send
        FR_DSR = 1 << 1,  // Data Set Ready
        FR_DCD = 1 << 2,  // Data Carrier Detect
        FR_BUSY = 1 << 3, // Busy/Transmitting
        FR_RXFE = 1 << 4, // Receive FIFO Empty
        FR_TXFF = 1 << 5, // Transmit FIFO FULL
        FR_RXFF = 1 << 6, // Receive FIFO Full
        FR_TXFE = 1 << 7, // Transmit FIFO Empty
        FR_RI = 1 << 8    // Ring Indicator
    };

    enum ris_bits : u32 {
        RIS_RX = 1 << 4,  // Receive Interrupt Status
        RIS_TX = 1 << 5,  // Transmit Interrupt Status
        RIS_RT = 1 << 6,  // Receive Timeout Interrupt Status
        RIS_FE = 1 << 7,  // Framing Error Interrupt Status
        RIS_PE = 1 << 8,  // Parity Error Interrupt Status
        RIS_BE = 1 << 9,  // Break Error Interrupt Status
        RIS_OE = 1 << 10, // Overrun Error Interrupt Status
        RIS_M = 0x7f      // Raw Interrupt Status Mask
    };

    enum lcr_bits : u32 {
        LCR_BRK = 1 << 0,    // Send Break
        LCR_PEN = 1 << 1,    // Parity Enable
        LCR_EPS = 1 << 2,    // Even Parity Select
        LCR_STP2 = 1 << 3,   // Two Stop Bits Select
        LCR_FEN = 1 << 4,    // FIFO Enable
        LCR_WLEN = 3 << 5,   // Word Length
        LCR_SPS = 1 << 7,    // Stick Parity Select
        LCR_IBRD_M = 0xffff, // Integer Baud Rate Divider Mask
        LCR_FBRD_M = 0x003f, // Fraction Baud Raid Divider Mask
        LCR_H_M = 0xff       // LCR Header Mask
    };

    enum cr_bits {
        CR_UARTEN = 1 << 0, // UART Enable
        CR_TXE = 1 << 8,    // Transmit Enable
        CR_RXE = 1 << 9     // Receive Enable
    };

    reg<u16> dr;   // Data Register
    reg<u8> rsr;   // Receive Status Register
    reg<u16> fr;   // Flag Register
    reg<u8> ilpr;  // IrDA Low-Power Counter Register
    reg<u16> ibrd; // Integer Baud Rate Register
    reg<u16> fbrd; // Fractional Baud Rate Register
    reg<u8> lcr;   // Line Control Register
    reg<u16> cr;   // Control Register
    reg<u16> ifls; // Interrupt FIFO Level select
    reg<u16> imsc; // Interrupt Mask Set/Clear Register
    reg<u16> ris;  // Raw Interrupt Status
    reg<u16> mis;  // Masked Interrupt Status
    reg<u16> icr;  // Interrupt Clear Register
    reg<u16> dmac; // DMA Control

    reg<u32, 4> pid; // Peripheral ID Register
    reg<u32, 4> cid; // Cell ID Register

    tlm_target_socket in;
    gpio_initiator_socket irq;

    serial_initiator_socket serial_tx;
    serial_target_socket serial_rx;

    bool is_enabled() const { return cr & CR_UARTEN; }
    bool is_rx_enabled() const { return cr & CR_RXE; }
    bool is_tx_enabled() const { return cr & CR_TXE; }

    pl011(const sc_module_name& name);
    virtual ~pl011();
    VCML_KIND(serial::pl011);

    virtual void reset() override;
};

} // namespace serial
} // namespace vcml

#endif
