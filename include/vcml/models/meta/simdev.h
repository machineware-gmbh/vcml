/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_META_SIMDEV_H
#define VCML_META_SIMDEV_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/peripheral.h"
#include "vcml/core/model.h"

#include "vcml/protocols/tlm.h"

namespace vcml {
namespace meta {

class simdev : public peripheral
{
private:
    void write_stop(u32 val);
    void write_exit(u32 val);
    void write_abrt(u32 val);

    u64 read_sclk();
    u64 read_hclk();

    void write_sout(u32 val);
    void write_serr(u32 val);

    u32 read_prng();

    // disabled
    simdev();
    simdev(const simdev&);

public:
    // simulation control
    reg<u32> stop;
    reg<u32> exit;
    reg<u32> abrt;

    // timing
    reg<u64> sclk;
    reg<u64> hclk;

    // output
    reg<u32> sout;
    reg<u32> serr;

    // random
    reg<u32> prng;

    tlm_target_socket in;

    simdev(const sc_module_name& name);
    virtual ~simdev();
    VCML_KIND(simdev);

    virtual void reset() override;
};

} // namespace meta
} // namespace vcml

#endif
