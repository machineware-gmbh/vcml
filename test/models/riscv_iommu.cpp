/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

const u64 MEM_ADDR = 0x80000000;
const u64 MEM_SIZE = 1 * MiB;

const u64 IOMMU_ADDR = 0x40000000;
const u64 IOMMU_SIZE = 1 * KiB;

const u64 IOMMU_CAPS = IOMMU_ADDR + 0;
const u64 IOMMU_FCTL = IOMMU_ADDR + 8;
const u64 IOMMU_DDTP = IOMMU_ADDR + 16;
const u64 IOMMU_CNTINH = IOMMU_ADDR + 92;
const u64 IOMMU_HPMCYCLES = IOMMU_ADDR + 96;

constexpr u64 iommu_iohpmctr(int i) {
    return IOMMU_ADDR + 104 + i * 8;
}

constexpr u64 iommu_iohpmevt(int i) {
    return IOMMU_ADDR + 352 + i * 8;
}

const u64 DDTP0_OFFSET = 16 * KiB;
const u64 DDTP0_ADDR = MEM_ADDR + DDTP0_OFFSET;
const u64 DDTP1_OFFSET = 32 * KiB;
const u64 DDTP1_ADDR = MEM_ADDR + DDTP1_OFFSET;

const u64 PGTP_OFFSET = 64 * KiB;
const u64 PGTP_ADDR = MEM_ADDR + PGTP_OFFSET;

class iommu_test : public test_base
{
public:
    generic::memory mem;
    generic::bus bus;

    riscv::iommu iommu;

    tlm_initiator_socket out;
    tlm_initiator_socket dma;

    gpio_target_socket cirq;
    gpio_target_socket firq;
    gpio_target_socket pmirq;
    gpio_target_socket pirq;

    iommu_test(const sc_module_name& nm):
        test_base(nm),
        mem("mem", 1 * MiB),
        bus("bus"),
        iommu("iommu"),
        out("out"),
        dma("dma"),
        cirq("cirq"),
        firq("firq"),
        pmirq("pmirq"),
        pirq("pirq") {
        bus.bind(mem.in, MEM_ADDR, MEM_ADDR + MEM_SIZE - 1);
        bus.bind(iommu.in, IOMMU_ADDR, IOMMU_ADDR + IOMMU_SIZE - 1);

        bus.bind(out);
        bus.bind(iommu.out);
        dma.bind(iommu.dma);

        iommu.cirq.bind(cirq);
        iommu.firq.bind(firq);
        iommu.pmirq.bind(pmirq);
        iommu.pirq.bind(pirq);

        clk.bind(mem.clk);
        clk.bind(bus.clk);
        clk.bind(iommu.clk);

        rst.bind(mem.rst);
        rst.bind(bus.rst);
        rst.bind(iommu.rst);

        dma.allow_dmi = false;

        EXPECT_STREQ(iommu.kind(), "vcml::riscv::iommu");
    }

    bool enable_counters(bool en) {
        for (int i = 1; i < 9; i++) {
            if (failed(out.writew(iommu_iohpmevt(i), en ? i : 0u)))
                return false;
            if (failed(out.writew(iommu_iohpmctr(i), 0u)))
                return false;
        }
        return true;
    }

    void test_capabilities() {
        u64 caps;
        ASSERT_OK(out.readw(IOMMU_CAPS, caps));
        EXPECT_EQ(caps, 0x000001ece7ef8f10);
    }

    void test_feature_control() {
        u32 fctl;
        ASSERT_OK(out.readw(IOMMU_FCTL, fctl));
        EXPECT_EQ(fctl, 0);
        ASSERT_OK(out.writew(IOMMU_FCTL, 7u));
        ASSERT_OK(out.readw(IOMMU_FCTL, fctl));
        EXPECT_EQ(fctl, 6u); // only WSI and GXL writable
        ASSERT_OK(out.writew(IOMMU_FCTL, 0u));
    }

    void test_iohpmcycles() {
        u64 t1, t2, t3, t4;
        ASSERT_OK(out.readw(IOMMU_HPMCYCLES, t1));
        wait(iommu.clk.cycles(100));
        ASSERT_OK(out.readw(IOMMU_HPMCYCLES, t2));
        EXPECT_EQ(t2 - t1, 100);
        ASSERT_OK(out.writew(IOMMU_CNTINH, 1u));
        wait(iommu.clk.cycles(100));
        ASSERT_OK(out.readw(IOMMU_HPMCYCLES, t3));
        EXPECT_LT(t3 - t2, 10);
        ASSERT_OK(out.writew(IOMMU_CNTINH, 0u));
        wait(iommu.clk.cycles(250));
        ASSERT_OK(out.readw(IOMMU_HPMCYCLES, t4));
        EXPECT_EQ(t4 - t3, 250);
    }

    void test_iommu_off() {
        out.writew(IOMMU_DDTP, 0u);
        ASSERT_AE(dma.writew(MEM_ADDR, 0xffffffff));
    }

    void test_iommu_bare() {
        out.writew(IOMMU_DDTP, 1u);
        ASSERT_OK(dma.writew(MEM_ADDR, 0xffffffff));
        u32 data;
        out.readw(MEM_ADDR, data);
        EXPECT_EQ(data, 0xffffffff);
    }

