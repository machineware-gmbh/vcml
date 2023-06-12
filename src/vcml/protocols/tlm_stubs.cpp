/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/protocols/tlm_stubs.h"

namespace vcml {

tlm_initiator_stub::tlm_initiator_stub(const sc_module_name& nm):
    sc_module(nm), tlm::tlm_bw_transport_if<>(), out("out") {
    out.bind(*this);
}

tlm::tlm_sync_enum tlm_initiator_stub::nb_transport_bw(tlm_generic_payload& tx,
                                                       tlm::tlm_phase& phase,
                                                       sc_time& t) {
    return tlm::TLM_COMPLETED;
}

void tlm_initiator_stub::invalidate_direct_mem_ptr(sc_dt::uint64 start,
                                                   sc_dt::uint64 end) {
    // nothing to do
}

tlm_target_stub::tlm_target_stub(const sc_module_name& nm,
                                 tlm_response_status r):
    sc_module(nm), m_response(r), in("in") {
    in.bind(*this);
}

void tlm_target_stub::b_transport(tlm_generic_payload& tx, sc_time& t) {
    tx.set_response_status(m_response);
}

unsigned int tlm_target_stub::transport_dbg(tlm_generic_payload& tx) {
    tx.set_response_status(m_response);
    return tx.is_response_ok() ? tx.get_data_length() : 0;
}

bool tlm_target_stub::get_direct_mem_ptr(tlm_generic_payload& tx,
                                         tlm_dmi& dmi) {
    dmi.allow_read_write();
    dmi.set_start_address(0x0);
    dmi.set_end_address((sc_dt::uint64)-1);
    return false;
}

tlm::tlm_sync_enum tlm_target_stub::nb_transport_fw(tlm_generic_payload& tx,
                                                    tlm::tlm_phase& phase,
                                                    sc_time& t) {
    return tlm::TLM_COMPLETED;
}

} // namespace vcml
