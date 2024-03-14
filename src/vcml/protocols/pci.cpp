/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/protocols/pci.h"

namespace vcml {

const char* pci_address_space_str(pci_address_space as) {
    switch (as) {
    case PCI_AS_CFG:
        return "PCI_AS_CFG";
    case PCI_AS_MMIO:
        return "PCI_AS_MMIO";
    case PCI_AS_IO:
        return "PCI_AS_IO";
    case PCI_AS_BAR0:
        return "PCI_AS_BAR0";
    case PCI_AS_BAR1:
        return "PCI_AS_BAR1";
    case PCI_AS_BAR2:
        return "PCI_AS_BAR2";
    case PCI_AS_BAR3:
        return "PCI_AS_BAR3";
    case PCI_AS_BAR4:
        return "PCI_AS_BAR4";
    case PCI_AS_BAR5:
        return "PCI_AS_BAR5";
    default:
        return "PCI_UNKNOWN_AS";
    }
}

const char* pci_command_str(pci_command cmd) {
    switch (cmd) {
    case PCI_READ:
        return "PCI_READ";
    case PCI_WRITE:
        return "PCI_WRITE";
    default:
        return "PCI_UNKNOWN_CMD";
    }
}

const char* pci_response_str(pci_response resp) {
    switch (resp) {
    case PCI_RESP_INCOMPLETE:
        return "PCI_RESP_INCOMPLETE";
    case PCI_RESP_SUCCESS:
        return "PCI_RESP_SUCCESS";
    case PCI_RESP_ADDRESS_ERROR:
        return "PCI_RESP_ADDRESS_ERROR";
    case PCI_RESP_COMMAND_ERROR:
        return "PCI_RESP_COMMAND_ERROR";
    default:
        return "PCI_UNKNOWN_RESP";
    }
}

const char* pci_irq_str(pci_irq irq) {
    switch (irq) {
    case PCI_IRQ_A:
        return "PCI_IRQ_A";
    case PCI_IRQ_B:
        return "PCI_IRQ_B";
    case PCI_IRQ_C:
        return "PCI_IRQ_C";
    case PCI_IRQ_D:
        return "PCI_IRQ_D";
    default:
        return "PCI_UNKNOWN_IRQ";
    }
}

tlm_command pci_translate_command(pci_command cmd) {
    switch (cmd) {
    case PCI_READ:
        return TLM_READ_COMMAND;
    case PCI_WRITE:
        return TLM_WRITE_COMMAND;
    default:
        VCML_ERROR("invalid pci command: %d", (int)cmd);
    }
}

pci_command pci_translate_command(tlm_command cmd) {
    switch (cmd) {
    case TLM_READ_COMMAND:
        return PCI_READ;
    case TLM_WRITE_COMMAND:
        return PCI_WRITE;
    default:
        VCML_ERROR("invalid tlm command: %d", (int)cmd);
    }
}

tlm_response_status pci_translate_response(pci_response resp) {
    switch (resp) {
    case PCI_RESP_INCOMPLETE:
        return TLM_INCOMPLETE_RESPONSE;
    case PCI_RESP_SUCCESS:
        return TLM_OK_RESPONSE;
    case PCI_RESP_ADDRESS_ERROR:
        return TLM_ADDRESS_ERROR_RESPONSE;
    case PCI_RESP_COMMAND_ERROR:
        return TLM_COMMAND_ERROR_RESPONSE;
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

ostream& operator<<(ostream& os, const pci_payload& tx) {
    stream_guard guard(os);
    os << pci_command_str(tx.command) << " " << pci_address_space_str(tx.space)
       << " ";

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

pci_base_initiator_socket::pci_base_initiator_socket(const char* nm,
                                                     address_space as):
    pci_base_initiator_socket_b(nm, as), m_stub(nullptr) {
}

pci_base_initiator_socket::~pci_base_initiator_socket() {
    if (m_stub)
        delete m_stub;
}

void pci_base_initiator_socket::stub() {
    VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", name());
    auto guard = get_hierarchy_scope();
    m_stub = new pci_target_stub(basename());
    bind(m_stub->pci_in);
}

pci_base_target_socket::pci_base_target_socket(const char* n, address_space a):
    pci_base_target_socket_b(n, a), m_stub(nullptr) {
}

pci_base_target_socket::~pci_base_target_socket() {
    if (m_stub)
        delete m_stub;
}

void pci_base_target_socket::stub() {
    VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", name());
    auto guard = get_hierarchy_scope();
    m_stub = new pci_initiator_stub(basename());
    m_stub->pci_out.bind(*this);
}

pci_initiator_socket::pci_initiator_socket(const char* nm, address_space as):
    pci_base_initiator_socket(nm, as),
    m_initiator(hierarchy_search<pci_initiator>()),
    m_transport(this) {
    VCML_ERROR_ON(!m_initiator, "%s outside pci_initiator", name());
    m_initiator->m_sockets.push_back(this);
    bind(m_transport);
}

pci_initiator_socket::~pci_initiator_socket() {
    if (m_initiator)
        stl_remove(m_initiator->m_sockets, this);
}

void pci_initiator_socket::transport(pci_payload& tx) {
    trace_fw(tx);
    get_interface(0)->pci_transport(tx);
    trace_bw(tx);
}

pci_target_socket::pci_target_socket(const char* nm, address_space space):
    pci_base_target_socket(nm, space),
    m_target(hierarchy_search<pci_target>()),
    m_transport(this) {
    VCML_ERROR_ON(!m_target, "%s outside pci_target", name());
    bind(m_transport);
    if (m_target)
        m_target->m_sockets.push_back(this);
}

pci_target_socket::~pci_target_socket() {
    if (m_target)
        stl_remove(m_target->m_sockets, this);
}

void pci_initiator_stub::pci_bar_map(const pci_bar& bar) {
    // nothing to do
}

void pci_initiator_stub::pci_bar_unmap(int barno) {
    // nothing to do
}

void pci_initiator_stub::pci_interrupt(pci_irq irq, bool state) {
    // nothing to do
}

void* pci_initiator_stub::pci_dma_ptr(vcml_access rw, u64 addr, u64 size) {
    return nullptr; // nothing to do
}

bool pci_initiator_stub::pci_dma_read(u64 addr, u64 size, void* data) {
    return false; // nothing to do
}

bool pci_initiator_stub::pci_dma_write(u64 addr, u64 size, const void* data) {
    return false; // nothing to do
}

pci_initiator_stub::pci_initiator_stub(const char* nm):
    pci_bw_transport_if(), pci_out(mkstr("%s_stub", nm).c_str()) {
    pci_out.bind(*this);
}

void pci_target_stub::pci_transport(pci_payload& tx) {
    // nothing to do
}

pci_target_stub::pci_target_stub(const char* nm):
    pci_fw_transport_if(), pci_in(mkstr("%s_stub", nm).c_str()) {
    pci_in.bind(*this);
}

static pci_base_initiator_socket* get_initiator_socket(sc_object* port) {
    return dynamic_cast<pci_base_initiator_socket*>(port);
}

static pci_base_target_socket* get_target_socket(sc_object* port) {
    return dynamic_cast<pci_base_target_socket*>(port);
}

static pci_base_initiator_socket* get_initiator_socket(sc_object* array,
                                                       size_t idx) {
    auto* base = dynamic_cast<pci_base_initiator_array*>(array);
    if (base)
        return &base->get(idx);
    auto* main = dynamic_cast<pci_initiator_array*>(array);
    if (main)
        return &main->get(idx);
    return nullptr;
}

static pci_base_target_socket* get_target_socket(sc_object* array,
                                                 size_t idx) {
    auto* base = dynamic_cast<pci_base_target_array*>(array);
    if (base)
        return &base->get(idx);
    auto* main = dynamic_cast<pci_target_array*>(array);
    if (main)
        return &main->get(idx);
    return nullptr;
}

void pci_stub(const sc_object& obj, const string& port) {
    sc_object* child = find_child(obj, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", obj.name(), port.c_str());

    auto* ini = get_initiator_socket(child);
    auto* tgt = get_target_socket(child);

    if (!ini && !tgt)
        VCML_ERROR("%s is not a valid pci socket", child->name());

    if (ini)
        ini->stub();
    if (tgt)
        tgt->stub();
}

void pci_stub(const sc_object& obj, const string& port, size_t idx) {
    sc_object* child = find_child(obj, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", obj.name(), port.c_str());

    pci_base_initiator_socket* isock = get_initiator_socket(child, idx);
    if (isock) {
        isock->stub();
        return;
    }

    pci_base_target_socket* tsock = get_target_socket(child, idx);
    if (tsock) {
        tsock->stub();
        return;
    }

    VCML_ERROR("%s is not a valid pci socket array", child->name());
}

void pci_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = get_initiator_socket(p1);
    auto* i2 = get_initiator_socket(p2);
    auto* t1 = get_target_socket(p1);
    auto* t2 = get_target_socket(p2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid pci port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid pci port", p2->name());

    if (i1 && i2)
        i1->bind(*i2);
    else if (i1 && t2)
        i1->bind(*t2);
    else if (t1 && i2)
        i2->bind(*t1);
    else if (t1 && t2)
        t1->bind(*t2);
}

void pci_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2, size_t idx2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = get_initiator_socket(p1);
    auto* i2 = get_initiator_socket(p2, idx2);
    auto* t1 = get_target_socket(p1);
    auto* t2 = get_target_socket(p2, idx2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid pci port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid pci port", p2->name());

    if (i1 && i2)
        i1->bind(*i2);
    else if (i1 && t2)
        i1->bind(*t2);
    else if (t1 && i2)
        i2->bind(*t1);
    else if (t1 && t2)
        t1->bind(*t2);
}

void pci_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = get_initiator_socket(p1, idx1);
    auto* i2 = get_initiator_socket(p2);
    auto* t1 = get_target_socket(p1, idx1);
    auto* t2 = get_target_socket(p2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid pci port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid pci port", p2->name());

    if (i1 && i2)
        i1->bind(*i2);
    else if (i1 && t2)
        i1->bind(*t2);
    else if (t1 && i2)
        i2->bind(*t1);
    else if (t1 && t2)
        t1->bind(*t2);
}

void pci_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2, size_t idx2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = get_initiator_socket(p1, idx1);
    auto* i2 = get_initiator_socket(p2, idx2);
    auto* t1 = get_target_socket(p1, idx1);
    auto* t2 = get_target_socket(p2, idx2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid pci port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid pci port", p2->name());

    if (i1 && i2)
        i1->bind(*i2);
    else if (i1 && t2)
        i1->bind(*t2);
    else if (t1 && i2)
        i2->bind(*t1);
    else if (t1 && t2)
        t1->bind(*t2);
}

} // namespace vcml
