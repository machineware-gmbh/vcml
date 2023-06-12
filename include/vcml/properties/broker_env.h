/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_BROKER_ENV_H
#define VCML_BROKER_ENV_H

#include "vcml/properties/broker.h"

namespace vcml {

class broker_env : public broker
{
public:
    broker_env();
    virtual ~broker_env();
    VCML_KIND(broker_env);

    virtual bool defines(const string& name) const override;
    virtual bool lookup(const string& name, string& value) override;
};

} // namespace vcml

#endif
