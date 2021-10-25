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

#include "testing.h"

enum : u64 {
    MMAP_PCI_CFG_ADDR  = 0x0,
    MMAP_PCI_CFG_SIZE  = 0x10000,
    MMAP_PCI_MMIO_ADDR = 0x40000,
    MMAP_PCI_MMIO_SIZE = 0x1000,
};

class virtio_pci_test: public test_base
{
public:
    tlm_initiator_socket MMIO;

    generic::bus MMIO_BUS;
    generic::pci_host PCI_ROOT;
    virtio::pci VIRTIO_PCI;

    sc_signal<bool> IRQ[4];

    sc_in<bool> INT_A;
    sc_in<bool> INT_B;
    sc_in<bool> INT_C;
    sc_in<bool> INT_D;

    virtio_pci_test(const sc_module_name& nm):
        test_base(nm),
        MMIO("MMIO"),
        MMIO_BUS("MMIO_BUS"),
        PCI_ROOT("PCI_ROOT", false),
        VIRTIO_PCI("VIRTIO_PCI"),
        IRQ(),
        INT_A("INT_A"),
        INT_B("INT_B"),
        INT_C("INT_C"),
        INT_D("INT_D") {
        PCI_ROOT.PCI_OUT[0].bind(VIRTIO_PCI.PCI_IN);
        VIRTIO_PCI.VIRTIO_OUT.stub();

        const range MMAP_PCI_CFG(MMAP_PCI_CFG_ADDR,
            MMAP_PCI_CFG_ADDR + MMAP_PCI_CFG_SIZE - 1);
        const range MMAP_PCI_MMIO(MMAP_PCI_MMIO_ADDR,
            MMAP_PCI_MMIO_ADDR + MMAP_PCI_MMIO_SIZE - 1);

        MMIO_BUS.bind(MMIO);
        MMIO_BUS.bind(PCI_ROOT.DMA_OUT);
        MMIO_BUS.bind(PCI_ROOT.CFG_IN, MMAP_PCI_CFG);
        MMIO_BUS.bind(PCI_ROOT.MMIO_IN[0], MMAP_PCI_MMIO, MMAP_PCI_MMIO_ADDR);

        PCI_ROOT.IRQ_A.bind(IRQ[0]);
        PCI_ROOT.IRQ_B.bind(IRQ[1]);
        PCI_ROOT.IRQ_C.bind(IRQ[2]);
        PCI_ROOT.IRQ_D.bind(IRQ[3]);

        INT_A.bind(IRQ[0]);
        INT_B.bind(IRQ[1]);
        INT_C.bind(IRQ[2]);
        INT_D.bind(IRQ[3]);

        MMIO_BUS.CLOCK.stub(100 * MHz);
        PCI_ROOT.CLOCK.stub(100 * MHz);
        VIRTIO_PCI.CLOCK.stub(100 * MHz);

        MMIO_BUS.RESET.stub();
        PCI_ROOT.RESET.stub();
        VIRTIO_PCI.RESET.stub();
    }

    template <typename T>
    void pci_read_cfg(u64 devno, u64 offset, T& data) {
        u64 addr = MMAP_PCI_CFG_ADDR + devno * 256 + offset;
        ASSERT_OK(MMIO.readw(addr, data))
            << "failed to read PCI config at offset " << std::hex << addr;
    }

    template <typename T>
    void pci_write_cfg(u64 devno, u64 offset, T data) {
        u64 addr = MMAP_PCI_CFG_ADDR + devno * 256 + offset;
        ASSERT_OK(MMIO.writew(addr, data))
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
    broker_arg broker(sc_argc(), sc_argv());
    log_term logger;
    logger.set_level(LOG_TRACE);
    virtio_pci_test test("virtio_pci");
    sc_core::sc_start();
}
