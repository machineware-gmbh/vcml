/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/pci/device.h"

namespace vcml {
namespace pci {

capability::capability(const string& nm, pci_cap_id id):
    name(nm),
    registers(),
    dev(hierarchy_search<device>()),
    cap_id(),
    nxt_ptr() {
    VCML_ERROR_ON(!dev, "PCI capability declared outside pci device");

    u8 prev_ptr = dev->curr_cap_ptr;
    dev->curr_cap_ptr = dev->curr_cap_off;

    cap_id = new_cap_reg_ro<u8>("cap_id", id);
    nxt_ptr = new_cap_reg_ro<u8>("nxt_ptr", prev_ptr);
}

capability::~capability() {
    for (reg_base* reg : registers)
        delete reg;
}

cap_pm::cap_pm(const string& nm, u16 caps):
    capability(nm, PCI_CAPABILITY_PM), pm_caps(), pm_ctrl() {
    pm_caps = new_cap_reg_ro<u16>("pm_caps", caps);
    pm_ctrl = new_cap_reg_rw<u32>("pm_ctrl", 0);
    pm_ctrl->on_write(&device::write_pm_ctrl);
}

void cap_msi::set_pending(unsigned int vector, bool set) {
    if (!msi_pending)
        return;

    const u32 mask = 1u << vector;

    if (set)
        *msi_pending |= mask;
    else
        *msi_pending &= ~mask;
}

cap_msi::cap_msi(const string& nm, u16 control):
    capability(nm, PCI_CAPABILITY_MSI),
    msi_control(),
    msi_addr(),
    msi_addr_hi(),
    msi_data(),
    msi_mask(),
    msi_pending() {
    msi_control = new_cap_reg_rw<u16>("msi_control", control);
    msi_control->on_write(&device::write_msi_ctrl);
    msi_addr = new_cap_reg_rw<u32>("msi_addr", 0);
    msi_addr->on_write(&device::write_msi_addr);

    if (control & PCI_MSI_64BIT)
        msi_addr_hi = new_cap_reg_rw<u32>("msi_addr_hi", 0);

    msi_data = new_cap_reg_rw<u16>("msi_data", 0);
    dev->curr_cap_off += 2; // reserved space

    if (control & PCI_MSI_VECTOR) {
        msi_mask = new_cap_reg_rw<u32>("msi_mask", 0);
        msi_mask->on_write(&device::write_msi_mask);
        msi_pending = new_cap_reg_ro<u32>("msi_pending", 0);
    }
}

void cap_msix::set_masked(unsigned int vector, bool set) {
    if (set)
        msix_table[vector].ctrl |= PCI_MSIX_MASKED;
    else
        msix_table[vector].ctrl &= ~PCI_MSIX_MASKED;
}

void cap_msix::set_pending(unsigned int vector, bool set) {
    const u32 mask = 1u << (vector % 32);
    if (set)
        msix_pba[vector / 32] |= mask;
    else
        msix_pba[vector / 32] &= ~mask;
}

cap_msix::cap_msix(const string& nm, u32 bar, size_t nvec, u32 off):
    capability(nm, PCI_CAPABILITY_MSIX),
    tbl(),
    pba(),
    tbl_as(PCI_AS_BAR0 + bar),
    pba_as(PCI_AS_BAR0 + bar),
    num_vectors(),
    msix_table(),
    msix_pba(),
    msix_control(),
    msix_bir_off() {
    VCML_ERROR_ON(bar < 0 || bar >= PCI_NUM_BARS, "invalid BAR specified");
    VCML_ERROR_ON(off & 7, "offset must be 8 byte aligned");

    size_t tblsz = nvec * sizeof(msix_entry);
    size_t pbasz = ((nvec + 31) / 32) * sizeof(u32);
    size_t total = off + tblsz + pbasz;

    if (dev->m_bars[bar].size < total)
        VCML_ERROR("MSIX vector table does not fit into BAR%d", bar);

    u16 control = (nvec - 1) & PCI_MSIX_TABLE_SIZE_MASK;
    u32 tbl_off = (bar & PCI_MSIX_BIR_MASK) | (off & ~PCI_MSIX_BIR_MASK);
    u32 pba_off = (bar & PCI_MSIX_BIR_MASK) |
                  ((off + tblsz) & ~PCI_MSIX_BIR_MASK);

    if (control != (nvec - 1))
        VCML_ERROR("too many MSIX vectors: %zu", nvec);

    msix_control = new_cap_reg_rw<u16>("msix_control", control);
    msix_control->on_write(&device::write_msix_ctrl);
    msix_bir_off = new_cap_reg_ro<u32>("msix_bir_off", tbl_off);
    msix_bir_off->on_write([&](u32 val) -> void {
        *msix_bir_off = val;
        update();
    });

    msix_pba_off = new_cap_reg_ro<u32>("msix_pba_off", pba_off);
    msix_pba_off->on_write([&](u32 val) -> void {
        *msix_pba_off = val;
        update();
    });

    update();
}

cap_msix::~cap_msix() {
    if (msix_table)
        delete[] msix_table;
    if (msix_pba)
        delete[] msix_pba;
}

void cap_msix::reset() {
    for (size_t i = 0; i < num_vectors; i++) {
        msix_table[i].addr = 0;
        msix_table[i].data = 0;
        msix_table[i].ctrl = PCI_MSIX_MASKED;
    }

    memset(msix_pba, 0, sizeof(u32) * ((num_vectors + 31) / 32));
}

void cap_msix::update() {
    u32 num_vec = (*msix_control & PCI_MSIX_TABLE_SIZE_MASK) + 1;
    u32 tbl_bar = *msix_bir_off & PCI_MSIX_BIR_MASK;
    u32 pba_bar = *msix_pba_off & PCI_MSIX_BIR_MASK;

    tbl_as = PCI_AS_BAR0 + tbl_bar;
    pba_as = PCI_AS_BAR0 + pba_bar;

    tbl.start = *msix_bir_off & ~PCI_MSIX_BIR_MASK;
    pba.start = *msix_pba_off & ~PCI_MSIX_BIR_MASK;

    tbl.end = tbl.start + num_vec * sizeof(msix_entry) - 1;
    pba.end = pba.start + ((num_vec + 31) / 32) * sizeof(u32);

    if (tbl.end >= dev->m_bars[tbl_bar].size)
        VCML_ERROR("MSIX table does not fit into BAR%u", tbl_bar);
    if (pba.end >= dev->m_bars[pba_bar].size)
        VCML_ERROR("MSIX PBA does not fit into BAR%u", pba_bar);

    if (num_vec != num_vectors) {
        delete[] msix_table;
        delete[] msix_pba;
        num_vectors = num_vec;
        msix_table = new msix_entry[num_vec];
        msix_pba = new u32[(num_vec + 31) / 32];
    }

    reset();
}

tlm_response_status cap_msix::read_tbl(const range& addr, void* data) {
    if (addr.length() != 4 || addr.start % 4)
        return TLM_BURST_ERROR_RESPONSE;

    unsigned int vector = addr.start / sizeof(msix_entry);
    unsigned int offset = addr.start % sizeof(msix_entry);
    VCML_ERROR_ON(vector > num_vectors, "read out of bounds");

    msix_entry* entry = msix_table + vector;
    memcpy(data, (u8*)(entry) + offset, addr.length());
    return TLM_OK_RESPONSE;
}

tlm_response_status cap_msix::write_tbl(const range& addr, const void* data) {
    if (addr.length() != 4 || addr.start % 4)
        return TLM_BURST_ERROR_RESPONSE;

    unsigned int vector = (addr.start - tbl.start) / sizeof(msix_entry);
    unsigned int offset = (addr.start - tbl.start) % sizeof(msix_entry);
    VCML_ERROR_ON(vector > num_vectors, "read out of bounds");

    msix_entry* entry = msix_table + vector;
    memcpy((u8*)(entry) + offset, data, addr.length());
    entry->addr &= ~3ull;
    entry->ctrl &= PCI_MSIX_MASKED;
    dev->m_msix_notify.notify(SC_ZERO_TIME);
    return TLM_OK_RESPONSE;
}

tlm_response_status cap_msix::read_pba(const range& addr, void* data) {
    if (addr.length() != 4 || addr.start % 4)
        return TLM_BURST_ERROR_RESPONSE;

    memcpy(data, (u8*)msix_pba + (addr.start - pba.start), addr.length());
    return TLM_OK_RESPONSE;
}

tlm_response_status cap_msix::write_pba(const range& addr, const void* data) {
    if (addr.length() != 4 || addr.start % 4)
        return TLM_BURST_ERROR_RESPONSE;

    // ignore writes to pba
    return TLM_OK_RESPONSE;
}

enum cap_cpie_init_defaults : u32 {
    PCIE_FLAGS_INIT = PCI_EXP_V3 | PCI_EXP_TYPE_ENDPOINT,
    PCIE_DEVCAP_INIT = PCI_EXP_DEVCAP_MAX_PAYLOAD_512 |
                       PCI_EXP_DEVCAP_L0S_128NS | PCI_EXP_DEVCAP_L1_UNLIMITED |
                       PCI_EXP_DEVCAP_RBE,
    PCIE_DEVCAP2_INIT = PCI_EXP_DEVCAP2_CTR_B | PCI_EXP_DEVCAP2_CTDS |
                        PCI_EXP_DEVCAP2_LTR | PCI_EXP_DEVCAP2_EXT_FMT,
    PCIE_DEVCTL_INIT = PCI_EXP_DEVCTL_RELAX | PCI_EXP_DEVCTL_MAX_PAYLOAD_128 |
                       PCI_EXP_DEVCTL_NO_SNOOP | PCI_EXP_DEVCTL_MAX_READ_512,
    PCIE_LINKCAP_INIT = PCI_EXP_LINKCAP_MLS_16G | PCI_EXP_LINKCAP_MLW_X4 |
                        PCI_EXP_LINKCAP_ASPM_L1 | PCI_EXP_LINKCAP_ASPM_OC,
    PCIE_LINKCAP2_INIT = PCI_EXP_LINKCAP2_SLS_2_5G | PCI_EXP_LINKCAP2_SLS_5G |
                         PCI_EXP_LINKCAP2_SLS_8G | PCI_EXP_LINKCAP2_SLS_16G,
    PCIE_LINKSTS_INIT = PCI_EXP_LINKSTS_CLS_16G | PCI_EXP_LINKSTS_NLW_X4,
    PCIE_LINKSTS2_INIT = PCI_EXP_LINKSTS2_EQC | PCI_EXP_LINKSTS2_EP1S |
                         PCI_EXP_LINKSTS2_EP2S | PCI_EXP_LINKSTS2_EP3S,
};

cap_pcie::cap_pcie(const string& nm):
    capability(nm, PCI_CAPABILITY_PCIE),
    flags(new_cap_reg_ro<u16>("flags", PCIE_FLAGS_INIT)),
    dev_cap(new_cap_reg_ro<u32>("dev_cap", PCIE_DEVCAP_INIT)),
    dev_ctl(new_cap_reg_rw<u16>("dev_ctl", PCIE_DEVCTL_INIT)),
    dev_sts(new_cap_reg_rw<u16>("dev_sts", 0)),
    link_cap(new_cap_reg_ro<u32>("link_cap", PCIE_LINKCAP_INIT)),
    link_ctl(new_cap_reg_rw<u16>("link_ctl", 0)),
    link_sts(new_cap_reg_rw<u16>("link_sts", PCIE_LINKSTS_INIT)),
    slot_cap(new_cap_reg_ro<u32>("slot_cap", 0)),
    slot_ctl(new_cap_reg_rw<u16>("slot_ctl", 0)),
    slot_sts(new_cap_reg_rw<u16>("slot_sts", 0)),
    root_cap(new_cap_reg_ro<u16>("root_cap", 0)),
    root_ctl(new_cap_reg_rw<u16>("root_ctl", 0)),
    root_sts(new_cap_reg_rw<u32>("root_sts", 0)),
    dev_cap2(new_cap_reg_ro<u32>("dev_cap2", PCIE_DEVCAP2_INIT)),
    dev_ctl2(new_cap_reg_rw<u16>("dev_ctl2", 0)),
    dev_sts2(new_cap_reg_rw<u16>("dev_sts2", 0)),
    link_cap2(new_cap_reg_ro<u32>("link_cap2", PCIE_LINKCAP2_INIT)),
    link_ctl2(new_cap_reg_rw<u16>("link_ctl2", 0)),
    link_sts2(new_cap_reg_rw<u16>("link_sts2", PCIE_LINKSTS2_INIT)),
    slot_cap2(new_cap_reg_ro<u32>("slot_cap2", 0)),
    slot_ctl2(new_cap_reg_rw<u16>("slot_ctl2", 0)),
    slot_sts2(new_cap_reg_rw<u16>("slot_sts2", 0)) {
    dev_sts->on_write(
        [&](u16 val) -> void { *dev_sts &= ~(val & PCI_EXP_DEVSTS_RW1C); });
}

device::device(const sc_module_name& nm, const pci_config& cfg):
    peripheral(nm),
    pci_target(),
    pcie("pcie", cfg.pcie),
    pci_vendor_id(PCI_AS_CFG, "pci_vendor_id", 0x0, cfg.vendor_id),
    pci_device_id(PCI_AS_CFG, "pci_device_id", 0x2, cfg.device_id),
    pci_command(PCI_AS_CFG, "pci_command", 0x4, 0),
    pci_status(PCI_AS_CFG, "pci_status", 0x6, pci_status_init(cfg.pcie)),
    pci_class(PCI_AS_CFG, "pci_class", 0x8, cfg.class_code),
    pci_cache_line(PCI_AS_CFG, "pci_cache_line", 0xc, 0),
    pci_latency_timer(PCI_AS_CFG, "pci_latency_timer", 0xd, 0),
    pci_header_type(PCI_AS_CFG, "pci_header_type", 0xe, 0),
    pci_bist(PCI_AS_CFG, "pci_bist", 0xf, 0),
    pci_bars(PCI_AS_CFG, "pci_bars", 0x10, 0),
    pci_subvendor_id(PCI_AS_CFG, "pci_subvendor_id", 0x2c, cfg.subvendor_id),
    pci_subdevice_id(PCI_AS_CFG, "pci_subdevice_id", 0x2e, cfg.subsystem_id),
    pci_cap_ptr(PCI_AS_CFG, "pci_cap_ptr", 0x34, 0),
    pci_int_line(PCI_AS_CFG, "pci_int_line", 0x3c, 0),
    pci_int_pin(PCI_AS_CFG, "pci_int_pin", 0x3d, cfg.int_pin),
    pci_min_grant(PCI_AS_CFG, "pci_min_grant", 0x3e, pci_get_min_grant(cfg)),
    pci_max_latency(PCI_AS_CFG, "pci_max_latency", 0x3f, pci_get_max_lat(cfg)),
    pcie_xcap(PCI_AS_CFG, "pcie_xcap", 0x100, 0),
    curr_cap_ptr(0),
    curr_cap_off(64),
    m_bars(),
    m_irq(PCI_IRQ_NONE),
    m_pm(nullptr),
    m_msi(nullptr),
    m_msix(nullptr),
    m_pcie(nullptr),
    m_msi_notify("msi_notify"),
    m_msix_notify("msix_notify") {
    pci_vendor_id.allow_read_only();
    pci_vendor_id.sync_never();

    pci_device_id.allow_read_only();
    pci_device_id.sync_never();

    pci_command.allow_read_write();
    pci_command.sync_always();
    pci_command.on_write(&device::write_command);

    pci_status.allow_read_write();
    pci_status.sync_always();
    pci_status.on_write(&device::write_status);

    pci_class.allow_read_only();
    pci_class.sync_never();

    pci_cache_line.allow_read_write();
    pci_cache_line.sync_never();

    pci_latency_timer.allow_read_write();
    pci_latency_timer.sync_never();

    pci_header_type.allow_read_only();
    pci_header_type.sync_never();

    pci_bist.allow_read_write();
    pci_bist.sync_always();

    pci_bars.allow_read_write();
    pci_bars.sync_always();
    pci_bars.on_write(&device::write_bars);

    pci_subvendor_id.allow_read_only();
    pci_subvendor_id.sync_never();

    pci_subdevice_id.allow_read_only();
    pci_subdevice_id.sync_never();

    pci_cap_ptr.allow_read_only();
    pci_cap_ptr.sync_never();
    pci_cap_ptr.on_read([&]() -> u8 { return (u8)curr_cap_ptr; });

    pci_int_line.allow_read_write();
    pci_int_line.sync_always();

    pci_int_pin.allow_read_only();
    pci_int_pin.sync_always();

    pcie_xcap.allow_read_only();
    pcie_xcap.sync_never();

    for (u32 i = 0; i < PCI_NUM_BARS; i++) {
        m_bars[i].barno = i;
        m_bars[i].size = 0;
    }

    if (pcie)
        m_pcie = new cap_pcie("pci_cap_pcie");
}

device::~device() {
    if (m_pcie)
        delete m_pcie;
    if (m_pm)
        delete m_pm;
    if (m_msi)
        delete m_msi;
    if (m_msix)
        delete m_msix;
}

void device::reset() {
    peripheral::reset();

    for (auto& bar : m_bars) {
        bar.addr_lo = PCI_BAR_UNMAPPED & ~(bar.size - 1);
        bar.addr_hi = bar.is_64bit ? PCI_BAR_UNMAPPED : 0u;
    }

    update_bars();

    if (m_msix)
        m_msix->reset();
}

void device::pci_declare_bar(int barno, u64 size, u32 type, void* ptr) {
    bool is_io = type & PCI_BAR_IO;
    bool is_64 = type & PCI_BAR_64;
    bool is_prefetch = type & PCI_BAR_PREFETCH;
    int maxbar = is_64 ? PCI_NUM_BARS - 1 : PCI_NUM_BARS;

    VCML_ERROR_ON(is_io && is_64, "IO BAR cannot be 64 bit");
    VCML_ERROR_ON(is_io && is_prefetch, "cannot prefetch IO BAR");
    VCML_ERROR_ON(barno >= maxbar, "barno %d out of bounds", barno);

    m_bars[barno].size = size;
    m_bars[barno].is_io = is_io;
    m_bars[barno].is_64bit = is_64;
    m_bars[barno].is_prefetch = is_prefetch;
    m_bars[barno].addr_lo = PCI_BAR_UNMAPPED & ~(size - 1);
    m_bars[barno].addr_hi = is_64 ? PCI_BAR_UNMAPPED : 0u;
    m_bars[barno].host = (u8*)ptr;
}

void device::pci_declare_pm_cap(u16 pm_caps) {
    auto guard = get_hierarchy_scope();
    VCML_ERROR_ON(m_pm, "pci_cap_pm already declared");
    m_pm = new cap_pm("pci_cap_pm", pm_caps);
}

void device::pci_declare_msi_cap(u16 msi_ctrl) {
    auto guard = get_hierarchy_scope();
    VCML_ERROR_ON(m_msi, "pci_cap_msi already declared");
    m_msi = new cap_msi("pci_cap_msi", msi_ctrl);
    SC_HAS_PROCESS(device);
    SC_THREAD(msi_process);
}

void device::pci_declare_msix_cap(u32 bar, size_t nvec, u32 offset) {
    auto guard = get_hierarchy_scope();
    VCML_ERROR_ON(m_msix, "pci_cap_msix already declared");
    m_msix = new cap_msix("pci_cap_msix", bar, nvec, offset);
    SC_HAS_PROCESS(device);
    SC_THREAD(msix_process);
}

void device::pci_interrupt(bool state, unsigned int vector) {
    if (msix_enabled())
        msix_interrupt(state, vector);
    else if (msi_enabled())
        msi_interrupt(state, vector);
    else
        pci_legacy_interrupt(state);
}

void device::msi_interrupt(bool state, unsigned int vector) {
    VCML_ERROR_ON(!m_msi, "not capable of sending MSIs");
    if (!msi_enabled() || !(pci_command & PCI_COMMAND_MMIO))
        return;

    m_msi->set_pending(vector, state);

    if (!m_msi->is_masked(vector) && m_msi->is_pending(vector))
        m_msi_notify.notify(SC_ZERO_TIME);
}

void device::msix_interrupt(bool state, unsigned int vector) {
    VCML_ERROR_ON(!m_msix, "not capable of sending MSIXs");
    if (!msix_enabled() || !(pci_command & PCI_COMMAND_MMIO))
        return;

    m_msix->set_pending(vector, state);

    if (!m_msix->is_masked(vector) && m_msix->is_pending(vector))
        m_msix_notify.notify(SC_ZERO_TIME);
}

void device::pci_legacy_interrupt(bool state) {
    if (state)
        pci_status |= PCI_STATUS_IRQ;
    else
        pci_status &= ~PCI_STATUS_IRQ;

    update_irqs();
}

void device::pci_bypass_cfgro(bool enable) {
    if (enable) {
        if (is_bypassing_cfgro())
            return; // already enabled

        for (auto* reg : get_registers(PCI_AS_CFG)) {
            if (reg->is_read_only()) {
                reg->allow_read_write();
                m_temp_rw_regs.push_back(reg);
            }
        }
    } else {
        for (auto* reg : m_temp_rw_regs)
            reg->allow_read_only();
        m_temp_rw_regs.clear();
    }
}

void device::pci_transport(const pci_target_socket& sck, pci_payload& pci) {
    tlm_generic_payload tx;
    tlm_command cmd = pci_translate_command(pci.command);
    tx_setup(tx, cmd, pci.addr, &pci.data, pci.size);
    receive(tx, pci.debug ? SBI_DEBUG : SBI_NONE, pci.space);
    pci.response = pci_translate_response(tx.get_response_status());
}

bool device::read_mem_bar(const range& addr, void* data, const tlm_sbi& sbi,
                          address_space as) {
    if (as < PCI_AS_BAR0 || as > PCI_AS_BAR5)
        return false;

    pci_bar* bar = m_bars + as - PCI_AS_BAR0;
    if (bar->host == nullptr)
        return false;
    if (addr.end >= bar->size)
        return false;

    memcpy(data, bar->host + addr.start, addr.length());
    return true;
}

bool device::write_mem_bar(const range& addr, const void* data,
                           const tlm_sbi& sbi, address_space as) {
    if (as < PCI_AS_BAR0 || as > PCI_AS_BAR5)
        return false;

    pci_bar* bar = m_bars + as - PCI_AS_BAR0;
    if (bar->host == nullptr)
        return false;
    if (addr.end >= bar->size)
        return false;

    memcpy(bar->host + addr.start, data, addr.length());
    return true;
}

tlm_response_status device::read(const range& addr, void* data,
                                 const tlm_sbi& info, address_space as) {
    if (m_msix && m_msix->tbl_as == as && addr.overlaps(m_msix->tbl))
        return m_msix->read_tbl(addr, data);
    if (m_msix && m_msix->pba_as == as && addr.overlaps(m_msix->pba))
        return m_msix->write_pba(addr, data);
    if (read_mem_bar(addr, data, info, as))
        return TLM_OK_RESPONSE;
    return peripheral::read(addr, data, info, as);
}

tlm_response_status device::write(const range& addr, const void* data,
                                  const tlm_sbi& info, address_space as) {
    if (m_msix && m_msix->tbl_as == as && addr.overlaps(m_msix->tbl))
        return m_msix->write_tbl(addr, data);
    if (m_msix && m_msix->pba_as == as && addr.overlaps(m_msix->pba))
        return m_msix->write_pba(addr, data);
    if (write_mem_bar(addr, data, info, as))
        return TLM_OK_RESPONSE;
    return peripheral::write(addr, data, info, as);
}

void device::msi_send(unsigned int vector) {
    u32 vmask = m_msi->num_vectors() - 1;
    u32 msi_data = (*m_msi->msi_data & ~vmask) | (vector & vmask);
    u64 msi_addr = *m_msi->msi_addr;

    if (m_msi->is_64bit())
        msi_addr |= (u64)(u32)*m_msi->msi_addr_hi << 32;

    if (!pci_dma_write(msi_addr, sizeof(msi_data), &msi_data))
        log_warn("DMA error while sending MSI%u", vector);
}

void device::msi_process() {
    while (true) {
        wait(m_msi_notify);
        for (unsigned int vec = 0; vec < m_msi->num_vectors(); vec++) {
            if (m_msi->is_pending(vec) && !m_msi->is_masked(vec)) {
                m_msi->set_pending(vec, false);
                msi_send(vec);
            }
        }
    }
}

void device::msix_send(unsigned int vector) {
    if (vector >= m_msix->num_vectors)
        VCML_ERROR("MSIX vector out of bounds: %u", vector);

    u32 msix_data = m_msix->msix_table[vector].data;
    u64 msix_addr = m_msix->msix_table[vector].addr;

    if (!pci_dma_write(msix_addr, sizeof(msix_data), &msix_data))
        log_warn("DMA error while sending MSIX%u", vector);
}

void device::msix_process() {
    while (true) {
        wait(m_msix_notify);
        for (unsigned int vec = 0; vec < m_msix->num_vectors; vec++) {
            if (m_msix->is_pending(vec) && !m_msix->is_masked(vec)) {
                m_msix->set_pending(vec, false);
                msix_send(vec);
            }
        }
    }
}

void device::write_bars(u32 val, size_t barno) {
    pci_bars[barno] = val;
    update_bars();
}

void device::write_command(u16 val) {
    u16 mask = PCI_COMMAND_IO | PCI_COMMAND_MMIO | PCI_COMMAND_BUS_MASTER |
               PCI_COMMAND_PARITY | PCI_COMMAND_SERR | PCI_COMMAND_NO_IRQ;

    if (!pcie) {
        mask |= PCI_COMMAND_SPECIAL | PCI_COMMAND_INVALIDATE |
                PCI_COMMAND_PALETTE | PCI_COMMAND_WAIT | PCI_COMMAND_FAST_B2B;
    }

    pci_command = val & mask;
    update_irqs();
    update_bars();
}

void device::write_status(u16 val) {
    u16 mask = PCI_STATUS_MASTER_PARITY_ERROR | PCI_STATUS_TX_TARGET_ABORT |
               PCI_STATUS_RX_TARGET_ABORT | PCI_STATUS_RX_MASTER_ABORT |
               PCI_STATUS_TX_SYSTEM_ERROR | PCI_STATUS_PARITY_ERROR;
    pci_status &= ~(val & mask);
}

void device::write_pm_ctrl(u32 val) {
    u32 mask = PCI_PM_CTRL_PSTATE_D3H;
    if (!(val & PCI_PM_CTRL_PME))
        mask |= PCI_PM_CTRL_PME_ENABLE;
    *m_pm->pm_ctrl = (*m_pm->pm_ctrl & ~mask) | (val & mask);
}

void device::write_msi_ctrl(u16 val) {
    size_t num_vectors = (val & PCI_MSI_QSIZE) >> 4;
    if (num_vectors > m_msi->max_vectors()) {
        log_warn("exceeding max MSI vectors %zu", num_vectors);
        num_vectors = m_msi->max_vectors();

        val = (val & ~PCI_MSI_QSIZE) | num_vectors << 4;
    }

    u16 mask = PCI_MSI_ENABLE | PCI_MSI_QSIZE;
    if (is_bypassing_cfgro())
        mask = 0xffff;

    *m_msi->msi_control = (*m_msi->msi_control & ~mask) | (val & mask);
}

void device::write_msi_addr(u32 val) {
    *m_msi->msi_addr = val & ~0x3;
}

void device::write_msi_mask(u32 val) {
    *m_msi->msi_mask = val & ((1ull << m_msi->num_vectors()) - 1);
    m_msi_notify.notify(SC_ZERO_TIME);
}

void device::write_msix_ctrl(u16 val) {
    u64 mask = PCI_MSIX_ENABLE | PCI_MSIX_ALL_MASKED;
    if (is_bypassing_cfgro())
        mask = 0xffffffffffffffff;

    *m_msix->msix_control = (*m_msix->msix_control & ~mask) | (val & mask);
    m_msix_notify.notify(SC_ZERO_TIME);
}

void device::update_bars() {
    for (unsigned int barno = 0; barno < PCI_NUM_BARS; barno++) {
        pci_bar* bar = m_bars + barno;
        if (bar->size == 0)
            continue;

        pci_bars[barno] &= bar->mask();
        if (bar->is_io)
            pci_bars[barno] |= PCI_BAR_IO;
        if (bar->is_64bit)
            pci_bars[barno] |= PCI_BAR_64;
        if (bar->is_prefetch)
            pci_bars[barno] |= PCI_BAR_PREFETCH;

        u64 addr = pci_bars[barno] & bar->mask();
        if (bar->is_64bit)
            addr |= (u64)pci_bars[barno + 1] << 32;
        if (!(pci_command & PCI_COMMAND_IO) && bar->is_io)
            addr = PCI_BAR_UNMAPPED;
        if (!(pci_command & PCI_COMMAND_MMIO) && !bar->is_io)
            addr = PCI_BAR_UNMAPPED;

        if (addr == bar->addr)
            continue;

        bar->addr = addr;

        if (bar->addr == PCI_BAR_UNMAPPED) {
            log_debug("unmapping BAR%d", barno);
            pci_bar_unmap(barno);
        } else {
            log_debug("mapping BAR%d to %s 0x%llx..0x%llx", barno,
                      bar->is_io ? "IO" : "MMIO", bar->addr,
                      bar->addr + bar->size - 1);
            pci_bar_map(m_bars[barno]);
        }
    }
}

void device::update_irqs() {
    pci_irq irq = PCI_IRQ_NONE;
    bool suppressed = pci_command & PCI_COMMAND_NO_IRQ;

    if ((pci_status & PCI_STATUS_IRQ) && !suppressed)
        irq = (pci_irq)(u8)pci_int_pin;

    if (irq == m_irq)
        return;

    if (m_irq != PCI_IRQ_NONE)
        pci_target::pci_interrupt(m_irq, false);

    m_irq = irq;

    if (m_irq != PCI_IRQ_NONE)
        pci_target::pci_interrupt(m_irq, true);
}

} // namespace pci
} // namespace vcml
