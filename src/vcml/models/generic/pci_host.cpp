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

#include "vcml/models/generic/pci_host.h"

namespace vcml { namespace generic {

    static void pci_decode_cfg(u64 addr, u32& bus, u32& devfn, u32& offset) {
        offset = addr & 0xff;
        devfn = (addr >> 8) & 0xff;
        bus = (addr >> 16) & 0xff;
    }

    static void pcie_decode_cfg(u64 addr, u32& bus, u32& devfn, u32& offset) {
        offset = addr & 0xff;
        devfn = (addr >> 12) & 0xff;
        bus = (addr >> 20) & 0x1ff;
    }

    static pci_address_space pci_target_space(int bar) {
        VCML_ERROR_ON(bar < 0 || bar > 5, "invalid bar %d", bar);
        return (pci_address_space)(PCI_AS_BAR0 + bar);
    }

    static pci_irq pci_irq_swizzle(pci_irq irq, u32 devno) {
        VCML_ERROR_ON(irq < PCI_IRQ_A || irq > PCI_IRQ_D, "irq invalid");
        return (pci_irq)((irq - 1) % 4 + 1);
    }

    const pci_host::pci_mapping pci_host::MAP_NONE = {
        /* devno = */ ~0u,
        /* barno = */ -1,
        /* space = */ PCI_AS_CFG,
        /* addr  = */ { 0, ~0ull },
    };

    const pci_host::pci_mapping&
    pci_host::lookup(const pci_payload& tx, bool io) const {
        const vector<pci_mapping>& map = io ? m_map_io : m_map_mmio;
        const range addr(tx.addr, tx.addr + tx.size - 1);
        for (auto& entry : map)
            if (entry.addr.includes(addr))
                return entry;
        return MAP_NONE;
    }

    pci_host::pci_host(const sc_module_name& nm, bool express):
        component(nm),
        pci_initiator(),
        pcie("pcie", express),
        DMA_OUT("DMA_OUT"),
        CFG_IN("CFG_IN", PCI_AS_CFG),
        MMIO_IN("MMIO_IN", PCI_AS_MMIO),
        IO_IN("IO_IN", PCI_AS_IO),
        PCI_OUT("PCI_OUT"),
        IRQ_A("IRQ_A"),
        IRQ_B("IRQ_B"),
        IRQ_C("IRQ_C"),
        IRQ_D("IRQ_D") {
    }

    pci_host::~pci_host() {
        // nothing to do
    }

    unsigned int pci_host::transport(tlm_generic_payload& tx,
        const tlm_sbi& sideband, address_space space) {
        if (tx.get_command() == TLM_IGNORE_COMMAND)
            return 0;

        u64 size = tx.get_data_length();

        // max 32bit, only naturally aligned accesses
        if (size > 4 || tx.get_address() % size) {
            tx.set_response_status(TLM_COMMAND_ERROR_RESPONSE);
            return 0;
        }

        // no streaming
        if (tx.get_streaming_width() != size) {
            tx.set_response_status(TLM_BURST_ERROR_RESPONSE);
            return 0;
        }

        // no byte-enable support
        if (tx.get_byte_enable_ptr() || tx.get_byte_enable_length()) {
            tx.set_response_status(TLM_BYTE_ENABLE_ERROR_RESPONSE);
            return 0;
        }

        pci_payload pci;
        pci.command = pci_translate_command(tx.get_command());
        pci.response = PCI_RESP_INCOMPLETE;
        pci.space = (pci_address_space)space;
        pci.debug = sideband.is_debug;
        pci.addr = tx.get_address();
        pci.size = size;
        pci.data = 0;

        if (tx.is_write())
            memcpy(&pci.data, tx.get_data_ptr(), pci.size);

        switch (space) {
        case PCI_AS_MMIO:
            pci_transport(pci, false);
            break;

        case PCI_AS_IO:
            pci_transport(pci, true);
            break;

        case PCI_AS_CFG:
            pci_transport_cfg(pci);
            if (pci.is_address_error()) {
                pci.response = PCI_RESP_SUCCESS;
                pci.data = 0;
            }
            break;

        default:
            VCML_ERROR("invalid address space %d", (int)space);
        }

        if (tx.is_read())
            memcpy(tx.get_data_ptr(), &pci.data, pci.size);

        tx.set_response_status(pci_translate_response(pci.response));
        return tx.is_response_ok() ? tx.get_data_length() : 0;
    }

    void pci_host::pci_transport(pci_payload& tx, bool io) {
        const auto& mapping = lookup(tx, io);
        if (!mapping.is_valid()) {
            tx.response = PCI_RESP_ADDRESS_ERROR;
            return;
        }

        if (!PCI_OUT.exists(mapping.devno))
            VCML_ERROR("invalid PCI mapping to nonexistent device");

        tx.addr -= mapping.addr.start;
        tx.space = pci_target_space(mapping.barno);
        PCI_OUT[mapping.devno].transport(tx);
    }

    void pci_host::pci_transport_cfg(pci_payload& tx) {
        u32 bus, devno, offset;
        if (pcie)
            pcie_decode_cfg(tx.addr, bus, devno, offset);
        else
            pci_decode_cfg(tx.addr, bus, devno, offset);

        if (PCI_OUT.exists(devno)) {
            tx.addr = offset;
            PCI_OUT[devno].transport(tx);
        } else {
            tx.data = ~0u;
            tx.response = PCI_RESP_SUCCESS;
        }
    }

    void pci_host::pci_bar_map(pci_initiator_socket& socket,
        const pci_bar& bar) {
        pci_bar_unmap(socket, bar.barno);

        u32 devno = pci_devno(socket);
        pci_address_space space = pci_target_space(bar.barno);
        range addr(bar.addr, bar.addr + bar.size - 1);
        pci_mapping mapping { devno, bar.barno, space, addr };

        if (bar.is_io)
            m_map_io.push_back(mapping);
        else
            m_map_mmio.push_back(mapping);
    }

    void pci_host::pci_bar_unmap(pci_initiator_socket& socket, int barno) {
        u32 devno = pci_devno(socket);
        auto match = [devno, barno] (const pci_mapping& entry) -> bool {
            return entry.devno == devno && entry.barno == barno;
        };

        stl_remove_erase_if(m_map_mmio, match);
        stl_remove_erase_if(m_map_io, match);
    }

    void* pci_host::pci_dma_ptr(pci_initiator_socket& socket, vcml_access rw,
        u64 addr, u64 size) {
        return DMA_OUT.lookup_dmi_ptr(addr, size, rw);
    }

    bool pci_host::pci_dma_read(pci_initiator_socket& socket, u64 addr,
        u64 size, void* data) {
        u32 devno = pci_devno(socket);
        return success(DMA_OUT.read(addr, data, size, SBI_CPUID(devno)));
    }

    bool pci_host::pci_dma_write(pci_initiator_socket& socket, u64 addr,
        u64 size, const void* data) {
        u32 devno = pci_devno(socket);
        return success(DMA_OUT.write(addr, data, size, SBI_CPUID(devno)));
    }

    void pci_host::pci_interrupt(pci_initiator_socket& socket, pci_irq irq,
        bool state) {
        pci_irq actual = pci_irq_swizzle(irq, pci_devno(socket));
        switch (actual) {
        case PCI_IRQ_A: IRQ_A = state; break;
        case PCI_IRQ_B: IRQ_B = state; break;
        case PCI_IRQ_C: IRQ_C = state; break;
        case PCI_IRQ_D: IRQ_D = state; break;
        default:
            VCML_ERROR("invalid PCI_IRQ %d", (int)actual);
        }
    }

}}
