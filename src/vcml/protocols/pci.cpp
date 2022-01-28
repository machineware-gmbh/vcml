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

#include "vcml/protocols/pci.h"

namespace vcml {

    const char* pci_address_space_str(pci_address_space as) {
        switch (as) {
        case PCI_AS_CFG:  return "PCI_AS_CFG";
        case PCI_AS_MMIO: return "PCI_AS_MMIO";
        case PCI_AS_IO:   return "PCI_AS_IO";
        case PCI_AS_BAR0: return "PCI_AS_BAR0";
        case PCI_AS_BAR1: return "PCI_AS_BAR1";
        case PCI_AS_BAR2: return "PCI_AS_BAR2";
        case PCI_AS_BAR3: return "PCI_AS_BAR3";
        case PCI_AS_BAR4: return "PCI_AS_BAR4";
        case PCI_AS_BAR5: return "PCI_AS_BAR5";
        default:
            return "PCI_UNKNOWN_AS";
        }
    }

    const char* pci_command_str(pci_command cmd) {
        switch (cmd) {
        case PCI_READ:  return "PCI_READ";
        case PCI_WRITE: return "PCI_WRITE";
        default:
            return "PCI_UNKNOWN_CMD";
        }
    }

    const char* pci_response_str(pci_response resp) {
        switch (resp) {
        case PCI_RESP_INCOMPLETE:    return "PCI_RESP_INCOMPLETE";
        case PCI_RESP_SUCCESS:       return "PCI_RESP_SUCCESS";
        case PCI_RESP_ADDRESS_ERROR: return "PCI_RESP_ADDRESS_ERROR";
        case PCI_RESP_COMMAND_ERROR: return "PCI_RESP_COMMAND_ERROR";
        default:
            return "PCI_UNKNOWN_RESP";
        }
    }

    const char* pci_irq_str(pci_irq irq) {
        switch (irq) {
        case PCI_IRQ_A: return "PCI_IRQ_A";
        case PCI_IRQ_B: return "PCI_IRQ_B";
        case PCI_IRQ_C: return "PCI_IRQ_C";
        case PCI_IRQ_D: return "PCI_IRQ_D";
        default:
            return "PCI_UNKNOWN_IRQ";
        }
    }

    tlm_command pci_translate_command(pci_command cmd) {
        switch (cmd) {
        case PCI_READ: return TLM_READ_COMMAND;
        case PCI_WRITE: return TLM_WRITE_COMMAND;
        default:
            VCML_ERROR("invalid pci command: %d", (int)cmd);
        }
    }

    pci_command pci_translate_command(tlm_command cmd) {
        switch (cmd) {
        case TLM_READ_COMMAND: return PCI_READ;
        case TLM_WRITE_COMMAND: return PCI_WRITE;
        default:
            VCML_ERROR("invalid tlm command: %d", (int)cmd);
        }
    }

    tlm_response_status pci_translate_response(pci_response resp) {
        switch (resp) {
        case PCI_RESP_INCOMPLETE: return TLM_INCOMPLETE_RESPONSE;
        case PCI_RESP_SUCCESS: return TLM_OK_RESPONSE;
        case PCI_RESP_ADDRESS_ERROR: return TLM_ADDRESS_ERROR_RESPONSE;
        case PCI_RESP_COMMAND_ERROR: return TLM_COMMAND_ERROR_RESPONSE;
        default:
            VCML_ERROR("invalid pci response %d", (int)resp);
        }
    }

    pci_response pci_translate_response(tlm_response_status resp) {
        switch (resp) {
        case TLM_OK_RESPONSE:
            return PCI_RESP_SUCCESS;
        case TLM_INCOMPLETE_RESPONSE:
            return PCI_RESP_INCOMPLETE;
        case TLM_ADDRESS_ERROR_RESPONSE:
            return PCI_RESP_ADDRESS_ERROR;

        case TLM_GENERIC_ERROR_RESPONSE:
        case TLM_COMMAND_ERROR_RESPONSE:
        case TLM_BURST_ERROR_RESPONSE:
        case TLM_BYTE_ENABLE_ERROR_RESPONSE:
            return PCI_RESP_COMMAND_ERROR;

        default:
            VCML_ERROR("invalid tlm response: %d", (int)resp);
        }
    }

    ostream& operator << (ostream& os, const pci_payload& tx) {
        stream_guard guard(os);
        os << pci_command_str(tx.command) << " "
           << pci_address_space_str(tx.space) << " ";

        // address
        os << "0x" << std::hex << std::setw(tx.addr > ~0u ? 16 : 8)
           << std::setfill('0') << tx.addr;

        // data
        os << " [";

        if (tx.size == 0)
            os << "<no data>";
        else {
            u32 i, size = min<u32>(tx.size, sizeof(tx.data));
            for (i = 0; i < size - 1; i++) {
                os << std::setw(2) << std::setfill('0')
                   << ((tx.data >> (i * 8)) & 0xff) << " ";
            }

            os << std::setw(2) << std::setfill('0')
               << ((tx.data >> (i * 8)) & 0xff);
        }

        os << "]";

        // response
        os << " (" << pci_response_str(tx.response) << ")";
        return os;
    }

    void pci_target::pci_bar_map(const pci_bar& bar) {
        for (auto& socket : m_sockets)
            (*socket)->pci_bar_map(bar);
    }

    void pci_target::pci_bar_unmap(int barno) {
        for (auto& socket : m_sockets)
            (*socket)->pci_bar_unmap(barno);
    }

