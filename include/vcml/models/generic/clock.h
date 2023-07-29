/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_GENERIC_CLOCK_H
#define VCML_GENERIC_CLOCK_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/module.h"
#include "vcml/core/model.h"

#include "vcml/protocols/clk.h"

namespace vcml {
namespace generic {

class clock : public module
{
public:
    property<hz_t> hz;
    clk_initiator_socket clk;

    clock() = delete;

    clock(const clock&) = delete;

    clock(const sc_module_name& nm, hz_t hz);
    virtual ~clock();
    VCML_KIND(clock);
};

} // namespace generic
} // namespace vcml

#endif
