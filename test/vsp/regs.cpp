/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "vsptest.h"

class regs_cpu : public test_vsp_cpu
{
public:
    u32 pc;
    u8 flags;
    u16 vec[2];
    u32 key;

    regs_cpu(const sc_module_name& nm):
        test_vsp_cpu(nm), pc(0x11223344), flags(0xab), vec{ 0x0102, 0x0304 } {
        define_cpureg_rw(0, "pc", sizeof(pc));
        define_cpureg_rw(1, "flags", sizeof(flags));
        define_cpureg_r(2, "vec", sizeof(vec[0]), 2);
        define_cpureg_w(3, "key", sizeof(key));
    }

    void* regptr(size_t regno) {
        switch (regno) {
        case 0:
            return &pc;
        case 1:
            return &flags;
        case 2:
            return vec;
        default:
            return &key;
        }
    }

    virtual bool read_reg_dbg(size_t regno, void* buf, size_t len) override {
        memcpy(buf, regptr(regno), len);
        return true;
    }

    virtual bool write_reg_dbg(size_t regno, const void* buf,
                               size_t len) override {
        memcpy(regptr(regno), buf, len);
        return true;
    }
};

TEST(vspserver, registers) {
    regs_cpu cpu("cpu");

    vspserver session("localhost", 0);
    thread client([&]() {
        test_vsp_client vsp(session.port()); // client 0
        ASSERT_TRUE(wait_for([&]() { return has_client(session, 0); }));

        EXPECT_EQ(vsp.command("lreg,cpu"), "OK,pc:4,flags:1,vec:2,key:4");

        EXPECT_EQ(vsp.command("getr,cpu"), "E,insufficient arguments 2");
        EXPECT_EQ(vsp.command("getr,cpu,pc"), "OK,0x44,0x33,0x22,0x11");
        EXPECT_EQ(vsp.command("getr,cpu,flags"), "OK,0xab");
        EXPECT_EQ(vsp.command("getr,cpu,vec"), "OK,0x02,0x01,0x04,0x03");
        EXPECT_EQ(vsp.command("getr,cpu,key"), "E,error reading key");
        EXPECT_EQ(vsp.command("getr,cpu,nothing"),
                  "E,no such register: nothing");

        // one value per register, in the order of the request
        EXPECT_EQ(vsp.command("getr,cpu,flags,pc,vec,flags"),
                  "OK,ab,44332211,02010403,ab");

        // registers that cannot be read have no value
        EXPECT_EQ(vsp.command("getr,cpu,key,pc"), "OK,,44332211");
        EXPECT_EQ(vsp.command("getr,cpu,pc,key"), "OK,44332211,");

        EXPECT_EQ(vsp.command("setr,cpu,flags,0x12"), "OK,1 bytes written");
        EXPECT_EQ(vsp.command("getr,cpu,pc,flags"), "OK,44332211,12");

        // fails entirely if any of the registers does not exist
        EXPECT_EQ(vsp.command("getr,cpu,pc,nothing,flags"),
                  "E,no such register: nothing");
        EXPECT_EQ(vsp.command("getr,nothing,pc,flags"),
                  "E,no such target: nothing");

        vsp.command("quit");
    });

    session.start();
    client.join();
}
