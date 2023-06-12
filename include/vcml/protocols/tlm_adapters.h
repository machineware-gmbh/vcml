/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_PROTOCOLS_TLM_ADAPTERS_H
#define VCML_PROTOCOLS_TLM_ADAPTERS_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/module.h"

namespace vcml {

template <unsigned int WIDTH_IN, unsigned int WIDTH_OUT>
class tlm_bus_width_adapter : public module
{
public:
    typedef tlm_bus_width_adapter<WIDTH_IN, WIDTH_OUT> this_type;
    simple_target_socket<tlm_bus_width_adapter, WIDTH_IN> in;
    simple_initiator_socket<tlm_bus_width_adapter, WIDTH_OUT> out;

    tlm_bus_width_adapter() = delete;

    tlm_bus_width_adapter(const sc_module_name& nm):
        module(nm), in("in"), out("out") {
        in.register_b_transport(this, &this_type::b_transport);
        in.register_transport_dbg(this, &this_type::transport_dbg);
        in.register_get_direct_mem_ptr(this, &this_type::get_direct_mem_ptr);
        out.register_invalidate_direct_mem_ptr(
            this, &this_type::invalidate_direct_mem_ptr);
    }

    virtual ~tlm_bus_width_adapter() = default;

    VCML_KIND(tlm_bus_width_adapter);

private:
    void b_transport(tlm_generic_payload& tx, sc_time& t) {
        trace_fw(out, tx, t);
        out->b_transport(tx, t);
        trace_bw(out, tx, t);
    }

    unsigned int transport_dbg(tlm_generic_payload& tx) {
        return out->transport_dbg(tx);
    }

    bool get_direct_mem_ptr(tlm_generic_payload& tx, tlm_dmi& dmi) {
        return out->get_direct_mem_ptr(tx, dmi);
    }

    void invalidate_direct_mem_ptr(sc_dt::uint64 s, sc_dt::uint64 e) {
        in->invalidate_direct_mem_ptr(s, e);
    }
};

} // namespace vcml

#endif
