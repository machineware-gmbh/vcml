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

    void initiator_stub::invalidate_direct_mem_ptr(sc_dt::uint64 start,
                                                   sc_dt::uint64 end) {
        /* nothing to do */
    }

    initiator_stub::initiator_stub(const sc_module_name& nm):
        sc_module(nm),
        OUT("OUT") {
        OUT.register_invalidate_direct_mem_ptr(this,
                &initiator_stub::invalidate_direct_mem_ptr);
    }

    initiator_stub::~initiator_stub() {
        /* nothing to do */
    }

    target_stub::target_stub(const sc_module_name& nm):
        sc_module(nm),
        IN("IN") {
        IN.register_b_transport(this, &target_stub::b_transport);
        IN.register_transport_dbg(this, &target_stub::transport_dbg);
    }

    target_stub::~target_stub() {
        /* nothing to do */
    }

    void target_stub::b_transport(tlm_generic_payload& tx, sc_time& t) {
        tx.set_response_status(TLM_OK_RESPONSE);
    }

    unsigned int target_stub::transport_dbg(tlm_generic_payload& trans) {
        sc_time zero_delay;
        b_transport(trans, zero_delay);
        return trans.is_response_ok() ? trans.get_data_length() : 0;
    }

}
