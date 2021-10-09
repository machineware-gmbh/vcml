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

#include "vcml/models/generic/pci_device.h"

namespace vcml { namespace generic {

    const char* pci_cap_str(pci_cap_id id) {
        switch (id) {
        case PCI_CAPABILITY_PM: return "PCI_CAP_PM";
        case PCI_CAPABILITY_MSI: return "PCI_CAP_MSI";
        case PCI_CAPABILITY_MSIX: return "PCI_CAP_MSIX";
        default:
            VCML_ERROR("unknown capability id %d", (int)id);
        }
    }

    pci_capability::pci_capability(pci_device* dev, pci_cap_id cap_id):
        CAP_ID(),
        CAP_NEXT() {
        u8 prev_ptr = dev->pci_cap_ptr;
        dev->pci_cap_ptr = dev->pci_cap_off;

        string name_id = mkstr("%s_ID", pci_cap_str(cap_id));
        string name_next = mkstr("%s_NEXT", pci_cap_str(cap_id));

        CAP_ID = dev->new_cap_reg_ro<u8>(name_id, cap_id);
        CAP_NEXT = dev->new_cap_reg_ro<u8>(name_next, prev_ptr);
    }

    pci_capability::~pci_capability() {
        if (CAP_ID)
            delete CAP_ID;
        if (CAP_NEXT)
            delete CAP_NEXT;
    }

    pci_cap_pm::pci_cap_pm(pci_device* dev, u16 caps):
        pci_capability(dev, PCI_CAPABILITY_PM),
        PM_CAPS(),
        PM_CTRL() {

        PM_CAPS = dev->new_cap_reg_ro<u16>("PCI_PM_CAPS", caps);
        PM_CTRL = dev->new_cap_reg_rw<u32>("PCI_PM_CTRL", 0);
        PM_CTRL->write = &pci_device::write_PM_CTRL;
    }

    pci_cap_pm::~pci_cap_pm() {
        if (PM_CAPS)
            delete PM_CAPS;
        if (PM_CTRL)
            delete PM_CTRL;
    }

    void pci_cap_msi::set_pending(unsigned int vector, bool set) {
        if (!MSI_PENDING)
            return;

        const u32 mask = 1u << vector;

        if (set)
            *MSI_PENDING |= mask;
        else
            *MSI_PENDING &= ~mask;
    }

    pci_cap_msi::pci_cap_msi(pci_device* dev, u16 control):
        pci_capability(dev, PCI_CAPABILITY_MSI),
        MSI_CONTROL(),
        MSI_ADDR(),
        MSI_ADDR_HI(),
        MSI_DATA(),
        MSI_MASK(),
        MSI_PENDING() {
        MSI_CONTROL = dev->new_cap_reg_rw<u16>("PCI_CAP_MSI_CONTROL", control);
        MSI_CONTROL->write = &pci_device::write_MSI_CTRL;
        MSI_ADDR = dev->new_cap_reg_rw<u32>("PCI_CAP_MSI_ADDR", 0);
        MSI_ADDR->write = &pci_device::write_MSI_ADDR;

        if (control & PCI_MSI_64BIT)
            MSI_ADDR_HI = dev->new_cap_reg_rw<u32>("PCI_CAP_MSI_ADDR_HI", 0);

        MSI_DATA = dev->new_cap_reg_rw<u16>("PCI_CAP_MSI_DATA", 0);
        dev->pci_cap_off += 2; // reserved space

        if (control & PCI_MSI_VECTOR) {
            MSI_MASK = dev->new_cap_reg_rw<u32>("PCI_CAP_MSI_MASK", 0);
            MSI_MASK->write = &pci_device::write_MSI_MASK;
            MSI_PENDING = dev->new_cap_reg_ro<u32>("PCI_CAP_MSI_PENDING", 0);
        }
    }

