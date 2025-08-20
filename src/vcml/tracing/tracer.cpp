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
