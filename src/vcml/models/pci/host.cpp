/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/pci/host.h"

namespace vcml {
namespace pci {

static void cam_decode_cfg(u64 addr, u32& bus, u32& devfn, u32& offset) {
    offset = addr & 0xff;
    devfn = (addr >> 8) & 0xff;
    bus = (addr >> 16) & 0xff;
}

static void ecam_decode_cfg(u64 addr, u32& bus, u32& devfn, u32& offset) {
    offset = addr & 0xfff;
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

const host::pci_mapping host::MAP_NONE = {
    /* devno = */ ~0u,
    /* limit = */ 0u,
    /* barno = */ -1,
    /* space = */ PCI_AS_CFG,
    /* addr  = */ { 0, ~0ull },
    /* dmi   = */ false,
};

const host::pci_mapping& host::lookup(const range& addr, bool io) const {
    const vector<pci_mapping>& map = io ? m_map_io : m_map_mmio;
    for (auto& entry : map) {
        if (entry.addr.includes(addr))
            return entry;
    }

    return MAP_NONE;
}

const host::pci_mapping& host::rev_lookup(u32 devno, int barno) const {
    for (auto& entry : m_map_mmio) {
        if (entry.devno == devno && entry.barno == barno)
            return entry;
    }

    return MAP_NONE;
}

host::host(const sc_module_name& nm, bool express):
    component(nm),
    pci_initiator(),
    pcie("pcie", express),
    dma_out("dma_out"),
    cfg_in("cfg_in", PCI_AS_CFG),
    mmio_in("mmio_in", PCI_AS_MMIO),
    io_in("io_in", PCI_AS_IO),
    pci_out("pci_out", 256u),
    irq_a("irq_a"),
    irq_b("irq_b"),
    irq_c("irq_c"),
    irq_d("irq_d") {
}

host::~host() {
    // nothing to do
}

const char* host::kind() const {
    if (pcie)
        return "vcml::pcie::host";
    else
        return "vcml::pci::host";
}

unsigned int host::transport(tlm_generic_payload& tx, const tlm_sbi& sideband,
                             address_space space) {
    if (tx.get_command() == TLM_IGNORE_COMMAND)
        return 0;

    u64 size = tx.get_data_length();

    // max 64bit, only naturally aligned accesses
    if (size > 8 || tx.get_address() % min(size, 4ull)) {
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

    pci_payload pci{};
    pci.command = pci_translate_command(tx.get_command());
    pci.response = PCI_RESP_INCOMPLETE;
    pci.space = (pci_address_space)space;
    pci.debug = sideband.is_debug;
    pci.addr = tx.get_address();
    pci.size = size;
    pci.data = 0;
    pci.dmihint = false;

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
        break;

    default:
        VCML_ERROR("invalid address space %d", (int)space);
    }

    if (tx.is_read())
        memcpy(tx.get_data_ptr(), &pci.data, pci.size);

    tx.set_response_status(pci_translate_response(pci.response));
    tx.set_dmi_allowed(pci.dmihint);
    return tx.is_response_ok() ? tx.get_data_length() : 0;
}

void host::pci_transport(pci_payload& tx, bool io) {
    const range addr(tx.addr, tx.addr + tx.size - 1);
    const auto& mapping = lookup(addr, io);
    if (!mapping.is_valid()) {
        tx.response = PCI_RESP_ADDRESS_ERROR;
        return;
    }

    if (tx.size > mapping.limit) {
        tx.response = PCI_RESP_COMMAND_ERROR;
        return;
    }

    if (!pci_out.exists(mapping.devno))
        VCML_ERROR("invalid PCI mapping to nonexistent device");

    tx.addr -= mapping.addr.start;
    tx.space = pci_target_space(mapping.barno);
    pci_out[mapping.devno].transport(tx);
}

void host::pci_transport_cfg(pci_payload& tx) {
    u32 bus, devno, offset;
    u64 addr = tx.addr;

    if (pcie)
        ecam_decode_cfg(addr, bus, devno, offset);
    else
        cam_decode_cfg(addr, bus, devno, offset);

    // not an error to access nonexistent devices or buses
    if (bus != 0 || !pci_out.exists(devno)) {
        tx.response = PCI_RESP_SUCCESS;
        tx.data = ~0u;
        return;
    }

    tx.addr = offset;
    pci_out[devno].transport(tx);
    tx.addr = addr;

    // treat nonexistent registers as reserved memory
    if (tx.is_address_error()) {
        tx.response = PCI_RESP_SUCCESS;
        tx.data = 0;
    }
}

void host::pci_bar_map(const pci_initiator_socket& s, const pci_bar& bar) {
    pci_bar_unmap(s, bar.barno);
    pci_address_space space = pci_target_space(bar.barno);

    u32 devno = pci_devno(s);
    u32 limit = bar.is_64bit ? 8 : 4;
    range addr(bar.addr, bar.addr + bar.size - 1);
    pci_mapping mapping{ devno, limit, bar.barno, space, addr };

    if (bar.is_io) {
        m_map_io.push_back(mapping);
        return;
    }

    m_map_mmio.push_back(mapping);
}

void host::pci_bar_unmap(const pci_initiator_socket& socket, int barno) {
    u32 devno = pci_devno(socket);

    for (auto it = m_map_io.begin(); it != m_map_io.end();) {
        if (it->devno == devno && it->barno == barno)
            it = m_map_io.erase(it);
        else
            it++;
    }

    for (auto it = m_map_mmio.begin(); it != m_map_mmio.end();) {
        if (it->devno == devno && it->barno == barno) {
            invalidate_dmi_mapping(*it, 0ull, ~0ull);
            it = m_map_mmio.erase(it);
        } else {
            it++;
        }
    }
}

void* host::pci_dma_ptr(const pci_initiator_socket& socket, vcml_access rw,
                        u64 addr, u64 size) {
    return dma_out.lookup_dmi_ptr(addr, size, rw);
}

bool host::pci_dma_read(const pci_initiator_socket& socket, u64 addr, u64 size,
                        void* data) {
    u32 devno = pci_devno(socket);
    return success(dma_out.read(addr, data, size, sbi_cpuid(devno)));
}

bool host::pci_dma_write(const pci_initiator_socket& socket, u64 addr,
                         u64 size, const void* data) {
    u32 devno = pci_devno(socket);
    return success(dma_out.write(addr, data, size, sbi_cpuid(devno)));
}

void host::pci_interrupt(const pci_initiator_socket& socket, pci_irq irq,
                         bool state) {
    pci_irq actual = pci_irq_swizzle(irq, pci_devno(socket));
    switch (actual) {
    case PCI_IRQ_A:
        irq_a = state;
        break;

    case PCI_IRQ_B:
        irq_b = state;
        break;

    case PCI_IRQ_C:
        irq_c = state;
        break;

    case PCI_IRQ_D:
        irq_d = state;
        break;

    default:
        VCML_ERROR("invalid PCI_IRQ %d", (int)actual);
    }
}

void host::pci_dmi_invalidate(const pci_initiator_socket& socket, int bar,
                              u64 start, u64 end) {
    u32 devno = pci_devno(socket);
    const auto& m = rev_lookup(devno, bar);
    if (!m.is_valid())
        VCML_ERROR("invalid DMI invalidation from dev%u bar%u", devno, bar);

    invalidate_dmi_mapping(m, start, end);
}

bool host::get_direct_mem_ptr(tlm_target_socket& socket,
                              tlm_generic_payload& tx, tlm_dmi& dmi) {
    // do not serve DMI for config or IO mappings
    if (socket.as != PCI_AS_MMIO)
        return false;

    const auto& m = lookup(tx, false);
    if (!m.is_valid())
        return false;

    if (!pci_out.exists(m.devno))
        VCML_ERROR("invalid PCI mapping to nonexistent device");

    pci_payload pci{};
    pci.command = pci_translate_command(tx.get_command());
    pci.response = PCI_RESP_INCOMPLETE;
    pci.space = (pci_address_space)m.space;
    pci.debug = false;
    pci.addr = tx.get_address() - m.addr.start;
    pci.size = tx.get_data_length();
    pci.data = 0;
    pci.dmihint = false;

    if (!pci_out[m.devno]->pci_get_dmi_ptr(pci, dmi))
        return false;

    u64 s = dmi.get_start_address() + m.addr.start;
    u64 e = dmi.get_end_address() + m.addr.start;

    if (s < m.addr.start) {
        log_warn("truncating DMI start from 0x%016llx to 0x%016llx", s,
                 m.addr.start);
        s = m.addr.start;
    }

    if (e > m.addr.end) {
        log_warn("truncating DMI end from 0x%016llx to 0x%016llx", e,
                 m.addr.end);
        e = m.addr.end;
    }

    dmi.set_start_address(s);
    dmi.set_end_address(e);

    m.dmi = true;

    return true;
}

void host::invalidate_dmi_mapping(const pci_mapping& m, u64 start, u64 end) {
    if (!m.dmi) // have we ever given out DMI for this mapping?
        return;

    if (start >= m.addr.length())
        return;

    if (end >= m.addr.length())
        end = m.addr.length() - 1;

    start += m.addr.start;
    end += m.addr.end;

    if (start <= m.addr.start && end >= m.addr.end)
        m.dmi = false;

    for (auto& [id, socket] : mmio_in)
        (*socket)->invalidate_direct_mem_ptr(start, end);
}

VCML_EXPORT_MODEL(vcml::pci::host, name, args) {
    return new host(name, false);
}

VCML_EXPORT_MODEL(vcml::pcie::host, name, args) {
    return new host(name, true);
}

} // namespace pci
} // namespace vcml
