/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_BROKER_LUA_H
#define VCML_BROKER_LUA_H

#include "vcml/properties/broker.h"

namespace vcml {

class broker_lua : public broker
{
public:
    broker_lua() = delete;
    broker_lua(const string& filename);
    virtual ~broker_lua();
    VCML_KIND(broker_lua);
};

} // namespace vcml

#endif
