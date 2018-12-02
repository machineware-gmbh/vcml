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

#ifndef VCML_OPENCORES_OCFBC_H
#define VCML_OPENCORES_OCFBC_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"

#include "vcml/ports.h"
#include "vcml/register.h"
#include "vcml/peripheral.h"
#include "vcml/slave_socket.h"
#include "vcml/master_socket.h"

#include "vcml/properties/property.h"
#include "vcml/debugging/vncserver.h"

namespace vcml { namespace opencores {

    class ocfbc: public peripheral
    {
    private:
        enum palette_info {
            PALETTE_ADDR = 0x800,
            PALETTE_SIZE = 0x200
        };

        const range m_palette_addr;
        u32 m_palette[PALETTE_SIZE];

        shared_ptr<debugging::vncserver> m_vnc;

        u32 read_STAT();

        u32 write_CTRL(u32 val);
        u32 write_HTIM(u32 val);
        u32 write_VTIM(u32 val);

        virtual tlm_response_status read(const range& addr, void* data,
                                         int flags) override;
        virtual tlm_response_status write(const range& addr, const void* data,
                                         int flags) override;

        // disabled
        ocfbc();
        ocfbc(const ocfbc&);

    public:
        enum control_status {
            CTLR_VEN   = 1 << 0,  /* video enable */
            CTLR_VIE   = 1 << 1,  /* vsync interrupt enable */
            CTLR_HIE   = 1 << 2,  /* hsync interrupt enable */
            CTLR_VBSIE = 1 << 3,  /* video bank switch interrupt enable */
            CTLR_CBSIE = 1 << 4,  /* CLUT bank switch interrupt enable */
            CTLR_VBSWE = 1 << 5,  /* video bank switching enable */
            CTLR_CBSWE = 1 << 6,  /* CLUT bank switching enable */
            CTLR_VBL1  = 0 << 7,  /* video memory burst length: 1 cycle */
            CTLR_VBL2  = 1 << 7,  /* video memory burst length: 2 cycles */
            CTLR_VBL4  = 2 << 7,  /* video memory burst length: 4 cycles */
            CTLR_VBL8  = 3 << 7,  /* video memory burst length: 8 cycles */
            CTLR_BPP8  = 0 << 9,  /* 8 bits per pixel */
            CTLR_BPP16 = 1 << 9,  /* 16 bits per pixel */
            CTLR_BPP24 = 2 << 9,  /* 24 bits per pixel */
            CTLR_BPP32 = 3 << 9,  /* 32 bits per pixel */
            CTLR_PC    = 1 << 11, /* 8 bit pseudo color */
        };

        reg<ocfbc, u32> CTLR;
        reg<ocfbc, u32> STAT;
        reg<ocfbc, u32> HTIM;
        reg<ocfbc, u32> VTIM;
        reg<ocfbc, u32> HVLEN;
        reg<ocfbc, u32> VBARA;

        out_port IRQ;
        slave_socket IN;
        master_socket OUT;

        property<u16> vncport;

        ocfbc(const sc_module_name& name);
        virtual ~ocfbc();
        VCML_KIND(ocfbc);
    };

}}

#endif
