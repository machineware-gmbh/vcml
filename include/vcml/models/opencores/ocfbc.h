/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_OPENCORES_OCFBC_H
#define VCML_OPENCORES_OCFBC_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/peripheral.h"
#include "vcml/core/model.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"

#include "vcml/properties/property.h"
#include "vcml/ui/console.h"

namespace vcml {
namespace opencores {

class ocfbc : public peripheral
{
private:
    enum palette_info : u64 {
        PALETTE_ADDR = 0x800,
        PALETTE_SIZE = 0x200 // 2x 256x4bytes
    };

    ui::console m_console;

    const range m_palette_addr;
    u32 m_palette[PALETTE_SIZE];

    u8* m_fb;

    u32 m_xres;
    u32 m_yres;
    u32 m_bpp;
    bool m_pc;

    u32 read_stat();

    void write_stat(u32 val);
    void write_ctrl(u32 val);
    void write_htim(u32 val);
    void write_vtim(u32 val);

    virtual tlm_response_status read(const range& addr, void* data,
                                     const tlm_sbi& info) override;
    virtual tlm_response_status write(const range& addr, const void* data,
                                      const tlm_sbi& info) override;

    sc_event m_enable;

    void create();
    void render();
    void update();

    bool cmd_info(const vector<string>& args, ostream& os);

    // disabled
    ocfbc();
    ocfbc(const ocfbc&);

public:
    enum control_bits : u32 {
        CTLR_VEN = 1 << 0,   // video enable
        CTLR_VIE = 1 << 1,   // vsync interrupt enable
        CTLR_HIE = 1 << 2,   // hsync interrupt enable
        CTLR_VBSIE = 1 << 3, // video bank switch interrupt enable
        CTLR_CBSIE = 1 << 4, // CLUT bank switch interrupt enable
        CTLR_VBSWE = 1 << 5, // video bank switching enable
        CTLR_CBSWE = 1 << 6, // CLUT bank switching enable
        CTLR_VBL1 = 0 << 7,  // video memory burst length: 1 cycle
        CTLR_VBL2 = 1 << 7,  // video memory burst length: 2 cycles
        CTLR_VBL4 = 2 << 7,  // video memory burst length: 4 cycles
        CTLR_VBL8 = 3 << 7,  // video memory burst length: 8 cycles
        CTLR_BPP8 = 0 << 9,  // 8 bits per pixel
        CTLR_BPP16 = 1 << 9, // 16 bits per pixel
        CTLR_BPP24 = 2 << 9, // 24 bits per pixel
        CTLR_BPP32 = 3 << 9, // 32 bits per pixel
        CTLR_PC = 1 << 11,   // 8 bit pseudo color
    };

    enum status_bits : u32 {
        STAT_SINT = 1 << 0,   // system error interrupt pending
        STAT_LUINT = 1 << 1,  // line FIFO underrun interrupt pending
        STAT_VINT = 1 << 4,   // vertical interrupt pending
        STAT_HINT = 1 << 5,   // horizontal interrupt pending
        STAT_VBSINT = 1 << 6, // video bank switch interrupt pending
        STAT_CBSINT = 1 << 7, // CLUT bank switch interrupt pending
        STAT_AVMP = 1 << 16,  // active video memory page
        STAT_ACMP = 1 << 17,  // active CLUT memory page
        STAT_HC0A = 1 << 20,  // hardware cursor 0 available
        STAT_HC1A = 1 << 24,  // hardware cursor 1 available
    };

    reg<u32> ctlr;
    reg<u32> stat;
    reg<u32> htim;
    reg<u32> vtim;
    reg<u32> hvlen;
    reg<u32> vbara;
    reg<u32> vbarb;

    gpio_initiator_socket irq;
    tlm_target_socket in;
    tlm_initiator_socket out;

    property<hz_t> clock;

    ocfbc(const sc_module_name& name);
    virtual ~ocfbc();
    VCML_KIND(opencores::ocfbc);

protected:
    virtual void end_of_simulation() override;
};

} // namespace opencores
} // namespace vcml

#endif
