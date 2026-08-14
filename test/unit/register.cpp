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
        handle_clock_update(0, clk.get_hz());
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
    local = sc_core::SC_ZERO_TIME;
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
    local = sc_core::SC_ZERO_TIME;
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

    u32 value = 0x98765432;
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
    local = sc_core::SC_ZERO_TIME;
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
    local = sc_core::SC_ZERO_TIME;
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
    EXPECT_TRUE(mock.test_reg_b.is_read_only());
    EXPECT_FALSE(mock.test_reg_b.is_write_only());

    local = sc_core::SC_ZERO_TIME;
    mock.test_reg_b.allow_write_only();
    tx_setup(tx, tlm::TLM_READ_COMMAND, 4, buffer, sizeof(buffer));

    EXPECT_CALL(mock, reg_read()).Times(0);
    EXPECT_EQ(mock.test_transport(tx), 0);
    EXPECT_EQ(tx.get_response_status(), tlm::TLM_COMMAND_ERROR_RESPONSE);
    EXPECT_EQ(mock.test_reg_a, 0xffffffffu);
    EXPECT_EQ(mock.test_reg_b, 0xffffffffu);
    EXPECT_EQ(local, cycle * mock.read_latency);
    EXPECT_FALSE(mock.test_reg_b.is_read_only());
    EXPECT_TRUE(mock.test_reg_b.is_write_only());

    local = sc_core::SC_ZERO_TIME;
    mock.test_reg_b.allow_read_ignore_write();
    tx_setup(tx, tlm::TLM_WRITE_COMMAND, 4, buffer, sizeof(buffer));

    EXPECT_CALL(mock, reg_write(_)).Times(0);
    EXPECT_EQ(mock.test_transport(tx), 4);
    EXPECT_EQ(tx.get_response_status(), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(mock.test_reg_a, 0xffffffffu);
    EXPECT_EQ(mock.test_reg_b, 0xffffffffu);
    EXPECT_EQ(local, cycle * mock.write_latency);
    EXPECT_FALSE(mock.test_reg_b.is_read_only());
    EXPECT_FALSE(mock.test_reg_b.is_write_only());
}

