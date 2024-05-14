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

static const pci_config TEST_CONFIG = {
    /* pcie            = */ true,
    /* vendor_id       = */ 0x1122,
    /* device_id       = */ 0x3344,
    /* subvendor_id    = */ 0x5566,
    /* subsystem_id    = */ 0x7788,
    /* class_code      = */ pci_class_code(0, 0, 0, 0),
    /* latency_timer   = */ 0,
    /* max_latency     = */ 0,
    /* min_grant       = */ 0,
    /* int_pin         = */ PCI_IRQ_A,
};

enum : u64 {
    TEST_REG_OFFSET = 0x0,
    TEST_REG_IO_OFF = 0x4,
    TEST_IRQ_VECTOR = 5,
    TEST_MSIX_NVEC = 128,

    PCI_VENDOR_OFFSET = 0x0,
    PCI_DEVICE_OFFSET = 0x2,
    PCI_COMMAND_OFFSET = 0x4,
    PCI_BAR0_OFFSET = 0x10,
    PCI_BAR1_OFFSET = 0x14,
    PCI_BAR2_OFFSET = 0x18,
    PCI_BAR3_OFFSET = 0x1c,
    PCI_BAR4_OFFSET = 0x20,
    PCI_BAR5_OFFSET = 0x28,
    PCI_CAP_OFFSET = 0x34,

    PCI_MSI_CTRL_OFF = 0x2,
    PCI_MSI_ADDR_OFF = 0x4,
    PCI_MSI_DATA_OFF = 0x8,
    PCI_MSI_MASK_OFF = 0xc,
    PCI_MSI_PEND_OFF = 0x10,

    PCI_MSIX_CTRL_OFF = 0x2,
    PCI_MSIX_BIR_OFF = 0x4,
    PCI_MSIX_PBA_OFF = 0x8,

    // MMIO space:
    //   0x00000 .. 0x0ffff: PCI CFG area
    //   0x10000 .. 0x1ffff: PCI MMIO area
    //   0x20000 .. 0x20fff: PCI MMIO area (for MSI-X table)
    //   0x40000 .. 0xfffff: PCI MSI area
    MMAP_PCI_CFG_ADDR = 0x0,
    MMAP_PCI_CFG_SIZE = 0x10000,
    MMAP_PCI_MMIO_ADDR = 0x10000,
    MMAP_PCI_MMIO_SIZE = 0x1000,
    MMAP_PCI_MSI_ADDR = 0x40000,
    MMAP_PCI_MSI_SIZE = 0xc0000,

    MMAP_PCI_MSIX_TABLE_ADDR = 0x20000,
    MMAP_PCI_MSIX_TABLE_SIZE = 0x1000,

    // IO space:
    //   0x02000 .. 0x02fff: PCI IO area
    MMAP_PCI_IO_ADDR = 0x2000,
    MMAP_PCI_IO_SIZE = 0x1000,
};

class pcie_test_device : public pci::device
{
public:
    u8 bar4[MMAP_PCI_MMIO_SIZE];

    pci_target_socket pci_in;
    reg<u32> test_reg;
    reg<u32> test_reg_io;

    void write_test_reg_io(u32 val) {
        if (val == 0x1234)
            pci_interrupt(true, TEST_IRQ_VECTOR);
        if (val == 0)
            pci_interrupt(false, TEST_IRQ_VECTOR);
    }

    pcie_test_device(const sc_module_name& nm):
        device(nm, TEST_CONFIG),
        bar4(),
        pci_in("PCI_IN"),
        test_reg(PCI_AS_BAR0, "TEST_REG", TEST_REG_OFFSET, 1234),
        test_reg_io(PCI_AS_BAR2, "TEST_REG_IO", TEST_REG_IO_OFF, 0x1234) {
        test_reg.allow_read_write();
        test_reg.sync_always();
        test_reg_io.allow_read_write();
        test_reg_io.sync_always();
        test_reg_io.on_write(&pcie_test_device::write_test_reg_io);
        pci_declare_bar(0, MMAP_PCI_MMIO_SIZE, PCI_BAR_MMIO | PCI_BAR_64);
        pci_declare_bar(2, MMAP_PCI_IO_SIZE, PCI_BAR_IO);
        pci_declare_bar(3, MMAP_PCI_MSIX_TABLE_SIZE, PCI_BAR_MMIO);
        pci_declare_bar(4, MMAP_PCI_MMIO_SIZE, PCI_BAR_MMIO | PCI_BAR_64,
                        bar4);
        pci_declare_pm_cap(PCI_PM_CAP_VER_1_2);
        pci_declare_msi_cap(PCI_MSI_VECTOR | PCI_MSI_QMASK32);
        pci_declare_msix_cap(3, TEST_MSIX_NVEC);
    }