    void* pci_target::pci_dma_ptr(vcml_access rw, u64 addr, u64 size) {
        void* ptr = nullptr;
        for (auto& socket : m_sockets)
            if ((ptr = (*socket)->pci_dma_ptr(rw, addr, size)))
                return ptr;
        return nullptr;
    }

    bool pci_target::pci_dma_read(u64 addr, u64 size, void* data) {
        for (auto& socket : m_sockets)
            if ((*socket)->pci_dma_read(addr, size, data))
                return true;
        return false;
    }

    bool pci_target::pci_dma_write(u64 addr, u64 size, const void* data) {
        bool result = false;
        for (auto& socket : m_sockets)
            result |= (*socket)->pci_dma_write(addr, size, data);
        return result;
    }

    void pci_target::pci_interrupt(pci_irq irq, bool state) {
        for (auto& socket : m_sockets)
            (*socket)->pci_interrupt(irq, state);
    }

    void pci_initiator_socket::pci_bar_map(const pci_bar& bar) {
        m_initiator->pci_bar_map(*this, bar);
    }

    void pci_initiator_socket::pci_bar_unmap(int barno) {
        m_initiator->pci_bar_unmap(*this, barno);
    }

    void* pci_initiator_socket::pci_dma_ptr(vcml_access rw, u64 addr, u64 sz) {
        return m_initiator->pci_dma_ptr(*this, rw, addr, sz);
    }

    bool pci_initiator_socket::pci_dma_read(u64 addr, u64 size, void* data) {
        return m_initiator->pci_dma_read(*this, addr, size, data);
    }

    bool pci_initiator_socket::pci_dma_write(u64 addr, u64 size,
        const void* data) {
        return m_initiator->pci_dma_write(*this, addr, size, data);
    }

    void pci_initiator_socket::pci_interrupt(pci_irq irq, bool state) {
        m_initiator->pci_interrupt(*this, irq, state);
    }

    pci_initiator_socket::pci_initiator_socket(const char* n, address_space a):
        pci_base_initiator_socket(n, a),
        pci_bw_transport_if(),
        m_initiator(hierarchy_search<pci_initiator>()),
        m_stub(nullptr) {
        VCML_ERROR_ON(!m_initiator, "%s outside pci_initiator", name());
        bind(*(pci_bw_transport_if*)this);
        if (m_initiator)
            m_initiator->m_sockets.push_back(this);
    }

    pci_initiator_socket::~pci_initiator_socket() {
        if (m_initiator)
            stl_remove_erase(m_initiator->m_sockets, this);
        if (m_stub)
            delete m_stub;
    }

    sc_core::sc_type_index pci_initiator_socket::get_protocol_types() const {
        return typeid(pci_protocol_types);
    }

    void pci_initiator_socket::transport(pci_payload& tx) {
        trace_fw(tx);
        (*this)->pci_transport(tx);
        trace_bw(tx);
    }

    void pci_initiator_socket::stub() {
        VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", name());
        hierarchy_guard guard(this);
        m_stub = new pci_target_stub(mkstr("%s_stub", basename()).c_str());
        bind(m_stub->PCI_IN);
    }

    void pci_target_socket::pci_transport(pci_payload& tx) {
        trace_fw(tx);
        m_target->pci_transport(*this, tx);
        trace_bw(tx);
    }

    pci_target_socket::pci_target_socket(const char* nm, address_space space):
        pci_base_target_socket(nm, space),
        pci_fw_transport_if(),
        m_target(hierarchy_search<pci_target>()),
        m_stub(nullptr) {
        VCML_ERROR_ON(!m_target, "%s outside pci_target", name());
        bind(*(pci_fw_transport_if*)this);
        if (m_target)
            m_target->m_sockets.push_back(this);
    }

    pci_target_socket::~pci_target_socket() {
        if (m_target)
            stl_remove_erase(m_target->m_sockets, this);
        if (m_stub)
            delete m_stub;
    }

    sc_core::sc_type_index pci_target_socket::get_protocol_types() const {
        return typeid(pci_protocol_types);
    }

    void pci_target_socket::stub() {
        VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", name());
        hierarchy_guard guard(this);
        m_stub = new pci_initiator_stub(mkstr("%s_stub", basename()).c_str());
        m_stub->PCI_OUT.bind(*this);
    }

    void pci_initiator_stub::pci_bar_map(pci_initiator_socket& socket,
        const pci_bar& bar) {
        // nothing to do
    }

    void pci_initiator_stub::pci_bar_unmap(pci_initiator_socket& socket,
        int barno) {
        // nothing to do
    }

    void pci_initiator_stub::pci_interrupt(pci_initiator_socket& socket,
        pci_irq irq, bool state) {
        // nothing to do
    }

    void* pci_initiator_stub::pci_dma_ptr(pci_initiator_socket& socket,
        vcml_access rw, u64 addr, u64 size) {
        return nullptr; // nothing to do
    }

    bool pci_initiator_stub::pci_dma_read(pci_initiator_socket& socket,
        u64 addr, u64 size, void* data) {
        return false; // nothing to do
    }

    bool pci_initiator_stub::pci_dma_write(pci_initiator_socket& socket,
        u64 addr, u64 size, const void* data) {
        return false; // nothing to do
    }

    pci_initiator_stub::pci_initiator_stub(const sc_module_name& nm):
        module(nm),
        pci_initiator(),
        PCI_OUT("PCI_OUT") {
    }

    void pci_target_stub::pci_transport(pci_target_socket& socket,
        pci_payload& tx) {
        // nothing to do
    }

    pci_target_stub::pci_target_stub(const sc_module_name& nm):
        module(nm),
        pci_target(),
        PCI_IN("PCI_IN") {
    }

}
