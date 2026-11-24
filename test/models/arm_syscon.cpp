/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

enum : u64 {
    SYS_ID = 0x00,
    SYS_LOCK = 0x20,
    SYS_CFGDATA0 = 0x24,
    SYS_CFGDATA1 = 0x28,
    SYS_FLAGS_S = 0x30,
    SYS_FLAGS_C = 0x34,
    SYS_NVFLAGS_S = 0x38,
    SYS_NVFLAGS_C = 0x3c,
    SYS_24MHZ = 0x5c,
    SYS_PROCID = 0x84,
    SYS_CFGDATA = 0xa0,
    SYS_CFGCTRL = 0xa4,
    SYS_CFGSTAT = 0xa8,
};

class syscon_test : public test_base
{
public:
    arm::syscon syscon;
    tlm_initiator_socket out;

    syscon_test(const sc_module_name& nm):
        test_base(nm), syscon("syscon"), out("out") {
        out.bind(syscon.in);
        rst.bind(syscon.rst);
        clk.bind(syscon.clk);
        syscon.setup_vexpress();

        add_test("strings", &syscon_test::test_strings);
        add_test("ids", &syscon_test::test_ids);
        add_test("lock", &syscon_test::test_lock);
        add_test("flags", &syscon_test::test_flags);
        add_test("clock", &syscon_test::test_clock);
        add_test("config", &syscon_test::test_config);
    }

    u32 read(u64 addr) {
        u32 data = 0;
        EXPECT_OK(out.readw(addr, data)) << "reading " << std::hex << addr;
        return data;
    }

    void write(u64 addr, u32 data) {
        EXPECT_OK(out.writew(addr, data)) << "writing " << std::hex << addr;
    }

    void test_strings() {
        EXPECT_STREQ(syscon.kind(), "vcml::arm::syscon");
        EXPECT_STREQ(syscon.version(), VCML_VERSION_STRING);
    }

    void test_ids() {
        EXPECT_EQ(read(SYS_ID), arm::syscon::SYSID_VEXPRESS);
        EXPECT_EQ(read(SYS_PROCID), arm::syscon::PROCID_VEXPRESS);

        u32 data = 0;
        EXPECT_CE(out.writew(SYS_ID, data));
        EXPECT_EQ(read(SYS_ID), arm::syscon::SYSID_VEXPRESS);
    }

    void test_lock() {
        u16 lock = 0xffff;
        EXPECT_OK(out.readw(SYS_LOCK, lock));
        EXPECT_EQ(lock, 0);

        lock = arm::syscon::LOCKVAL_LOCK;
        EXPECT_OK(out.writew(SYS_LOCK, lock));
        EXPECT_OK(out.readw(SYS_LOCK, lock));
        EXPECT_EQ(lock, arm::syscon::LOCKVAL_LOCK);

        lock = 0xffff;
        EXPECT_OK(out.writew(SYS_LOCK, lock));
        EXPECT_OK(out.readw(SYS_LOCK, lock));
        EXPECT_EQ(lock, 0x7fff);
    }

    void test_flags() {
        for (u64 addr : { SYS_FLAGS_S, SYS_NVFLAGS_S }) {
            u64 set = addr;
            u64 clr = addr + 4;
            EXPECT_EQ(read(set), 0);
            EXPECT_EQ(read(clr), 0);

            write(set, 0x00f0);
            write(set, 0x0f05);
            EXPECT_EQ(read(set), 0x0ff5);
            EXPECT_EQ(read(clr), 0x0ff5);

            write(clr, 0x0f04);
            EXPECT_EQ(read(set), 0x00f1);
            EXPECT_EQ(read(clr), 0x00f1);

            write(clr, 0xffffffff);
            EXPECT_EQ(read(set), 0);
        }

        write(SYS_FLAGS_S, 0xa);
        EXPECT_EQ(read(SYS_NVFLAGS_S), 0);
        write(SYS_NVFLAGS_S, 0x5);
        EXPECT_EQ(read(SYS_FLAGS_S), 0xa);
        EXPECT_EQ(read(SYS_NVFLAGS_S), 0x5);
    }

    void test_clock() {
        wait(sc_time(1, SC_MS));
        u32 before = sc_time_stamp().to_seconds() * 24.0 * MHz;
        u32 ticks = read(SYS_24MHZ);
        EXPECT_EQ(ticks, before);
        EXPECT_GE(ticks, 24000);

        wait(sc_time(1, SC_MS));
        EXPECT_EQ(read(SYS_24MHZ), ticks + 24000);

        u32 data = 0;
        EXPECT_CE(out.writew(SYS_24MHZ, data));
    }

    void test_config() {
        write(SYS_CFGDATA0, 0x11223344);
        write(SYS_CFGDATA1, 0x55667788);
        write(SYS_CFGDATA, 0x99aabbcc);
        EXPECT_EQ(read(SYS_CFGDATA0), 0x11223344);
        EXPECT_EQ(read(SYS_CFGDATA1), 0x55667788);
        EXPECT_EQ(read(SYS_CFGDATA), 0x99aabbcc);

        write(SYS_CFGCTRL, 0x7fffffff);
        EXPECT_EQ(read(SYS_CFGCTRL), 0x7ff3ffff);
        write(SYS_CFGCTRL, 0x80000001);
        EXPECT_EQ(read(SYS_CFGCTRL), 0x00000001);

        write(SYS_CFGSTAT, 0xffffffff);
        EXPECT_EQ(read(SYS_CFGSTAT), 0x3);
        write(SYS_CFGSTAT, 0);
        EXPECT_EQ(read(SYS_CFGSTAT), 0);

        write(SYS_PROCID, 0x12345678);
        EXPECT_EQ(read(SYS_PROCID), 0x12345678);
    }
};

TEST(arm, syscon) {
    syscon_test test("test");
    sc_core::sc_start();
}
