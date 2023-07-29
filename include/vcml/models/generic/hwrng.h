/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_GENERIC_HWRNG_H
#define VCML_GENERIC_HWRNG_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/peripheral.h"
#include "vcml/core/model.h"

#include "vcml/protocols/tlm.h"

namespace vcml {
namespace generic {

class hwrng : public peripheral
{
private:
    u32 read_rng();

public:
    reg<u32> rng;
    tlm_target_socket in;

    property<bool> pseudo;
    property<u32> seed;

    hwrng() = delete;
    hwrng(const sc_module_name& nm);
    virtual ~hwrng();
    VCML_KIND(hwrng);

    virtual void reset() override;
};

} // namespace generic
} // namespace vcml

#endif
