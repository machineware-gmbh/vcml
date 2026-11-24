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
    OCFBC_CTLR = 0x00,
    OCFBC_STAT = 0x04,
    OCFBC_HTIM = 0x08,
    OCFBC_VTIM = 0x0c,
    OCFBC_HVLEN = 0x10,
    OCFBC_VBARA = 0x14,
    OCFBC_VBARB = 0x18,
    OCFBC_PALETTE0 = 0x800,
    OCFBC_PALETTE1 = 0xc00,
    OCFBC_END = 0x1000,
};

enum : u32 {
    VMEM_SIZE = 0x10000,
    VMEM_A = 0x1000,
    VMEM_B = 0x8000,
    CTLR_VEN = opencores::ocfbc::CTLR_VEN,
    CTLR_VIE = opencores::ocfbc::CTLR_VIE,
    CTLR_HIE = opencores::ocfbc::CTLR_HIE,
    CTLR_VBSIE = opencores::ocfbc::CTLR_VBSIE,
    CTLR_CBSIE = opencores::ocfbc::CTLR_CBSIE,
    CTLR_VBSWE = opencores::ocfbc::CTLR_VBSWE,
    CTLR_CBSWE = opencores::ocfbc::CTLR_CBSWE,
    CTLR_VBL2 = opencores::ocfbc::CTLR_VBL2,
    CTLR_VBL4 = opencores::ocfbc::CTLR_VBL4,
    CTLR_VBL8 = opencores::ocfbc::CTLR_VBL8,
    CTLR_BPP8 = opencores::ocfbc::CTLR_BPP8,
    CTLR_BPP16 = opencores::ocfbc::CTLR_BPP16,
    CTLR_BPP24 = opencores::ocfbc::CTLR_BPP24,
    CTLR_BPP32 = opencores::ocfbc::CTLR_BPP32,
    CTLR_PC = opencores::ocfbc::CTLR_PC,
    STAT_SINT = opencores::ocfbc::STAT_SINT,
    STAT_VINT = opencores::ocfbc::STAT_VINT,
    STAT_HINT = opencores::ocfbc::STAT_HINT,
    STAT_VBSINT = opencores::ocfbc::STAT_VBSINT,
    STAT_CBSINT = opencores::ocfbc::STAT_CBSINT,
    STAT_AVMP = opencores::ocfbc::STAT_AVMP,
    STAT_ACMP = opencores::ocfbc::STAT_ACMP,
};

class ocfbc_test : public test_base, public ui::display_if
{
public:
    opencores::ocfbc ocfbc;
    generic::memory vmem;
    tlm_initiator_socket out;
    gpio_target_socket irq;

    ui::display* display;
    ui::videomode mode;
    u8* fbptr;
    size_t setups;
    size_t renders;

    const sc_time frame;

    virtual void display_setup(const ui::videomode& m, u8* fb) override {
        mode = m;
        fbptr = fb;
        setups++;
    }

    virtual void display_render(u32 x, u32 y, u32 w, u32 h) override {
        EXPECT_EQ(x, 0);
        EXPECT_EQ(y, 0);
        EXPECT_EQ(w, mode.xres);
        EXPECT_EQ(h, mode.yres);
        renders++;
    }

    ocfbc_test(const sc_module_name& nm):
        test_base(nm),
        ui::display_if(),
        ocfbc("ocfbc"),
        vmem("vmem", VMEM_SIZE),
        out("out"),
        irq("irq"),
        display(),
        mode(),
        fbptr(),
        setups(),
        renders(),
        frame(1.0 / 60, SC_SEC) {
        out.bind(ocfbc.in);
        ocfbc.out.bind(vmem.in);
        ocfbc.irq.bind(irq);
        rst.bind(ocfbc.rst);
        clk.bind(ocfbc.clk);
        rst.bind(vmem.rst);
        clk.bind(vmem.clk);

        auto* obj = sc_core::sc_find_object("test.ocfbc.display");
        display = dynamic_cast<ui::display*>(obj);
        VCML_ERROR_ON(!display, "display not found");
        display->attach(this);

        add_test("strings", &ocfbc_test::test_strings);
        add_test("registers", &ocfbc_test::test_registers);
        add_test("palette", &ocfbc_test::test_palette);
        add_test("copy", &ocfbc_test::test_copy);
        add_test("mapped", &ocfbc_test::test_mapped);
        add_test("formats", &ocfbc_test::test_formats);
        add_test("pseudocolor", &ocfbc_test::test_pseudocolor);
        add_test("interrupts", &ocfbc_test::test_interrupts);
        add_test("bank_switch", &ocfbc_test::test_bank_switch);
        add_test("bus_error", &ocfbc_test::test_bus_error);
        add_test("burst", &ocfbc_test::test_burst);
    }

