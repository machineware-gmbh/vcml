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

        EXPECT_STREQ(iommu.kind(), "vcml::riscv::iommu");
    }

    void test_capabilities() {
        u64 caps;
        ASSERT_OK(out.readw(IOMMU_CAPS, caps));
        EXPECT_EQ(caps, 0x000001ece7ef8f10);
    }

    virtual void run_test() override {
        test_capabilities();
        wait(SC_ZERO_TIME);
    }
};

TEST(riscv, iommu) {
    iommu_test test("test");
    sc_core::sc_start();
}
