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

TEST(model, kinds) {
    const char* types[] = {
        "vcml::can::bus",       "vcml::ethernet::network",
        "vcml::generic::hwrng", "vcml::i2c::oci2c",
        "vcml::riscv::clint",   "vcml::riscv::plic",
        "vcml::riscv::simdev",  "vcml::serial::uart16550",
        "vcml::usb::xhcipci",   "vcml::ui::console",
    };

    for (const char* type : types) {
        vcml::model m(sc_gen_unique_name("model"), type);
        EXPECT_STREQ(m->kind(), type);
    }

    vcml::model uart("uart", "vcml::serial::uart16550");
    module& mod = uart;
    auto* base = dynamic_cast<vcml::serial::uart*>(&mod);
    ASSERT_NE(base, nullptr);
    EXPECT_STREQ(base->vcml::serial::uart::kind(), "vcml::serial::uart");

    vcml::model xhci("xhci", "vcml::usb::xhcipci");
    module& pci = xhci;
    EXPECT_THAT(kinds_of_children(pci), Contains("vcml::pci::endpoint"));

    const std::pair<const char*, const char*> gates[] = {
        { "vcml::generic::not_gate", "vcml::gate_not" },
        { "vcml::generic::and_gate", "vcml::gate_and" },
        { "vcml::generic::or_gate", "vcml::gate_or" },
        { "vcml::generic::nand_gate", "vcml::gate_nand" },
        { "vcml::generic::nor_gate", "vcml::gate_nor" },
        { "vcml::generic::xor_gate", "vcml::gate_xor" },
    };

    for (const auto& [type, kind] : gates) {
        vcml::model gate(sc_gen_unique_name("gate"), type);
        EXPECT_STREQ(gate->kind(), kind);
    }

    vcml::model keyboard("keyboard", "vcml::usb::keyboard");
    module& kbd = keyboard;
    auto* usbdev = dynamic_cast<vcml::usb::device*>(&kbd);
    ASSERT_NE(usbdev, nullptr);
    EXPECT_STREQ(usbdev->vcml::usb::device::kind(), "vcml::usb::device");
}
