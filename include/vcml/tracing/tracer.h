/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_TRACING_TRACER_H
#define VCML_TRACING_TRACER_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"

#include "vcml/tracing/protocol.h"
#include "vcml/tracing/activity.h"

namespace vcml {

class tracer
{
private:
    mutable mutex m_mtx;

public:
    tracer();
    virtual ~tracer();

    virtual void trace(const trace_activity& act) = 0;

    template <typename PAYLOAD>
    void do_trace(const trace_activity_proto<PAYLOAD>& msg) {
        lock_guard<mutex> guard(m_mtx);
        trace(msg);
    }

    template <typename PAYLOAD>
    static void record(trace_direction dir, const sc_object& port,
                       const PAYLOAD& payload,
                       const sc_time& t = SC_ZERO_TIME) {
        auto& tracers = tracer::all();
        dir = translate_direction_default<PAYLOAD>(dir);
        if (!tracers.empty() && dir != TRACE_NONE) {
            trace_activity_proto<PAYLOAD> msg(dir, port, payload, t);
            for (tracer* tr : tracers)
                tr->do_trace(msg);
        }
    }

    static bool any() { return !all().empty(); }

protected:
    static void print_timing(ostream& os, const trace_activity& msg) {
        print_timing(os, msg.t, msg.cycle);
    }

    static void print_timing(ostream& os, const sc_time& time, u64 delta);

    static unordered_set<tracer*>& all();
};

} // namespace vcml

#endif
