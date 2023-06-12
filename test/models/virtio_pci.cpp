/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

enum : u64 {
    MMAP_PCI_CFG_ADDR = 0x0,
    MMAP_PCI_CFG_SIZE = 0x10000,
    MMAP_PCI_MMIO_ADDR = 0x40000,
    MMAP_PCI_MMIO_SIZE = 0x1000,
};

class virtio_pci_test : public test_base
{
public:
    tlm_initiator_socket mmio;

    generic::bus mmio_bus;
    pci::host pci_root;
    virtio::pci virtio_pci;

    gpio_target_socket int_a;
    gpio_target_socket int_b;
    gpio_target_socket int_c;
    gpio_target_socket int_d;

    virtio_pci_test(const sc_module_name& nm):
        test_base(nm),
        mmio("mmio"),
        mmio_bus("mmio_bus"),
        pci_root("pci_root", false),
        virtio_pci("virtio_pci"),
        int_a("int_a"),
        int_b("int_b"),
        int_c("int_c"),
        int_d("int_d") {
        pci_root.pci_out[0].bind(virtio_pci.pci_in);
        virtio_pci.virtio_out.stub();

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

        mmio_bus.clk.stub(100 * MHz);
        pci_root.clk.stub(100 * MHz);
        virtio_pci.clk.stub(100 * MHz);

        mmio_bus.rst.stub();
        pci_root.rst.stub();
        virtio_pci.rst.stub();
    }

    template <typename T>
    void pci_read_cfg(u64 devno, u64 offset, T& data) {
        u64 addr = MMAP_PCI_CFG_ADDR + devno * 256 + offset;
        ASSERT_OK(mmio.readw(addr, data))
            << "failed to read PCI config at offset " << std::hex << addr;
    }

    template <typename T>
    void pci_write_cfg(u64 devno, u64 offset, T data) {
        u64 addr = MMAP_PCI_CFG_ADDR + devno * 256 + offset;
        ASSERT_OK(mmio.writew(addr, data))
            << "failed to write PCI config at offset " << std::hex << addr;
    }

    u8 find_virtio_cap(u8 type) {
        for (u8 off = 0x33; off != 0; pci_read_cfg(0, off + 1, off)) {
            u8 cap_id, cap_type;
            pci_read_cfg(0, off, cap_id);
            if (cap_id == PCI_CAPABILITY_VENDOR) {
                pci_read_cfg(0, off + 3, cap_type);
                if (cap_type == type)
                    return off;
            }
        }

        return 0;
    }

    virtual void run_test() override {
        u16 vendor, device;
        pci_read_cfg(0, 0, vendor);
        pci_read_cfg(0, 2, device);

        EXPECT_EQ(vendor, PCI_VENDOR_QUMRANET);
        EXPECT_EQ(device, PCI_DEVICE_VIRTIO);

        EXPECT_TRUE(find_virtio_cap(virtio::VIRTIO_PCI_CAP_COMMON));
        EXPECT_TRUE(find_virtio_cap(virtio::VIRTIO_PCI_CAP_NOTIFY));
        EXPECT_TRUE(find_virtio_cap(virtio::VIRTIO_PCI_CAP_ISR));
        EXPECT_TRUE(find_virtio_cap(virtio::VIRTIO_PCI_CAP_DEVICE));
    }
};

TEST(virtio, pci) {
    virtio_pci_test test("virtio_pci");
    sc_core::sc_start();
}
