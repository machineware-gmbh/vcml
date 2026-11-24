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
    NVMEM_SIZE = timers::rtc1742::NVMEM_2K,
    NVMEM_LAST = NVMEM_SIZE - 9,
    RTC_CONTROL = NVMEM_SIZE - 8,
    RTC_SECONDS = NVMEM_SIZE - 7,
    RTC_MINUTES = NVMEM_SIZE - 6,
    RTC_HOUR = NVMEM_SIZE - 5,
    RTC_DAY = NVMEM_SIZE - 4,
    RTC_DATE = NVMEM_SIZE - 3,
    RTC_MONTH = NVMEM_SIZE - 2,
    RTC_YEAR = NVMEM_SIZE - 1,
};

enum : u8 {
    CONTROL_W = timers::rtc1742::CONTROL_W,
    CONTROL_R = timers::rtc1742::CONTROL_R,
    CENTURY_M = 0x3f,
    SECONDS_OSC = timers::rtc1742::SECONDS_OSC,
    DAY_BF = timers::rtc1742::DAY_BF,
    DAY_FT = timers::rtc1742::DAY_FT,
};

class rtc1742_test : public test_base
{
public:
    timers::rtc1742 rtc;
    tlm_initiator_socket out;

    rtc1742_test(const sc_module_name& nm):
        test_base(nm), rtc("rtc"), out("out") {
        out.bind(rtc.in);
        rst.bind(rtc.rst);
        clk.bind(rtc.clk);

        add_test("strings", &rtc1742_test::test_strings);
        add_test("nvmem", &rtc1742_test::test_nvmem);
        add_test("set_time", &rtc1742_test::test_set_time);
        add_test("read_latch", &rtc1742_test::test_read_latch);
        add_test("flags", &rtc1742_test::test_flags);
        add_test("host_time", &rtc1742_test::test_host_time);
        add_test("commands", &rtc1742_test::test_commands);
    }

    u8 read(u64 addr) {
        u8 data = 0;
        EXPECT_OK(out.readw(addr, data)) << "reading " << std::hex << addr;
        return data;
    }

    void write(u64 addr, u8 data) {
        EXPECT_OK(out.writew(addr, data)) << "writing " << std::hex << addr;
    }

    void set_time(u8 century, u8 year, u8 month, u8 date, u8 hour, u8 minutes,
                  u8 seconds) {
        write(RTC_CONTROL, CONTROL_W);
        write(RTC_CONTROL, CONTROL_W | century);
        write(RTC_YEAR, year);
        write(RTC_MONTH, month);
        write(RTC_DATE, date);
        write(RTC_HOUR, hour);
        write(RTC_MINUTES, minutes);
        write(RTC_SECONDS, seconds);
        write(RTC_CONTROL, century);
    }

    void latch_time() {
        write(RTC_CONTROL, read(RTC_CONTROL) & CENTURY_M);
        write(RTC_CONTROL, read(RTC_CONTROL) | CONTROL_R);
    }

    void test_strings() {
        EXPECT_STREQ(rtc.kind(), "vcml::timers::rtc1742");
        EXPECT_STREQ(rtc.version(), VCML_VERSION_STRING);
        EXPECT_TRUE(rtc.sctime);
        EXPECT_TRUE(rtc.nvmem.get().empty());
    }

    void test_nvmem() {
        write(0, 0x12);
        write(1, 0x34);
        write(NVMEM_LAST, 0xab);
        EXPECT_EQ(read(0), 0x12);
        EXPECT_EQ(read(1), 0x34);
        EXPECT_EQ(read(NVMEM_LAST), 0xab);

        u32 data = 0xcafebabe;
        EXPECT_OK(out.writew(0x100, data));
        EXPECT_EQ(read(0x100), 0xbe);
        EXPECT_EQ(read(0x103), 0xca);
        data = 0;
        EXPECT_OK(out.readw(0x100, data));
        EXPECT_EQ(data, 0xcafebabe);

        EXPECT_AE(out.readw(NVMEM_SIZE, data));
        EXPECT_AE(out.writew(NVMEM_SIZE, data));

        write(0, 0);
        write(1, 0);
        write(NVMEM_LAST, 0);
        data = 0;
        EXPECT_OK(out.writew(0x100, data));
    }

    void test_set_time() {
        set_time(0x20, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06);
        EXPECT_EQ(read(RTC_CONTROL) & (CONTROL_W | CONTROL_R), 0);

        latch_time();
        EXPECT_EQ(read(RTC_CONTROL), CONTROL_R | 0x20);
        EXPECT_EQ(read(RTC_YEAR), 0x01);
        EXPECT_EQ(read(RTC_MONTH), 0x02);
        EXPECT_EQ(read(RTC_DATE), 0x03);
        EXPECT_EQ(read(RTC_HOUR), 0x04);
        EXPECT_EQ(read(RTC_MINUTES), 0x05);
        EXPECT_EQ(read(RTC_SECONDS), 0x06);

        wait(sc_time(10, SC_SEC));
        latch_time();
        EXPECT_EQ(read(RTC_HOUR), 0x04);
        EXPECT_EQ(read(RTC_MINUTES), 0x05);
        EXPECT_EQ(read(RTC_SECONDS), 0x16);

        wait(sc_time(3600 + 55 * 60 + 44, SC_SEC));
        latch_time();
        EXPECT_EQ(read(RTC_DATE), 0x03);
        EXPECT_EQ(read(RTC_HOUR), 0x06);
        EXPECT_EQ(read(RTC_MINUTES), 0x01);
        EXPECT_EQ(read(RTC_SECONDS), 0x00);

        set_time(0x19, 0x99, 0x12, 0x31, 0x23, 0x59, 0x58);
        wait(sc_time(3, SC_SEC));
        latch_time();
        EXPECT_EQ(read(RTC_CONTROL), CONTROL_R | 0x20);
        EXPECT_EQ(read(RTC_YEAR), 0x00);
        EXPECT_EQ(read(RTC_MONTH), 0x01);
        EXPECT_EQ(read(RTC_DATE), 0x01);
        EXPECT_EQ(read(RTC_HOUR), 0x00);
        EXPECT_EQ(read(RTC_MINUTES), 0x00);
        EXPECT_EQ(read(RTC_SECONDS), 0x01);
    }