    pci_cap_msi::~pci_cap_msi() {
        if (MSI_CONTROL)
            delete MSI_CONTROL;
        if (MSI_ADDR)
            delete MSI_ADDR;
        if (MSI_ADDR_HI)
            delete MSI_ADDR_HI;
        if (MSI_DATA)
            delete MSI_DATA;
        if (MSI_MASK)
            delete MSI_MASK;
        if (MSI_PENDING)
            delete MSI_PENDING;
    }

    void pci_cap_msix::set_masked(unsigned int vector, bool set) {
        if (set)
            msix_table[vector].addr |= PCI_MSIX_MASKED;
        else
            msix_table[vector].addr &= ~PCI_MSIX_MASKED;
    }

    void pci_cap_msix::set_pending(unsigned int vector, bool set) {
        if (set)
            msix_table[vector].addr |= PCI_MSIX_PENDING;
        else
            msix_table[vector].addr &= ~PCI_MSIX_PENDING;
    }

    pci_cap_msix::pci_cap_msix(pci_device* dev, u32 b, size_t nvec, u32 off):
        pci_capability(dev, PCI_CAPABILITY_MSIX),
        mem(off, off + nvec * sizeof(msix_entry) - 1),
        bar(b),
        bar_as(PCI_AS_BAR0 + bar),
        num_vectors(nvec),
        msix_table(),
        MSIX_CONTROL(),
        MSIX_ADDR_HI(),
        MSIX_BIR_OFF() {
        VCML_ERROR_ON(bar < 0 || bar > PCI_NUM_BARS, "invalid BAR specified");
        VCML_ERROR_ON(!dev->m_bars[bar].size, "BAR%u not declared", bar);

        if (dev->m_bars[bar].size < (nvec * 8) + off)
            VCML_ERROR("MSIX vector table does not fit into BAR%d", bar);

        u16 ctrl = (nvec - 1) & PCI_MSIX_TABLE_SIZE_MASK;
        u32 boff = (bar & PCI_MSIX_BIR_MASK) | (off & ~PCI_MSIX_BIR_MASK);

        if (ctrl != (nvec - 1))
            VCML_ERROR("too many MSIX vectors: %zu", nvec);

        msix_table = new msix_entry[nvec];

        MSIX_CONTROL = dev->new_cap_reg_rw<u16>("PCI_CAP_MSIX_CONTROL", ctrl);
        MSIX_CONTROL->write = &pci_device::write_MSIX_CTRL;
        MSIX_ADDR_HI = dev->new_cap_reg_rw("PCI_CAP_MSIX_ADDR_HI", 0u);
        MSIX_BIR_OFF = dev->new_cap_reg_ro("PCI_CAP_MSIX_BIR", boff);
    }

    pci_cap_msix::~pci_cap_msix() {
        if (msix_table)
            delete [] msix_table;
        if (MSIX_CONTROL)
            delete MSIX_CONTROL;
        if (MSIX_ADDR_HI)
            delete MSIX_ADDR_HI;
        if (MSIX_BIR_OFF)
            delete MSIX_BIR_OFF;
    }

    void pci_cap_msix::reset() {
        for (size_t vector = 0; vector < num_vectors; vector++) {
            msix_table[vector].data = 0;
            msix_table[vector].addr = PCI_MSIX_MASKED;
        }
    }

    tlm_response_status pci_cap_msix::read_table(const range& addr,
        void* data) {
        if (addr.length() != 4 || addr.start % 4)
            return TLM_BURST_ERROR_RESPONSE;

        unsigned int vector = (addr.start - mem.start) / sizeof(msix_entry);
        unsigned int offset = (addr.start - mem.start) % sizeof(msix_entry);
        VCML_ERROR_ON(vector > num_vectors, "read out of bounds");

        if (offset == 0)
            *(u32*)data = msix_table[vector].data;
        else
            *(u32*)data = msix_table[vector].addr;

        return TLM_OK_RESPONSE;
    }

