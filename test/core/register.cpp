/******************************************************************************
 *                                                                            *
 * Copyright 2018 Jan Henrik Weinstock                                        *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License");            *
 * you may not use this file except in compliance with the License.           *
 * You may obtain a copy of the License at                                    *
 *                                                                            *
 *     http://www.apache.org/licenses/LICENSE-2.0                             *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 *                                                                            *
 ******************************************************************************/

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace ::testing;

#include "vcml.h"

using namespace ::vcml;

class mock_peripheral : public peripheral
{
public:
    reg<u32> test_reg_a;
    reg<u32> test_reg_b;

    MOCK_METHOD(u32, reg_read, ());
    MOCK_METHOD(void, reg_write, (u32));

    mock_peripheral(const sc_core::sc_module_name& nm =
                        sc_core::sc_gen_unique_name("mock_peripheral")):
        peripheral(nm, ENDIAN_LITTLE, 1, 10),
        test_reg_a("test_reg_a", 0x0, 0xffffffff),
        test_reg_b("test_reg_b", 0x4, 0xffffffff) {
        test_reg_b.allow_read_write();
        test_reg_b.on_read(&mock_peripheral::reg_read);
        test_reg_b.on_write(&mock_peripheral::reg_write);
        clk.stub(100 * MHz);
        rst.stub();
        handle_clock_update(0, clk.read());
    }

    unsigned int test_transport(tlm::tlm_generic_payload& tx) {
        return transport(tx, SBI_NONE, VCML_AS_DEFAULT);
    }
};

TEST(registers, read) {
    mock_peripheral mock;
    sc_core::sc_time cycle(1.0 / mock.clk, sc_core::SC_SEC);
    sc_core::sc_time& local = mock.local_time();
    tlm::tlm_generic_payload tx;

    unsigned char buffer[] = { 0xcc, 0xcc, 0xcc, 0xcc };
    unsigned char expect[] = { 0x37, 0x13, 0x00, 0x00 };

    mock.test_reg_a = 0x1337;
    local           = sc_core::SC_ZERO_TIME;
    tx_setup(tx, tlm::TLM_READ_COMMAND, 0, buffer, sizeof(buffer));

    EXPECT_EQ(mock.test_transport(tx), 4);
    EXPECT_EQ(mock.test_reg_a, 0x00001337u);
    EXPECT_EQ(mock.test_reg_b, 0xffffffffu);
    EXPECT_EQ(buffer[0], expect[0]);
    EXPECT_EQ(buffer[1], expect[1]);
    EXPECT_EQ(buffer[2], expect[2]);
    EXPECT_EQ(buffer[3], expect[3]);
    EXPECT_EQ(local, cycle * mock.read_latency);
    EXPECT_TRUE(tx.is_response_ok());
}

TEST(registers, read_callback) {
    mock_peripheral mock;
    sc_core::sc_time cycle(1.0 / mock.clk, sc_core::SC_SEC);
    sc_core::sc_time& local = mock.local_time();
    tlm::tlm_generic_payload tx;

    unsigned char buffer[] = { 0xcc, 0xcc, 0xcc, 0xcc };
    unsigned char expect[] = { 0x37, 0x13, 0x00, 0x00 };

    mock.test_reg_b = 0x1337;
    local           = sc_core::SC_ZERO_TIME;
    tx_setup(tx, tlm::TLM_READ_COMMAND, 4, buffer, sizeof(buffer));

    EXPECT_CALL(mock, reg_read()).WillOnce(Return(mock.test_reg_b.get()));
    EXPECT_EQ(mock.test_transport(tx), 4);
    EXPECT_EQ(mock.test_reg_a, 0xffffffffu);
    EXPECT_EQ(mock.test_reg_b, 0x00001337u);
    EXPECT_EQ(buffer[0], expect[0]);
    EXPECT_EQ(buffer[1], expect[1]);
    EXPECT_EQ(buffer[2], expect[2]);
    EXPECT_EQ(buffer[3], expect[3]);
    EXPECT_EQ(local, cycle * mock.read_latency);
    EXPECT_TRUE(tx.is_response_ok());
}

