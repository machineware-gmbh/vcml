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

class bus_harness : public test_base
{
public:
    bool check_invalidate;

    generic::memory mem1;
    generic::memory mem2;
    generic::bus bus;

    tlm_initiator_socket out1;
    tlm_initiator_socket out2;
    tlm_target_socket in;

    MOCK_METHOD(void, invalidate, (u64, u64));

    virtual void invalidate_direct_mem_ptr(tlm_initiator_socket& origin,
                                           u64 start, u64 end) override {
        if (check_invalidate)
            invalidate(start, end);
        tlm_host::invalidate_direct_mem_ptr(origin, start, end);
    }

    bus_harness(const sc_module_name& nm):
        test_base(nm),
        check_invalidate(false),
        mem1("mem1", 0x2000),
        mem2("mem2", 0x2000),
        bus("bus"),
        out1("out1"),
        out2("out2"),
        in("in") {
        clk_bind(*this, "clk", mem1, "clk");
        clk_bind(*this, "clk", mem2, "clk");
        clk_bind(*this, "clk", bus, "clk");

        gpio_bind(*this, "rst", mem1, "rst");
        gpio_bind(*this, "rst", mem2, "rst");
        gpio_bind(*this, "rst", bus, "rst");

        tlm_bind(bus, *this, "out1");
        tlm_bind(bus, *this, "out2");

        tlm_bind(bus, mem1, "in", 0x0000, 0x1fff, 0);
        tlm_bind(bus, mem2, "in", 0x2000, 0x3fff, 0);
        tlm_bind(bus, mem1, "in", 0x6000, 0x7fff, 0);
        tlm_bind(bus, *this, "in", 0x8000, 0x9fff, 0x10000);

        bus.bind(out1, mem1.in, 0xa000, 0xbfff);
        bus.bind(out2, mem2.in, 0xc000, 0xdfff);

        bus.stub(0xe000, 0xe7ff);
        tlm_stub(bus, *this, "out2", 0xe800, 0xefff);

        add_test("read_write", &bus_harness::test_read_write);
        add_test("dmi", &bus_harness::test_dmi);
        add_test("lenient", &bus_harness::test_lenient);
        add_test("stubs", &bus_harness::test_stubs);
        add_test("mmap", &bus_harness::test_mmap);
        add_test("mappings", &bus_harness::test_mappings);
        add_test("invalid_mapping", &bus_harness::test_invalid_mapping);
    }

    void test_read_write() {
        u32 data;

        ASSERT_OK(out1.writew<u32>(0x0000, 0x11111111ul))
            << "cannot write 0x0000 (mem1 + 0x0)";
        ASSERT_OK(out1.writew<u32>(0x0004, 0xfffffffful))
            << "cannot write 0x0004 (mem1 + 0x4)";
        ASSERT_OK(out1.writew<u32>(0x2000, 0x55555555ul))
            << "cannot write 0x2000 (mem2 + 0x0)";
        ASSERT_OK(out1.writew<u32>(0x2004, 0xbbbbbbbbul))
            << "cannot write 0x2004 (mem2 + 0x4)";
        ASSERT_AE(out1.writew<u16>(0x4000, 0x1234ul))
            << "bus reported success for writing to unmapped address";
        EXPECT_OK(out1.readw<u32>(0x6000, data));
        EXPECT_EQ(data, 0x11111111ul);

        ASSERT_OK(out1.readw<u32>(0x0000, data))
            << "cannot read 0x0000 (mem1 + 0x0)";
        EXPECT_EQ(data, 0x11111111ul)
            << "read invalid data from 0x0000 (mem1 + 0x0)";
        ASSERT_OK(out1.readw<u32>(0x0004, data))
            << "cannot read 0x0004 (mem1 + 0x4)";
        EXPECT_EQ(data, 0xfffffffful)
            << "read invalid data from 0x0004 (mem1 + 0x4)";
        ASSERT_OK(out1.readw<u32>(0x2000, data))
            << "cannot read 0x2000 (mem2 + 0x0)";
        EXPECT_EQ(data, 0x55555555ul)
            << "read invalid data from 0x2000 (mem2 + 0x0)";
        ASSERT_OK(out1.readw<u32>(0x2004, data))
            << "cannot read 0x2004 (mem2 + 0x4)";
        EXPECT_EQ(data, 0xbbbbbbbbul)
            << "read invalid data from 0x2000 (mem2 + 0x4)";
        ASSERT_AE(out1.readw<u32>(0x4000, data))
            << "bus reported success for reading from unmapped address";
    }