    void test_iommu_lvl1_dev1_bare() {
        u64* ddtp = (u64*)(mem.data() + DDTP0_OFFSET);
        ddtp[8] = 0x0000000000000011;  // dev[1].tc = V | DTF
        ddtp[9] = 0x0000000000000000;  // dev[1].gatp = bare
        ddtp[10] = 0x0000000000003000; // dev[1].ta.pscid = 3
        ddtp[11] = 0x0000000000000000; // dev[1].satp = bare
        ddtp[12] = 0x0000000000000000; // dev[1].msiptp
        ddtp[13] = 0x0000000000000000; // dev1].msi_addr_mask
        ddtp[14] = 0x0000000000000000; // dev[1].msi_addr_patter
        ddtp[15] = 0x0000000000000000; // dev[1].reserverd
        ASSERT_OK(out.writew(IOMMU_DDTP, DDTP0_ADDR | 2));

        tlm_sbi info = sbi_cpuid(1);
        ASSERT_OK(dma.writew(MEM_ADDR, 0xabababab, info));
        u32 data;
        out.readw(MEM_ADDR, data);
        EXPECT_EQ(data, 0xabababab);
    }

    void test_iommu_lvl2_dev2_sv39() {
        u64* pgtp = (u64*)(mem.data() + PGTP_OFFSET);
        pgtp[0] = (MEM_ADDR >> 2) | 0x0f; // 0x0 -> MEM_ADDR | RWX | V

        u64* ddtp0 = (u64*)(mem.data() + DDTP0_OFFSET);
        ddtp0[0] = DDTP1_ADDR >> 2 | 1;

        u64* ddtp1 = (u64*)(mem.data() + DDTP1_OFFSET);
        u64 gatp = PGTP_ADDR >> 12 | 8ull << 60;
        ddtp1[16] = 0x0000000000000091; // dev[1].tc = V | DTF | GADE
        ddtp1[17] = gatp;               // dev[1].gatp = sv39
        ddtp1[18] = 0x0000000000003000; // dev[1].ta.pscid = 3
        ddtp1[19] = 0x0000000000000000; // dev[1].satp = bare
        ddtp1[20] = 0x0000000000000000; // dev[1].msiptp
        ddtp1[21] = 0x0000000000000000; // dev1].msi_addr_mask
        ddtp1[22] = 0x0000000000000000; // dev[1].msi_addr_patter
        ddtp1[23] = 0x0000000000000000; // dev[1].reserverd
        ASSERT_OK(out.writew(IOMMU_DDTP, DDTP0_ADDR | 3));

        EXPECT_TRUE(enable_counters(true));

        tlm_sbi info = sbi_cpuid(2);
        ASSERT_OK(dma.writew(0, 0xefefefef, info));
        u32 data;
        out.readw(MEM_ADDR, data);
        EXPECT_EQ(data, 0xefefefef);
        ASSERT_OK(dma.writew(4, 0x12121212, info));
        out.readw(MEM_ADDR + 4, data);
        EXPECT_EQ(data, 0x12121212);

        // check for DA update
        EXPECT_EQ(pgtp[0], (MEM_ADDR >> 2) | 0xcf);

        u64 ux_reqs = 0;
        u64 tx_reqs = 0;
        u64 tlb_misses = 0;
        u64 ddt_walks = 0;
        u64 pdt_walks = 0;
        u64 table_walks_s = 0;
        u64 table_walks_g = 0;

        ASSERT_OK(out.readw(iommu_iohpmctr(1), ux_reqs));
        ASSERT_OK(out.readw(iommu_iohpmctr(2), tx_reqs));
        ASSERT_OK(out.readw(iommu_iohpmctr(4), tlb_misses));
        ASSERT_OK(out.readw(iommu_iohpmctr(5), ddt_walks));
        ASSERT_OK(out.readw(iommu_iohpmctr(6), pdt_walks));
        ASSERT_OK(out.readw(iommu_iohpmctr(7), table_walks_s));
        ASSERT_OK(out.readw(iommu_iohpmctr(8), table_walks_g));

        EXPECT_EQ(ux_reqs, 0);
        EXPECT_EQ(tx_reqs, 2);
        EXPECT_EQ(tlb_misses, 1);
        EXPECT_EQ(ddt_walks, 2);
        EXPECT_EQ(pdt_walks, 0);
        EXPECT_EQ(table_walks_s, 0);
        EXPECT_EQ(table_walks_g, 1);

        EXPECT_TRUE(enable_counters(false));
    }

    virtual void run_test() override {
        test_capabilities();
        wait(SC_ZERO_TIME);
        test_feature_control();
        wait(SC_ZERO_TIME);
        test_iohpmcycles();
        wait(SC_ZERO_TIME);
        test_iommu_off();
        wait(SC_ZERO_TIME);
        test_iommu_bare();
        wait(SC_ZERO_TIME);
        test_iommu_lvl1_dev1_bare();
        wait(SC_ZERO_TIME);
        test_iommu_lvl2_dev2_sv39();
        wait(SC_ZERO_TIME);
    }
};

TEST(riscv, iommu) {
    iommu_test test("test");
    sc_core::sc_start();
}