    tlm_response_status pci_cap_msix::write_table(const range& addr,
        const void* data) {
        if (addr.length() != 4 || addr.start % 4)
            return TLM_BURST_ERROR_RESPONSE;

        unsigned int vector = (addr.start - mem.start) / sizeof(msix_entry);
        unsigned int offset = (addr.start - mem.start) % sizeof(msix_entry);
        VCML_ERROR_ON(vector > num_vectors, "read out of bounds");

        if (offset == 0)
            msix_table[vector].data = *(u32*)data;
        else {
            msix_table[vector].addr &= PCI_MSIX_PENDING;
            msix_table[vector].addr |= *(u32*)data & ~PCI_MSIX_PENDING;
        }

        return TLM_OK_RESPONSE;
    }

    pci_device::pci_device(const sc_module_name& nm, const pci_config& cfg):
        peripheral(nm),
        pci_target(),
        pcie("pcie", cfg.pcie),
        PCI_VENDOR_ID(PCI_AS_CFG, "PCI_VENDOR_ID", 0x0, cfg.vendor_id),
        PCI_DEVICE_ID(PCI_AS_CFG, "PCI_DEVICE_ID", 0x2, cfg.device_id),
        PCI_COMMAND(PCI_AS_CFG, "PCI_COMMAND", 0x4, 0),
        PCI_STATUS(PCI_AS_CFG, "PCI_STATUS", 0x6, PCI_STATUS_INIT(cfg.pcie)),
        PCI_CLASS(PCI_AS_CFG, "PCI_CLASS", 0x8, cfg.class_code),
        PCI_CACHE_LINE(PCI_AS_CFG, "PCI_CACHE_LINE", 0xc, 0),
        PCI_LATENCY_TIMER(PCI_AS_CFG, "PCI_LATENCY_TIMER", 0xd, 0),
        PCI_HEADER_TYPE(PCI_AS_CFG, "PCI_HEADER_TYPE", 0xe, 0),
        PCI_BIST(PCI_AS_CFG, "PCI_BIST", 0xf, 0),
        PCI_BAR(PCI_AS_CFG, "PCI_BAR", 0x10, PCI_BAR_UNMAPPED),
        PCI_SUBVENDOR_ID(PCI_AS_CFG,"PCI_SUBVENDOR_ID",0x2c, cfg.subvendor_id),
        PCI_SUBDEVICE_ID(PCI_AS_CFG,"PCI_SUBDEVICE_ID",0x2e, cfg.subsystem_id),
        PCI_CAP_PTR(PCI_AS_CFG, "PCI_CAP_PTR", 0x34, 0),
        PCI_INT_LINE(PCI_AS_CFG, "PCI_INT_LINE", 0x3c, 0),
        PCI_INT_PIN(PCI_AS_CFG, "PCI_INT_PIN", 0x3d, cfg.int_pin),
        PCI_MIN_GRANT(PCI_AS_CFG, "PCI_MIN_GRANT", 0x3e, pci_min_grant(cfg)),
        PCI_MAX_LATENCY(PCI_AS_CFG, "PCI_MAX_LATENCY", 0x3f, pci_max_lat(cfg)),
        PCIE_XCAP(PCI_AS_CFG, "PCIE_XCAP", 0x100, 0),
        pci_cap_ptr(0),
        pci_cap_off(64),
        m_bars(),
        m_irq(PCI_IRQ_NONE),
        m_pm(nullptr),
        m_msi(nullptr),
        m_msix(nullptr),
        m_msi_notify("msi_notify"),
        m_msix_notify("msix_notify") {

        PCI_VENDOR_ID.allow_read_only();
        PCI_VENDOR_ID.sync_never();

        PCI_DEVICE_ID.allow_read_only();
        PCI_DEVICE_ID.sync_never();

        PCI_COMMAND.allow_read_write();
        PCI_COMMAND.sync_always();
        PCI_COMMAND.write = &pci_device::write_COMMAND;

        PCI_STATUS.allow_read_write();
        PCI_STATUS.sync_always();
        PCI_STATUS.write = &pci_device::write_STATUS;

        PCI_CLASS.allow_read_only();
        PCI_CLASS.sync_never();

        PCI_CACHE_LINE.allow_read_write();
        PCI_CACHE_LINE.sync_never();

        PCI_LATENCY_TIMER.allow_read_write();
        PCI_LATENCY_TIMER.sync_never();

        PCI_HEADER_TYPE.allow_read_only();
        PCI_HEADER_TYPE.sync_never();

        PCI_BIST.allow_read_write();
        PCI_BIST.sync_always();

        PCI_BAR.allow_read_write();
        PCI_BAR.sync_always();
        PCI_BAR.tagged_write = &pci_device::write_BAR;

        PCI_SUBVENDOR_ID.allow_read_only();
        PCI_SUBVENDOR_ID.sync_never();

        PCI_SUBDEVICE_ID.allow_read_only();
        PCI_SUBDEVICE_ID.sync_never();

        PCI_CAP_PTR.allow_read_only();
        PCI_CAP_PTR.sync_never();
        PCI_CAP_PTR.read = &pci_device::read_CAP_PTR;

        PCI_INT_LINE.allow_read_write();
        PCI_INT_LINE.sync_always();

        PCI_INT_PIN.allow_read_only();
        PCI_INT_PIN.sync_always();

        PCIE_XCAP.allow_read_only();
        PCIE_XCAP.sync_never();

        for (u32 i = 0; i < PCI_NUM_BARS; i++) {
            m_bars[i].barno = i;
            m_bars[i].size = 0;
        }
    }

