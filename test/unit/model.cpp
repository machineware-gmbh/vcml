/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "vcml.h"

using namespace ::testing;
using namespace ::vcml;

class my_model : public module
{
public:
    my_model(const sc_module_name& nm): module(nm) {}
    virtual ~my_model() = default;
    VCML_KIND(my_model);
};

VCML_EXPORT_MODEL(my_model, name, args) {
    EXPECT_EQ(args.size(), 3);
    EXPECT_EQ(args[0], "abc");
    EXPECT_EQ(args[1], "def");
    EXPECT_EQ(args[2], "hij");
    return new my_model(name);
}

TEST(model, create) {
    vcml::model m("m", "my_model abc def hij");
    EXPECT_STREQ(m->kind(), "vcml::my_model");
    EXPECT_STREQ(m->name(), "m");
}

TEST(model, empty) {
    vcml::model m("m", "empty");
    EXPECT_STREQ(m->kind(), "empty");
    EXPECT_STREQ(m->name(), "m");
}

TEST(mode, nonexistent) {
    EXPECT_THROW(vcml::model("m", "nothing"), std::exception);
}

TEST(mode, duplicate) {
    EXPECT_FALSE(vcml::model::define("my_model", nullptr));
}

TEST(model, list_ports) {
    {
        // force linking of uart8250.o
        vcml::serial::uart8250 dummy("dummy");
    }

    vcml::model uart("uart", "vcml::serial::uart8250");
    auto sockets = list_sockets(uart);
    EXPECT_EQ(sockets["clk"], "vcml::clk_target_socket");
    EXPECT_EQ(sockets["in"], "vcml::tlm_target_socket");
    EXPECT_EQ(sockets["irq"], "vcml::gpio_initiator_socket");
    EXPECT_EQ(sockets["rst"], "vcml::gpio_target_socket");
    EXPECT_EQ(sockets["serial_rx"], "vcml::serial_target_socket");
    EXPECT_EQ(sockets["serial_tx"], "vcml::serial_initiator_socket");
}