TEST(registers, secure) {
    mock_peripheral mock;

    sc_core::sc_time cycle(1.0 / mock.clk, sc_core::SC_SEC);
    sc_core::sc_time& local = mock.local_time();

    tlm::tlm_generic_payload tx;
    u8 buffer[] = { 0x11, 0x22, 0x33, 0x44 };

    local = sc_core::SC_ZERO_TIME;
    mock.test_reg_b.set_secure();
    tx_setup(tx, tlm::TLM_WRITE_COMMAND, 4, buffer, sizeof(buffer));

    EXPECT_CALL(mock, reg_write(_)).Times(0);
    EXPECT_EQ(mock.test_transport(tx), 0);
    EXPECT_EQ(tx.get_response_status(), tlm::TLM_COMMAND_ERROR_RESPONSE);
    EXPECT_EQ(local, cycle * mock.write_latency);

    local = sc_core::SC_ZERO_TIME;
    tx_setup(tx, tlm::TLM_WRITE_COMMAND, 4, buffer, sizeof(buffer));

    EXPECT_CALL(mock, reg_write(_));
    EXPECT_EQ(mock.transport(tx, SBI_SECURE, VCML_AS_DEFAULT), 4);
    EXPECT_EQ(tx.get_response_status(), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(local, cycle * mock.write_latency);
}

TEST(registers, privilege) {
    mock_peripheral mock;

    sc_core::sc_time cycle(1.0 / mock.clk, sc_core::SC_SEC);
    sc_core::sc_time& local = mock.local_time();

    tlm::tlm_generic_payload tx;
    u8 buffer[] = { 0x11, 0x22, 0x33, 0x44 };

    local = sc_core::SC_ZERO_TIME;
    mock.test_reg_b.set_privilege(1);
    tx_setup(tx, tlm::TLM_WRITE_COMMAND, 4, buffer, sizeof(buffer));

    EXPECT_CALL(mock, reg_write(_)).Times(0);
    EXPECT_EQ(mock.test_transport(tx), 0);
    EXPECT_EQ(tx.get_response_status(), tlm::TLM_COMMAND_ERROR_RESPONSE);
    EXPECT_EQ(local, cycle * mock.write_latency);

    tlm_sbi sbi = sbi_privilege(1);
    local = sc_core::SC_ZERO_TIME;
    tx_setup(tx, tlm::TLM_WRITE_COMMAND, 4, buffer, sizeof(buffer));

    EXPECT_CALL(mock, reg_write(_));
    EXPECT_EQ(mock.transport(tx, sbi, VCML_AS_DEFAULT), 4);
    EXPECT_EQ(tx.get_response_status(), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(local, cycle * mock.write_latency);
}

TEST(registers, misaligned_accesses) {
    mock_peripheral mock;

    sc_core::sc_time cycle(1.0 / mock.clk, sc_core::SC_SEC);
    sc_core::sc_time& local = mock.local_time();

    tlm::tlm_generic_payload tx;
    unsigned char buffer[] = { 0x11, 0x22, 0x33, 0x44 };

    mock.test_reg_a = 0;
    local = sc_core::SC_ZERO_TIME;
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
    local = sc_core::SC_ZERO_TIME;
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
    EXPECT_TRUE(mock.test_reg_a.is_banked());

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

    buffer = val1;
    bank.cpuid = 1;
    tx_setup(tx, tlm::TLM_WRITE_COMMAND, 0, &buffer, 1);
    EXPECT_EQ(mock.transport(tx, bank1, VCML_AS_DEFAULT), 1);
    EXPECT_TRUE(tx.is_response_ok());

    buffer = val2;
    bank.cpuid = 2;
    tx_setup(tx, tlm::TLM_WRITE_COMMAND, 0, &buffer, 1);
    EXPECT_EQ(mock.transport(tx, bank2, VCML_AS_DEFAULT), 1);
    EXPECT_TRUE(tx.is_response_ok());

    buffer = 0x0;
    bank.cpuid = 1;
    tx_setup(tx, tlm::TLM_READ_COMMAND, 0, &buffer, 1);
    EXPECT_EQ(mock.transport(tx, bank1, VCML_AS_DEFAULT), 1);
    EXPECT_TRUE(tx.is_response_ok());
    EXPECT_EQ(buffer, val1);

    buffer = 0x0;
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
    local = sc_core::SC_ZERO_TIME;
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

    EXPECT_FALSE(mock.test_reg_a < 3u);
    EXPECT_TRUE(mock.test_reg_b < 4u);

    EXPECT_FALSE(mock.test_reg_a > 3u);
    EXPECT_TRUE(mock.test_reg_b > 2u);

    EXPECT_TRUE(mock.test_reg_a <= 3u);
    EXPECT_FALSE(mock.test_reg_b <= 2u);

    EXPECT_TRUE(mock.test_reg_a >= 3u);
    EXPECT_FALSE(mock.test_reg_b >= 4u);

    EXPECT_EQ(mock.test_reg_a++, 3u);
    EXPECT_EQ(mock.test_reg_a, 4u);
    EXPECT_EQ(++mock.test_reg_a, 5u);

    EXPECT_EQ(mock.test_reg_b--, 3u);
    EXPECT_EQ(mock.test_reg_b, 2u);
    EXPECT_EQ(--mock.test_reg_b, 1u);

    EXPECT_EQ(mock.test_reg_b += 1, 2u);
    EXPECT_EQ(mock.test_reg_a -= 1, 4u);

    EXPECT_EQ(mock.test_reg_a ^= 1, 5u);
    EXPECT_EQ(mock.test_reg_b ^= 2, 0u);

    EXPECT_EQ(mock.test_reg_a |= 1, 5u);
    EXPECT_EQ(mock.test_reg_b |= 4, 4u);

    EXPECT_EQ(mock.test_reg_a &= 1, 1u);
    EXPECT_EQ(mock.test_reg_b &= 2, 0u);
}

TEST(registers, range) {
    mock_peripheral mock;

    EXPECT_EQ(mock.test_reg_a.get_range(), range(0, 3));
    EXPECT_EQ(mock.test_reg_b.get_range(), range(4, 7));
}

TEST(registers, get_address) {
    mock_peripheral mock;

    EXPECT_EQ(mock.test_reg_a.get_address(), 0x0);
    EXPECT_EQ(mock.test_reg_b.get_address(), 0x4);
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
        handle_clock_update(0, clk.get_hz());
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
        reg<u64> reg_void;
        reg<u64> reg_dbg;
        reg<u64> reg_tag;
        reg<u64> reg_tag_dbg;

        u64 read_void() { return reg_void; }
        u64 read_dbg(bool) { return reg_dbg; }
        u64 read_tag(size_t) { return reg_tag; }
        u64 read_tag_dbg(size_t, bool) { return reg_tag_dbg; }

        void write_void(u64 val) { reg_void = val; }
        void write_dbg(u64 val, bool) { reg_dbg = val; }
        void write_tag(u64 val, size_t) { reg_tag = val; }
        void write_tag_dbg(u64 val, size_t, bool) { reg_tag_dbg = val; }

        wrapper(const sc_core::sc_module_name& nm):
            sc_core::sc_module(nm),
            reg_void("reg_void", 0x00),
            reg_dbg("reg_dbg", 0x08),
            reg_tag("reg_tag", 0x10),
            reg_tag_dbg("reg_tag_dbg", 0x18) {
            reg_void.allow_read_write();
            reg_void.on_read(&wrapper::read_void);
            reg_void.on_write(&wrapper::write_void);
            reg_dbg.allow_read_write();
            reg_dbg.on_read(&wrapper::read_dbg);
            reg_dbg.on_write(&wrapper::write_dbg);
            reg_tag.allow_read_write();
            reg_tag.on_read(&wrapper::read_tag);
            reg_tag.on_write(&wrapper::write_tag);
            reg_tag_dbg.allow_read_write();
            reg_tag_dbg.on_read(&wrapper::read_tag_dbg);
            reg_tag_dbg.on_write(&wrapper::write_tag_dbg);
        }

        virtual ~wrapper() = default;
    };

    wrapper w;

    hierarchy_test(const sc_core::sc_module_name& nm):
        peripheral(nm), w("w") {}
};

TEST(registers, hierarchy) {
    hierarchy_test h("h");
    EXPECT_STREQ(h.w.reg_void.name(), "h.w.reg_void");
    std::vector<reg_base*> regs = h.get_registers();
    ASSERT_FALSE(regs.empty());
}

TEST(registers, hierarchy_search_callback) {
    hierarchy_test h("h_search");

    tlm::tlm_generic_payload tx;
    u64 data;

    data = 0x11;
    tx_setup(tx, tlm::TLM_WRITE_COMMAND, 0x00, &data, sizeof(data));
    EXPECT_EQ(h.transport(tx, SBI_NONE, VCML_AS_DEFAULT), sizeof(data));
    data = 0;
    tx_setup(tx, tlm::TLM_READ_COMMAND, 0x00, &data, sizeof(data));
    EXPECT_EQ(h.transport(tx, SBI_NONE, VCML_AS_DEFAULT), sizeof(data));
    EXPECT_EQ(data, 0x11u);

    data = 0x22;
    tx_setup(tx, tlm::TLM_WRITE_COMMAND, 0x08, &data, sizeof(data));
    EXPECT_EQ(h.transport(tx, SBI_NONE, VCML_AS_DEFAULT), sizeof(data));
    data = 0;
    tx_setup(tx, tlm::TLM_READ_COMMAND, 0x08, &data, sizeof(data));
    EXPECT_EQ(h.transport(tx, SBI_NONE, VCML_AS_DEFAULT), sizeof(data));
    EXPECT_EQ(data, 0x22u);

    data = 0x33;
    tx_setup(tx, tlm::TLM_WRITE_COMMAND, 0x10, &data, sizeof(data));
    EXPECT_EQ(h.transport(tx, SBI_NONE, VCML_AS_DEFAULT), sizeof(data));
    data = 0;
    tx_setup(tx, tlm::TLM_READ_COMMAND, 0x10, &data, sizeof(data));
    EXPECT_EQ(h.transport(tx, SBI_NONE, VCML_AS_DEFAULT), sizeof(data));
    EXPECT_EQ(data, 0x33u);

    data = 0x44;
    tx_setup(tx, tlm::TLM_WRITE_COMMAND, 0x18, &data, sizeof(data));
    EXPECT_EQ(h.transport(tx, SBI_NONE, VCML_AS_DEFAULT), sizeof(data));
    data = 0;
    tx_setup(tx, tlm::TLM_READ_COMMAND, 0x18, &data, sizeof(data));
    EXPECT_EQ(h.transport(tx, SBI_NONE, VCML_AS_DEFAULT), sizeof(data));
    EXPECT_EQ(data, 0x44u);
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
    EXPECT_CALL(mock, reg_read());
    EXPECT_EQ(mock.test_transport(tx), sizeof(data));
    EXPECT_EQ(tx.get_response_status(), TLM_OK_RESPONSE);

    tx_setup(tx, TLM_READ_COMMAND, 4, &data, sizeof(u8));
    EXPECT_CALL(mock, reg_read()).Times(0);
    EXPECT_EQ(mock.test_transport(tx), 0);
    EXPECT_EQ(tx.get_response_status(), TLM_COMMAND_ERROR_RESPONSE);

    tx_setup(tx, TLM_READ_COMMAND, 5, &data, sizeof(data));
    EXPECT_CALL(mock, reg_read()).Times(0);
    EXPECT_EQ(mock.test_transport(tx), 0);
    EXPECT_EQ(tx.get_response_status(), TLM_COMMAND_ERROR_RESPONSE);

    tx_setup(tx, TLM_READ_COMMAND, 5, &data, sizeof(u8));
    EXPECT_CALL(mock, reg_read()).Times(0);
    EXPECT_EQ(mock.test_transport(tx), 0);
    EXPECT_EQ(tx.get_response_status(), TLM_COMMAND_ERROR_RESPONSE);
}

class mock_peripheral_mask : public peripheral
{
public:
    reg<u32> test_reg;
    reg<u32, 4> array_reg;
    reg<u32, 2> scalar_masked_reg;

    mock_peripheral_mask(const sc_module_name& nm):
        peripheral(nm),
        test_reg("test_reg", 0x0),
        array_reg("array_reg", 0x10, { 1, 2, 4, 8 }),
        scalar_masked_reg("scalar_masked_reg", 0x20, { 0xaa00u, 0xbbu }) {
        test_reg.allow_read_write();
        array_reg.allow_read_write();
        scalar_masked_reg.allow_read_write();
        test_reg.on_write_mask(0x10101010);
        array_reg.on_write_mask({ { 1, 2, 4, 8 } });
        scalar_masked_reg.on_write_mask(0xffu);
        clk.stub(100 * MHz);
        rst.stub();
    }
};

TEST(registers, masking) {
    mock_peripheral_mask mock("masking");

    u32 data = ~0u;
    tlm_generic_payload tx;

    tx_setup(tx, TLM_WRITE_COMMAND, 0, &data, sizeof(data));
    EXPECT_EQ(mock.transport(tx, SBI_NONE, VCML_AS_DEFAULT), 4);
    EXPECT_EQ(mock.test_reg, 0x10101010u);

    tx_setup(tx, TLM_READ_COMMAND, 0x10, &data, sizeof(data));
    EXPECT_EQ(mock.transport(tx, SBI_NONE, VCML_AS_DEFAULT), 4);
    EXPECT_EQ(data, 1u);

    tx_setup(tx, TLM_READ_COMMAND, 0x14, &data, sizeof(data));
    EXPECT_EQ(mock.transport(tx, SBI_NONE, VCML_AS_DEFAULT), 4);
    EXPECT_EQ(data, 2u);

    tx_setup(tx, TLM_READ_COMMAND, 0x18, &data, sizeof(data));
    EXPECT_EQ(mock.transport(tx, SBI_NONE, VCML_AS_DEFAULT), 4);
    EXPECT_EQ(data, 4u);

    tx_setup(tx, TLM_READ_COMMAND, 0x1c, &data, sizeof(data));
    EXPECT_EQ(mock.transport(tx, SBI_NONE, VCML_AS_DEFAULT), 4);
    EXPECT_EQ(data, 8u);

    data = ~0u;
    tx_setup(tx, TLM_WRITE_COMMAND, 0x10, &data, sizeof(data));
    EXPECT_EQ(mock.transport(tx, SBI_NONE, VCML_AS_DEFAULT), 4);
    EXPECT_EQ(mock.array_reg[0], 1);

    tx_setup(tx, TLM_WRITE_COMMAND, 0x14, &data, sizeof(data));
    EXPECT_EQ(mock.transport(tx, SBI_NONE, VCML_AS_DEFAULT), 4);
    EXPECT_EQ(mock.array_reg[1], 2);

    tx_setup(tx, TLM_WRITE_COMMAND, 0x18, &data, sizeof(data));
    EXPECT_EQ(mock.transport(tx, SBI_NONE, VCML_AS_DEFAULT), 4);
    EXPECT_EQ(mock.array_reg[2], 4);

    tx_setup(tx, TLM_WRITE_COMMAND, 0x1c, &data, sizeof(data));
    EXPECT_EQ(mock.transport(tx, SBI_NONE, VCML_AS_DEFAULT), 4);
    EXPECT_EQ(mock.array_reg[3], 8);
}

TEST(registers, write_mask_scalar_array) {
    mock_peripheral_mask mock("write_mask_scalar_array");

    u32 val = 0xffu;

    mock.scalar_masked_reg.do_write(0, 0, sizeof(u32), &val, false);
    EXPECT_EQ(mock.scalar_masked_reg[0], 0xaaffu);
    EXPECT_EQ(mock.scalar_masked_reg[1], 0x00bbu);

    mock.scalar_masked_reg.do_write(1, 0, sizeof(u32), &val, false);
    EXPECT_EQ(mock.scalar_masked_reg[0], 0xaaffu);
    EXPECT_EQ(mock.scalar_masked_reg[1], 0x00ffu);
}

class mock_peripheral_mask_tagged : public peripheral
{
public:
    reg<u32> tagged_reg;

    mock_peripheral_mask_tagged(const sc_module_name& nm):
        peripheral(nm), tagged_reg("tagged_reg", 0x0, 0xdeadbeef) {
        tagged_reg.allow_read_write();
        tagged_reg.tag = 42;
        tagged_reg.on_write_mask(0x0000ffffu);
        clk.stub(100 * MHz);
        rst.stub();
    }
};

TEST(registers, write_mask_nonzero_tag) {
    mock_peripheral_mask_tagged mock("write_mask_nonzero_tag");

    u32 val = 0xffffffffu;
    mock.tagged_reg.do_write(0, 0, sizeof(u32), &val, false);
    EXPECT_EQ(mock.tagged_reg, 0xdeadffffu);
}

class test_peripheral_sockets : public peripheral
{
public:
    tlm_target_socket in_a;
    tlm_target_socket in_b;

    reg<u32> test_reg_a;
    reg<u32> test_reg_b;
    reg<u32, 2> test_reg_array;

    test_peripheral_sockets(
        const sc_module_name& nm = sc_gen_unique_name("peripheral_sockets")):
        peripheral(nm, ENDIAN_LITTLE, 1, 10),
        in_a("in_a", VCML_AS_TEST1),
        in_b("in_b", VCML_AS_TEST2),
        test_reg_a(in_a, "test_reg_a", 0x0, 0xffffffff),
        test_reg_b(in_b, "test_reg_b", 0x0, 0xffffffff),
        test_reg_array(in_a, "test_reg_array", 0x4, { 0x11u, 0x22u }) {
        test_reg_b.allow_read_write();
        test_reg_b.allow_read_write();
        clk.stub(100 * MHz);
        rst.stub();
        handle_clock_update(0, clk.get_hz());
    }

    unsigned int test_transport(tlm::tlm_generic_payload& tx,
                                address_space as) {
        return transport(tx, SBI_NONE, as);
    }
};

TEST(registers, socket_address_spaces) {
    // reg_a and reg_b both live at 0x0, but in different address spaces
    test_peripheral_sockets mock;

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

    EXPECT_EQ(mock.test_reg_array[0], 0x11u);
    EXPECT_EQ(mock.test_reg_array[1], 0x22u);
    tx.set_address(0x04);
    EXPECT_EQ(mock.test_transport(tx, VCML_AS_TEST1), 4);
    EXPECT_EQ(mock.test_reg_array[0], 0x44332211u);
    EXPECT_EQ(mock.test_reg_array[1], 0x22u);
    EXPECT_TRUE(tx.is_response_ok());
    mock.reset();
    tx_reset(tx);

    EXPECT_EQ(mock.test_reg_array[0], 0x11u);
    EXPECT_EQ(mock.test_reg_array[1], 0x22u);
    tx.set_address(0x08);
    EXPECT_EQ(mock.test_transport(tx, VCML_AS_TEST1), 4);
    EXPECT_EQ(mock.test_reg_array[0], 0x11u);
    EXPECT_EQ(mock.test_reg_array[1], 0x44332211u);
    EXPECT_TRUE(tx.is_response_ok());
    mock.reset();
    tx_reset(tx);
}

TEST(registers, peripheral_cmd_mmap) {
    mock_peripheral mock;
    mock.execute("mmap", std::cout);
    std::cout << std::endl;
    mock.execute("mmap", { "0" }, std::cout);
    std::cout << std::endl;
    mock.execute("mmap", { "111" }, std::cout);
    std::cout << std::endl;
}

class mock_peripheral_minmax : public peripheral
{
public:
    reg<u32> test_reg;

    mock_peripheral_minmax(const sc_module_name& nm):
        peripheral(nm), test_reg("test_reg", 0x0) {
        test_reg.allow_read_write();
        clk.stub(100 * MHz);
        rst.stub();
        set_access_size(2, 4);
        aligned_accesses_only();
    }
};

TEST(registers, minmaxsize) {
    mock_peripheral_minmax mock("minmax");

    EXPECT_EQ(mock.test_reg.get_min_access_size(), 2);
    EXPECT_EQ(mock.test_reg.get_max_access_size(), 4);
    EXPECT_TRUE(mock.test_reg.is_aligned_accesses_only());
    EXPECT_FALSE(mock.test_reg.is_natural_accesses_only());

    u32 data;
    tlm_generic_payload tx;

    data = 0x12345678;
    tx_setup(tx, TLM_WRITE_COMMAND, 0, &data, sizeof(data));
    EXPECT_EQ(mock.transport(tx, SBI_NONE, VCML_AS_DEFAULT), 4);
    EXPECT_EQ(mock.test_reg, data);

    mock.test_reg = 0xffffffff;
    tx_setup(tx, TLM_WRITE_COMMAND, 0x1, &data, sizeof(data));
    EXPECT_EQ(mock.transport(tx, SBI_NONE, VCML_AS_DEFAULT), 0);
    EXPECT_TRUE(failed(tx));
    EXPECT_EQ(mock.test_reg, 0xffffffffu);

    data = 0;
    mock.test_reg = 0xaabbccdd;
    tx_setup(tx, TLM_READ_COMMAND, 0x2, &data, 2);
    EXPECT_EQ(mock.transport(tx, SBI_NONE, VCML_AS_DEFAULT), 2);
    EXPECT_EQ(mock.test_reg, 0xaabbccdd);
    EXPECT_EQ(data, 0xaabb);

    u64 data64 = -1;
    tx_setup(tx, TLM_WRITE_COMMAND, 0, &data64, sizeof(data64));
    EXPECT_EQ(mock.transport(tx, SBI_NONE, VCML_AS_DEFAULT), 0);
    EXPECT_EQ(mock.test_reg, 0xaabbccdd);
}

TEST(registers, debug_read_write) {
    mock_peripheral mock("mock");

    mock.test_reg_a.on_read(
        [](size_t tag, bool debug) { return debug ? 44 : 42; });
    mock.test_reg_a.on_write([&](u32 val, size_t tag, bool debug) {
        mock.test_reg_a = val + (int)debug;
    });

    u32 data = 0;
    mock.test_reg_a.do_read(0, 0, 4, &data, false);
    EXPECT_EQ(data, 42);
    mock.test_reg_a.do_read(0, 0, 4, &data, true);
    EXPECT_EQ(data, 44);

    data = 1;
    mock.test_reg_a.do_write(0, 0, 4, &data, false);
    EXPECT_EQ(mock.test_reg_a, data);
    mock.test_reg_a.do_write(0, 0, 4, &data, true);
    EXPECT_EQ(mock.test_reg_a, data + 1);
}

class mock_peripheral_debug : public peripheral
{
public:
    reg<u32> test_reg;
    reg<u32> test_reg_tag;

    MOCK_METHOD(u32, reg_read, (bool));
    MOCK_METHOD(void, reg_write, (u32, bool));

    MOCK_METHOD(u32, reg_read_tag, (size_t, bool));
    MOCK_METHOD(void, reg_write_tag, (u32, size_t, bool));

    mock_peripheral_debug(
        const sc_core::sc_module_name& nm =
            sc_core::sc_gen_unique_name("mock_peripheral_debug")):
        peripheral(nm, ENDIAN_LITTLE, 1, 10),
        test_reg("test_reg", 0x0, 0xffffffff),
        test_reg_tag("test_reg_tag", 0x4, 0xffffffff) {
        test_reg.allow_read_write();
        test_reg.on_read(&mock_peripheral_debug::reg_read);
        test_reg.on_write(&mock_peripheral_debug::reg_write);
        test_reg_tag.allow_read_write();
        test_reg_tag.on_read(&mock_peripheral_debug::reg_read_tag);
        test_reg_tag.on_write(&mock_peripheral_debug::reg_write_tag);
        test_reg_tag.tag = 123;
        clk.stub(100 * MHz);
        rst.stub();
        handle_clock_update(0, clk.get_hz());
    }
};

TEST(registers, debug_read_write_periph) {
    mock_peripheral_debug mock("mock");

    u32 data = 0;
    EXPECT_CALL(mock, reg_read(true)).WillOnce(Return(50));
    mock.test_reg.do_read(0, 0, 4, &data, true);
    EXPECT_EQ(data, 50);

    data = 0;
    EXPECT_CALL(mock, reg_read_tag(123, true)).WillOnce(Return(55));
    mock.test_reg_tag.do_read(0, 0, 4, &data, true);
    EXPECT_EQ(data, 55);

    data = 56;
    EXPECT_CALL(mock, reg_write(data, true));
    mock.test_reg.do_write(0, 0, 4, &data, true);

    data = 57;
    EXPECT_CALL(mock, reg_write_tag(data, 123, true));
    mock.test_reg_tag.do_write(0, 0, 4, &data, true);
}

TEST(registers, has_fn) {
    mock_peripheral mock;

    EXPECT_FALSE(mock.test_reg_a.has_readfn());
    EXPECT_FALSE(mock.test_reg_a.has_writefn());

    mock.test_reg_a.on_read([]() -> u32 { return 0; });
    EXPECT_TRUE(mock.test_reg_a.has_readfn());
    EXPECT_FALSE(mock.test_reg_a.has_writefn());

    mock.test_reg_a.on_write([](u32) {});
    EXPECT_TRUE(mock.test_reg_a.has_readfn());
    EXPECT_TRUE(mock.test_reg_a.has_writefn());
}

class mock_peripheral_strided : public peripheral
{
public:
    reg<u32, 4, 0x10> test_reg;

    reg<u8, 4, 4> test_reg_a;
    reg<u8, 4, 4> test_reg_b;
    reg<u8, 4, 4> test_reg_c;

    MOCK_METHOD(u8, reg_a_read, (size_t));
    MOCK_METHOD(void, reg_a_write, (u8, size_t));
    MOCK_METHOD(u8, reg_b_read, (size_t));
    MOCK_METHOD(void, reg_b_write, (u8, size_t));
    MOCK_METHOD(u8, reg_c_read, (size_t));
    MOCK_METHOD(void, reg_c_write, (u8, size_t));

    mock_peripheral_strided(const sc_core::sc_module_name& nm):
        peripheral(nm, ENDIAN_LITTLE, 1, 10),
        test_reg("test_reg", 0x0, 0xffffffff),
        test_reg_a("test_reg_a", 0x100),
        test_reg_b("test_reg_b", 0x101),
        test_reg_c("test_reg_c", 0x102) {
        test_reg.allow_read_write();
        test_reg_a.allow_read_write();
        test_reg_a.on_read(&mock_peripheral_strided::reg_a_read);
        test_reg_a.on_write(&mock_peripheral_strided::reg_a_write);
        test_reg_b.allow_read_write();
        test_reg_b.on_read(&mock_peripheral_strided::reg_b_read);
        test_reg_b.on_write(&mock_peripheral_strided::reg_b_write);
        test_reg_c.allow_read_write();
        test_reg_c.on_read(&mock_peripheral_strided::reg_c_read);
        test_reg_c.on_write(&mock_peripheral_strided::reg_c_write);
        clk.stub(100 * MHz);
        rst.stub();
        handle_clock_update(0, clk.get_hz());
        EXPECT_EQ(test_reg.get_size(), 52);
        EXPECT_EQ(test_reg_a.get_size(), 13);
        EXPECT_EQ(test_reg_b.get_size(), 13);
        EXPECT_EQ(test_reg_c.get_size(), 13);
    }
};

TEST(registers, strided) {
    mock_peripheral_strided mock("mock");

    u32 data = 0;
    tlm_generic_payload tx;

    data = 0x11111111;
    tx_setup(tx, TLM_WRITE_COMMAND, 0x00, &data, sizeof(data));
    EXPECT_EQ(mock.transport(tx, SBI_NONE, VCML_AS_DEFAULT), 4);
    EXPECT_EQ(mock.test_reg[0], data);

    data = 0x22222222;
    tx_setup(tx, TLM_WRITE_COMMAND, 0x10, &data, sizeof(data));
    EXPECT_EQ(mock.transport(tx, SBI_NONE, VCML_AS_DEFAULT), 4);
    EXPECT_EQ(mock.test_reg[1], data);

    data = 0x33333333;
    tx_setup(tx, TLM_WRITE_COMMAND, 0x20, &data, sizeof(data));
    EXPECT_EQ(mock.transport(tx, SBI_NONE, VCML_AS_DEFAULT), 4);
    EXPECT_EQ(mock.test_reg[2], data);

    data = 0x44444444;
    tx_setup(tx, TLM_WRITE_COMMAND, 0x30, &data, sizeof(data));
    EXPECT_EQ(mock.transport(tx, SBI_NONE, VCML_AS_DEFAULT), 4);
    EXPECT_EQ(mock.test_reg[3], data);

    data = 0xffffffff;
    tx_setup(tx, TLM_WRITE_COMMAND, 0x04, &data, sizeof(data));
    EXPECT_EQ(mock.transport(tx, SBI_NONE, VCML_AS_DEFAULT), 0);
    EXPECT_EQ(tx.get_response_status(), TLM_ADDRESS_ERROR_RESPONSE);

    data = 0xffeeddcc;
    tx_setup(tx, TLM_WRITE_COMMAND, 0x108, &data, sizeof(data));
    EXPECT_CALL(mock, reg_a_write(0xcc, 2));
    EXPECT_CALL(mock, reg_b_write(0xdd, 2));
    EXPECT_CALL(mock, reg_c_write(0xee, 2));
    EXPECT_EQ(mock.transport(tx, SBI_NONE, VCML_AS_DEFAULT), 3);
}

class mock_peripheral_strided_broken : public peripheral
{
public:
    reg<u32, 4, 8> test_reg_a;
    reg<u32, 4, 6> test_reg_b;

    mock_peripheral_strided_broken(const sc_core::sc_module_name& nm):
        peripheral(nm, ENDIAN_LITTLE, 1, 10),
        test_reg_a("test_reg_a", 0x0),
        test_reg_b("test_reg_b", 0x4) {
        clk.stub(100 * MHz);
        rst.stub();
        handle_clock_update(0, clk.get_hz());
    }
};

TEST(registers, strided_broken) {
    EXPECT_DEATH({ mock_peripheral_strided_broken mock("mock"); }, "overlap");
}

TEST(registers, str_scalar) {
    mock_peripheral mock;

    mock.test_reg_a = 0xdeadbeef;
    EXPECT_EQ(mock.test_reg_a.str(), "0xdeadbeef");

    mock.test_reg_a.str("0x12345678");
    EXPECT_EQ(mock.test_reg_a, 0x12345678u);

    mock.test_reg_a.str("255");
    EXPECT_EQ(mock.test_reg_a, 255u);
}

TEST(registers, str_array) {
    mock_peripheral_strided mock("mock");

    mock.test_reg[0] = 0x1337;
    mock.test_reg[1] = 0x22222222;
    mock.test_reg[2] = 0x33333333;
    mock.test_reg[3] = 0x44444444;
    EXPECT_EQ(mock.test_reg.str(),
              "0x00001337 0x22222222 0x33333333 0x44444444");

    mock.test_reg.str("0x1234 0x000bbbbb 0xcccccccc 0xdddddddd");
    EXPECT_EQ(mock.test_reg[0], 0x1234u);
    EXPECT_EQ(mock.test_reg[1], 0xbbbbbu);
    EXPECT_EQ(mock.test_reg[2], 0xccccccccu);
    EXPECT_EQ(mock.test_reg[3], 0xddddddddu);
}