    virtual ~pcie_test_device() = default;
};

class pcie_test : public test_base
{
public:
    generic::bus mmio_bus;
    generic::bus io_bus;

    pci::host pcie_root;
    pcie_test_device pcie_device;

    tlm_initiator_socket mmio;
    tlm_initiator_socket io;
    tlm_target_socket msi;

    u64 msi_addr;
    u32 msi_data;

    virtual unsigned int transport(tlm_generic_payload& tx,
                                   const tlm_sbi& sideband,
                                   address_space as) override {
        EXPECT_TRUE(tx.is_write());
        EXPECT_EQ(as, VCML_AS_DEFAULT);
        EXPECT_EQ(tx.get_data_length(), sizeof(u32));
        msi_addr = MMAP_PCI_MSI_ADDR + tx.get_address();
        msi_data = *reinterpret_cast<u32*>(tx.get_data_ptr());
        tx.set_response_status(TLM_OK_RESPONSE);
        return tx.get_data_length();
    }

    pcie_test(const sc_module_name& nm):
        test_base(nm),
        mmio_bus("mmio_bus"),
        io_bus("io_bus"),
        pcie_root("pcie_root", TEST_CONFIG.pcie),
        pcie_device("pcie_device"),
        mmio("mmio"),
        io("io"),
        msi("msi"),
        msi_addr(),
        msi_data() {
        pcie_root.pci_out[0].bind(pcie_device.pci_in);

        const range mmap_pci_msi(MMAP_PCI_MSI_ADDR,
                                 MMAP_PCI_MSI_ADDR + MMAP_PCI_MSI_SIZE - 1);
        const range mmap_pci_cfg(MMAP_PCI_CFG_ADDR,
                                 MMAP_PCI_CFG_ADDR + MMAP_PCI_CFG_SIZE - 1);
        const range mmap_pci_mmio(
            MMAP_PCI_MMIO_ADDR,
            MMAP_PCI_MSIX_TABLE_ADDR + MMAP_PCI_MSIX_TABLE_SIZE - 1);
        const range mmap_pci_io(MMAP_PCI_IO_ADDR,
                                MMAP_PCI_IO_ADDR + MMAP_PCI_IO_SIZE - 1);

        mmio_bus.bind(mmio);
        mmio_bus.bind(pcie_root.dma_out);
        mmio_bus.bind(msi, mmap_pci_msi);
        mmio_bus.bind(pcie_root.cfg_in, mmap_pci_cfg);
        mmio_bus.bind(pcie_root.mmio_in[0], mmap_pci_mmio, MMAP_PCI_MMIO_ADDR);

        io_bus.bind(io);
        io_bus.bind(pcie_root.io_in[0], mmap_pci_io, MMAP_PCI_IO_ADDR);

        pcie_root.irq_a.stub();
        pcie_root.irq_b.stub();
        pcie_root.irq_c.stub();
        pcie_root.irq_d.stub();

        clk.bind(mmio_bus.clk);
        clk.bind(io_bus.clk);
        clk.bind(pcie_root.clk);
        clk.bind(pcie_device.clk);

        rst.bind(mmio_bus.rst);
        rst.bind(io_bus.rst);
        rst.bind(pcie_root.rst);
        rst.bind(pcie_device.rst);
    }

    template <typename T>
    void pcie_read_cfg(u64 devno, u64 offset, T& data) {
        u64 addr = MMAP_PCI_CFG_ADDR + devno * 4096 + offset;
        ASSERT_OK(mmio.readw(addr, data))
            << "failed to read PCIe config at offset " << std::hex << addr;
    }

