/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_TRACING_ACTIVITY_H
#define VCML_TRACING_ACTIVITY_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"

#include "vcml/tracing/protocol.h"

namespace vcml {

struct trace_activity {
    trace_direction dir;
    const sc_object& port;
    bool error;
    sc_time t;
    u64 cycle;

    trace_activity(trace_direction d, const sc_object& p, bool err,
                   const sc_time& dt):
        dir(d),
        port(p),
        error(err),
        t(sc_time_stamp() + dt),
        cycle(sc_delta_count()) {}
    virtual ~trace_activity() = default;

    virtual size_t protocol_id() const = 0;
    virtual string protocol_name() const = 0;
    virtual string to_string() const = 0;
    virtual string to_json() const = 0;
    virtual string termcolor() const = 0;

    template <typename PAYLOAD>
    const PAYLOAD& get_payload() const;
};

template <typename PAYLOAD>
struct trace_activity_proto : trace_activity {
    const PAYLOAD& payload;

    trace_activity_proto(trace_direction d, const sc_object& sender,
                         const PAYLOAD& tx, const sc_time& dt):
        trace_activity(d, sender, failed(tx), dt), payload(tx) {}
    virtual ~trace_activity_proto() = default;

    virtual string protocol_name() const override {
        return protocol<PAYLOAD>::NAME;
    }

    virtual size_t protocol_id() const override {
        return protocol<PAYLOAD>::ID;
    }

    virtual string to_string() const override {
        return mwr::to_string<PAYLOAD>(payload);
    }

    virtual string to_json() const override {
        return protocol<PAYLOAD>::to_json(payload);
    }

    virtual string termcolor() const override {
        return protocol<PAYLOAD>::TERMCOLOR;
    }
};

template <typename PAYLOAD>
const PAYLOAD& trace_activity::get_payload() const {
    using U = trace_activity_proto<PAYLOAD>;
    const U* activity = dynamic_cast<const U*>(this);
    VCML_ERROR_ON(!activity, "invalid payload type");
    return activity->payload;
}

} // namespace vcml

#endif
