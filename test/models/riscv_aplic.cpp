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

class aplic_test : public test_base
{
public:
    tlm_target_socket msi_m;
    tlm_target_socket msi_s;

    tlm_initiator_socket out_m;
    tlm_initiator_socket out_s;

    gpio_initiator_socket irq1;
    gpio_initiator_socket irq2;

    riscv::aplic aplic_m;
    riscv::aplic aplic_s;

    MOCK_METHOD(void, msi_receive, (u64 addr, u32 eiid), ());

    virtual unsigned int transport(tlm_target_socket& sock,
                                   tlm_generic_payload& tx,
                                   const tlm_sbi& info) override {
        VCML_ERROR_ON(tx.get_data_length() != 4, "invalid data length");
        msi_receive(tx.get_address(), *(u32*)tx.get_data_ptr());
        tx.set_response_status(TLM_OK_RESPONSE);
        return tx.get_data_length();
    }

    aplic_test(const sc_module_name& nm):
        test_base(nm),
        msi_m("msi_m"),
        msi_s("msi_s"),
        out_m("out_m"),
        out_s("out_s"),
        irq1("irq1"),
        irq2("irq2"),
        aplic_m("aplic_m"),
        aplic_s("aplic_s", aplic_m) {
        rst.bind(aplic_m.rst);
        rst.bind(aplic_s.rst);

        clk.bind(aplic_m.clk);
        clk.bind(aplic_s.clk);

        irq1.bind(aplic_m.irq_in[1]);
        irq2.bind(aplic_s.irq_in[2]);

        out_m.bind(aplic_m.in);
        out_s.bind(aplic_s.in);

        aplic_m.msi.bind(msi_m);
        aplic_s.msi.bind(msi_s);
    }

    virtual void run_test() override {
        u32 data;

        ASSERT_STREQ(aplic_m.kind(), "vcml::riscv::aplic");
        ASSERT_STREQ(aplic_s.kind(), "vcml::riscv::aplic");

        // make sure only naturally aligned accesses work
        for (auto* r : aplic_m.get_registers()) {
            ASSERT_CE(out_m.readw(r->get_address() + 1, data));
            ASSERT_CE(out_m.writew<u64>(r->get_address(), 0));
        }

        // make sure no contexts got spawned
        ASSERT_AE(out_m.readw(0x4000, data));
        ASSERT_AE(out_m.readw(0x4010, data));
        ASSERT_AE(out_s.readw(0x4000, data));
        ASSERT_AE(out_s.readw(0x4010, data));

        // reset
        aplic_m.reset();
        aplic_s.reset();

        // setup controllers
        ASSERT_OK(out_m.writew(0x0000, 0xfffffffe));
        ASSERT_OK(out_m.readw(0x0000, data));
        ASSERT_EQ(data, 0x80000100);

        ASSERT_OK(out_s.writew(0x0000, 0xfffffffe));
        ASSERT_OK(out_s.readw(0x0000, data));
        ASSERT_EQ(data, 0x80000100);

        // setup interrupts, delegate irq2
        ASSERT_OK(out_m.writew(0x0004, 0x00000006));
        ASSERT_OK(out_m.writew(0x0008, 0x00000400));
        ASSERT_OK(out_s.writew(0x0008, 0x00000004));
        ASSERT_OK(out_m.writew(0x3004, 0x00000102));
        ASSERT_OK(out_s.writew(0x3008, 0x00000206));

        // make sure we cannot change delegated irq targets
        ASSERT_OK(out_m.writew(0x3008, 0x00000404));
        ASSERT_OK(out_m.readw(0x3008, data));
        ASSERT_EQ(data, 0);

        // enable interrupts
        ASSERT_OK(out_m.writew(0x1e00, 0x00000007));
        ASSERT_OK(out_m.readw(0x1e00, data));
        ASSERT_EQ(data, 2);
        ASSERT_OK(out_s.writew(0x1edc, 0x00000002));
        ASSERT_OK(out_s.readw(0x1e00, data));
        ASSERT_EQ(data, 4);

        // disable interrupts
        ASSERT_OK(out_m.writew(0x1fdc, 0x00000001));
        ASSERT_OK(out_m.readw(0x1e00, data));
        ASSERT_EQ(data, 0);
        ASSERT_OK(out_s.writew(0x1f00, 0x00000004));
        ASSERT_OK(out_s.readw(0x1e00, data));
        ASSERT_EQ(data, 0);

        // re-enable interrupts
        ASSERT_OK(out_m.writew(0x1edc, 0x00000001));
        ASSERT_OK(out_m.readw(0x1e00, data));
        ASSERT_EQ(data, 2);
        ASSERT_OK(out_s.writew(0x1e00, 0x00000005));
        ASSERT_OK(out_s.readw(0x1e00, data));
        ASSERT_EQ(data, 4);

        // setup msi target addresses
        ASSERT_OK(out_m.writew(0x1bc0, 0x00000004));
        ASSERT_OK(out_m.writew(0x1bc4, 0x00000001));

        // s-mode addresses should not be accessible
        ASSERT_CE(out_s.readw(0x1bc0, data));
        ASSERT_CE(out_s.readw(0x1bc4, data));
        ASSERT_CE(out_s.readw(0x1bc8, data));
        ASSERT_CE(out_s.readw(0x1bcc, data));

        // generate msi manually
        EXPECT_CALL(*this, msi_receive(0x100000004000, 20));
        ASSERT_OK(out_m.writew(0x3000, 20));
    }
};

TEST(aplic, aplic) {
    aplic_test test("test");
    sc_core::sc_start();
}
