/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_BROKER_ARG_H
#define VCML_BROKER_ARG_H

#include "vcml/properties/broker.h"

namespace vcml {

class broker_arg : public broker
{
public:
    broker_arg() = delete;
    broker_arg(int argc, const char* const* argv);
    virtual ~broker_arg();
    VCML_KIND(broker_arg);
};

} // namespace vcml

#endif
