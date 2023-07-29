/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_META_THROTTLE_H
#define VCML_META_THROTTLE_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/module.h"
#include "vcml/core/model.h"

#include "vcml/logging/logger.h"
#include "vcml/properties/property.h"

namespace vcml {
namespace meta {

class throttle : public module
{
private:
    bool m_throttling;

    u64 m_start;
    u64 m_extra;

    void update();

public:
    property<sc_time> update_interval;
    property<double> rtf;

    throttle(const sc_module_name& nm);
    virtual ~throttle() = default;
    VCML_KIND(throttle);

    bool is_throttling() const { return m_throttling; }

protected:
    virtual void session_suspend() override;
    virtual void session_resume() override;
};

} // namespace meta
} // namespace vcml

#endif
