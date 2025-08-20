/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_TRACER_INSCIGHT_H
#define VCML_TRACER_INSCIGHT_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"

#include "vcml/tracing/tracer.h"

namespace vcml {

class tracer_inscight : public tracer
{
public:
    virtual void trace(const trace_activity&) override;

    tracer_inscight();
    virtual ~tracer_inscight();
};

} // namespace vcml

#endif