    template <typename T>
    void pcie_write_cfg(u64 devno, u64 offset, T data) {
        u64 addr = MMAP_PCI_CFG_ADDR + devno * 4096 + offset;
        ASSERT_OK(mmio.writew(addr, data))
            << "failed to write PCIe config at offset " << std::hex << addr;
    }

    u8 find_cap(u8 cap_id) {
        u8 cap, cap_off;
        pcie_read_cfg(0, PCI_CAP_OFFSET, cap_off);
        while (cap_off != 0) {
            pcie_read_cfg(0, cap_off, cap);
            if (cap == cap_id)
                return cap_off;
            pcie_read_cfg(0, cap_off + 1, cap_off);
        }

        return 0;
    }

    virtual void run_test() override {
        u16 vendor_id = 0, device_id = 0;
        pcie_read_cfg(0, PCI_VENDOR_OFFSET, vendor_id);
        pcie_read_cfg(0, PCI_DEVICE_OFFSET, device_id);
        ASSERT_EQ(vendor_id, TEST_CONFIG.vendor_id) << "no vendor at slot 0";
        ASSERT_EQ(device_id, TEST_CONFIG.device_id) << "no device at slot 0";
        ASSERT_TRUE(find_cap(PCI_CAPABILITY_PM)) << "cannot find PM cap";
        ASSERT_TRUE(find_cap(PCI_CAPABILITY_MSI)) << "cannot find MSI cap";
        ASSERT_TRUE(find_cap(PCI_CAPABILITY_MSIX)) << "cannot find MSIX cap";

        u32 nodev = 0;
        pcie_read_cfg(1, PCI_VENDOR_OFFSET, nodev);
        EXPECT_EQ(nodev, 0xffffffff) << "vendor/device reported at slot 1";

        //
        // test mapping bar0
        //
        u32 dummy = 0; // make sure, nothing has been mapped yet
        EXPECT_AE(mmio.readw(MMAP_PCI_MMIO_ADDR, dummy))
            << "something has already been mapped to PCI MMIO address range";
        EXPECT_AE(io.readw(MMAP_PCI_IO_ADDR, dummy))
            << "something has already been mapped to PCI IO address range";

        u16 command = 3; // setup device for MMIO + IO
        pcie_write_cfg(0, PCI_COMMAND_OFFSET, command);

        u32 bar = 0xffffffff;
        pcie_write_cfg(0, PCI_BAR0_OFFSET, bar);
        pcie_read_cfg(0, PCI_BAR0_OFFSET, bar);

        // should be 4k size | PCI_BAR_MMIO | PCI_BAR_64
        EXPECT_EQ(bar, 0xfffff004) << "invalid BAR0 initialization value";

        // setup bar0
        u64 bar0 = MMAP_PCI_MMIO_ADDR | PCI_BAR_64 | PCI_BAR_MMIO;
        pcie_write_cfg(0, PCI_BAR1_OFFSET, (u32)(bar0 >> 32));
        pcie_write_cfg(0, PCI_BAR0_OFFSET, (u32)(bar0));

        u32 val = 0; // read bar0 offset 0 (TEST_REG)
        EXPECT_OK(mmio.readw(MMAP_PCI_MMIO_ADDR + TEST_REG_OFFSET, val))
            << "BAR0 setup failed: cannot read BAR0 range";
        EXPECT_EQ(val, 1234) << "read wrong value from BAR0 area";

        //
        // test MSI interrupt
        //
        u32 bar2 = MMAP_PCI_IO_ADDR | PCI_BAR_IO;
        pcie_write_cfg(0, PCI_BAR2_OFFSET, bar2);

        u8 cap_off = find_cap(PCI_CAPABILITY_MSI);
        ASSERT_NE(cap_off, 0) << "MSI capability not found";

        pcie_write_cfg(0, cap_off + PCI_MSI_ADDR_OFF, (u32)MMAP_PCI_MSI_ADDR);
        pcie_write_cfg(0, cap_off + PCI_MSI_DATA_OFF, (u16)0xa00);
        u16 msi_control;
        pcie_read_cfg(0, cap_off + PCI_MSI_CTRL_OFF, msi_control);
        EXPECT_EQ(msi_control, PCI_MSI_VECTOR | PCI_MSI_QMASK32)
            << "failed to read MSI control register";
        msi_control |= PCI_MSI_ENABLE | PCI_MSI_QMASK32 << 3;
        pcie_write_cfg(0, cap_off + PCI_MSI_CTRL_OFF, msi_control);
        msi_data = msi_addr = 0;

        // write bar2 offset 4 (TEST_REG_IO) to trigger MSI interrupt
        EXPECT_OK(io.writew(MMAP_PCI_IO_ADDR + TEST_REG_IO_OFF, 0x1234))
            << "BAR2 setup failed: cannot read BAR2 range";
        wait_clock_cycle();
        EXPECT_EQ(msi_data, 0xa00 | TEST_IRQ_VECTOR) << "MSI did not arrive";
        EXPECT_EQ(msi_addr, MMAP_PCI_MSI_ADDR) << "MSI did not arrive";

        EXPECT_OK(io.writew(MMAP_PCI_IO_ADDR + TEST_REG_IO_OFF, 0))
            << "BAR2 setup failed: cannot read BAR2 range";

        //
        // test MSI masking
        //
        msi_data = msi_addr = 0;
        pcie_write_cfg(0, cap_off + PCI_MSI_MASK_OFF, 0xffffffff);

        // write bar2 offset 4 (TEST_REG_IO) to trigger MSI interrupt
        EXPECT_OK(io.writew(MMAP_PCI_IO_ADDR + TEST_REG_IO_OFF, 0x1234))
            << "BAR0 setup failed: cannot read BAR2 range";
        wait_clock_cycle();
        EXPECT_EQ(msi_data, 0) << "MSI arrived despite masked";
        EXPECT_EQ(msi_addr, 0) << "MSI arrived despite masked";

        u16 msi_pending = 0;
        pcie_read_cfg(0, cap_off + PCI_MSI_PEND_OFF, msi_pending);
        EXPECT_EQ(msi_pending, 1u << TEST_IRQ_VECTOR)
            << "MSI pending bit not set";

        EXPECT_OK(io.writew(MMAP_PCI_IO_ADDR + TEST_REG_IO_OFF, 0))
            << "BAR0 setup failed: cannot read BAR2 range";

        msi_control &= ~PCI_MSI_ENABLE;
        pcie_write_cfg(0, cap_off + PCI_MSI_CTRL_OFF, msi_control);

        //
        // test MSI-X interrupt
        //
        u8 msix_off = find_cap(PCI_CAPABILITY_MSIX);
        ASSERT_TRUE(msix_off) << "could not find MSIX capability";
        u16 msix_ctrl;
        pcie_read_cfg(0, msix_off + PCI_MSIX_CTRL_OFF, msix_ctrl);
        EXPECT_EQ(msix_ctrl, TEST_MSIX_NVEC - 1);

        u32 bir, pba, pba_expect = TEST_MSIX_NVEC * 16 | 3;
        pcie_read_cfg(0, msix_off + PCI_MSIX_BIR_OFF, bir);
        EXPECT_EQ(bir, 3) << "MSIX BIR not pointing to BAR3";
        pcie_read_cfg(0, msix_off + PCI_MSIX_PBA_OFF, pba);
        EXPECT_EQ(pba, pba_expect) << "MSIX PBA not pointing to BAR3";
        msix_ctrl |= PCI_MSIX_ENABLE;
        pcie_write_cfg(0, msix_off + PCI_MSIX_CTRL_OFF, msix_ctrl);

        u64 bar3 = MMAP_PCI_MSIX_TABLE_ADDR | PCI_BAR_MMIO;
        pcie_write_cfg(0, PCI_BAR3_OFFSET, (u32)bar3);

        msi_addr = msi_data = 0;
        u64 msix_table_addr = MMAP_PCI_MSIX_TABLE_ADDR + TEST_IRQ_VECTOR * 16;
        u32 msix_addr, msix_data, msix_mask;
        EXPECT_OK(mmio.readw(msix_table_addr + 0, msix_addr))
            << "cannot read MSIX vector table";
        EXPECT_OK(mmio.readw(msix_table_addr + 8, msix_data))
            << "cannot read MSIX vector table";
        EXPECT_OK(mmio.readw(msix_table_addr + 12, msix_mask))
            << "cannot read MSIX vector table";
        EXPECT_EQ(msix_addr & 3, 0)
            << "MSIX vector table addr entry corrupted";
        EXPECT_EQ(msix_data, 0) << "MSIX vector table data entry corrupted";
        EXPECT_EQ(msix_mask, PCI_MSIX_MASKED)
            << "MSIX vector table mask entry corrupted";
        msix_addr = MMAP_PCI_MSI_ADDR + 0x44;
        msix_data = 1234567;
        EXPECT_OK(mmio.writew(msix_table_addr + 0, msix_addr + 3))
            << "cannot write MSIX vector table";
        EXPECT_OK(mmio.writew(msix_table_addr + 8, msix_data))
            << "cannot write MSIX vector table";
        EXPECT_OK(io.writew(MMAP_PCI_IO_ADDR + TEST_REG_IO_OFF, 0x1234))
            << "BAR2 setup failed: cannot read BAR2 range";
        wait_clock_cycle();
        EXPECT_EQ(msi_addr, 0) << "got MSIX address despite masked";
        EXPECT_EQ(msi_data, 0) << "got MSIX data despite masked";

        msix_mask = ~PCI_MSIX_MASKED; // trigger MSI by unmasking
        EXPECT_OK(mmio.writew(msix_table_addr + 12, msix_mask))
            << "cannot write MSIX vector table";
        wait_clock_cycle();
        EXPECT_EQ(msi_addr, msix_addr) << "got wrong MSIX address";
        EXPECT_EQ(msi_data, msix_data) << "got wrong MSIX data";

        //
        // test resetting bar0 & bar2
        //
        pcie_write_cfg(0, PCI_BAR0_OFFSET, 0xffffffff);
        pcie_write_cfg(0, PCI_BAR2_OFFSET, 0xffffffff);

        // should not be accessible anymore
        dummy = 0;
        EXPECT_AE(mmio.readw(MMAP_PCI_MMIO_ADDR, dummy))
            << "PCI BAR0 area remained active";
        EXPECT_AE(io.readw(MMAP_PCI_IO_ADDR, dummy))
            << "PCI BAR2 area remained active";

        //
        // test mapping bar4
        //
        bar = 0xffffffff;
        pcie_write_cfg(0, PCI_BAR4_OFFSET, bar);
        pcie_read_cfg(0, PCI_BAR4_OFFSET, bar);

        // should be 4k size | PCI_BAR_MMIO | PCI_BAR_64
        EXPECT_EQ(bar, 0xfffff004) << "invalid BAR4 initialization value";

        // setup bar0
        u64 bar4 = MMAP_PCI_MMIO_ADDR | PCI_BAR_64 | PCI_BAR_MMIO;
        pcie_write_cfg(0, PCI_BAR5_OFFSET, (u32)(bar4 >> 32));
        pcie_write_cfg(0, PCI_BAR4_OFFSET, (u32)(bar4));

        val = 0x87654321;
        EXPECT_OK(mmio.writew(MMAP_PCI_MMIO_ADDR, val))
            << "BAR4 setup failed: cannot write BAR4 range";
        EXPECT_EQ(pcie_device.bar4[0], 0x21);
        EXPECT_EQ(pcie_device.bar4[1], 0x43);
        EXPECT_EQ(pcie_device.bar4[2], 0x65);
        EXPECT_EQ(pcie_device.bar4[3], 0x87);

        u8* dmi = mmio.lookup_dmi_ptr(MMAP_PCI_MMIO_ADDR, MMAP_PCI_MMIO_SIZE,
                                      VCML_ACCESS_READ_WRITE);
        EXPECT_EQ(dmi, pcie_device.bar4);

        //
        // test unmapping bar 4
        //
        pcie_write_cfg(0, PCI_BAR5_OFFSET, 0xffffffff);
        pcie_write_cfg(0, PCI_BAR4_OFFSET, 0xffffffff);
        dmi = mmio.lookup_dmi_ptr(MMAP_PCI_MMIO_ADDR, MMAP_PCI_MMIO_SIZE,
                                  VCML_ACCESS_READ_WRITE);
        EXPECT_EQ(dmi, nullptr);
    }
};

TEST(pci, simulate) {
    pcie_test test("pcie");
    sc_core::sc_start();
}
