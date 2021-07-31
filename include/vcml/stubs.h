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

#ifndef VCML_STUBS_H
#define VCML_STUBS_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"

namespace vcml {

    class initiator_stub: public sc_module,
                          protected tlm::tlm_bw_transport_if<>
    {
    public:
        tlm_initiator_socket<64> OUT;

        initiator_stub() = delete;
        initiator_stub(const initiator_stub&) = delete;
        initiator_stub(initiator_stub&&) = delete;

        initiator_stub(const sc_module_name& name);
        virtual ~initiator_stub() = default;
        VCML_KIND(initiator_stub);

    protected:
        virtual tlm::tlm_sync_enum nb_transport_bw(tlm_generic_payload& tx,
                                                   tlm::tlm_phase& phase,
                                                   sc_time& t);

        virtual void invalidate_direct_mem_ptr(sc_dt::uint64 start,
                                               sc_dt::uint64 end);
    };

    class target_stub: public sc_module,
                       protected tlm::tlm_fw_transport_if<>
    {
    private:
        tlm_response_status m_response;

    public:
        tlm_target_socket<64> IN;

        target_stub() = delete;
        target_stub(const target_stub&) = delete;
        target_stub(target_stub&&) = delete;

        target_stub(const sc_module_name& name,
                    tlm_response_status response = TLM_ADDRESS_ERROR_RESPONSE);
        virtual ~target_stub() = default;
        VCML_KIND(target_stub);

    protected:
        virtual void b_transport(tlm_generic_payload& tx, sc_time& t);
        virtual unsigned int transport_dbg(tlm_generic_payload& tx);
        virtual bool get_direct_mem_ptr(tlm_generic_payload& tx, tlm_dmi& dmi);
        virtual tlm::tlm_sync_enum nb_transport_fw(tlm_generic_payload& tx,
                                                   tlm::tlm_phase& phase,
                                                   sc_time& t);
    };

}

#endif
