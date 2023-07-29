/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_ARM_SYSCON_H
#define VCML_ARM_SYSCON_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/range.h"
#include "vcml/core/model.h"
#include "vcml/core/peripheral.h"

#include "vcml/protocols/tlm.h"

namespace vcml {
namespace arm {

class syscon : public peripheral
{
private:
    u32 read_clc_k24();

    void write_lockval(u16 val);
    void write_flags_s(u32 val);
    void write_flags_c(u32 val);
    void write_nvflags_s(u32 val);
    void write_nvflags_c(u32 val);
    void write_sys_cfgctrl(u32 val);
    void write_sys_cfgstat(u32 val);

    // disabled
    syscon();
    syscon(const syscon&);

public:
    enum sysid_bits : u32 {
        SYSID_VERSATILE = 0x41007004,
        SYSID_VEXPRESS = 0x1190f500
    };

    enum procid_bits : u32 {
        PROCID_VERSATILE = 0x02000000,
        PROCID_VEXPRESS = 0x0c000191
    };

    enum lockval_bits : u16 {
        LOCKVAL_LOCK = 0xa05f,
        LOCKVAL_M = 0x7fff,
    };

    reg<u32> sys_id;
    reg<u16> lockval;
    reg<u32> cfgdata0;
    reg<u32> cfgdata1;
    reg<u32> flags_s;
    reg<u32> flags_c;
    reg<u32> nvflags_s;
    reg<u32> nvflags_c;
    reg<u32> clck24;
    reg<u32> proc_id;
    reg<u32> sys_cfgdata;
    reg<u32> sys_cfgctrl;
    reg<u32> sys_cfgstat;

    tlm_target_socket in;

    syscon(const sc_module_name& name);
    virtual ~syscon();
    VCML_KIND(arm::syscon);

    void setup_versatile();
    void setup_vexpress();
};

inline void syscon::setup_versatile() {
    sys_id.set_default(SYSID_VERSATILE);
    proc_id.set_default(PROCID_VERSATILE);
}

inline void syscon::setup_vexpress() {
    sys_id.set_default(SYSID_VEXPRESS);
    proc_id.set_default(PROCID_VEXPRESS);
}

} // namespace arm
} // namespace vcml

#endif
