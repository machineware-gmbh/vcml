/******************************************************************************
 *                                                                            *
 * Copyright 2022 MachineWare GmbH                                            *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is unpublished proprietary work and may not be used or disclosed to   *
 * third parties, copied or duplicated in any form, in whole or in part,      *
 * without prior written permission of the authors.                           *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

class arch_timer_test : public test_base
{
public:
    arm::arch_timer timer;

    tlm_initiator_socket ctl;
    tlm_initiator_socket cnt0;
    gpio_target_socket irq0_phys;
    gpio_target_socket irq0_virt;

    arch_timer_test(const sc_module_name& nm):
        test_base(nm),
        timer("timer"),
        ctl("ctl"),
        cnt0("cnt0"),
        irq0_phys("irq0_phys"),
        irq0_virt("irq0_virt") {
        ctl.bind(timer.timer_in);
        cnt0.bind(timer.frame_in[0]);
        timer.irq_phys[0].bind(irq0_phys);
        timer.irq_virt[0].bind(irq0_virt);

        rst.bind(timer.rst);
        clk.bind(timer.clk);

        EXPECT_STREQ(timer.kind(), "vcml::arm::arch_timer");
        EXPECT_STREQ(timer.frames[0].kind(),
                     "vcml::arm::arch_timer::cntframe");
    }

    constexpr u64 ctl_voff(size_t idx) { return 0x80 + idx * 8; }

    enum cnt0_addr : u64 {
        CNTPCT = 0x00,
        CNTVCT = 0x08,
        CNTP_CVAL = 0x20,
        CNTP_TVAL = 0x28,
        CNTP_CTL = 0x2c,
        CNTV_CVAL = 0x30,
        CNTV_TVAL = 0x38,
        CNTV_CTL = 0x3c,
    };

    virtual void run_test() override {
        ASSERT_FALSE(irq0_phys) << "irq_phys did not reset";
        ASSERT_FALSE(irq0_virt) << "irq_virt did not reset";

        wait(1, SC_MS); // clock ticks at 100MHz

        // setup virtual offset and check physical and virtual counters
        u64 pct = 0, vct = 0;
        ASSERT_OK(ctl.writew(ctl_voff(0), 4000));
        ASSERT_OK(cnt0.readw(CNTPCT, pct));
        ASSERT_OK(cnt0.readw(CNTVCT, vct));
        EXPECT_EQ(pct, 100000);
        EXPECT_EQ(vct, 100000 - 4000);

        // schedule a virtual timer to fire in 2ms
        u32 tval = 0;
        u32 ctl = 0;
        u64 cmp = 0;
        ASSERT_OK(cnt0.writew(CNTV_TVAL, 200000));
        ASSERT_OK(cnt0.readw(CNTV_CVAL, cmp));
        EXPECT_EQ(cmp, 100000 - 4000 + 200000);
        ASSERT_OK(cnt0.writew<u32>(CNTV_CTL, bit(0))); // enable
        wait(5, SC_US);
        ASSERT_OK(cnt0.readw(CNTV_TVAL, tval));
        EXPECT_EQ(tval, 200000 - 500);
        wait(irq0_virt.default_event());
        ASSERT_OK(cnt0.readw(CNTV_CTL, ctl));
        EXPECT_EQ(ctl, bit(2) | bit(0));
        ASSERT_OK(cnt0.writew<u32>(CNTV_CTL, bit(1) | bit(0)));
        ASSERT_OK(cnt0.readw(CNTV_CTL, ctl));
        EXPECT_EQ(ctl, bit(2) | bit(1) | bit(0));
        EXPECT_FALSE(irq0_virt);
        ASSERT_OK(cnt0.readw(CNTV_TVAL, tval));
        EXPECT_EQ(tval, 0);

        // we should be at 3ms now
        ASSERT_EQ(sc_time_stamp(), sc_time(3, SC_MS));

        // schedule a physical timer to fire at 10ms
        ASSERT_OK(cnt0.writew(CNTP_CVAL, 1000000));
        ASSERT_OK(cnt0.readw(CNTP_CVAL, cmp));
        EXPECT_EQ(cmp, 1000000);
        ASSERT_OK(cnt0.readw(CNTP_TVAL, tval));
        EXPECT_EQ(tval, 0); // timer is still off
        ASSERT_OK(cnt0.writew<u32>(CNTP_CTL, bit(1) | bit(0)));
        ASSERT_OK(cnt0.readw(CNTP_TVAL, tval));
        EXPECT_EQ(tval, 1000000 - 300000);
        wait(5, SC_MS);
        ASSERT_OK(cnt0.readw(CNTP_TVAL, tval));
        EXPECT_EQ(tval, 1000000 - 300000 - 500000);
        wait(3, SC_MS);

        // should be at 11ms now
        ASSERT_EQ(sc_time_stamp(), sc_time(11, SC_MS));

        // interrupt should be pending but not active
        EXPECT_FALSE(irq0_phys);
        ASSERT_OK(cnt0.readw(CNTP_CTL, ctl));
        EXPECT_EQ(ctl, bit(2) | bit(1) | bit(0));
        ASSERT_OK(cnt0.writew<u32>(CNTP_CTL, bit(0))); // unmask irq
        EXPECT_TRUE(irq0_phys);
        ASSERT_OK(cnt0.writew<u32>(CNTP_TVAL, 1000)); // reschedule
        EXPECT_FALSE(irq0_phys);
        ASSERT_OK(cnt0.readw(CNTP_CTL, ctl));
        EXPECT_EQ(ctl, bit(0));

        // turn off both timers
        ASSERT_OK(cnt0.writew(CNTP_CTL, 0));
        ASSERT_OK(cnt0.writew(CNTV_CTL, 0));
        EXPECT_FALSE(irq0_phys);
        EXPECT_FALSE(irq0_virt);
    }
};

TEST(models, arch_timer) {
    arch_timer_test test("test");
    sc_core::sc_start();
}