TEST(registers, write) {
    mock_peripheral mock;
    sc_core::sc_time cycle(1.0 / mock.clk, sc_core::SC_SEC);
    sc_core::sc_time& local = mock.local_time();
    tlm::tlm_generic_payload tx;

    unsigned char buffer[] = { 0x11, 0x22, 0x33, 0x44 };

    local = sc_core::SC_ZERO_TIME;
    tx_setup(tx, tlm::TLM_WRITE_COMMAND, 0, buffer, sizeof(buffer));

    EXPECT_EQ(mock.test_transport(tx), 4);
    EXPECT_EQ(mock.test_reg_a, 0x44332211u);
    EXPECT_EQ(mock.test_reg_b, 0xffffffffu);
    EXPECT_EQ(local, cycle * mock.write_latency);
    EXPECT_TRUE(tx.is_response_ok());
}

TEST(registers, write_callback) {
    mock_peripheral mock;
    sc_core::sc_time cycle(1.0 / mock.clk, sc_core::SC_SEC);
    sc_core::sc_time& local = mock.local_time();
    tlm::tlm_generic_payload tx;

    u32 value              = 0x98765432;
    unsigned char buffer[] = { 0x11, 0x22, 0x33, 0x44 };

    local = sc_core::SC_ZERO_TIME;
    tx_setup(tx, tlm::TLM_WRITE_COMMAND, 4, buffer, sizeof(buffer));

    EXPECT_CALL(mock, reg_write(0x44332211)).WillOnce(Invoke([&](u32 val) {
        mock.test_reg_b = value;
    }));

    EXPECT_EQ(mock.test_transport(tx), 4);
    EXPECT_EQ(mock.test_reg_a, 0xffffffff);
    EXPECT_EQ(mock.test_reg_b, value);
    EXPECT_EQ(local, cycle * mock.write_latency);
    EXPECT_TRUE(tx.is_response_ok());
}

TEST(registers, read_byte_enable) {
    mock_peripheral mock;
    sc_core::sc_time cycle(1.0 / mock.clk, sc_core::SC_SEC);
    sc_core::sc_time& local = mock.local_time();
    tlm::tlm_generic_payload tx;

    unsigned char buffer[] = { 0xcc, 0xcc, 0x00, 0x00 };
    unsigned char bebuff[] = { 0xff, 0xff, 0x00, 0x00 };
    unsigned char expect[] = { 0x37, 0x13, 0x00, 0x00 };

    mock.test_reg_a = 0x1337;
    local           = sc_core::SC_ZERO_TIME;
    tx_setup(tx, tlm::TLM_READ_COMMAND, 0, buffer, sizeof(buffer));
    tx.set_byte_enable_ptr(bebuff);
    tx.set_byte_enable_length(sizeof(bebuff));

    EXPECT_EQ(mock.test_transport(tx), 2);
    EXPECT_EQ(mock.test_reg_a, 0x00001337u);
    EXPECT_EQ(mock.test_reg_b, 0xffffffffu);
    EXPECT_EQ(buffer[0], expect[0]);
    EXPECT_EQ(buffer[1], expect[1]);
    EXPECT_EQ(buffer[2], expect[2]);
    EXPECT_EQ(buffer[3], expect[3]);
    EXPECT_EQ(local, cycle * mock.read_latency);
    EXPECT_TRUE(tx.is_response_ok());
}