    void test_dmi() {
        u32 data;
        ASSERT_OK(out1.writew(0x0000, data = 0x11111111));
        ASSERT_OK(out1.writew(0x2000, data = 0x55555555));

        tlm_dmi dmi;
        tlm_dmi_cache& cache = out1.dmi_cache();
        EXPECT_TRUE(cache.lookup(0x0000, 0x2000, TLM_READ_COMMAND, dmi))
            << "bus did not forward DMI region of mem1";
        EXPECT_TRUE(cache.lookup(0x2000, 0x2000, TLM_READ_COMMAND, dmi))
            << "bus did not forward DMI region of mem2";

        if (cache.get_entries().size() > 1) {
            EXPECT_NE(cache.get_entries()[0].get_start_address(),
                      cache.get_entries()[1].get_start_address())
                << "bus forwarded overlapping DMI regions";
            EXPECT_NE(cache.get_entries()[0].get_dmi_ptr(),
                      cache.get_entries()[1].get_dmi_ptr())
                << "bus forwarded overlapping DMI pointers";
        }

        check_invalidate = true;

        EXPECT_CALL(*this, invalidate(0x0000, 0x1fff)).Times(2);
        EXPECT_CALL(*this, invalidate(0x6000, 0x7fff)).Times(2);
        EXPECT_CALL(*this, invalidate(0xa000, 0xbfff)).Times(1);
        mem1.unmap_dmi(0, 0x1fff);
        ASSERT_EQ(cache.get_entries().size(), 1)
            << "bus did not forward DMI invalidation";
        EXPECT_EQ(cache.get_entries()[0].get_start_address(), 0x2000)
            << "bus invalidated wrong DMI region";

        EXPECT_CALL(*this, invalidate(0x8000, 0x9fff)).Times(2);
        in->invalidate_direct_mem_ptr(0, ~0ull);

        EXPECT_CALL(*this, invalidate(0x8100, 0x8fff)).Times(2);
        in->invalidate_direct_mem_ptr(0x10100, 0x10fff);

        EXPECT_CALL(*this, invalidate(_, _)).Times(0);
        in->invalidate_direct_mem_ptr(0, 0x9fff);

        EXPECT_CALL(*this, invalidate(0x8000, 0x800f)).Times(2);
        in->invalidate_direct_mem_ptr(0x9000, 0x1000f);

        EXPECT_AE(out1.readw<u32>(0xc000, data))
            << "bus transaction went through for area unmapped for out1";
        EXPECT_AE(out2.readw<u32>(0xa000, data))
            << "bus transaction went through for area unmapped for out2";

        EXPECT_OK(out1.readw<u32>(0xa000, data))
            << "cannot access memory at privately mapped area";
        EXPECT_EQ(data, 0x11111111)
            << "unexpected data from memory at privately mapped area";
        EXPECT_OK(out2.readw<u32>(0xc000, data))
            << "cannot access memory at privately mapped area";
        EXPECT_EQ(data, 0x55555555)
            << "unexpected data from memory at privately mapped area";

        check_invalidate = false;
    }

    void test_lenient() {
        u32 data = 0;
        bus.lenient = true;
        EXPECT_OK(out1.writew<u32>(0xc000, data));
        bus.lenient = false;
        EXPECT_AE(out1.writew<u32>(0xc000, data));
    }

    void test_stubs() {
        u32 data;
        EXPECT_OK(out1.readw<u32>(0xe000, data))
            << "cannot read from stubbed address area";
        EXPECT_OK(out1.writew<u32>(0xe0f0, data))
            << "cannot write to stubbed address range";
        EXPECT_AE(out1.readw<u32>(0xe800, data))
            << "unexpected data from privately stubbed area";
        EXPECT_OK(out2.readw<u32>(0xe800, data))
            << "cannot read from privately stubbed area";
    }

    void test_mmap() {
        bus.execute("mmap", std::cout);
        std::cout << std::endl;
    }

    void test_mappings() {
        EXPECT_EQ(bus.get_all_mappings().size(), 8);
        EXPECT_EQ(bus.get_source_mappings(bus.in[0]).size(), 6);
        EXPECT_EQ(bus.get_source_mappings(bus.in[1]).size(), 7);
        EXPECT_EQ(bus.get_target_mappings(bus.out[0]).size(), 3);
        EXPECT_EQ(bus.get_target_mappings(bus.out[1]).size(), 2);
    }

    void test_invalid_mapping() {
        EXPECT_THROW({ bus.map(0, 0x2000, 0x2ffff); }, std::exception);
    }
};

TEST(generic_bus, transfer) {
    bus_harness test("test");
    sc_core::sc_start();
}
