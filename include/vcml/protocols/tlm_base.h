/******************************************************************************
 *                                                                            *
 * Copyright (C) 2023 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_PROTOCOLS_TLM_BASE_H
#define VCML_PROTOCOLS_TLM_BASE_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/range.h"
#include "vcml/core/thctl.h"
#include "vcml/core/module.h"

#include "vcml/protocols/base.h"
#include "vcml/protocols/tlm_sbi.h"
#include "vcml/protocols/tlm_exmon.h"
#include "vcml/protocols/tlm_stubs.h"
#include "vcml/protocols/tlm_adapters.h"

namespace vcml {

class tlm_base_initiator_socket : public tlm::tlm_initiator_socket<>,
                                  public hierarchy_element
{
private:
    tlm_target_stub* m_stub;

public:
    tlm_base_initiator_socket(const char* nm,
                              address_space as = VCML_AS_DEFAULT):
        tlm::tlm_initiator_socket<>(nm), hierarchy_element(), m_stub() {}
    virtual ~tlm_base_initiator_socket() { delete m_stub; }
    VCML_KIND(tlm_base_initiator_socket);

    bool is_stubbed() const { return m_stub != nullptr; }
    void stub(tlm_response_status r = TLM_ADDRESS_ERROR_RESPONSE) {
        VCML_ERROR_ON(m_stub, "socket %s already stubbed", name());
        auto guard = get_hierarchy_scope();
        m_stub = new tlm_target_stub(strcat(basename(), "_stub").c_str(), r);
        tlm::tlm_initiator_socket<>::bind(m_stub->in);
    }
};

class tlm_base_target_socket : public tlm::tlm_target_socket<>,
                               public hierarchy_element
{
private:
    tlm_initiator_stub* m_stub;

public:
    tlm_base_target_socket(const char* nm, address_space as = VCML_AS_DEFAULT):
        tlm::tlm_target_socket<>(nm), hierarchy_element(), m_stub() {}
    virtual ~tlm_base_target_socket() { delete m_stub; }
    VCML_KIND(tlm_base_target_socket);

    bool is_stubbed() const { return m_stub != nullptr; }
    void stub() {
        VCML_ERROR_ON(m_stub, "socket %s already stubbed", name());
        auto guard = get_hierarchy_scope();
        m_stub = new tlm_initiator_stub(strcat(basename(), "_stub").c_str());
        m_stub->out.bind(*this);
    }
};

using tlm_base_initiator_array = socket_array<tlm_base_initiator_socket>;
using tlm_base_target_array = socket_array<tlm_base_target_socket>;

} // namespace vcml

#endif
