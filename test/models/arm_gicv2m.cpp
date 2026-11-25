/******************************************************************************
 *                                                                            *
 * Copyright (C) 2023 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

enum test_config : size_t {
    BASE_SPI = 46,
    NUM_SPI = 127,
};

class gicv2m_stim : public test_base
{
public:
    MOCK_METHOD(void, gpio_notify,
                (const gpio_target_socket& socket, bool state,
                 gpio_vector vector),
                (override));
    arm::gicv2m gicv2m;

    tlm_initiator_socket out;

    gpio_target_array<> in;

    gicv2m_stim(const sc_module_name& nm):
        test_base(nm), gicv2m("gicv2m"), out("out"), in("in") {
        clk.bind(gicv2m.clk);
        rst.bind(gicv2m.rst);

        out.bind(gicv2m.in);

        for (size_t i = BASE_SPI; i < BASE_SPI + NUM_SPI; i++)
            gicv2m.out[i].bind(in[i]);

        add_test("strings", &gicv2m_stim::test_strings);
        add_test("typer", &gicv2m_stim::test_typer);
        add_test("setspi", &gicv2m_stim::test_setspi);
        add_test("iidr", &gicv2m_stim::test_iidr);
    }

    enum addresses : size_t {
        TYPER_ADDR = 0x008,
        SETSPI_ADDR = 0x040,
        IIDR_ADDR = 0xfcc,
    };

    void test_strings() {
        EXPECT_STREQ(gicv2m.kind(), "vcml::arm::gicv2m");
        EXPECT_STREQ(gicv2m.version(), VCML_VERSION_STRING);
    }

    void test_typer() {
        u32 val = ~0;
        EXPECT_OK(out.readw(TYPER_ADDR, val)) << "failed to read TYPER reg";
        EXPECT_EQ(val, BASE_SPI << 16 | NUM_SPI);
    }

    void test_setspi() {
        Sequence s;
        for (size_t i = BASE_SPI; i < BASE_SPI + NUM_SPI; i++) {
            EXPECT_CALL(*this, gpio_notify(Ref(in[i]), true, GPIO_NO_VECTOR))
                .InSequence(s);
            EXPECT_CALL(*this, gpio_notify(Ref(in[i]), false, GPIO_NO_VECTOR))
                .InSequence(s);
            EXPECT_OK(out.writew(SETSPI_ADDR, i))
                << "failed to write SETSPI reg";
        }

        EXPECT_CALL(*this, gpio_notify(_, _, _)).Times(0).InSequence(s);
        EXPECT_OK(out.writew(SETSPI_ADDR, BASE_SPI + NUM_SPI))
            << "failed to write SETSPI reg";
    }

    void test_iidr() {
        u32 val = ~0;
        EXPECT_OK(out.readw(IIDR_ADDR, val)) << "failed to read IIDR reg";
        EXPECT_EQ(val, 'M' << 20);
    }
};

TEST(gicv2m, gicv2m) {
    vcml::broker broker("test");
    broker.define("gicv2m_stim.gicv2m.base_spi", BASE_SPI);
    broker.define("gicv2m_stim.gicv2m.num_spi", NUM_SPI);

    gicv2m_stim stim("gicv2m_stim");
    sc_core::sc_start();
}