    pci_device::~pci_device() {
        if (m_pm)
            delete m_pm;
        if (m_msi)
            delete m_msi;
        if (m_msix)
            delete m_msix;
    }

    void pci_device::reset() {
        peripheral::reset();

        for (auto& bar : m_bars) {
            bar.addr_lo = PCI_BAR_UNMAPPED & ~(bar.size - 1);
            bar.addr_hi = bar.is_64bit ? PCI_BAR_UNMAPPED : 0u;
        }

        update_bars();

        if (m_msix)
            m_msix->reset();
    }

    tlm_response_status pci_device::read(const range& addr, void* data,
        const tlm_sbi& info, address_space as) {
        if (m_msix && m_msix->bar_as == as && addr.overlaps(m_msix->mem))
            return m_msix->read_table(addr, data);
        return peripheral::read(addr, data, info, as);
    }

    tlm_response_status pci_device::write(const range& addr, const void* data,
        const tlm_sbi& info, address_space as) {
        if (m_msix && m_msix->bar_as == as && addr.overlaps(m_msix->mem))
            return m_msix->write_table(addr, data);
        return peripheral::write(addr, data, info, as);
    }


    void pci_device::pci_declare_bar(int barno, u64 size, u32 type) {
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
    }

    void pci_device::pci_declare_pm_cap(u16 pm_caps) {
        VCML_ERROR_ON(m_pm, "PCI_CAP_PM already declared");
        m_pm = new pci_cap_pm(this, pm_caps);
    }

    void pci_device::pci_declare_msi_cap(u16 msi_ctrl) {
        VCML_ERROR_ON(m_msi, "PCI_CAP_MSI already declared");
        m_msi = new pci_cap_msi(this, msi_ctrl);
        SC_HAS_PROCESS(pci_device);
        SC_THREAD(msi_process);
    }

    void pci_device::pci_declare_msix_cap(u32 bar, size_t nvec, u32 offset) {
        VCML_ERROR_ON(m_msix, "PCI_CAP_MSIX already declared");
        m_msix = new pci_cap_msix(this, bar, nvec, offset);
        SC_HAS_PROCESS(pci_device);
        SC_THREAD(msix_process);
    }

    void pci_device::pci_interrupt(bool state, unsigned int vector) {
        if (msix_enabled())
            msix_interrupt(state, vector);
        else if (msi_enabled())
            msi_interrupt(state, vector);
        else
            pci_legacy_interrupt(state);
    }

