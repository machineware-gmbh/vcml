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
const u64 IOMMU_CQB = IOMMU_ADDR + 24;
const u64 IOMMU_CQH = IOMMU_ADDR + 32;
const u64 IOMMU_CQT = IOMMU_ADDR + 36;
const u64 IOMMU_FQB = IOMMU_ADDR + 40;
const u64 IOMMU_FQH = IOMMU_ADDR + 48;
const u64 IOMMU_FQT = IOMMU_ADDR + 52;
const u64 IOMMU_CQCSR = IOMMU_ADDR + 72;
const u64 IOMMU_FQCSR = IOMMU_ADDR + 76;
const u64 IOMMU_IPSR = IOMMU_ADDR + 84;
const u64 IOMMU_CNTINH = IOMMU_ADDR + 92;
const u64 IOMMU_HPMCYCLES = IOMMU_ADDR + 96;
const u64 IOMMU_TR_REQ_IOVA = IOMMU_ADDR + 600;
const u64 IOMMU_TR_REQ_CTL = IOMMU_ADDR + 608;
const u64 IOMMU_TR_RESPONSE = IOMMU_ADDR + 616;
const u64 IOMMU_ICVEC = IOMMU_ADDR + 760;

constexpr u64 iommu_iohpmctr(int i) {
    return IOMMU_ADDR + 104 + i * 8;
}

constexpr u64 iommu_iohpmevt(int i) {
    return IOMMU_ADDR + 352 + i * 8;
}

constexpr u64 iommu_msi_cfg_tbl_addr(int i) {
    return IOMMU_ADDR + 768 + i * 16;
}

constexpr u64 iommu_msi_cfg_tbl_ctrl(int i) {
    return IOMMU_ADDR + 768 + i * 16 + 8;
}

