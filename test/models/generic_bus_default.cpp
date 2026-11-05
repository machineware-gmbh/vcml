/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

class bus_default_harness : public test_base
{
public:
    bool check_invalidate;

    generic::memory mem_any;
    generic::memory mem_def1;
    generic::memory mem_def2;
    generic::memory mem_glob;
    generic::bus bus;

    tlm_initiator_socket out1;
    tlm_initiator_socket out2;
    tlm_initiator_socket out3;

    MOCK_METHOD(void, invalidate, (size_t, u64, u64));

    virtual void invalidate_direct_mem_ptr(tlm_initiator_socket& origin,
                                           u64 start, u64 end) override {
        if (check_invalidate) {
            size_t idx = &origin == &out1 ? 1 : &origin == &out2 ? 2 : 3;
            invalidate(idx, start, end);
        }

        tlm_host::invalidate_direct_mem_ptr(origin, start, end);
    }

    bus_default_harness(const sc_module_name& nm):
        test_base(nm),
        check_invalidate(false),
        mem_any("mem_any", 0x10000),
        mem_def1("mem_def1", 0x10000),
        mem_def2("mem_def2", 0x10000),
        mem_glob("mem_glob", 0x10000),
        bus("bus"),
        out1("out1"),
        out2("out2"),
        out3("out3") {
        for (sc_object* obj : { (sc_object*)&mem_any, (sc_object*)&mem_def1,
                                (sc_object*)&mem_def2, (sc_object*)&mem_glob,
                                (sc_object*)&bus }) {
            clk_bind(*this, "clk", *obj, "clk");
            gpio_bind(*this, "rst", *obj, "rst");
        }

        tlm_bind(bus, *this, "out1");
        tlm_bind(bus, *this, "out2");
        tlm_bind(bus, *this, "out3");

        bus.bind(mem_any.in, 0x1000, 0x1fff);
        bus.bind_default(out1, mem_def1.in);
        bus.bind_default(out2, mem_def2.in, 0x100);
        bus.bind_default(mem_glob.in);

        add_test("routing", &bus_default_harness::test_routing);
        add_test("dmi", &bus_default_harness::test_dmi);
        add_test("mappings", &bus_default_harness::test_mappings);
        add_test("mmap", &bus_default_harness::test_mmap);
    }

    void test_routing() {
        ASSERT_OK(out1.writew<u32>(0x5000, 0x11111111));
        ASSERT_OK(out2.writew<u32>(0x5000, 0x22222222));
        ASSERT_OK(out3.writew<u32>(0x5000, 0x33333333));
        EXPECT_EQ(mem_def1.get<u32>(0x5000), 0x11111111);
        EXPECT_EQ(mem_def2.get<u32>(0x5100), 0x22222222);
        EXPECT_EQ(mem_glob.get<u32>(0x5000), 0x33333333);

        ASSERT_OK(out1.writew<u32>(0x1000, 0x44444444));
        EXPECT_EQ(mem_any.get<u32>(0x0), 0x44444444);
        ASSERT_OK(out2.writew<u32>(0x1004, 0x55555555));
        EXPECT_EQ(mem_any.get<u32>(0x4), 0x55555555);
        EXPECT_EQ(mem_def1.get<u32>(0x1000), 0u);
        EXPECT_EQ(mem_def2.get<u32>(0x1104), 0u);
    }

    void test_dmi() {
        u32 data;
        ASSERT_OK(out1.readw<u32>(0x5000, data));
        ASSERT_OK(out2.readw<u32>(0x5000, data));

        tlm_dmi dmi;
        ASSERT_TRUE(out1.dmi_cache().lookup(0x5000, 4, TLM_READ_COMMAND, dmi));
        EXPECT_EQ(dmi.get_start_address(), 0x2000);
        EXPECT_EQ(dmi.get_end_address(), 0xffff);
        EXPECT_EQ(dmi.get_dmi_ptr(), mem_def1.data() + 0x2000);
        EXPECT_FALSE(out1.dmi_cache().lookup(0x1000, 4, TLM_READ_COMMAND, dmi))
            << "default route granted DMI over a mapped region";

        ASSERT_TRUE(out2.dmi_cache().lookup(0x5000, 4, TLM_READ_COMMAND, dmi));
        EXPECT_EQ(dmi.get_start_address(), 0x2000);
        EXPECT_EQ(dmi.get_end_address(), 0xfeff);
        EXPECT_EQ(dmi.get_dmi_ptr(), mem_def2.data() + 0x2100);

        check_invalidate = true;
        EXPECT_CALL(*this, invalidate(_, _, _)).Times(0);
        EXPECT_CALL(*this, invalidate(1, 0x0, 0xffff)).Times(1);
        mem_def1.unmap_dmi(0x0, 0xffff);

        EXPECT_CALL(*this, invalidate(2, 0x0, 0xff)).Times(1);
        mem_def2.unmap_dmi(0x100, 0x1ff);
        check_invalidate = false;
    }

    void test_mappings() {
        size_t def1 = bus.out.index_of(bus.out[1]);
        size_t glob = bus.out.index_of(bus.out[3]);

        EXPECT_EQ(bus.get_all_mappings().size(), 4);
        auto src1 = bus.get_source_mappings(bus.in[0]);
        ASSERT_EQ(src1.size(), 2);
        EXPECT_EQ(src1.back().target, def1);
        auto src3 = bus.get_source_mappings(bus.in[2]);
        ASSERT_EQ(src3.size(), 2);
        EXPECT_EQ(src3.back().target, glob);
        EXPECT_EQ(bus.get_target_mappings(bus.out[2]).size(), 1);

        ASSERT_TRUE(bus.get_default_mapping(0));
        EXPECT_EQ(bus.get_default_mapping(0)->offset, 0);
        EXPECT_EQ(bus.get_default_mapping(1)->offset, 0x100);
        EXPECT_EQ(bus.get_default_mapping(2)->target, glob);

        EXPECT_THROW(bus.map_default(def1, 0, 0), std::exception);
    }

    void test_mmap() {
        bus.execute("mmap", std::cout);
        std::cout << std::endl;
    }
};

TEST(generic_bus, source_default) {
    bus_default_harness test("test");
    sc_core::sc_start();
}
