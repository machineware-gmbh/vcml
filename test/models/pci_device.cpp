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
    /* pcie            = */ false,
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

    PCI_VENDOR_OFFSET = 0x0,
    PCI_DEVICE_OFFSET = 0x2,
    PCI_COMMAND_OFFSET = 0x4,
    PCI_BAR0_OFFSET = 0x10,
    PCI_BAR1_OFFSET = 0x14,
    PCI_BAR2_OFFSET = 0x18,
    PCI_CAP_OFFSET = 0x34,

    PCI_MSI_CTRL_OFF = 0x2,
    PCI_MSI_ADDR_OFF = 0x4,
    PCI_MSI_DATA_OFF = 0x8,
    PCI_MSI_MASK_OFF = 0xc,
    PCI_MSI_PEND_OFF = 0x10,

    // MMIO space:
    //   0x00000 .. 0x0ffff: PCI CFG area
    //   0x10000 .. 0x1ffff: PCI MMIO area
    //   0x40000 .. 0xfffff: PCI MSI area
    MMAP_PCI_CFG_ADDR = 0x0,
    MMAP_PCI_CFG_SIZE = 0x10000,
    MMAP_PCI_MMIO_ADDR = 0x100010000,
    MMAP_PCI_MMIO_SIZE = 0x1000,
    MMAP_PCI_MSI_ADDR = 0x40000,
    MMAP_PCI_MSI_SIZE = 0xc0000,

    // IO space:
    //   0x02000 .. 0x02fff: PCI IO area
    MMAP_PCI_IO_ADDR = 0x2000,
    MMAP_PCI_IO_SIZE = 0x1000,
};

class pci_test_device : public pci::device
{
public:
    pci_target_socket pci_in;
    reg<u32> test_reg;
    reg<u32> test_reg_io;

    void write_test_reg_io(u32 val) {
        if (val == 0x1234)
            pci_interrupt(true, TEST_IRQ_VECTOR);
        if (val == 0)
            pci_interrupt(false, TEST_IRQ_VECTOR);
    }

    pci_test_device(const sc_module_name& nm):
        device(nm, TEST_CONFIG),
        pci_in("PCI_IN"),
        test_reg(PCI_AS_BAR0, "TEST_REG", TEST_REG_OFFSET, 1234),
        test_reg_io(PCI_AS_BAR2, "TEST_REG_IO", TEST_REG_IO_OFF, 0x1234) {
        test_reg.allow_read_write();
        test_reg.sync_always();
        test_reg_io.allow_read_write();
        test_reg_io.sync_always();
        test_reg_io.on_write(&pci_test_device::write_test_reg_io);
        pci_declare_bar(0, MMAP_PCI_MMIO_SIZE, PCI_BAR_MMIO | PCI_BAR_64);
        pci_declare_bar(2, MMAP_PCI_IO_SIZE, PCI_BAR_IO);
        pci_declare_pm_cap(PCI_PM_CAP_VER_1_1);
    }

    virtual ~pci_test_device() = default;
};

class pci_test : public test_base
{
public:
    generic::bus mmio_bus;
    generic::bus io_bus;

    pci::host pci_root;
    pci_test_device pci_device;

    tlm_initiator_socket mmio;
    tlm_initiator_socket io;
    tlm_target_socket msi;

    gpio_target_socket int_a;
    gpio_target_socket int_b;
    gpio_target_socket int_c;
    gpio_target_socket int_d;

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

