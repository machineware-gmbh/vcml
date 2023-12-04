/******************************************************************************
 *                                                                            *
 * Copyright (C) 2023 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/protocols/tlm_probe.h"

namespace vcml {

void tlm_probe::b_transport(tlm_generic_payload& tx, sc_time& t) {
    out->b_transport(tx, t);
}

unsigned int tlm_probe::transport_dbg(tlm_generic_payload& tx) {
    return out->transport_dbg(tx);
}

bool tlm_probe::get_direct_mem_ptr(tlm_generic_payload& tx, tlm_dmi& dmi) {
    return out->get_direct_mem_ptr(tx, dmi);
}

void tlm_probe::invalidate_direct_mem_ptr(u64 start, u64 end) {
    return in->invalidate_direct_mem_ptr(start, end);
}

tlm::tlm_sync_enum tlm_probe::nb_transport_fw(tlm_generic_payload& tx,
                                              tlm::tlm_phase& phase,
                                              sc_time& t) {
    return out->nb_transport_fw(tx, phase, t);
}

tlm::tlm_sync_enum tlm_probe::nb_transport_bw(tlm_generic_payload& trans,
                                              tlm::tlm_phase& phase,
                                              sc_time& t) {
    return in->nb_transport_bw(trans, phase, t);
}

tlm_probe::tlm_probe(const sc_module_name& nm):
    module(nm), in("in"), out("out") {
    in.bind(*this);
    out.bind(*this);
}

} // namespace vcml