    void pci_device::msi_interrupt(bool state, unsigned int vector) {
        VCML_ERROR_ON(!m_msi, "not capable of sending MSIs");
        if (!msi_enabled() || !(PCI_COMMAND & PCI_COMMAND_MMIO))
            return;

        m_msi->set_pending(vector, state);

        if (!m_msi->is_masked(vector) && m_msi->is_pending(vector))
            m_msi_notify.notify(SC_ZERO_TIME);
    }

    void pci_device::msix_interrupt(bool state, unsigned int vector) {
        VCML_ERROR_ON(!m_msix, "not capable of sending MSIXs");
        if (!msix_enabled() || !(PCI_COMMAND & PCI_COMMAND_MMIO))
            return;

        m_msix->set_pending(vector, state);

        if (!m_msix->is_masked(vector) && m_msix->is_pending(vector))
            m_msix_notify.notify(SC_ZERO_TIME);
    }

    void pci_device::pci_legacy_interrupt(bool state) {
        if (state)
            PCI_STATUS |= PCI_STATUS_IRQ;
        else
            PCI_STATUS &= ~PCI_STATUS_IRQ;

        update_irqs();
    }

    void pci_device::pci_transport(pci_target_socket& sck, pci_payload& pci) {
        tlm_generic_payload tx;
        tlm_command cmd = pci_translate_command(pci.command);
        tx_setup(tx, cmd, pci.addr, &pci.data, pci.size);
        peripheral::receive(tx, pci.debug ? SBI_DEBUG : SBI_NONE, pci.space);
        pci.response = pci_translate_response(tx.get_response_status());
    }

    void pci_device::msi_send(unsigned int vector) {
        u32 vmask = m_msi->num_vectors() - 1;
        u32 msi_data = (*m_msi->MSI_DATA & ~vmask) | (vector & vmask);
        u64 msi_addr = *m_msi->MSI_ADDR;

        if (m_msi->is_64bit())
            msi_addr |= (u64)(u32)*m_msi->MSI_ADDR_HI << 32;

        if (!pci_dma_write(msi_addr, sizeof(msi_data), &msi_data))
            log_warn("DMA error while sending MSI%u", vector);
    }

