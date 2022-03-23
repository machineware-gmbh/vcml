/******************************************************************************
 *                                                                            *
 * Copyright 2018 Jan Henrik Weinstock                                        *
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