    virtual ~ocfbc_test() { display->detach(this); }

    u32 read(u64 addr) {
        u32 data = 0;
        EXPECT_OK(out.readw(addr, data)) << "reading " << std::hex << addr;
        return data;
    }

    void write(u64 addr, u32 data) {
        EXPECT_OK(out.writew(addr, data)) << "writing " << std::hex << addr;
    }

    void fill(u32 base, size_t len, u8 seed) {
        for (size_t i = 0; i < len; i++)
            vmem.data()[base + i] = (u8)(seed + i * 5);
    }

    void enable(u32 xres, u32 yres, u32 ctlr, bool dmi) {
        ocfbc.allow_dmi = dmi;
        write(OCFBC_HTIM, xres - 1);
        write(OCFBC_VTIM, yres - 1);
        write(OCFBC_VBARA, VMEM_A);
        write(OCFBC_VBARB, VMEM_B);
        write(OCFBC_CTLR, ctlr | CTLR_VEN);
        wait(2 * frame);
    }

    void disable() {
        write(OCFBC_CTLR, 0);
        wait(2 * frame);
        write(OCFBC_STAT, 0);
        EXPECT_FALSE(irq);
        ocfbc.allow_dmi = true;
    }

    string info() {
        stringstream ss;
        EXPECT_TRUE(ocfbc.execute("info", ss));
        return ss.str();
    }

    void test_strings() {
        EXPECT_STREQ(ocfbc.kind(), "vcml::opencores::ocfbc");
        EXPECT_STREQ(ocfbc.version(), VCML_VERSION_STRING);
        EXPECT_EQ(ocfbc.clock, 60);
    }

    void test_registers() {
        EXPECT_EQ(read(OCFBC_CTLR), 0);
        EXPECT_EQ(read(OCFBC_STAT), 0);

        for (u64 addr : { OCFBC_HTIM, OCFBC_VTIM, OCFBC_HVLEN, OCFBC_VBARA,
                          OCFBC_VBARB }) {
            write(addr, 0x12345678 + addr);
            EXPECT_EQ(read(addr), 0x12345678 + addr) << std::hex << addr;
        }

        write(OCFBC_STAT, 0xffffffff);
        EXPECT_EQ(read(OCFBC_STAT), 0xff);
        write(OCFBC_STAT, 0);
        EXPECT_EQ(read(OCFBC_STAT), 0);

        write(OCFBC_CTLR, CTLR_VBL4 | CTLR_BPP16 | CTLR_PC);
        EXPECT_EQ(read(OCFBC_CTLR), CTLR_VBL4 | CTLR_BPP16 | CTLR_PC);
        write(OCFBC_CTLR, 0);

        u32 data = 0;
        EXPECT_AE(out.readw(OCFBC_VBARB + 4, data));
        EXPECT_AE(out.readw(OCFBC_PALETTE0 - 4, data));
    }

    void test_palette() {
        for (u32 i = 0; i < 512; i++)
            write(OCFBC_PALETTE0 + i * 4, 0xab000000 + i);
        for (u32 i = 0; i < 512; i++)
            ASSERT_EQ(read(OCFBC_PALETTE0 + i * 4), 0xab000000 + i);

        u8 data = 0;
        EXPECT_OK(out.readw(OCFBC_END - 1, data));
        EXPECT_AE(out.readw(OCFBC_END, data));
        EXPECT_AE(out.writew(OCFBC_END, data));

        for (u32 i = 0; i < 512; i++)
            write(OCFBC_PALETTE0 + i * 4, 0);
    }

