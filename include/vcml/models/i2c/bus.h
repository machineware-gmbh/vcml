/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_I2C_BUS_H
#define VCML_I2C_BUS_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/component.h"
#include "vcml/core/model.h"

#include "vcml/logging/logger.h"
#include "vcml/properties/property.h"

#include "vcml/protocols/i2c.h"

namespace vcml {
namespace i2c {

class bus : public component, public i2c_host
{
public:
    bus() = delete;
    bus(const bus&) = delete;
    bus(const sc_module_name& nm);
    virtual ~bus() = default;
    VCML_KIND(i2c::bus);

    i2c_target_socket i2c_in;
    i2c_initiator_array<> i2c_out;

    virtual void reset() override;

    virtual void i2c_transport(i2c_target_socket& socket,
                               i2c_payload& tx) override;

    unsigned int next_free() const;

    void bind(i2c_initiator_socket& initiator);
    unsigned int bind(i2c_target_socket& target);
};

} // namespace i2c
} // namespace vcml

#endif
