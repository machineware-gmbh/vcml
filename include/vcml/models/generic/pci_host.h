/******************************************************************************
 *                                                                            *
 * Copyright 2021 Jan Henrik Weinstock                                        *
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

#ifndef VCML_GENERIC_PCI_HOST_H
#define VCML_GENERIC_PCI_HOST_H

#include "vcml/common/types.h"
#include "vcml/common/range.h"
#include "vcml/common/systemc.h"
#include "vcml/module.h"

#include "vcml/component.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"
#include "vcml/protocols/pci.h"

namespace vcml {
namespace generic {

class pci_host : public component, public pci_initiator
{
private:
    struct pci_mapping {
        u32 devno;
        int barno;
        address_space space;
        range addr;

        bool is_valid() const { return barno >= 0 && barno < 6; }
    };

    static const pci_mapping MAP_NONE;

    vector<pci_mapping> m_map_mmio;
    vector<pci_mapping> m_map_io;

    const pci_mapping& lookup(const pci_payload& pci, bool io) const;

public:
    property<bool> pcie;

    tlm_initiator_socket dma_out;
    tlm_target_socket cfg_in;
    tlm_target_socket_array<> mmio_in;
    tlm_target_socket_array<> io_in;

    pci_initiator_socket_array<256> pci_out;

    gpio_initiator_socket irq_a;
    gpio_initiator_socket irq_b;
    gpio_initiator_socket irq_c;
    gpio_initiator_socket irq_d;

    pci_host(const sc_module_name& nm, bool express = true);
    virtual ~pci_host();
    VCML_KIND(pci_host);

protected:
    u32 pci_devno(const pci_initiator_socket& socket) const {
        return (u32)pci_out.index_of(socket);
    }

    virtual unsigned int transport(tlm_generic_payload& tx,
                                   const tlm_sbi& sideband,
                                   address_space as) override;

    virtual void pci_transport_cfg(pci_payload& tx);
    virtual void pci_transport(pci_payload& tx, bool io);

    virtual void pci_bar_map(const pci_initiator_socket& socket,
                             const pci_bar& bar) override;
    virtual void pci_bar_unmap(const pci_initiator_socket& socket,
                               int barno) override;
    virtual void* pci_dma_ptr(const pci_initiator_socket& socket,
                              vcml_access rw, u64 addr, u64 size) override;
    virtual bool pci_dma_read(const pci_initiator_socket& socket, u64 addr,
                              u64 size, void* data) override;
    virtual bool pci_dma_write(const pci_initiator_socket& socket, u64 addr,
                               u64 size, const void* data) override;
    virtual void pci_interrupt(const pci_initiator_socket& socket, pci_irq irq,
                               bool state) override;
};

} // namespace generic
} // namespace vcml

#endif
