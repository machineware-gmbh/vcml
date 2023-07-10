/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_CAN_BACKEND_H
#define VCML_CAN_BACKEND_H

#include "vcml/core/types.h"
#include "vcml/protocols/can.h"
#include "vcml/logging/logger.h"

namespace vcml {
namespace can {

class bridge;

class backend
{
protected:
    bridge* m_parent;
    string m_type;

public:
    logger& log;

    bridge* parent() { return m_parent; }
    const char* type() const { return m_type.c_str(); }

    backend(bridge* br);
    virtual ~backend();

    backend() = delete;
    backend(const backend&) = delete;
    backend(backend&&) = default;

    virtual void send_to_host(const can_frame& frame) = 0;
    virtual void send_to_guest(can_frame frame);

    static backend* create(bridge* br, const string& type);
};

} // namespace can
} // namespace vcml

#endif