    void test_copy() {
        const u32 xres = 8, yres = 4, bpp = 4;
        fill(VMEM_A, xres * yres * bpp, 0x10);

        size_t nsetups = setups;
        size_t nrenders = renders;
        enable(xres, yres, CTLR_BPP32, false);
        EXPECT_EQ(setups, nsetups + 1);
        EXPECT_GE(renders, nrenders + 2);
        EXPECT_EQ(mode.xres, xres);
        EXPECT_EQ(mode.yres, yres);
        EXPECT_EQ(mode.bpp, bpp);
        EXPECT_NE(fbptr, vmem.data() + VMEM_A);
        EXPECT_EQ(memcmp(fbptr, vmem.data() + VMEM_A, xres * yres * bpp), 0);
        EXPECT_THAT(info(), HasSubstr("resolution:  8x4@60Hz"));
        EXPECT_THAT(info(), HasSubstr("framebuffer: copied"));
        EXPECT_THAT(info(), HasSubstr("interrupt:   cleared"));

        u32 pixel = 0;
        memcpy(&pixel, vmem.data() + VMEM_A + (2 * xres + 3) * bpp, bpp);
        EXPECT_EQ(display->read_pixel(3, 2), pixel);

        fill(VMEM_A, xres * yres * bpp, 0x77);
        EXPECT_NE(memcmp(fbptr, vmem.data() + VMEM_A, xres * yres * bpp), 0);
        wait(frame);
        EXPECT_EQ(memcmp(fbptr, vmem.data() + VMEM_A, xres * yres * bpp), 0);
        EXPECT_EQ(read(OCFBC_STAT), 0);
        EXPECT_FALSE(irq);

        nrenders = renders;
        disable();
        wait(4 * frame);
        EXPECT_LE(renders, nrenders + 2);
    }

    void test_mapped() {
        const u32 xres = 16, yres = 8;
        size_t nsetups = setups;
        size_t nrenders = renders;
        enable(xres, yres, CTLR_BPP32, true);
        EXPECT_EQ(setups, nsetups + 1);
        EXPECT_GE(renders, nrenders + 2);
        EXPECT_EQ(mode.xres, xres);
        EXPECT_EQ(mode.yres, yres);
        EXPECT_EQ(fbptr, vmem.data() + VMEM_A);
        EXPECT_THAT(info(), HasSubstr("resolution:  16x8@60Hz"));
        EXPECT_THAT(info(), HasSubstr("framebuffer: mapped"));
        disable();
    }

    void test_formats() {
        const u32 xres = 8, yres = 2;
        const u32 formats[] = { CTLR_BPP8, CTLR_BPP16, CTLR_BPP24,
                                CTLR_BPP32 };
        for (u32 bpp = 1; bpp <= 4; bpp++) {
            fill(VMEM_A, xres * yres * bpp, 0x20 * bpp);
            enable(xres, yres, formats[bpp - 1] | CTLR_VBL2, false);
            EXPECT_EQ(mode.xres, xres);
            EXPECT_EQ(mode.yres, yres);
            EXPECT_EQ(mode.bpp, bpp);
            EXPECT_EQ(mode.size, xres * yres * bpp);
            EXPECT_EQ(memcmp(fbptr, vmem.data() + VMEM_A, mode.size), 0)
                << bpp * 8 << "bpp";
            disable();
        }
    }

    void test_pseudocolor() {
        const u32 xres = 4, yres = 2;
        for (u32 i = 0; i < 256; i++) {
            write(OCFBC_PALETTE0 + i * 4, 0x00112200 + i);
            write(OCFBC_PALETTE1 + i * 4, 0x00334400 + i);
        }

        for (u32 i = 0; i < xres * yres; i++)
            vmem.data()[VMEM_A + i] = 0xf0 + i;

        enable(xres, yres, CTLR_BPP8 | CTLR_PC, true);
        EXPECT_EQ(mode.bpp, 4);
        EXPECT_EQ(mode.size, xres * yres * 4);
        EXPECT_THAT(info(), HasSubstr("framebuffer: copied"));
        for (u32 y = 0; y < yres; y++) {
            for (u32 x = 0; x < xres; x++) {
                EXPECT_EQ(display->read_pixel(x, y), 0xff1122f0 + y * xres + x)
                    << "pixel " << x << "," << y;
            }
        }

        write(OCFBC_CTLR, read(OCFBC_CTLR) | CTLR_CBSWE);
        wait(2 * frame);
        EXPECT_EQ(read(OCFBC_STAT) & STAT_ACMP, STAT_ACMP);
        EXPECT_EQ(read(OCFBC_CTLR) & CTLR_CBSWE, 0);
        EXPECT_EQ(display->read_pixel(1, 1), 0xff3344f5);
        EXPECT_FALSE(irq);

        write(OCFBC_CTLR, read(OCFBC_CTLR) | CTLR_CBSWE | CTLR_CBSIE);
        wait(2 * frame);
        EXPECT_EQ(read(OCFBC_STAT) & STAT_ACMP, 0);
        EXPECT_EQ(display->read_pixel(1, 1), 0xff1122f5);
        EXPECT_TRUE(irq);

        write(OCFBC_STAT, STAT_CBSINT);
        write(OCFBC_STAT, 0);
        disable();
        for (u32 i = 0; i < 512; i++)
            write(OCFBC_PALETTE0 + i * 4, 0);
    }

