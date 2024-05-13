/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/usb/xhcipci.h"

namespace vcml {
namespace usb {

static const pci_config XHCI_PCIE_CONFIG = {
    /* pcie            = */ true,
    /* vendor_id       = */ 0x1234,
    /* device_id       = */ 0x5678,
    /* subvendor_id    = */ 0xffff,
    /* subsystem_id    = */ 0xffff,
    /* class_code      = */ pci_class_code(0xc, 0x3, 0x30, 1),
    /* latency_timer   = */ 0,
    /* max_latency     = */ 0,
    /* min_grant       = */ 0,
    /* int_pin         = */ PCI_IRQ_A,
};

constexpr size_t XHCIPCI_MEMORY_SIZE = 0x4000;
constexpr size_t XHCIPCI_MSIX_OFFSET = 0x3000;

xhcipci::xhcipci(const sc_module_name& nm):
    module(nm),
    usb_host_if(),
    m_ep("ep", XHCI_PCIE_CONFIG),
    m_xhci("xhci"),
    pci_in("pci_in"),
    usb_out("usb_out") {
    pci_in.bind(m_ep.pci_in);
    m_xhci.usb_out.bind(usb_out);

    m_ep.bar_out[0].bind(m_xhci.in);
    m_xhci.irq.bind(m_ep.irq_in[0]);
    m_xhci.dma.bind(m_ep.dma_in);

    m_ep.pci_declare_bar(0, XHCIPCI_MEMORY_SIZE, PCI_BAR_MMIO | PCI_BAR_64);
    m_ep.pci_declare_pm_cap(PCI_PM_CAP_VER_1_1);
    m_ep.pci_declare_msi_cap(PCI_MSI_VECTOR | PCI_MSI_64BIT | PCI_MSI_QMASK1);
    m_ep.pci_declare_msix_cap(0, m_ep.irq_in.count(), XHCIPCI_MSIX_OFFSET);

    m_ep.rst.stub();
    m_xhci.rst.stub();

    m_ep.clk.stub(100 * MHz);
    m_xhci.clk.stub(100 * MHz);
}

xhcipci::~xhcipci() {
    // nothing to do
}

} // namespace usb
} // namespace vcml
