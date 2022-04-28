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

#ifndef VCML_PROTOCOLS_BASE_H
#define VCML_PROTOCOLS_BASE_H

#include "vcml/common/types.h"
#include "vcml/common/systemc.h"

#include "vcml/tracing/tracer.h"
#include "vcml/properties/property.h"

#if SYSTEMC_VERSION < SYSTEMC_VERSION_2_3_2
#define VCML_PROTO_OVERRIDE
#else
#define VCML_PROTO_OVERRIDE override
#endif

namespace vcml {

template <typename FW, typename BW, unsigned int WIDTH = 1, int N = 1,
          sc_core::sc_port_policy POL = sc_core::SC_ONE_OR_MORE_BOUND>
class base_initiator_socket
    : public tlm::tlm_base_initiator_socket<WIDTH, FW, BW, N, POL>
{
public:
    property<bool> trace;
    property<bool> trace_errors;

    base_initiator_socket(const char* nm, address_space as = VCML_AS_DEFAULT):
        tlm::tlm_base_initiator_socket<WIDTH, FW, BW, N, POL>(nm),
        trace(this, "trace", false),
        trace_errors(this, "trace_errors", false) {
        trace.inherit_default();
        trace_errors.inherit_default();
    }

    virtual sc_type_index get_protocol_types() const VCML_PROTO_OVERRIDE {
        return typeid(void);
    }

    bool is_bound() const {
        using base = tlm::tlm_base_initiator_socket<WIDTH, FW, BW, N, POL>;
        const sc_core::sc_port_b<FW>& port = base::get_base_port();
        return const_cast<sc_core::sc_port_b<FW>&>(port).bind_count();
    }

protected:
    template <typename PAYLOAD>
    void trace_fw(const PAYLOAD& tx, const sc_time& t = SC_ZERO_TIME) {
        if (trace)
            tracer::record(TRACE_FW, *this, tx, t);
    }

    template <typename PAYLOAD>
    void trace_bw(const PAYLOAD& tx, const sc_time& t = SC_ZERO_TIME) {
        if (trace || (trace_errors && failed(tx)))
            tracer::record(TRACE_BW, *this, tx, t);
    }
};

template <typename FW, typename BW, unsigned int WIDTH = 1, int N = 1,
          sc_core::sc_port_policy POL = sc_core::SC_ONE_OR_MORE_BOUND>
class base_target_socket
    : public tlm::tlm_base_target_socket<WIDTH, FW, BW, N, POL>
{
public:
    const address_space as;

    property<bool> trace;
    property<bool> trace_errors;

    base_target_socket(const char* nm, address_space space = VCML_AS_DEFAULT):
        tlm::tlm_base_target_socket<WIDTH, FW, BW, N, POL>(nm),
        as(space),
        trace(this, "trace", false),
        trace_errors(this, "trace_errors", false) {
        trace.inherit_default();
        trace_errors.inherit_default();
    }

    virtual sc_type_index get_protocol_types() const VCML_PROTO_OVERRIDE {
        return typeid(void);
    }

    bool is_bound() const {
        using base = tlm::tlm_base_target_socket<WIDTH, FW, BW, N, POL>;
        const sc_core::sc_port_b<BW>& port = base::get_base_port();
        return const_cast<sc_core::sc_port_b<BW>&>(port).bind_count();
    }

protected:
    template <typename PAYLOAD>
    void trace_fw(const PAYLOAD& tx, const sc_time& t = SC_ZERO_TIME) {
        if (trace)
            tracer::record(TRACE_FW, *this, tx, t);
    }

    template <typename PAYLOAD>
    void trace_bw(const PAYLOAD& tx, const sc_time& t = SC_ZERO_TIME) {
        if (trace || (trace_errors && failed(tx)))
            tracer::record(TRACE_BW, *this, tx, t);
    }
};

template <typename FW, typename BW, unsigned int WIDTH = 1>
using multi_initiator_socket = base_initiator_socket<FW, BW, WIDTH, 0>;

template <typename FW, typename BW, unsigned int WIDTH = 1>
using multi_target_socket = base_target_socket<FW, BW, WIDTH, 0>;

} // namespace vcml

#endif
