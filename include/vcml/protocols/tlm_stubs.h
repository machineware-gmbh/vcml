/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_PROTOCOLS_TLM_STUBS_H
#define VCML_PROTOCOLS_TLM_STUBS_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"

namespace vcml {

class tlm_initiator_stub : public sc_module,
                           protected tlm::tlm_bw_transport_if<>
{
public:
    tlm::tlm_initiator_socket<> out;

    tlm_initiator_stub() = delete;
    tlm_initiator_stub(const tlm_initiator_stub&) = delete;
    tlm_initiator_stub(tlm_initiator_stub&&) = delete;

    tlm_initiator_stub(const sc_module_name& name);
    virtual ~tlm_initiator_stub() = default;
    VCML_KIND(tlm_initiator_stub);

protected:
    virtual tlm::tlm_sync_enum nb_transport_bw(tlm_generic_payload& tx,
                                               tlm::tlm_phase& phase,
                                               sc_time& t) override;

    virtual void invalidate_direct_mem_ptr(sc_dt::uint64 start,
                                           sc_dt::uint64 end) override;
};

class tlm_target_stub : public sc_module, protected tlm::tlm_fw_transport_if<>
{
private:
    tlm_response_status m_response;

public:
    tlm::tlm_target_socket<> in;

    tlm_target_stub() = delete;
    tlm_target_stub(const tlm_target_stub&) = delete;
    tlm_target_stub(tlm_target_stub&&) = delete;

    tlm_target_stub(const sc_module_name& name,
                    tlm_response_status response = TLM_ADDRESS_ERROR_RESPONSE);
    virtual ~tlm_target_stub() = default;
    VCML_KIND(tlm_target_stub);

protected:
    virtual void b_transport(tlm_generic_payload& tx, sc_time& t) override;
    virtual unsigned int transport_dbg(tlm_generic_payload& tx) override;
    virtual bool get_direct_mem_ptr(tlm_generic_payload& tx,
                                    tlm_dmi& dmi) override;
    virtual tlm::tlm_sync_enum nb_transport_fw(tlm_generic_payload& tx,
                                               tlm::tlm_phase& phase,
                                               sc_time& t) override;
};

} // namespace vcml

#endif