TEST(registers, write_byte_enable) {
    mock_peripheral mock;
    sc_core::sc_time cycle(1.0 / mock.clk, sc_core::SC_SEC);
    sc_core::sc_time& local = mock.local_time();
    tlm::tlm_generic_payload tx;

    unsigned char buffer[] = { 0x11, 0x22, 0x33, 0x44 };
    unsigned char bebuff[] = { 0xff, 0x00, 0xff, 0x00 };

    mock.test_reg_a = 0;
    local           = sc_core::SC_ZERO_TIME;
    tx_setup(tx, tlm::TLM_WRITE_COMMAND, 0, buffer, sizeof(buffer));
    tx.set_byte_enable_ptr(bebuff);
    tx.set_byte_enable_length(sizeof(bebuff));

    EXPECT_EQ(mock.test_transport(tx), 2);
    EXPECT_EQ(mock.test_reg_a, 0x00330011u);
    EXPECT_EQ(mock.test_reg_b, 0xffffffffu);
    EXPECT_EQ(local, cycle * mock.write_latency);
    EXPECT_TRUE(tx.is_response_ok());
}

TEST(registers, permissions) {
    mock_peripheral mock;

    sc_core::sc_time cycle(1.0 / mock.clk, sc_core::SC_SEC);
    sc_core::sc_time& local = mock.local_time();

    tlm::tlm_generic_payload tx;
    unsigned char buffer[] = { 0x11, 0x22, 0x33, 0x44 };

    local = sc_core::SC_ZERO_TIME;
    mock.test_reg_b.allow_read_only();
    tx_setup(tx, tlm::TLM_WRITE_COMMAND, 4, buffer, sizeof(buffer));

    EXPECT_CALL(mock, reg_write(_)).Times(0);
    EXPECT_EQ(mock.test_transport(tx), 0);
    EXPECT_EQ(tx.get_response_status(), tlm::TLM_COMMAND_ERROR_RESPONSE);
    EXPECT_EQ(mock.test_reg_a, 0xffffffffu);
    EXPECT_EQ(mock.test_reg_b, 0xffffffffu);
    EXPECT_EQ(local, cycle * mock.write_latency);

    local = sc_core::SC_ZERO_TIME;
    mock.test_reg_b.allow_write_only();
    tx_setup(tx, tlm::TLM_READ_COMMAND, 4, buffer, sizeof(buffer));

    EXPECT_CALL(mock, reg_read()).Times(0);
    EXPECT_EQ(mock.test_transport(tx), 0);
    EXPECT_EQ(tx.get_response_status(), tlm::TLM_COMMAND_ERROR_RESPONSE);
    EXPECT_EQ(mock.test_reg_a, 0xffffffffu);
    EXPECT_EQ(mock.test_reg_b, 0xffffffffu);
    EXPECT_EQ(local, cycle * mock.read_latency);
}

TEST(registers, misaligned_accesses) {
    mock_peripheral mock;

    sc_core::sc_time cycle(1.0 / mock.clk, sc_core::SC_SEC);
    sc_core::sc_time& local = mock.local_time();

    tlm::tlm_generic_payload tx;
    unsigned char buffer[] = { 0x11, 0x22, 0x33, 0x44 };

    mock.test_reg_a = 0;
    local           = sc_core::SC_ZERO_TIME;
    tx_setup(tx, tlm::TLM_WRITE_COMMAND, 1, buffer, 2);

    EXPECT_EQ(mock.test_transport(tx), 2);
    EXPECT_EQ(mock.test_reg_a, 0x00221100u);
    EXPECT_EQ(mock.test_reg_b, 0xffffffffu);
    EXPECT_EQ(local, cycle * mock.write_latency);
    EXPECT_TRUE(tx.is_response_ok());

    local = sc_core::SC_ZERO_TIME;
    tx_setup(tx, tlm::TLM_WRITE_COMMAND, 1, buffer, 4);

    EXPECT_CALL(mock, reg_write(0xffffff44)).WillOnce(Invoke([&](u32 val) {
        mock.test_reg_b = val;
    }));

    EXPECT_EQ(mock.test_transport(tx), 4); // !
    EXPECT_EQ(mock.test_reg_a, 0x33221100u);
    EXPECT_EQ(local, cycle * mock.write_latency);
    EXPECT_TRUE(tx.is_response_ok());

    unsigned char largebuf[8] = { 0xff };
    local                     = sc_core::SC_ZERO_TIME;
    tx_setup(tx, tlm::TLM_READ_COMMAND, 0, largebuf, 8);

    EXPECT_CALL(mock, reg_read()).WillOnce(Return(mock.test_reg_b.get()));
    EXPECT_EQ(mock.test_transport(tx), 8);
    EXPECT_EQ(largebuf[0], 0x00);
    EXPECT_EQ(largebuf[1], 0x11);
    EXPECT_EQ(largebuf[2], 0x22);
    EXPECT_EQ(largebuf[3], 0x33);
    EXPECT_EQ(largebuf[4], 0x44);
    EXPECT_EQ(largebuf[5], 0xff);
    EXPECT_EQ(largebuf[6], 0xff);
    EXPECT_EQ(largebuf[7], 0xff);
    EXPECT_EQ(local, cycle * mock.read_latency);
    EXPECT_TRUE(tx.is_response_ok());
}

