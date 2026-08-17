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

using namespace vcml;

TEST(memory, alignment) {
    EXPECT_TRUE(is_aligned(0x1000, VCML_ALIGN_4K));
    EXPECT_FALSE(is_aligned(0x1001, VCML_ALIGN_4K));

    tlm_memory mem(8 * KiB, VCML_ALIGN_8M);
    EXPECT_TRUE(is_aligned(mem.data(), VCML_ALIGN_8M));
}

TEST(alignment, output) {
    std::stringstream ss;

    ss << VCML_ALIGN_8K;
    EXPECT_EQ(ss.str(), "8k");
    ss.str("");

    ss << VCML_ALIGN_256M;
    EXPECT_EQ(ss.str(), "256M");
    ss.str("");

    ss << VCML_ALIGN_1G;
    EXPECT_EQ(ss.str(), "1G");
    ss.str("");
}

TEST(alignment, input) {
    std::stringstream ss;
    vcml::alignment a;

    ss.seekg(0);
    ss.str("128k");
    ss >> a;
    EXPECT_EQ(a, VCML_ALIGN_128K);

    ss.seekg(0);
    ss.str("64M");
    ss >> a;
    EXPECT_EQ(a, VCML_ALIGN_64M);

    ss.seekg(0);
    ss.str("1K");
    ss >> a;
    EXPECT_EQ(a, VCML_ALIGN_1K);
}

TEST(memory, readwrite) {
    tlm_memory mem(1);

    u8 data = 0x42;

    mem.discard_writes(true);
    mem[0] = 0;
    EXPECT_OK(mem.write(0, data)) << "discard_writes mem rejected write";
    EXPECT_EQ(mem[0], 0) << "discard_writes mem stored the write";

    mem.discard_writes(false);
    EXPECT_OK(mem.write(range(0, 0), &data, false)) << "write failed";
    EXPECT_EQ(mem[0], data) << "data not stored";

    EXPECT_AE(mem.write(range(1, 1), &data, false))
        << "out of bounds write succeeded";

    mem.allow_read_only();
    mem[0] = 0;

    EXPECT_CE(mem.write(range(0, 0), &data, false))
        << "read-only memory permitted write";
    EXPECT_EQ(mem[0], 0) << "read-only memory got overwritten";

    EXPECT_OK(mem.write(range(0, 0), &data, true))
        << "read-only memory denied debug write";
    EXPECT_EQ(mem[0], data) << "debug write has no effect";
}

TEST(memory, transport) {
    tlm_memory mem(1);
    u8 data = 0x42;
    tlm_generic_payload tx;
    tlm_sbi sbi;

    tx_setup(tx, TLM_WRITE_COMMAND, 0, &data, sizeof(data));
    mem.transport(tx, sbi);
    EXPECT_EQ(tx.get_response_status(), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(mem[0], 0x42);

    mem[0] = 0x23;
    tx_setup(tx, TLM_READ_COMMAND, 0, &data, sizeof(data));
    mem.transport(tx, sbi);
    EXPECT_EQ(tx.get_response_status(), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(data, 0x23);
}

TEST(memory, move) {
    const size_t size = 4 * KiB;

    tlm_memory orig(size);
    u8* data = orig.data();

    tlm_memory move = std::move(orig);

    EXPECT_EQ(orig.size(), 0) << "size not zero after move"; // NOLINT
    EXPECT_EQ(move.size(), size) << "size not copied correctly";

    EXPECT_EQ(orig.data(), nullptr) << "memory pointer not cleared after move";
    EXPECT_EQ(move.data(), data) << "memory pointer not moved";
}

TEST(memory, sharing) {
    const size_t size = 16 * KiB;
    const string name = "/vcml-test-shared";
    tlm_memory a(name, size);
    tlm_memory b(name, size);

    for (size_t i = 0; i < size; i++) {
        a[i] = (u8)i;
        ASSERT_EQ(a[i], b[i]) << "mismatch at position " << i;
    }
}

TEST(memory, sharing_wrong_size) {
    const size_t size = 16 * KiB;
    const string name = "/vcml-test-shared-size";
    tlm_memory a(name, size);
    EXPECT_DEATH({ tlm_memory b(name, size * 2); }, "unexpected size");
    EXPECT_DEATH({ tlm_memory b(name, size / 2); }, "unexpected size");
}

TEST(memory, move_constructor) {
    const size_t size = 4 * KiB;
    const string name = "/vcml-test-move-shared";

    tlm_memory orig(name, size);
    orig[0] = 0xab;
    EXPECT_TRUE(orig.is_shared());

    tlm_memory moved = std::move(orig);

    EXPECT_FALSE(orig.is_shared()); // NOLINT
    EXPECT_EQ(orig.size(), 0);      // NOLINT

    EXPECT_TRUE(moved.is_shared());
    EXPECT_EQ(moved.size(), size);
    EXPECT_STREQ(moved.shared_name(), name.c_str());

    u8 data;
    EXPECT_OK(moved.read(0, data));
    EXPECT_EQ(data, 0xab);
}

TEST(memory, aligned_oob_rejected) {
    const size_t size = 4 * KiB;
    tlm_memory mem(size, VCML_ALIGN_8M); // force extra size by aligment
    vector<u8> buf(size, 0xff);

    EXPECT_OK(mem.read(range(0, size - 1), buf.data(), false))
        << "accesses within [0, size-1] must succeed";

    EXPECT_AE(mem.read(range(size, size), buf.data(), false))
        << "read into alignment must be rejected";
    EXPECT_AE(mem.write(range(size, size), buf.data(), false))
        << "write into alignment must berejected";
}

TEST(memory, fill) {
    tlm_memory mem(4 * KiB, VCML_ALIGN_4K);

    ASSERT_OK(mem.fill(0xaa, false));
    ASSERT_EQ(mem[0], 0xaa);

    mem.discard_writes(true);
    ASSERT_OK(mem.fill(0xbb, true));
    ASSERT_EQ(mem[0], 0xbb);
    ASSERT_OK(mem.fill(0xcc, false));
    ASSERT_EQ(mem[0], 0xcc);
}