const u64 DDTP0_OFFSET = 16 * KiB;
const u64 DDTP0_ADDR = MEM_ADDR + DDTP0_OFFSET;
const u64 DDTP1_OFFSET = 32 * KiB;
const u64 DDTP1_ADDR = MEM_ADDR + DDTP1_OFFSET;
const u64 CMDQ_OFFSET = 40 * KiB;
const u64 CMDQ_ADDR = MEM_ADDR + CMDQ_OFFSET;
const u64 FLTQ_OFFSET = 44 * KiB;
const u64 FLTQ_ADDR = MEM_ADDR + FLTQ_OFFSET;
const u64 PGTP_OFFSET = 64 * KiB;
const u64 PGTP_ADDR = MEM_ADDR + PGTP_OFFSET;
const u64 MSIP_OFFSET = 80 * KiB;
const u64 MSIP_ADDR = MEM_ADDR + MSIP_OFFSET;

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
        iommu("iommu", false),
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

        add_test("capabilities", &iommu_test::test_capabilities);
        add_test("feature_control", &iommu_test::test_feature_control);
        add_test("iohpmcycles", &iommu_test::test_iohpmcycles);
        add_test("iommu_off", &iommu_test::test_iommu_off);
        add_test("iommu_bare", &iommu_test::test_iommu_bare);
        add_test("iommu_lvl1_bare", &iommu_test::test_iommu_lvl1_bare);
        add_test("iommu_lvl2_sv39", &iommu_test::test_iommu_lvl2_sv39);
        add_test("command_queue", &iommu_test::test_iommu_command_queue);
        add_test("fault_queue", &iommu_test::test_iommu_fault_queue);
        add_test("msi_flat", &iommu_test::test_iommu_msi_flat);
        add_test("msi_mrif", &iommu_test::test_iommu_msi_mrif);
        add_test("tr_debug", &iommu_test::test_iommu_tr_debug);
        add_test("iommu_dmi", &iommu_test::test_iommu_dmi);

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
        ASSERT_OK(out.writew(IOMMU_DDTP, 0u));
        ASSERT_AE(dma.writew(MEM_ADDR, 0xffffffff));
    }

    void test_iommu_bare() {
        u32 data;
        ASSERT_OK(out.writew(IOMMU_DDTP, 1u));
        ASSERT_OK(dma.writew(MEM_ADDR, 0xffffffff));
        ASSERT_OK(out.readw(MEM_ADDR, data));
        EXPECT_EQ(data, 0xffffffff);
    }

    void test_iommu_lvl1_bare() {
        u64* ddtp = (u64*)(mem.data() + DDTP0_OFFSET);
        ddtp[8] = 0x0000000000000011;  // dev[1].tc = V | DTF
        ddtp[9] = 0x0000000000000000;  // dev[1].gatp = bare
        ddtp[10] = 0x0000000000003000; // dev[1].ta.pscid = 3
        ddtp[11] = 0x0000000000000000; // dev[1].satp = bare
        ddtp[12] = 0x0000000000000000; // dev[1].msiptp
        ddtp[13] = 0x0000000000000000; // dev[1].msi_addr_mask
        ddtp[14] = 0x0000000000000000; // dev[1].msi_addr_patter
        ddtp[15] = 0x0000000000000000; // dev[1].reserverd
        ASSERT_OK(out.writew(IOMMU_DDTP, (DDTP0_ADDR >> 2) | 2));

        u32 data;
        tlm_sbi info = sbi_cpuid(1);
        ASSERT_OK(dma.writew(MEM_ADDR, 0xabababab, info));
        ASSERT_OK(out.readw(MEM_ADDR, data));
        EXPECT_EQ(data, 0xabababab);
    }

    void test_iommu_lvl2_sv39() {
        u64* pgtp = (u64*)(mem.data() + PGTP_OFFSET);
        memset(pgtp, 0, 4096);
        pgtp[0] = (MEM_ADDR >> 2) | 0x1f; // 0x0 -> MEM_ADDR | U | RWX | V

        u64* ddtp0 = (u64*)(mem.data() + DDTP0_OFFSET);
        ddtp0[0] = DDTP1_ADDR >> 2 | 1;

        u64* ddtp1 = (u64*)(mem.data() + DDTP1_OFFSET);
        u64 gatp = PGTP_ADDR >> 12 | 8ull << 60;
        ddtp1[16] = 0x0000000000000091; // dev[1].tc = V | DTF | GADE
        ddtp1[17] = gatp;               // dev[1].gatp = sv39
        ddtp1[18] = 0x0000000000003000; // dev[1].ta.pscid = 3
        ddtp1[19] = 0x0000000000000000; // dev[1].satp = bare
        ddtp1[20] = 0x0000000000000000; // dev[1].msiptp
        ddtp1[21] = 0x0000000000000000; // dev[1].msi_addr_mask
        ddtp1[22] = 0x0000000000000000; // dev[1].msi_addr_patter
        ddtp1[23] = 0x0000000000000000; // dev[1].reserverd
        ASSERT_OK(out.writew(IOMMU_DDTP, (DDTP0_ADDR >> 2) | 3));

        EXPECT_TRUE(enable_counters(true));

        tlm_sbi info = sbi_cpuid(2);
        ASSERT_OK(dma.writew(0, 0xefefefef, info));
        u32 data;
        ASSERT_OK(out.readw(MEM_ADDR, data));
        EXPECT_EQ(data, 0xefefefef);
        ASSERT_OK(dma.writew(4, 0x12121212, info));
        ASSERT_OK(out.readw(MEM_ADDR + 4, data));
        EXPECT_EQ(data, 0x12121212);

        // check for DA update
        EXPECT_EQ(pgtp[0], (MEM_ADDR >> 2) | 0xdf);

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

        EXPECT_EQ(ux_reqs, 2);
        EXPECT_EQ(tx_reqs, 0);
        EXPECT_EQ(tlb_misses, 1);
        EXPECT_EQ(ddt_walks, 2);
        EXPECT_EQ(pdt_walks, 0);
        EXPECT_EQ(table_walks_s, 0);
        EXPECT_EQ(table_walks_g, 1);

        EXPECT_TRUE(enable_counters(false));
    }

    void test_iommu_command_queue() {
        ASSERT_OK(out.writew(IOMMU_FCTL, 2u)); // enable WSI

        u64* cq = (u64*)(mem.data() + CMDQ_OFFSET);
        cq[0] = 0x0000000000000001; // iotinval.vma
        cq[2] = 0x0000700300003481; // iotinval.gvma
        cq[3] = 0x00abcdef00000000; // iotinval.addr
        cq[4] = 0x0000000000000003; // iodir.inval_ddt
        cq[6] = 0x000005020000b083; // iodir.inval_pdt
        cq[8] = 0xcafebabe00000c02; // iofence.c wsi
        cq[9] = (MEM_ADDR + 8) >> 2;

        u64 cqb = CMDQ_ADDR >> 2 | 2; // 8 entries
        ASSERT_OK(out.writew(IOMMU_CQB, cqb));
        ASSERT_OK(out.writew(IOMMU_CQCSR, 1u));
        wait(1, SC_MS);

        u32 cqcsr;
        ASSERT_OK(out.readw(IOMMU_CQCSR, cqcsr));
        EXPECT_EQ(cqcsr, 0x10001u);

        ASSERT_OK(out.writew(IOMMU_CQT, 5u));
        wait(1, SC_MS);

        u32 cqt;
        ASSERT_OK(out.readw(IOMMU_CQH, cqt));
        EXPECT_EQ(cqt, 5);

        ASSERT_OK(out.readw(IOMMU_CQCSR, cqcsr));
        EXPECT_EQ(cqcsr, 0x10801u);
        ASSERT_OK(out.writew(IOMMU_CQCSR, 0x800u));
        wait(1, SC_MS);
        ASSERT_OK(out.readw(IOMMU_CQCSR, cqcsr));
        EXPECT_EQ(cqcsr, 0u);

        u32 data;
        ASSERT_OK(out.readw(MEM_ADDR + 8, data));
        EXPECT_EQ(data, 0xcafebabe);
    }

    void test_iommu_fault_queue() {
        u32 msi, fqh, fqt, data;
        ASSERT_OK(out.writew(IOMMU_FCTL, 0u));       // enable MSI
        ASSERT_OK(out.writew(IOMMU_ICVEC, 6u << 4)); // fiv = 7
        ASSERT_OK(out.writew(iommu_msi_cfg_tbl_addr(7), MEM_ADDR + 20));
        ASSERT_OK(out.writew(iommu_msi_cfg_tbl_ctrl(7), 99ull));

        // setup g-stage page tables
        u64* pgtp = (u64*)(mem.data() + PGTP_OFFSET);
        memset(pgtp, 0, 4096);
        pgtp[2] = (MEM_ADDR >> 2) | 0x1e; // 0x0 -> MEM_ADDR | U | RWX | !V

        // setup device directory table
        u64* ddtp0 = (u64*)(mem.data() + DDTP0_OFFSET);
        ddtp0[0] = DDTP1_ADDR >> 2 | 1;
        u64* ddtp1 = (u64*)(mem.data() + DDTP1_OFFSET);
        u64 gatp = PGTP_ADDR >> 12 | 9ull << 60;
        ddtp1[16] = 0x0000000000000001; // dev[2].tc = V | DTF | GADE
        ddtp1[17] = gatp;               // dev[2].gatp = sv48
        ddtp1[18] = 0x0000000000003000; // dev[2].ta.pscid = 3
        ddtp1[19] = 0x0000000000000000; // dev[2].satp = bare
        ddtp1[20] = 0x0000000000000000; // dev[2].msiptp
        ddtp1[21] = 0x0000000000000000; // dev[2].msi_addr_mask
        ddtp1[22] = 0x0000000000000000; // dev[2].msi_addr_patter
        ddtp1[23] = 0x0000000000000000; // dev[2].reserverd
        ASSERT_OK(out.writew(IOMMU_DDTP, (DDTP0_ADDR >> 2) | 3));

        // setup fault queue
        u64 fqb = FLTQ_ADDR >> 2 | 1; // 4 entries
        ASSERT_OK(out.writew(IOMMU_FQB, fqb));
        ASSERT_OK(out.writew(IOMMU_FQCSR, 3u)); // fqen | fqie
        wait(1, SC_MS);

        tlm_sbi info = sbi_cpuid(2);
        ASSERT_AE(dma.writew(0x1000000001c, 0xefefefef, info));

        // check fault msi
        ASSERT_OK(out.readw(MEM_ADDR + 20, msi));
        EXPECT_EQ(msi, 99);

        // clear fault queue irq
        ASSERT_OK(out.writew(IOMMU_IPSR, 2u));
        ASSERT_OK(out.writew(MEM_ADDR + 20, 0u));

        // trigger enough errors to cause a fault queue overflow
        ASSERT_AE(dma.writew(0x10000000020, 0xefefefef, info));
        ASSERT_AE(dma.readw(0x10000000024, data, info));
        ASSERT_AE(dma.readw(0x10000000028, data, info));
        wait(1, SC_MS);

        // check fault msi again
        ASSERT_OK(out.readw(MEM_ADDR + 20, msi));
        EXPECT_EQ(msi, 99);

        // check fault queue
        ASSERT_OK(out.readw(IOMMU_FQH, fqh));
        ASSERT_OK(out.readw(IOMMU_FQT, fqt));
        EXPECT_EQ(fqh, 0u);
        EXPECT_EQ(fqt, 3u);

        // read faults
        for (u32 i = fqh; i < fqt; i++) {
            u64 fault, res0, iotval, iotval2;
            ASSERT_OK(out.readw(FLTQ_ADDR + i * 32 + 0, fault));
            ASSERT_OK(out.readw(FLTQ_ADDR + i * 32 + 8, res0));
            ASSERT_OK(out.readw(FLTQ_ADDR + i * 32 + 16, iotval));
            ASSERT_OK(out.readw(FLTQ_ADDR + i * 32 + 24, iotval2));

            EXPECT_EQ(fault, i > 1 ? 0x2080000000d : 0x20c0000000f);
            EXPECT_EQ(res0, 0);
            EXPECT_EQ(iotval, 0x1000000001c + 4 * i);
            EXPECT_EQ(iotval2, 0x1000000001c + 4 * i);
        }

        // disable queue
        ASSERT_OK(out.writew(IOMMU_FQCSR, 0x200)); // clear ov and disable
        wait(1, SC_MS);
        ASSERT_OK(out.readw(IOMMU_FQCSR, data));
        EXPECT_EQ(data, 0);

        // reset interrupts
        ASSERT_OK(out.writew(IOMMU_IPSR, ~0u));
    }

    void test_iommu_msi_flat() {
        u64* msip = (u64*)(mem.data() + MSIP_OFFSET);
        msip[0] = (MEM_ADDR + 0x1000) >> 2 | 7; // PPN | M_FLAT | V
        msip[1] = 0x0000000000000000;           // reserved/unused
        msip[2] = (MEM_ADDR + 0x2000) >> 2 | 7; // PPN | M_FLAT | V
        msip[3] = 0x0000000000000000;           // reserved/unused

        u64 msiptp = (3ull << 60) | MSIP_ADDR >> 12; // flat | ppn
        u64* ddtp = (u64*)(mem.data() + DDTP0_OFFSET);
        ddtp[8] = 0x0000000000000011;  // dev[1].tc = V | DTF
        ddtp[9] = 0x0000000000000000;  // dev[1].gatp = bare
        ddtp[10] = 0x0000000000003000; // dev[1].ta.pscid = 3
        ddtp[11] = 0x0000000000000000; // dev[1].satp = bare
        ddtp[12] = msiptp;             // dev[1] msiptp = flat
        ddtp[13] = 0xfffff0000000000f; // dev[1].msi_addr_mask
        ddtp[14] = 0x00000aabbbbcccc0; // dev[1].msi_addr_patter
        ddtp[15] = 0x0000000000000000; // dev[1].reserverd
        ASSERT_OK(out.writew(IOMMU_DDTP, (DDTP0_ADDR >> 2) | 2));

        u32 data;
        tlm_sbi info = sbi_cpuid(1);

        ASSERT_OK(dma.writew(0xaaaabbbbcccc010c, 60u, info));
        ASSERT_OK(out.readw(MEM_ADDR + 0x110c, data));
        EXPECT_EQ(data, 60u);

        ASSERT_OK(dma.writew(0xaaaabbbbcccc110c, 27u, info));
        ASSERT_OK(out.readw(MEM_ADDR + 0x210c, data));
        EXPECT_EQ(data, 27u);
    }

    void test_iommu_msi_mrif() {
        *(u64*)(mem.data() + 0x0000) = 0x0000000000000000; // notification area
        *(u64*)(mem.data() + 0x1000) = 0x0000000000000000; // msi1 pending bits
        *(u64*)(mem.data() + 0x1008) = 0xffffffffffffffff; // msi1 enabled bits
        *(u64*)(mem.data() + 0x2010) = 0x0000000000000003; // msi2 pending bits
        *(u64*)(mem.data() + 0x2018) = 0xffffffffffffffff; // msi2 enabled bits

        u64* msip = (u64*)(mem.data() + MSIP_OFFSET);
        msip[0] = (MEM_ADDR + 0x1000) >> 2 | 3; // PPN | M_MRIF | V
        msip[1] = MEM_ADDR >> 2 | bit(60) | 5;  // NPPN, NID = 1029
        msip[2] = (MEM_ADDR + 0x2000) >> 2 | 3; // PPN | M_MRIF | V
        msip[3] = MEM_ADDR >> 2 | bit(60) | 90; // NPPN, NID = 1114

        u64 msiptp = (3ull << 60) | MSIP_ADDR >> 12; // flat | ppn
        u64* ddtp = (u64*)(mem.data() + DDTP0_OFFSET);
        ddtp[8] = 0x0000000000000011;  // dev[1].tc = V | DTF
        ddtp[9] = 0x0000000000000000;  // dev[1].gatp = bare
        ddtp[10] = 0x0000000000003000; // dev[1].ta.pscid = 3
        ddtp[11] = 0x0000000000000000; // dev[1].satp = bare
        ddtp[12] = msiptp;             // dev[1] msiptp = flat
        ddtp[13] = 0xfffff0000000000f; // dev[1].msi_addr_mask
        ddtp[14] = 0x00000aabbbbcccc0; // dev[1].msi_addr_patter
        ddtp[15] = 0x0000000000000000; // dev[1].reserverd
        ASSERT_OK(out.writew(IOMMU_DDTP, (DDTP0_ADDR >> 2) | 2));

        u32 data, mrif;
        tlm_sbi info = sbi_cpuid(1);

        ASSERT_OK(dma.writew(0xaaaabbbbcccc0000, 4u, info));
        ASSERT_OK(out.readw(MEM_ADDR + 0x1000, data));
        ASSERT_OK(out.readw(MEM_ADDR + 0x0000, mrif));
        EXPECT_EQ(data, bit(4));
        EXPECT_EQ(mrif, 1029);

        ASSERT_OK(dma.writew(0xaaaabbbbcccc1000, 70u, info));
        ASSERT_OK(out.readw(MEM_ADDR + 0x2010, data));
        ASSERT_OK(out.readw(MEM_ADDR + 0x0000, mrif));
        EXPECT_EQ(data, bit(6) | 3);
        EXPECT_EQ(mrif, 1114);
    }

    void test_iommu_tr_debug() {
        u64* pgtp = (u64*)(mem.data() + PGTP_OFFSET);
        memset(pgtp, 0, 4096);
        pgtp[3] = (MEM_ADDR >> 2) | 0xd7; // 0xc..0 -> MEM | DA | U | RW | V

        // setup device directory table
        u64* ddtp0 = (u64*)(mem.data() + DDTP0_OFFSET);
        ddtp0[0] = DDTP1_ADDR >> 2 | 1;
        u64* ddtp1 = (u64*)(mem.data() + DDTP1_OFFSET);
        u64 gatp = PGTP_ADDR >> 12 | 8ull << 60;
        ddtp1[24] = 0x0000000000000001; // dev[3].tc = V | DTF | GADE
        ddtp1[25] = gatp;               // dev[3].gatp = sv48
        ddtp1[26] = 0x0000000000003000; // dev[3].ta.pscid = 3
        ddtp1[27] = 0x0000000000000000; // dev[3].satp = bare
        ddtp1[28] = 0x0000000000000000; // dev[3].msiptp
        ddtp1[29] = 0x0000000000000000; // dev[3].msi_addr_mask
        ddtp1[30] = 0x0000000000000000; // dev[3].msi_addr_patter
        ddtp1[31] = 0x0000000000000000; // dev[3].reserverd

        ASSERT_OK(out.writew(IOMMU_DDTP, (DDTP0_ADDR >> 2) | 3));

        // begin test
        u64 iova = 0xc0000008;
        u64 ctrl = 3ull << 40 | 0x9;
        ASSERT_OK(out.writew(IOMMU_TR_REQ_IOVA, iova));
        ASSERT_OK(out.writew(IOMMU_TR_REQ_CTL, ctrl));

        while (ctrl & 1) {
            ASSERT_OK(out.readw(IOMMU_TR_REQ_CTL, ctrl));
            wait(1, SC_MS);
        }

        u64 resp;
        ASSERT_OK(out.readw(IOMMU_TR_RESPONSE, resp));
        EXPECT_EQ(resp, MEM_ADDR >> 2);

        // test an unmapped address
        iova = 0x0;
        ctrl = 3ull << 40 | 0x1;
        ASSERT_OK(out.writew(IOMMU_TR_REQ_IOVA, iova));
        ASSERT_OK(out.writew(IOMMU_TR_REQ_CTL, ctrl));

        while (ctrl & 1) {
            ASSERT_OK(out.readw(IOMMU_TR_REQ_CTL, ctrl));
            wait(1, SC_MS);
        }

        ASSERT_OK(out.readw(IOMMU_TR_RESPONSE, resp));
        EXPECT_EQ(resp, 1);
    }

    void test_iommu_dmi() {
        u64* pgtp = (u64*)(mem.data() + PGTP_OFFSET);
        memset(pgtp, 0, 4096);
        pgtp[7] = (MEM_ADDR >> 2) | 0xd7; // 0xc..0 -> MEM | DA | U | RW | V

        // setup device directory table
        u64* ddtp0 = (u64*)(mem.data() + DDTP0_OFFSET);
        ddtp0[0] = DDTP1_ADDR >> 2 | 1;
        u64* ddtp1 = (u64*)(mem.data() + DDTP1_OFFSET);
        u64 gatp = PGTP_ADDR >> 12 | 8ull << 60;
        ddtp1[32] = 0x0000000000000001; // dev[4].tc = V | DTF | GADE
        ddtp1[33] = gatp;               // dev[4].gatp = sv48
        ddtp1[34] = 0x0000000000003000; // dev[4].ta.pscid = 3
        ddtp1[35] = 0x0000000000000000; // dev[4].satp = bare
        ddtp1[36] = 0x0000000000000000; // dev[4].msiptp
        ddtp1[37] = 0x0000000000000000; // dev[4].msi_addr_mask
        ddtp1[38] = 0x0000000000000000; // dev[4].msi_addr_patter
        ddtp1[39] = 0x0000000000000000; // dev[4].reserverd

        ASSERT_OK(out.writew(IOMMU_DDTP, (DDTP0_ADDR >> 2) | 3));

        iommu.flush_contexts();
        iommu.flush_tlb_g();
        iommu.flush_tlb_s();

        dma.allow_dmi = true;

        // test dmi
        u64 addr = 0x1c0000000;

        tlm_dmi dmi;
        tlm_sbi sbi = sbi_cpuid(4) | sbi_asid(SBI_ASID_GLOBAL);
        tlm_generic_payload tx;
        tx_set_sbi(tx, sbi);
        tx.set_address(addr);
        EXPECT_TRUE(dma->get_direct_mem_ptr(tx, dmi));
        EXPECT_TRUE(dmi.is_read_write_allowed());
        EXPECT_EQ(dmi.get_start_address(), addr);
        EXPECT_EQ(tx.get_address(), addr);

        // test dmi cache invalidation
        EXPECT_OK(dma.writew(addr, 1234u, sbi));
        EXPECT_TRUE(dma.dmi_cache().lookup(addr, 1, TLM_WRITE_COMMAND, dmi));
        mem.in->invalidate_direct_mem_ptr(0, ~0ull);
        EXPECT_FALSE(dma.dmi_cache().lookup(addr, 1, TLM_READ_COMMAND, dmi));

        dma.allow_dmi = false;
    }
};

TEST(riscv, iommu) {
    iommu_test test("test");
    sc_core::sc_start();
}
