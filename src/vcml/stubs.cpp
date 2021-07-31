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

#include "vcml/stubs.h"

namespace vcml {

    initiator_stub::initiator_stub(const sc_module_name& nm):
        sc_module(nm),
        tlm::tlm_bw_transport_if<>(),
        OUT("OUT") {
        OUT.bind(*this);
    }

    tlm::tlm_sync_enum initiator_stub::nb_transport_bw(tlm_generic_payload& tx,
        tlm::tlm_phase& phase, sc_time& t) {
        return tlm::TLM_COMPLETED;
    }

    void invalidate_direct_mem_ptr(sc_dt::uint64 start, sc_dt::uint64 end) {
        // nothing to do
    }

    target_stub::target_stub(const sc_module_name& nm, tlm_response_status r):
        sc_module(nm),
        m_response(r),
        IN("IN") {
        IN.bind(*this);
    }

    void target_stub::b_transport(tlm_generic_payload& tx, sc_time& t) {
        tx.set_response_status(m_response);
    }

    unsigned int target_stub::transport_dbg(tlm_generic_payload& tx) {
        tx.set_response_status(m_response);
        return tx.is_response_ok() ? tx.get_data_length() : 0;
    }

    bool target_stub::get_direct_mem_ptr(tlm_generic_payload& tx, tlm_dmi& d) {
        d.allow_read_write();
        d.set_start_address(0x0);
        d.set_end_address((sc_dt::uint64)-1);
        return false;
    }

    tlm::tlm_sync_enum target_stub::nb_transport_fw(tlm_generic_payload& tx,
        tlm::tlm_phase& phase, sc_time& t) {
        return tlm::TLM_COMPLETED;
    }

}
