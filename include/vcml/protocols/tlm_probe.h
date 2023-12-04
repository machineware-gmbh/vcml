/******************************************************************************
 *                                                                            *
 * Copyright (C) 2023 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_PROTOCOLS_TLM_PROBE_H
#define VCML_PROTOCOLS_TLM_PROBE_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/range.h"
#include "vcml/core/thctl.h"
#include "vcml/core/module.h"

#include "vcml/protocols/tlm_sbi.h"
#include "vcml/protocols/tlm_exmon.h"
#include "vcml/protocols/tlm_stubs.h"
#include "vcml/protocols/tlm_adapters.h"
#include "vcml/protocols/tlm_base.h"

namespace vcml {

class tlm_probe : public module,
                  protected tlm::tlm_fw_transport_if<>,
                  protected tlm::tlm_bw_transport_if<>
{
protected:
    virtual void b_transport(tlm_generic_payload& tx, sc_time& dt) override;
    virtual unsigned int transport_dbg(tlm_generic_payload& tx) override;
    virtual bool get_direct_mem_ptr(tlm_generic_payload& tx,
                                    tlm_dmi& dmi) override;
    virtual void invalidate_direct_mem_ptr(u64 start, u64 end) override;
    virtual tlm::tlm_sync_enum nb_transport_fw(tlm_generic_payload& tx,
                                               tlm::tlm_phase& phase,
                                               sc_time& t) override;
    virtual tlm::tlm_sync_enum nb_transport_bw(tlm_generic_payload& trans,
                                               tlm::tlm_phase& phase,
                                               sc_time& t) override;

public:
    tlm_base_target_socket in;
    tlm_base_initiator_socket out;

    tlm_probe(const sc_module_name& nm);
    virtual ~tlm_probe() = default;
    VCML_KIND(tlm_probe);
};

} // namespace vcml

#endif