    pci_test(const sc_module_name& nm):
        test_base(nm),
        mmio_bus("mmio_bus"),
        io_bus("io_bus"),
        pci_root("pci_root", TEST_CONFIG.pcie),
        pci_device("pci_device"),
        mmio("mmio"),
        io("io"),
        msi("msi"),
        int_a("int_a"),
        int_b("int_b"),
        int_c("int_c"),
        int_d("int_d"),
        msi_addr(),
        msi_data() {
        pci_root.pci_out[0].bind(pci_device.pci_in);

        const range mmap_pci_msi(MMAP_PCI_MSI_ADDR,
                                 MMAP_PCI_MSI_ADDR + MMAP_PCI_MSI_SIZE - 1);
        const range mmap_pci_cfg(MMAP_PCI_CFG_ADDR,
                                 MMAP_PCI_CFG_ADDR + MMAP_PCI_CFG_SIZE - 1);
        const range mmap_pci_mmio(MMAP_PCI_MMIO_ADDR,
                                  MMAP_PCI_MMIO_ADDR + MMAP_PCI_MMIO_SIZE - 1);
        const range mmap_pci_io(MMAP_PCI_IO_ADDR,
                                MMAP_PCI_IO_ADDR + MMAP_PCI_IO_SIZE - 1);

        mmio_bus.bind(mmio);
        mmio_bus.bind(pci_root.dma_out);
        mmio_bus.bind(msi, mmap_pci_msi);
        mmio_bus.bind(pci_root.cfg_in, mmap_pci_cfg);
        mmio_bus.bind(pci_root.mmio_in[0], mmap_pci_mmio, MMAP_PCI_MMIO_ADDR);

        io_bus.bind(io);
        io_bus.bind(pci_root.io_in[0], mmap_pci_io, MMAP_PCI_IO_ADDR);

        pci_root.irq_a.bind(int_a);
        pci_root.irq_b.bind(int_b);
        pci_root.irq_c.bind(int_c);
        pci_root.irq_d.bind(int_d);

        clk.bind(mmio_bus.clk);
        clk.bind(io_bus.clk);
        clk.bind(pci_root.clk);
        clk.bind(pci_device.clk);

        rst.bind(mmio_bus.rst);
        rst.bind(io_bus.rst);
        rst.bind(pci_root.rst);
        rst.bind(pci_device.rst);
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

    virtual void run_test() override {
        u16 vendor_id = 0, device_id = 0;
        pci_read_cfg(0, PCI_VENDOR_OFFSET, vendor_id);
        pci_read_cfg(0, PCI_DEVICE_OFFSET, device_id);
        EXPECT_EQ(vendor_id, TEST_CONFIG.vendor_id) << "no vendor at slot 0";
        EXPECT_EQ(device_id, TEST_CONFIG.device_id) << "no device at slot 0";

        u32 nodev = 0;
        pci_read_cfg(1, PCI_VENDOR_OFFSET, nodev);
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
        pci_write_cfg(0, PCI_COMMAND_OFFSET, command);

        u32 bar = 0xffffffff;
        pci_write_cfg(0, PCI_BAR0_OFFSET, bar);
        pci_read_cfg(0, PCI_BAR0_OFFSET, bar);

        // should be 4k size | PCI_BAR_MMIO | PCI_BAR_64
        EXPECT_EQ(bar, 0xfffff004) << "invalid BAR0 initialization value";

        // setup bar0
        u64 bar0 = MMAP_PCI_MMIO_ADDR | PCI_BAR_64 | PCI_BAR_MMIO;
        pci_write_cfg(0, PCI_BAR1_OFFSET, (u32)(bar0 >> 32));
        pci_write_cfg(0, PCI_BAR0_OFFSET, (u32)(bar0));

        u32 val = 0; // read bar0 offset 0 (TEST_REG)
        EXPECT_OK(mmio.readw(MMAP_PCI_MMIO_ADDR + TEST_REG_OFFSET, val))
            << "BAR0 setup failed: cannot read BAR0 range";
        EXPECT_EQ(val, 1234) << "read wrong value from BAR0 area";

        //
        // test legacy interrupts
        //
        u32 bar2 = MMAP_PCI_IO_ADDR | PCI_BAR_IO;
        pci_write_cfg(0, PCI_BAR2_OFFSET, bar2);

        // write bar2 offset 4 (TEST_REG_IO) to trigger interrupt
        EXPECT_FALSE(int_a.read()) << "interrupt already active";
        EXPECT_OK(io.writew(MMAP_PCI_IO_ADDR + TEST_REG_IO_OFF, 0x1234))
            << "BAR0 setup failed: cannot read BAR2 range";
        wait_clock_cycle();
        EXPECT_TRUE(int_a.read()) << "interrupt did not get raised";
        EXPECT_OK(io.writew(MMAP_PCI_IO_ADDR + TEST_REG_IO_OFF, 0))
            << "BAR0 setup failed: cannot read BAR2 range";
        wait_clock_cycle();
        EXPECT_FALSE(int_a.read()) << "interrupt did not get lowered";

        //
        // test resetting bar0 & bar2
        //
        pci_write_cfg(0, PCI_BAR0_OFFSET, 0xffffffff);
        pci_write_cfg(0, PCI_BAR2_OFFSET, 0xffffffff);

        // should not be accessible anymore
        dummy = 0;
        EXPECT_AE(mmio.readw(MMAP_PCI_MMIO_ADDR, dummy))
            << "PCI BAR0 area remained active";
        EXPECT_AE(io.readw(MMAP_PCI_IO_ADDR, dummy))
            << "PCI BAR2 area remained active";
    }
};

TEST(pci, simulate) {
    pci_test test("pci");
    sc_core::sc_start();
}
