/******************************************************************************
 *                                                                            *
 * Copyright 2022 Jan Henrik Weinstock                                        *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License");            *
 * you may not use this file except in compliance with the License.           *
 * You may obtain a copy of the License at                                    *
 *                                                                            *
 *     http://www.apache.org/licenses/LICENSE-2.0                             *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 *                                                                            *
 ******************************************************************************/

#include "vcml/tracing/tracer.h"
#include "vcml/logging/publisher.h"

namespace vcml {

const char* protocol_name(protocol_kind kind) {
    switch (kind) {
    case PROTO_TLM:
        return "TLM";
    case PROTO_IRQ:
        return "IRQ";
    case PROTO_PCI:
        return "PCI";
    case PROTO_SPI:
        return "SPI";
    case PROTO_SD:
        return "SD";
    case PROTO_VIRTIO:
        return "VIRTIO";
    default:
        return "unknown protocol";
    }
}

tracer::tracer() {
    all().insert(this);
}

tracer::~tracer() {
    all().erase(this);
}

void tracer::print_timing(ostream& os, const sc_time& time, u64 delta) {
    // use same formatting as logger
    publisher::print_timing(os, time, delta);
}

unordered_set<tracer*>& tracer::all() {
    static unordered_set<tracer*> tracers;
    return tracers;
}

} // namespace vcml