TEST(registers, banking) {
    mock_peripheral mock;
    mock.test_reg_a.set_banked();

    sc_core::sc_time cycle(1.0 / mock.clk, sc_core::SC_SEC);

    tlm::tlm_generic_payload tx;
    sbiext bank;
    tlm_sbi bank1, bank2;
    const u8 val1 = 0xab;
    const u8 val2 = 0xcd;
    unsigned char buffer;

    bank1.cpuid = 1;
    bank2.cpuid = 2;

    tx.set_extension(&bank);

    buffer     = val1;
    bank.cpuid = 1;
    tx_setup(tx, tlm::TLM_WRITE_COMMAND, 0, &buffer, 1);
    EXPECT_EQ(mock.transport(tx, bank1, VCML_AS_DEFAULT), 1);
    EXPECT_TRUE(tx.is_response_ok());

    buffer     = val2;
    bank.cpuid = 2;
    tx_setup(tx, tlm::TLM_WRITE_COMMAND, 0, &buffer, 1);
    EXPECT_EQ(mock.transport(tx, bank2, VCML_AS_DEFAULT), 1);
    EXPECT_TRUE(tx.is_response_ok());

    buffer     = 0x0;
    bank.cpuid = 1;
    tx_setup(tx, tlm::TLM_READ_COMMAND, 0, &buffer, 1);
    EXPECT_EQ(mock.transport(tx, bank1, VCML_AS_DEFAULT), 1);
    EXPECT_TRUE(tx.is_response_ok());
    EXPECT_EQ(buffer, val1);

    buffer     = 0x0;
    bank.cpuid = 2;
    tx_setup(tx, tlm::TLM_READ_COMMAND, 0, &buffer, 1);
    EXPECT_EQ(mock.transport(tx, bank2, VCML_AS_DEFAULT), 1);
    EXPECT_TRUE(tx.is_response_ok());
    EXPECT_EQ(buffer, val2);

    tx.clear_extension(&bank);
}

TEST(registers, endianess) {
    mock_peripheral mock;
    mock.set_big_endian();

    sc_core::sc_time cycle(1.0 / mock.clk, sc_core::SC_SEC);
    sc_core::sc_time& local = mock.local_time();

    tlm::tlm_generic_payload tx;
    u32 buffer = 0;

    mock.test_reg_a = 0x11223344;
    tx_setup(tx, tlm::TLM_READ_COMMAND, 0, &buffer, 4);
    EXPECT_EQ(mock.test_transport(tx), 4);
    EXPECT_EQ(buffer, 0x44332211);
    EXPECT_EQ(local, cycle * mock.read_latency);
    EXPECT_TRUE(tx.is_response_ok());

    buffer = 0xeeff00cc;
    local  = sc_core::SC_ZERO_TIME;
    tx_setup(tx, tlm::TLM_WRITE_COMMAND, 0, &buffer, 4);
    EXPECT_EQ(mock.test_transport(tx), 4);
    EXPECT_EQ(mock.test_reg_a, 0xcc00ffeeu);
    EXPECT_EQ(local, cycle * mock.write_latency);
    EXPECT_TRUE(tx.is_response_ok());
}

