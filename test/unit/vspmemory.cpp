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

using debugging::symbol;
using debugging::symtab;
using debugging::SYMKIND_FUNCTION;
using debugging::SYMKIND_OBJECT;

// Pages of 0x100 bytes, each byte holding the low byte of its address. Page
// 1 does not translate, and 0x210..0x21f is a hole in the bus behind page 2.
class test_mem_target : public module, public debugging::target
{
public:
    u8 mem[0x400];

    test_mem_target(const sc_module_name& nm):
        module(nm), debugging::target(static_cast<sc_object&>(*this)) {
        for (size_t i = 0; i < sizeof(mem); i++)
            mem[i] = (u8)i;

        symtab& syms = const_cast<symtab&>(symbols());
        syms.insert(
            symbol("main", SYMKIND_FUNCTION, ENDIAN_LITTLE, 0x20, 0x40, 0x40));
        syms.insert(
            symbol("counter", SYMKIND_OBJECT, ENDIAN_LITTLE, 4, 0x300, 0x300));
        syms.insert(
            symbol("a,b", SYMKIND_OBJECT, ENDIAN_LITTLE, 0, 0x304, 0x304));
    }

    virtual bool page_size(u64& size) override {
        size = 0x100;
        return true;
    }

    virtual bool virt_to_phys(u64 va, u64& pa) override {
        pa = va;
        return va < 0x100 || va >= 0x200;
    }

    // reads up to the first byte that fails, like a bus read would
    virtual u64 read_pmem_dbg(u64 addr, void* buf, u64 size) override {
        u64 n = 0;
        for (; n < size && addr + n < sizeof(mem); n++) {
            if (addr + n >= 0x210 && addr + n < 0x220)
                break;
            ((u8*)buf)[n] = mem[addr + n];
        }

        return n;
    }

    // two byte instructions, shown as their bytes
    virtual bool disassemble(u8* ibuf, u64& addr, string& code) override {
        code = mkstr("op %02hhx,%02hhx", ibuf[0], ibuf[1]);
        addr += 2;
        return true;
    }

    virtual void stacktrace(vector<debugging::stackframe>& trace,
                            size_t limit) override {
        trace.clear();
        trace.push_back({ 0x44, 0x3f0, symbols().find_function(0x44) });
        if (limit > 1)
            trace.push_back({ 0x80, 0x3f8, nullptr });
    }
};

// One test: SystemC elaborates one module hierarchy per process.
TEST(vspserver, memory_and_symbols) {
    test_mem_target tgt("tgt");

    u8 data[8];
    debugging::vmem_status status[8];
    memset(data, 0x55, sizeof(data));
    EXPECT_EQ(tgt.read_vmem_dbg(0xfe, data, 8, status), 2u);
    EXPECT_EQ(data[0], 0xfe);
    EXPECT_EQ(data[1], 0xff);
    EXPECT_EQ(status[1], debugging::VMEM_OK);
    EXPECT_EQ(status[2], debugging::VMEM_MMU_FAULT);
    EXPECT_EQ(data[2], 0);

    // a bus error fails the entire chunk of its page
    memset(data, 0x55, sizeof(data));
    EXPECT_EQ(tgt.read_vmem_dbg(0x2fe, data, 4, status), 4u);
    EXPECT_EQ(status[1], debugging::VMEM_OK);
    EXPECT_EQ(status[2], debugging::VMEM_OK);
    memset(data, 0x55, sizeof(data));
    EXPECT_EQ(tgt.read_vmem_dbg(0x20e, data, 4, status), 0u);
    EXPECT_EQ(status[0], debugging::VMEM_BUS_ERROR);
    EXPECT_EQ(status[3], debugging::VMEM_BUS_ERROR);
    EXPECT_EQ(data[0], 0);

    // without status, failed bytes are zeroed just the same
    memset(data, 0x55, sizeof(data));
    EXPECT_EQ(tgt.read_vmem_dbg(0xfc, data, 8), 4u);
    EXPECT_EQ(data[0], 0xfc);
    EXPECT_EQ(data[4], 0);
    memset(data, 0x55, sizeof(data));
    EXPECT_EQ(tgt.read_vmem_dbg(0x20e, data, 4), 0u);
    EXPECT_EQ(data[0], 0);
    EXPECT_EQ(data[3], 0);

    {
        vspserver session("localhost", 0);
        thread client([&]() {
            test_vsp_client vsp(session.port()); // client 0

            // without status, as before: no telling errors from zeroes
            EXPECT_EQ(vsp.command("vread,tgt,0,2"), "OK,0x00,0x01");
            EXPECT_EQ(vsp.command("vread,tgt,0xfe,4"),
                      "OK,0xfe,0xff,0x00,0x00");
            EXPECT_EQ(vsp.command("vread,tgt,0x20f,2"), "OK,0x00,0x00");

            // a page that does not translate
            EXPECT_EQ(vsp.command("vread,tgt,0xfc,8,status"),
                      "OK,fc,fd,fe,ff,--,--,--,--");

            // a bus error fails the entire chunk of its page
            EXPECT_EQ(vsp.command("vread,tgt,0x20e,4,status"),
                      "OK,**,**,**,**");
            EXPECT_EQ(vsp.command("vread,tgt,0x2fe,4,status"),
                      "OK,fe,ff,00,01");

            // kind, address, size (0 if unknown) and name of each symbol
            string syms = vsp.command("lsym,tgt");
            EXPECT_TRUE(starts_with(syms, "OK,F,0x40,32,main,")) << syms;
            EXPECT_NE(syms.find(",O,0x300,4,counter"), string::npos) << syms;
            EXPECT_NE(syms.find(",O,0x304,0,a\\,b"), string::npos) << syms;
            EXPECT_TRUE(starts_with(vsp.command("lsym,nope"), "E,"));

            // virt, phys, bytes, symbol, offset and code of each instruction
            EXPECT_EQ(vsp.command("disas,tgt,0x40,2"),
                      "OK,0x40,0x40,4041,main,0x0,op 40\\,41,"
                      "0x42,0x42,4243,main,0x2,op 42\\,43");
            EXPECT_EQ(vsp.command("disas,tgt,0x80,1"),
                      "OK,0x80,0x80,8081,,,op 80\\,81");
            EXPECT_TRUE(starts_with(vsp.command("disas,tgt,0x100,1"), "E,"));

            // pc, fp, symbol and offset of each frame
            EXPECT_EQ(vsp.command("stack,tgt"),
                      "OK,0x44,0x3f0,main,0x4,0x80,0x3f8,,");
            EXPECT_EQ(vsp.command("stack,tgt,1"), "OK,0x44,0x3f0,main,0x4");

            vsp.command("quit");
        });

        session.start();
        client.join();
    }
}
