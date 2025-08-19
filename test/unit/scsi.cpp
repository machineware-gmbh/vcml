/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include <gtest/gtest.h>
#include "vcml.h"

using namespace vcml;
using namespace vcml::audio;

TEST(scsi, transfer) {
    EXPECT_TRUE(block::scsi_command_transfers_to_device(0x2a));
    EXPECT_FALSE(block::scsi_command_transfers_from_device(0x2a));
    EXPECT_FALSE(block::scsi_command_transfers_to_device(0x28));
    EXPECT_TRUE(block::scsi_command_transfers_from_device(0x28));
}

TEST(scsi, strings) {
    block::scsi_disk disk("disk");
    EXPECT_STREQ(disk.kind(), "vcml::block::scsi_disk");

    for (int i = 0; i < 0xff; i++) {
        EXPECT_NE(block::scsi_command_str(i), nullptr);
        EXPECT_NE(block::scsi_response_str(i), nullptr);
    }
}

TEST(scsi, inquire) {
    block::scsi_disk disk("disk");
    block::scsi_request req{};
    req.command[0] = 0x12;
    ASSERT_TRUE(success(disk.scsi_handle_command(req)));
    EXPECT_EQ(disk.get_sense(), block::SENSE_NOTHING);
    ASSERT_EQ(req.payload.size(), 36);

    char vendor[8]{};
    memcpy(vendor, req.payload.data() + 8, 8);
    EXPECT_STREQ(vendor, "MWARE  ");

    char product[16]{};
    memcpy(product, req.payload.data() + 16, 16);
    EXPECT_STREQ(product, "VCML-SCSIDRIVE ");

    char revision[16]{};
    memcpy(revision, req.payload.data() + 32, 4);
    EXPECT_STREQ(revision, "1.0");
}

TEST(scsi, mode_sense) {
    block::scsi_disk disk("disk");
    block::scsi_request req{};
    req.command[0] = 0x1a;
    req.command[2] = 0x3f;
    ASSERT_TRUE(success(disk.scsi_handle_command(req)));
    EXPECT_FALSE(req.payload.empty());

    req.command[0] = 0x1a;
    req.command[1] = 0x08;
    req.command[2] = 0x08;
    ASSERT_TRUE(success(disk.scsi_handle_command(req)));
    EXPECT_FALSE(req.payload.empty());
}

TEST(scsi, read_sense) {
    block::scsi_disk disk("disk");
    disk.set_sense(block::SENSE_MEDIUM_ERROR);
    EXPECT_NE(disk.get_sense(), block::SENSE_NOTHING);

    block::scsi_request req{};
    req.command[0] = 0x03;
    ASSERT_TRUE(success(disk.scsi_handle_command(req)));
    EXPECT_EQ(req.payload.size(), 18);

    EXPECT_EQ(req.payload[2], block::SENSE_MEDIUM_ERROR.key);
    EXPECT_EQ(req.payload[12], block::SENSE_MEDIUM_ERROR.asc);
    EXPECT_EQ(req.payload[13], block::SENSE_MEDIUM_ERROR.ascq);
}

TEST(scsi, read_capacity) {
    block::scsi_disk disk("disk");
    block::scsi_request req{};
    req.command[0] = 0x25;
    ASSERT_TRUE(success(disk.scsi_handle_command(req)));
    EXPECT_EQ(req.payload.size(), 8);

    u32 bsz = req.payload[7] | (u32)req.payload[6] << 8 |
              (u32)req.payload[5] << 16 | (u32)req.payload[4] << 24;
    u32 lba = req.payload[3] | (u32)req.payload[2] << 8 |
              (u32)req.payload[1] << 16 | (u32)req.payload[0] << 24;

    EXPECT_EQ(disk.blocksize(), bsz);
    EXPECT_EQ(udivup(disk.capacity(), disk.blocksize()) - 1, lba);
}

TEST(scsi, read_write) {
    block::scsi_disk disk("disk");
    block::scsi_request req_w{};
    req_w.command[0] = 0x2a; // write10
    req_w.command[5] = 0x02; // offset 1024
    req_w.command[8] = 0x01; // size 512
    req_w.payload.assign(512, 0xab);
    ASSERT_TRUE(success(disk.scsi_handle_command(req_w)));

    block::scsi_request req_f{};
    req_f.command[0] = 0x35; // flush
    ASSERT_TRUE(success(disk.scsi_handle_command(req_f)));

    block::scsi_request req_r{};
    req_r.command[0] = 0x28; // read10
    req_r.command[5] = 0x02; // offset 1024
    req_r.command[8] = 0x01; // size 512
    ASSERT_TRUE(success(disk.scsi_handle_command(req_r)));

    EXPECT_EQ(req_w.payload, req_r.payload);
}

TEST(scsi, illegal_command) {
    block::scsi_disk disk("disk");
    block::scsi_request req{};
    req.command[0] = 0xee;
    ASSERT_TRUE(failed(disk.scsi_handle_command(req)));
    EXPECT_EQ(disk.get_sense(), block::SENSE_ILLEGAL_REQ);

    block::scsi_request req_s{};
    req_s.command[0] = 0x03;
    ASSERT_TRUE(success(disk.scsi_handle_command(req_s)));
    EXPECT_EQ(req_s.payload.size(), 18);

    EXPECT_EQ(req_s.payload[2], block::SENSE_ILLEGAL_REQ.key);
    EXPECT_EQ(req_s.payload[12], block::SENSE_ILLEGAL_REQ.asc);
    EXPECT_EQ(req_s.payload[13], block::SENSE_ILLEGAL_REQ.ascq);
}