TEST(registers, operators) {
    mock_peripheral mock;

    mock.test_reg_a = 3;
    mock.test_reg_b = 3;

    EXPECT_TRUE(mock.test_reg_a == 3u);
    EXPECT_TRUE(mock.test_reg_b == 3u);

    EXPECT_FALSE(mock.test_reg_a != 3u);
    EXPECT_FALSE(mock.test_reg_b != 3u);

    EXPECT_EQ(mock.test_reg_a++, 3u);
    EXPECT_EQ(mock.test_reg_a, 4u);
    EXPECT_EQ(++mock.test_reg_a, 5u);

    EXPECT_EQ(mock.test_reg_b--, 3u);
    EXPECT_EQ(mock.test_reg_b, 2u);
    EXPECT_EQ(--mock.test_reg_b, 1u);

    EXPECT_EQ(mock.test_reg_b += 1, 2u);
    EXPECT_EQ(mock.test_reg_a -= 1, 4u);
}

enum : address_space {
    VCML_AS_TEST1 = VCML_AS_DEFAULT + 1,
    VCML_AS_TEST2 = VCML_AS_DEFAULT + 2,
};

class mock_peripheral_as : public peripheral
{
public:
    reg<u32> test_reg_a;
    reg<u32> test_reg_b;

    mock_peripheral_as(const sc_core::sc_module_name& nm =
                           sc_core::sc_gen_unique_name("mock_peripheral_as")):
        peripheral(nm, ENDIAN_LITTLE, 1, 10),
        test_reg_a(VCML_AS_TEST1, "test_reg_a", 0x0, 0xffffffff),
        test_reg_b(VCML_AS_TEST2, "test_reg_b", 0x0, 0xffffffff) {
        test_reg_b.allow_read_write();
        test_reg_b.allow_read_write();
        clk.stub(100 * MHz);
        rst.stub();
        handle_clock_update(0, clk.read());
    }

    unsigned int test_transport(tlm::tlm_generic_payload& tx,
                                address_space as) {
        return transport(tx, SBI_NONE, as);
    }
};

TEST(registers, address_spaces) {
    // reg_a and reg_b both live at 0x0, but in different address spaces
    mock_peripheral_as mock;

    tlm::tlm_generic_payload tx;
    unsigned char buffer[] = { 0x11, 0x22, 0x33, 0x44 };
    tx_setup(tx, tlm::TLM_WRITE_COMMAND, 0, buffer, sizeof(buffer));

    // writes to default address space should get lost in the void
    EXPECT_EQ(mock.test_transport(tx, VCML_AS_DEFAULT), 0);
    EXPECT_EQ(mock.test_reg_a, 0xffffffffu);
    EXPECT_EQ(mock.test_reg_b, 0xffffffffu);
    EXPECT_EQ(tx.get_response_status(), tlm::TLM_ADDRESS_ERROR_RESPONSE);
    mock.reset();
    tx_reset(tx);

    // writes to VCML_AS_TEST1 should only change reg_a
    EXPECT_EQ(mock.test_transport(tx, VCML_AS_TEST1), 4);
    EXPECT_EQ(mock.test_reg_a, 0x44332211u);
    EXPECT_EQ(mock.test_reg_b, 0xffffffffu);
    EXPECT_TRUE(tx.is_response_ok());
    mock.reset();
    tx_reset(tx);

    // writes to VCML_AS_TEST2 should only change reg_b
    EXPECT_EQ(mock.test_transport(tx, VCML_AS_TEST2), 4);
    EXPECT_EQ(mock.test_reg_a, 0xffffffffu);
    EXPECT_EQ(mock.test_reg_b, 0x44332211u);
    EXPECT_TRUE(tx.is_response_ok());
    mock.reset();
    tx_reset(tx);
}

class lambda_test : public peripheral
{
public:
    reg<u32> test_reg;
    lambda_test(const sc_core::sc_module_name& nm):
        peripheral(nm), test_reg("REG", 0) {
        test_reg.allow_read_only();
        test_reg.on_read([&]() -> u32 { return 0x42; });
    };

