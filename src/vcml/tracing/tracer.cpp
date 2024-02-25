/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/tracing/tracer.h"

namespace vcml {

const char* protocol_name(protocol_kind kind) {
    switch (kind) {
    case PROTO_TLM:
        return "TLM";
    case PROTO_GPIO:
        return "GPIO";
    case PROTO_CLK:
        return "CLK";
    case PROTO_PCI:
        return "PCI";
    case PROTO_I2C:
        return "I2C";
    case PROTO_SPI:
        return "SPI";
    case PROTO_SD:
        return "SD";
    case PROTO_SERIAL:
        return "SERIAL";
    case PROTO_VIRTIO:
        return "VIRTIO";
    case PROTO_ETHERNET:
        return "ETHERNET";
    case PROTO_CAN:
        return "CAN";
    case PROTO_USB:
        return "USB";
    default:
        return "unknown protocol";
    }
}

tracer::tracer(): m_mtx() {
    all().insert(this);
}

tracer::~tracer() {
    all().erase(this);
}

void tracer::print_timing(ostream& os, const sc_time& time, u64 delta) {
    // use same formatting as logger
    mwr::publisher::print_timing(os, time_to_ns(time));
}

unordered_set<tracer*>& tracer::all() {
    static unordered_set<tracer*> tracers;
    return tracers;
}

} // namespace vcml
