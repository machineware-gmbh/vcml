/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/tracing/tracer_inscight.h"
#include "vcml/logging/logger.h"

namespace vcml {

#if defined(HAVE_INSCIGHT) && defined(INSCIGHT_TRANSACTION_TRACE_FW) && \
    defined(INSCIGHT_TRANSACTION_TRACE_BW)

static ::inscight::protocol_kind inscight_protocol(u64 proto_id) {
    return (::inscight::protocol_kind)proto_id;
}

void tracer_inscight::trace(const trace_activity& msg) {
    auto json = msg.to_json();
    auto kind = inscight_protocol(msg.protocol_id());
    auto time = time_to_ps(msg.t);
    if (is_backward_trace(msg.dir)) {
        // NOLINTNEXTLINE(clang-analyzer-unix.Malloc)
        INSCIGHT_TRANSACTION_TRACE_BW(msg.port, time, kind, json.c_str());
    } else {
        // NOLINTNEXTLINE(clang-analyzer-unix.Malloc)
        INSCIGHT_TRANSACTION_TRACE_FW(msg.port, time, kind, json.c_str());
    }
}

#else

void tracer_inscight::trace(const trace_activity& msg) {
    (void)msg; // nothing to do
}

#endif

tracer_inscight::tracer_inscight(): tracer() {
#if !defined(HAVE_INSCIGHT) || !defined(INSCIGHT_TRANSACTION_TRACE_FW) || \
    !defined(INSCIGHT_TRANSACTION_TRACE_BW)
    log_warn("InSCight tracing not available");
#endif
}

tracer_inscight::~tracer_inscight() {
    // nothing to do
}

} // namespace vcml
