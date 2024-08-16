/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_I2C_SIFIVE_H
#define VCML_I2C_SIFIVE_H

#include "vcml/core/types.h"
#include "vcml/core/range.h"
#include "vcml/core/systemc.h"
#include "vcml/core/peripheral.h"
#include "vcml/core/model.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"
#include "vcml/protocols/i2c.h"

#include "vcml/models/i2c/oci2c.h"

namespace vcml {
namespace i2c {

class sifive : public oci2c
{
public:
    sifive(const sc_module_name& nm);
    virtual ~sifive();
    VCML_KIND(i2c::sifive);
    virtual void reset() override;
};

} // namespace i2c
} // namespace vcml

#endif