    virtual ~lambda_test() = default;
};

TEST(registers, lambda) {
    lambda_test test("lambda");

    u32 data = 0;
    tlm::tlm_generic_payload tx;
    tx_setup(tx, tlm::TLM_READ_COMMAND, 0, &data, sizeof(data));
    test.transport(tx, SBI_NONE, VCML_AS_DEFAULT);
    EXPECT_TRUE(tx.is_response_ok());
    EXPECT_EQ(data, 0x42);
}

class hierarchy_test : public peripheral
{
public:
    class wrapper : public sc_core::sc_module
    {
    public:
        reg<u64> test_reg;

        wrapper(const sc_core::sc_module_name& nm):
            sc_core::sc_module(nm), test_reg("test_reg", 0) {}

        virtual ~wrapper() = default;
    };

    wrapper w;

    hierarchy_test(const sc_core::sc_module_name& nm):
        peripheral(nm), w("w") {}
};

TEST(registers, hierarchy) {
    hierarchy_test h("h");
    EXPECT_STREQ(h.w.test_reg.name(), "h.w.test_reg");
    std::vector<reg_base*> regs = h.get_registers();
    ASSERT_FALSE(regs.empty());
    EXPECT_STREQ(regs[0]->name(), "h.w.test_reg");
    EXPECT_EQ(regs[0], (reg_base*)&h.w.test_reg);
}

TEST(registers, bitfields) {
    mock_peripheral mock;

    typedef field<1, 4, u32> TEST_FIELD;

    mock.test_reg_a = 0xaaaaaaaa;

    u32 val = get_field<TEST_FIELD>(mock.test_reg_a);
    EXPECT_EQ(val, 5);
    mock.test_reg_a.set_field<TEST_FIELD>(val - 1);
    EXPECT_EQ(mock.test_reg_a, 0xaaaaaaa8);
    mock.test_reg_a.set_field<TEST_FIELD>(val);
    EXPECT_EQ(mock.test_reg_a, 0xaaaaaaaa);
    mock.test_reg_a.set_field<TEST_FIELD>();
    EXPECT_EQ(mock.test_reg_a, 0xaaaaaabe);
    set_field<TEST_FIELD>(mock.test_reg_a, val);
    EXPECT_EQ(mock.test_reg_a, 0xaaaaaaaa);
}

TEST(registers, natural_alignment) {
    u32 data = 0;
    tlm_generic_payload tx;
    mock_peripheral mock;
    mock.natural_accesses_only();

    EXPECT_TRUE(mock.test_reg_a.is_natural_accesses_only());
    EXPECT_TRUE(mock.test_reg_b.is_natural_accesses_only());

    mock.test_reg_a.natural_accesses_only(false);
    mock.test_reg_b.natural_accesses_only();

    EXPECT_FALSE(mock.test_reg_a.is_natural_accesses_only());
    ASSERT_TRUE(mock.test_reg_b.is_natural_accesses_only());

    tx_setup(tx, TLM_READ_COMMAND, 4, &data, sizeof(data));
    EXPECT_EQ(mock.test_transport(tx), sizeof(data));
    EXPECT_EQ(tx.get_response_status(), TLM_OK_RESPONSE);

    tx_setup(tx, TLM_READ_COMMAND, 4, &data, sizeof(u8));
    EXPECT_EQ(mock.test_transport(tx), 0);
    EXPECT_EQ(tx.get_response_status(), TLM_COMMAND_ERROR_RESPONSE);

    tx_setup(tx, TLM_READ_COMMAND, 5, &data, sizeof(data));
    EXPECT_EQ(mock.test_transport(tx), 0);
    EXPECT_EQ(tx.get_response_status(), TLM_COMMAND_ERROR_RESPONSE);

    tx_setup(tx, TLM_READ_COMMAND, 5, &data, sizeof(u8));
    EXPECT_EQ(mock.test_transport(tx), 0);
    EXPECT_EQ(tx.get_response_status(), TLM_COMMAND_ERROR_RESPONSE);
}