    void pci_device::msi_process() {
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

    void pci_device::msix_send(unsigned int vector) {
        if (vector >= m_msix->num_vectors)
            VCML_ERROR("MSIX vector out of bounds: %u", vector);

        const u64 mask = PCI_MSIX_PENDING | PCI_MSIX_MASKED;
        u32 msix_data = m_msix->msix_table[vector].data;
        u64 msix_addr = (u64)(m_msix->msix_table[vector].addr & ~mask) |
                        (u64)*m_msix->MSIX_ADDR_HI << 32;

        if (!pci_dma_write(msix_addr, sizeof(msix_data), &msix_data))
            log_warn("DMA error while sending MSIX%u", vector);
    }

    void pci_device::msix_process() {
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

    u32 pci_device::write_BAR(u32 val, u32 barno) {
        PCI_BAR[barno] = val;
        update_bars();
        return PCI_BAR[barno];
    }

    u16 pci_device::write_COMMAND(u16 val) {
        u16 mask = PCI_COMMAND_IO          |
                   PCI_COMMAND_MMIO        |
                   PCI_COMMAND_BUS_MASTER  |
                   PCI_COMMAND_PARITY      |
                   PCI_COMMAND_SERR        |
                   PCI_COMMAND_NO_IRQ;

        if (!pcie) {
            mask |= PCI_COMMAND_SPECIAL    |
                    PCI_COMMAND_INVALIDATE |
                    PCI_COMMAND_PALETTE    |
                    PCI_COMMAND_WAIT       |
                    PCI_COMMAND_FAST_B2B;
        }

        PCI_COMMAND = val & mask;
        update_irqs();
        update_bars();
        return PCI_COMMAND;
    }

    u16 pci_device::write_STATUS(u16 val) {
        u16 mask = PCI_STATUS_MASTER_PARITY_ERROR |
                   PCI_STATUS_TX_TARGET_ABORT |
                   PCI_STATUS_RX_TARGET_ABORT |
                   PCI_STATUS_RX_MASTER_ABORT |
                   PCI_STATUS_TX_SYSTEM_ERROR |
                   PCI_STATUS_PARITY_ERROR;
        return PCI_STATUS & ~(val & mask);
    }

    u32 pci_device::write_PM_CTRL(u32 val) {
        u32 mask = PCI_PM_CTRL_PSTATE_D3H;
        if (!(val & PCI_PM_CTRL_PME))
            mask |= PCI_PM_CTRL_PME_ENABLE;
        return (*m_pm->PM_CTRL & ~mask) | (val & mask);
    }

    u16 pci_device::write_MSI_CTRL(u16 val) {
        size_t num_vectors = (val & PCI_MSI_QSIZE) >> 4;
        if (num_vectors > m_msi->max_vectors()) {
            log_warn("exceeding max MSI vectors %zu", num_vectors);
            num_vectors = m_msi->max_vectors();
            val = (val & ~PCI_MSI_QSIZE) | num_vectors << 4;
        }

        u16 mask = PCI_MSI_ENABLE | PCI_MSI_QSIZE;
        return (*m_msi->MSI_CONTROL & ~mask) | (val & mask);
    }

    u32 pci_device::write_MSI_ADDR(u32 val) {
        return val & ~0x3;
    }

    u32 pci_device::write_MSI_MASK(u32 val) {
        *m_msi->MSI_MASK = val & ((1ul << m_msi->num_vectors()) - 1);
        m_msi_notify.notify(SC_ZERO_TIME);
        return *m_msi->MSI_MASK;
    }

    u16 pci_device::write_MSIX_CTRL(u16 val) {
        const u64 mask = PCI_MSIX_ENABLE;
        return (*m_msix->MSIX_CONTROL & ~mask) | (val & mask);
    }

    void pci_device::update_bars() {
        for (unsigned int barno = 0; barno < PCI_NUM_BARS; barno++) {
            pci_bar* bar = m_bars + barno;
            if (bar->size == 0)
                continue;

            PCI_BAR[barno] &= bar->mask();

            if (!(PCI_COMMAND & PCI_COMMAND_IO) && bar->is_io)
                PCI_BAR[barno] = PCI_BAR_UNMAPPED;
            if (!(PCI_COMMAND & PCI_COMMAND_MMIO) && !bar->is_io)
                PCI_BAR[barno] = PCI_BAR_UNMAPPED;
            if (bar->is_io)
                PCI_BAR[barno] |= PCI_BAR_IO;
            if (bar->is_64bit)
                PCI_BAR[barno] |= PCI_BAR_64;
            if (bar->is_prefetch)
                PCI_BAR[barno] |= PCI_BAR_PREFETCH;

            u64 addr = PCI_BAR[barno] & bar->mask();
            if (bar->is_64bit)
                addr |= (u64)PCI_BAR[barno + 1] << 32;

            u32 lo = bar->addr_lo;
            bar->addr = addr;

            if (lo == bar->addr_lo)
                continue;

            if (bar->addr_lo == (PCI_BAR_UNMAPPED & bar->mask())) {
                log_debug("unmapping BAR%d", barno);
                pci_bar_unmap(barno);
            } else {
                log_debug("mapping BAR%d to %s 0x%lx..0x%lx", barno,
                          bar->is_io ? "IO" : "MMIO", bar->addr,
                          bar->addr + bar->size -1);
                pci_bar_map(m_bars[barno]);
            }
        }
    }

    void pci_device::update_irqs() {
        pci_irq irq = PCI_IRQ_NONE;
        if ((PCI_STATUS & PCI_STATUS_IRQ) && !(PCI_COMMAND & PCI_COMMAND_NO_IRQ))
            irq = (pci_irq)(u8)PCI_INT_PIN;

        if (irq == m_irq)
            return;

        if (m_irq != PCI_IRQ_NONE)
            pci_target::pci_interrupt(m_irq, false);

        m_irq = irq;

        if (m_irq != PCI_IRQ_NONE)
            pci_target::pci_interrupt(m_irq, true);
    }

}}
