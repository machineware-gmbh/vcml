/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

class test_processor : public processor
{
public:
    size_t cycles;

    test_processor(const sc_module_name& nm):
        processor(nm, "riscv"), cycles() {
        define_cpureg_rw(0, "zero", 4);
        define_cpureg_rw(1, "ra", 4);
        define_cpureg_rw(2, "sp", 4);
        define_cpureg_rw(3, "gp", 4);
        define_cpureg_rw(4, "tp", 4);
        define_cpureg_rw(5, "t0", 4);
        define_cpureg_rw(6, "t1", 4);
        define_cpureg_rw(7, "t2", 4);
        define_cpureg_rw(8, "s0", 4);
        define_cpureg_rw(9, "s1", 4);
        define_cpureg_rw(10, "a0", 4);
        define_cpureg_rw(11, "a1", 4);
        define_cpureg_rw(12, "a2", 4);
        define_cpureg_rw(13, "a3", 4);
        define_cpureg_rw(14, "a4", 4);
        define_cpureg_rw(15, "a5", 4);
        define_cpureg_rw(16, "a6", 4);
        define_cpureg_rw(17, "a7", 4);
        define_cpureg_rw(18, "s2", 4);
        define_cpureg_rw(19, "s3", 4);
        define_cpureg_rw(20, "s4", 4);
        define_cpureg_rw(21, "s5", 4);
        define_cpureg_rw(22, "s6", 4);
        define_cpureg_rw(23, "s7", 4);
        define_cpureg_rw(24, "s8", 4);
        define_cpureg_rw(25, "s9", 4);
        define_cpureg_rw(26, "s10", 4);
        define_cpureg_rw(27, "s11", 4);
        define_cpureg_rw(28, "t3", 4);
        define_cpureg_rw(29, "t4", 4);
        define_cpureg_rw(30, "t5", 4);
        define_cpureg_rw(31, "t6", 4);
        define_cpureg_rw(32, "pc", 4);
    }

    virtual void simulate(size_t ninsn) override { cycles += ninsn; }
    virtual u64 cycle_count() const override { return cycles; }
    virtual bool read_reg_dbg(size_t regno, void* buf, size_t len) override {
        memset(buf, 0, len);
        return true;
    }
    virtual bool write_reg_dbg(size_t regno, const void* buf,
                               size_t len) override {
        return true;
    }
};

class gdbserver_test : public test_base
{
public:
    test_processor cpu0;
    test_processor cpu1;
    test_processor cpu2;
    test_processor cpu3;

    model gdb;

    gdbserver_test(const sc_module_name& nm):
        test_base(nm),
        cpu0("cpu0"),
        cpu1("cpu1"),
        cpu2("cpu2"),
        cpu3("cpu3"),
        gdb("gdb",
            "vcml::meta::gdbserver test.cpu0 test.cpu1 test.cpu2 test.cpu3") {
        rst.bind(cpu0.rst);
        clk.bind(cpu0.clk);
        rst.bind(cpu1.rst);
        clk.bind(cpu1.clk);
        rst.bind(cpu2.rst);
        clk.bind(cpu2.clk);
        rst.bind(cpu3.rst);
        clk.bind(cpu3.clk);
        cpu0.insn.stub();
        cpu0.data.stub();
        cpu1.insn.stub();
        cpu1.data.stub();
        cpu2.insn.stub();
        cpu2.data.stub();
        cpu3.insn.stub();
        cpu3.data.stub();

        add_test("strings", &gdbserver_test::test_strings);
        add_test("targets", &gdbserver_test::test_targets);
        add_test("properties", &gdbserver_test::test_properties);
    }

    void test_strings() {
        EXPECT_STREQ(gdb->kind(), "vcml::meta::gdbserver");
        EXPECT_STREQ(gdb->version(), VCML_VERSION_STRING);
    }

    void test_targets() {
        module& m = gdb;
        auto* model = dynamic_cast<vcml::meta::gdbserver*>(&m);
        ASSERT_NE(model, nullptr);
        EXPECT_FALSE(model->targets().empty());
        ASSERT_EQ(model->targets().size(), 4);
        EXPECT_STREQ(model->targets()[0]->target_name(), "test.cpu0");
        EXPECT_STREQ(model->targets()[1]->target_name(), "test.cpu1");
        EXPECT_STREQ(model->targets()[2]->target_name(), "test.cpu2");
        EXPECT_STREQ(model->targets()[3]->target_name(), "test.cpu3");
    }

    template <typename T>
    property<T>& find_property(const string& full_name) {
        auto* attr = vcml::find_attribute(full_name);
        VCML_ERROR_ON(!attr, "property '%s' not found", full_name.c_str());
        auto* prop = dynamic_cast<property<T>*>(attr);
        VCML_ERROR_ON(!prop, "property '%s' invalid", full_name.c_str());
        return *prop;
    }

    void test_properties() {
        auto& gdb_host = find_property<string>("test.gdb.host");
        auto& gdb_port = find_property<int>("test.gdb.port");
        auto& gdb_wait = find_property<bool>("test.gdb.wait");
        auto& gdb_echo = find_property<bool>("test.gdb.echo");

        EXPECT_EQ(gdb_host.get(), "localhost");
        EXPECT_NE(gdb_port, 0u);
        EXPECT_FALSE(gdb_wait);
        EXPECT_FALSE(gdb_echo);
    }
};

TEST(meta, gdbserver) {
    gdbserver_test test("test");
    sc_core::sc_start();
}