    void test_interrupts() {
        enable(8, 4, CTLR_BPP32, false);
        EXPECT_EQ(read(OCFBC_STAT), 0);
        EXPECT_FALSE(irq);

        write(OCFBC_CTLR, read(OCFBC_CTLR) | CTLR_VIE);
        wait(2 * frame);
        EXPECT_EQ(read(OCFBC_STAT), STAT_VINT);
        EXPECT_TRUE(irq);
        EXPECT_THAT(info(), HasSubstr("interrupt:   set"));

        write(OCFBC_CTLR, read(OCFBC_CTLR) & ~CTLR_VIE);
        write(OCFBC_STAT, 0);
        EXPECT_EQ(read(OCFBC_STAT), 0);
        EXPECT_FALSE(irq);
        wait(2 * frame);
        EXPECT_FALSE(irq);

        write(OCFBC_CTLR, read(OCFBC_CTLR) | CTLR_HIE);
        wait(2 * frame);
        EXPECT_EQ(read(OCFBC_STAT), STAT_HINT);
        EXPECT_TRUE(irq);

        write(OCFBC_CTLR, read(OCFBC_CTLR) & ~CTLR_HIE);
        write(OCFBC_STAT, 0);
        EXPECT_FALSE(irq);
        disable();
    }

    void test_bank_switch() {
        const u32 xres = 8, yres = 4, size = xres * yres * 4;
        fill(VMEM_A, size, 0x11);
        fill(VMEM_B, size, 0x99);

        enable(xres, yres, CTLR_BPP32, false);
        EXPECT_EQ(memcmp(fbptr, vmem.data() + VMEM_A, size), 0);

        write(OCFBC_CTLR, read(OCFBC_CTLR) | CTLR_VBSWE);
        wait(2 * frame);
        EXPECT_EQ(read(OCFBC_STAT) & STAT_AVMP, STAT_AVMP);
        EXPECT_EQ(read(OCFBC_CTLR) & CTLR_VBSWE, 0);
        EXPECT_EQ(memcmp(fbptr, vmem.data() + VMEM_B, size), 0);
        EXPECT_FALSE(irq);

        write(OCFBC_CTLR, read(OCFBC_CTLR) | CTLR_VBSWE | CTLR_VBSIE);
        wait(2 * frame);
        EXPECT_EQ(read(OCFBC_STAT) & STAT_AVMP, 0);
        EXPECT_EQ(memcmp(fbptr, vmem.data() + VMEM_A, size), 0);
        EXPECT_TRUE(irq);

        write(OCFBC_STAT, STAT_VBSINT);
        write(OCFBC_STAT, 0);
        disable();
    }

    void test_bus_error() {
        ocfbc.allow_dmi = false;
        write(OCFBC_HTIM, 7);
        write(OCFBC_VTIM, 3);
        write(OCFBC_VBARA, VMEM_SIZE);
        write(OCFBC_CTLR, CTLR_VEN | CTLR_BPP32 | CTLR_VBL8);
        wait(2 * frame);
        EXPECT_EQ(read(OCFBC_STAT), STAT_SINT);
        EXPECT_TRUE(irq);

        write(OCFBC_VBARA, VMEM_A);
        write(OCFBC_STAT, 0);
        EXPECT_FALSE(irq);
        wait(2 * frame);
        EXPECT_EQ(read(OCFBC_STAT), 0);
        EXPECT_FALSE(irq);
        disable();
    }

    void test_burst() {
        const u32 xres = 3, yres = 2, size = xres * yres * 4;
        for (u32 burst : { 0u, +CTLR_VBL2, +CTLR_VBL4, +CTLR_VBL8 }) {
            fill(VMEM_A, size + 16, 0x30 + (burst >> 7));
            enable(xres, yres, CTLR_BPP32 | burst, false);
            EXPECT_EQ(mode.size, size);
            EXPECT_EQ(memcmp(fbptr, vmem.data() + VMEM_A, size), 0);
            disable();
        }
    }
};

TEST(opencores, ocfbc) {
    ocfbc_test test("test");
    sc_core::sc_start();
}
