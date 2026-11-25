/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

enum : u64 {
    MMAP_PCI_CFG_ADDR = 0x0,
    MMAP_PCI_CFG_SIZE = 0x10000,
    MMAP_PCI_MMIO_ADDR = 0x40000,
    MMAP_PCI_MMIO_SIZE = 0x10000,
    PCI_VENDOR_OFFSET = 0x00,
    PCI_DEVICE_OFFSET = 0x02,
    PCI_COMMAND_OFFSET = 0x04,
    PCI_BAR0_OFFSET = 0x10,
    PCI_BAR1_OFFSET = 0x14,
    PCI_CAP_OFFSET = 0x34,
    XHCI_HCIVERSION = MMAP_PCI_MMIO_ADDR + 0x00,
    XHCI_HCSPARAMS1 = MMAP_PCI_MMIO_ADDR + 0x04,
};

class xhcipci_test : public test_base
{
public:
    tlm_initiator_socket mmio;
    generic::bus mmio_bus;
    pci::host pci_root;
    usb::xhcipci xhcipci;

    gpio_target_socket int_a;
    gpio_target_socket int_b;
    gpio_target_socket int_c;
    gpio_target_socket int_d;

    xhcipci_test(const sc_module_name& nm):
        test_base(nm),
        mmio("mmio"),
        mmio_bus("mmio_bus"),
        pci_root("pci_root", true),
        xhcipci("xhcipci"),
        int_a("int_a"),
        int_b("int_b"),
        int_c("int_c"),
        int_d("int_d") {
        pci_root.pci_out[0].bind(xhcipci.pci_in);

        const range mmap_pci_cfg(MMAP_PCI_CFG_ADDR,
                                 MMAP_PCI_CFG_ADDR + MMAP_PCI_CFG_SIZE - 1);
        const range mmap_pci_mmio(MMAP_PCI_MMIO_ADDR,
                                  MMAP_PCI_MMIO_ADDR + MMAP_PCI_MMIO_SIZE - 1);

        mmio_bus.bind(mmio);
        mmio_bus.bind(pci_root.dma_out);
        mmio_bus.bind(pci_root.cfg_in, mmap_pci_cfg);
        mmio_bus.bind(pci_root.mmio_in[0], mmap_pci_mmio, MMAP_PCI_MMIO_ADDR);

        pci_root.irq_a.bind(int_a);
        pci_root.irq_b.bind(int_b);
        pci_root.irq_c.bind(int_c);
        pci_root.irq_d.bind(int_d);

        for (component* comp :
             std::initializer_list<component*>{ &mmio_bus, &pci_root }) {
            clk.bind(comp->clk);
            rst.bind(comp->rst);
        }

        add_test("strings", &xhcipci_test::test_strings);
        add_test("config", &xhcipci_test::test_config);
        add_test("registers", &xhcipci_test::test_registers);
    }

    template <typename T>
    T pci_read_cfg(u64 offset) {
        T data = 0;
        EXPECT_OK(mmio.readw(MMAP_PCI_CFG_ADDR + offset, data))
            << "failed to read PCI config at offset " << std::hex << offset;
        return data;
    }

    template <typename T>
    void pci_write_cfg(u64 offset, T data) {
        EXPECT_OK(mmio.writew(MMAP_PCI_CFG_ADDR + offset, data))
            << "failed to write PCI config at offset " << std::hex << offset;
    }

    bool has_cap(u8 cap_id) {
        u8 off = pci_read_cfg<u8>(PCI_CAP_OFFSET);
        while (off != 0) {
            if (pci_read_cfg<u8>(off) == cap_id)
                return true;
            off = pci_read_cfg<u8>(off + 1);
        }

        return false;
    }

    void test_strings() {
        EXPECT_STREQ(xhcipci.kind(), "vcml::usb::xhcipci");
        EXPECT_STREQ(xhcipci.version(), VCML_VERSION_STRING);
        EXPECT_THAT(
            kinds_of_children(xhcipci),
            IsSupersetOf({ "vcml::pci::endpoint", "vcml::usb::xhci" }));
    }

    void test_config() {
        EXPECT_EQ(pci_read_cfg<u16>(PCI_VENDOR_OFFSET), 0x1234);
        EXPECT_EQ(pci_read_cfg<u16>(PCI_DEVICE_OFFSET), 0x5678);
        EXPECT_TRUE(has_cap(PCI_CAPABILITY_PCIE));
        EXPECT_TRUE(has_cap(PCI_CAPABILITY_PM));
        EXPECT_TRUE(has_cap(PCI_CAPABILITY_MSI));
        EXPECT_TRUE(has_cap(PCI_CAPABILITY_MSIX));
        EXPECT_FALSE(has_cap(PCI_CAPABILITY_VENDOR));

        pci_write_cfg<u32>(PCI_BAR0_OFFSET, 0xffffffff);
        EXPECT_EQ(pci_read_cfg<u32>(PCI_BAR0_OFFSET),
                  0xffffc000 | PCI_BAR_64 | PCI_BAR_MMIO);
    }

    void test_registers() {
        u32 data = 0;
        EXPECT_AE(mmio.readw(XHCI_HCIVERSION, data));

        pci_write_cfg<u32>(PCI_BAR0_OFFSET, (u32)MMAP_PCI_MMIO_ADDR);
        pci_write_cfg<u32>(PCI_BAR1_OFFSET, 0);
        pci_write_cfg<u16>(PCI_COMMAND_OFFSET, 7);

        EXPECT_OK(mmio.readw(XHCI_HCIVERSION, data));
        EXPECT_EQ(data >> 16, 0x100);
        EXPECT_OK(mmio.readw(XHCI_HCSPARAMS1, data));
        EXPECT_NE(data >> 24, 0);

        pci_write_cfg<u16>(PCI_COMMAND_OFFSET, 0);
        EXPECT_AE(mmio.readw(XHCI_HCIVERSION, data));
    }
};

TEST(usb, xhcipci) {
    xhcipci_test test("test");
    sc_core::sc_start();
}
