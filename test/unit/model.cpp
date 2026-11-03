/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "vcml.h"

using namespace ::testing;
using namespace ::vcml;

class my_submodule : public module
{
public:
    property<int> nested;
    my_submodule(const sc_module_name& nm): module(nm), nested("nested", -7) {}
    virtual ~my_submodule() = default;
    VCML_KIND(my_submodule);
};

class my_model : public module
{
public:
    property<size_t> num_things;
    property<string> label;
    property<u32, 3> triple;
    property<vector<int>> list;
    my_submodule sub;
    my_model(const sc_module_name& nm):
        module(nm),
        num_things("num_things", 42),
        label("label", "abc"),
        triple("triple", { 1, 2, 3 }),
        list("list", { 4, 5 }),
        sub("sub") {}
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

TEST(model, get_property) {
    vcml::model m("m", "my_model abc def hij");
    EXPECT_EQ(m.get_property<size_t>("num_things"), 42);
    EXPECT_EQ(m.get_property<string>("label"), "abc");
    EXPECT_EQ(m.get_property<int>("sub.nested"), -7);
    EXPECT_THROW(m.get_property<string>("sub.nested"), std::exception);
    EXPECT_THROW(m.get_property<int>("nosub.nested"), std::exception);
    EXPECT_THROW(m.get_property<int>("nothing"), std::exception);
    EXPECT_THROW(m.get_property<int>("sub.nothing"), std::exception);
    EXPECT_THROW(m.get_property<int>("num_things"), std::exception);
}

TEST(model, get_array_property) {
    vcml::model m("m", "my_model abc def hij");
    EXPECT_EQ((m.get_property<u32, 3>("triple")), 1);
    EXPECT_EQ((m.get_property<u32, 3>("triple", 2)), 3);
    EXPECT_THROW((m.get_property<u32, 3>("triple", 3)), std::exception);
    EXPECT_THROW(m.get_property<u32>("triple"), std::exception);
    EXPECT_EQ((m.get_property_or_default<u32, 3>("triple", 9, 1)), 2);
    EXPECT_EQ((m.get_property_or_default<u32, 3>("nothing", 9, 1)), 9);
    EXPECT_EQ(m.get_property<vector<int>>("list"), vector<int>({ 4, 5 }));
}

TEST(model, get_property_or_default) {
    vcml::model m("m", "my_model abc def hij");
    EXPECT_EQ(m.get_property_or_default<size_t>("num_things", 1), 42);
    EXPECT_EQ(m.get_property_or_default<int>("sub.nested", 1), -7);
    EXPECT_EQ(m.get_property_or_default<int>("nothing", 1), 1);
    EXPECT_EQ(m.get_property_or_default<string>("sub.nothing", "x"), "x");
    EXPECT_THROW(m.get_property_or_default<int>("label", 1), std::exception);
}

TEST(model, empty) {
    vcml::model m("m", "null");
    EXPECT_STREQ(m->kind(), "null");
    EXPECT_STREQ(m->name(), "m");

    vcml::model m2("m2", "");
    EXPECT_STREQ(m2->kind(), "null");
    EXPECT_STREQ(m2->name(), "m2");
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