    void test_read_latch() {
        set_time(0x20, 0x10, 0x06, 0x15, 0x12, 0x00, 0x00);

        write(RTC_CONTROL, 0x20 | CONTROL_R);
        EXPECT_EQ(read(RTC_SECONDS), 0x00);
        wait(sc_time(5, SC_SEC));
        EXPECT_EQ(read(RTC_SECONDS), 0x00);

        write(RTC_CONTROL, 0x20);
        EXPECT_EQ(read(RTC_CONTROL), 0x20);

        write(RTC_CONTROL, 0x20 | CONTROL_R);
        EXPECT_EQ(read(RTC_CONTROL), 0x20 | CONTROL_R);
        EXPECT_EQ(read(RTC_SECONDS), 0x05);
        write(RTC_CONTROL, 0x20);
    }

    void test_flags() {
        EXPECT_EQ(read(RTC_DAY) & DAY_BF, DAY_BF);

        write(RTC_SECONDS, SECONDS_OSC);
        write(RTC_DAY, DAY_BF | DAY_FT);
        latch_time();
        EXPECT_EQ(read(RTC_SECONDS) & SECONDS_OSC, SECONDS_OSC);
        EXPECT_EQ(read(RTC_DAY) & (DAY_BF | DAY_FT), DAY_BF | DAY_FT);

        write(RTC_CONTROL, 0);
        write(RTC_SECONDS, 0);
        write(RTC_DAY, 0);
        latch_time();
        EXPECT_EQ(read(RTC_SECONDS) & SECONDS_OSC, 0);
        EXPECT_EQ(read(RTC_DAY) & (DAY_BF | DAY_FT), 0);
    }

    void test_host_time() {
        rtc.sctime = false;

        for (int attempt = 0; attempt < 10; attempt++) {
            time_t before = time(NULL);
            latch_time();
            time_t after = time(NULL);
            if (before != after)
                continue;

            struct tm now = *gmtime(&before);
            auto bcd = [](int val) -> u8 {
                return (val / 10) << 4 | val % 10;
            };
            EXPECT_EQ(read(RTC_CONTROL) & CENTURY_M,
                      bcd((now.tm_year + 1900) / 100));
            EXPECT_EQ(read(RTC_YEAR), bcd(now.tm_year % 100));
            EXPECT_EQ(read(RTC_MONTH), bcd(now.tm_mon + 1));
            EXPECT_EQ(read(RTC_DATE), bcd(now.tm_mday));
            EXPECT_EQ(read(RTC_HOUR), bcd(now.tm_hour));
            EXPECT_EQ(read(RTC_MINUTES), bcd(now.tm_min));
            EXPECT_EQ(read(RTC_SECONDS), bcd(now.tm_sec));
            rtc.sctime = true;
            return;
        }

        rtc.sctime = true;
        ADD_FAILURE() << "host time kept changing";
    }

    void test_commands() {
        stringstream ss;
        EXPECT_EQ(read(RTC_DAY) & DAY_BF, DAY_BF);
        EXPECT_TRUE(rtc.execute("battery", ss));
        EXPECT_EQ(read(RTC_DAY) & DAY_BF, 0);
        EXPECT_TRUE(rtc.execute("battery", ss));
        EXPECT_EQ(read(RTC_DAY) & DAY_BF, DAY_BF);

        string file = mkstr("%s/vcml_rtc1742_%d.bin", mwr::temp_dir().c_str(),
                            mwr::getpid());
        for (u64 addr = 0; addr <= NVMEM_LAST; addr++)
            write(addr, (u8)(addr * 3 + 1));

        EXPECT_TRUE(rtc.execute("save", { file }, ss));
        EXPECT_TRUE(mwr::file_exists(file));

        for (u64 addr = 0; addr <= NVMEM_LAST; addr++)
            write(addr, 0);

        EXPECT_TRUE(rtc.execute("load", { file }, ss));
        for (u64 addr = 0; addr <= NVMEM_LAST; addr++)
            ASSERT_EQ(read(addr), (u8)(addr * 3 + 1)) << "address " << addr;

        std::remove(file.c_str());
        EXPECT_TRUE(rtc.execute("load", { file }, ss));
        EXPECT_EQ(read(0), 1);
        EXPECT_FALSE(rtc.execute("load", ss));
        EXPECT_FALSE(rtc.execute("save", ss));

        for (u64 addr = 0; addr <= NVMEM_LAST; addr++)
            write(addr, 0);
    }
};

TEST(timers, rtc1742) {
    rtc1742_test test("test");
    sc_core::sc_start();
}
