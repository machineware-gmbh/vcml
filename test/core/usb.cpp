/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

TEST(usb, speed) {
    EXPECT_STREQ(usb_speed_str(USB_SPEED_LOW), "USB_SPEED_LOW");
    EXPECT_STREQ(usb_speed_str(USB_SPEED_FULL), "USB_SPEED_FULL");
    EXPECT_STREQ(usb_speed_str(USB_SPEED_HIGH), "USB_SPEED_HIGH");
    EXPECT_STREQ(usb_speed_str(USB_SPEED_SUPER), "USB_SPEED_SUPER");
    EXPECT_STREQ(usb_speed_str(USB_SPEED_NONE), "USB_SPEED_NONE");
}

TEST(usb, packet) {
    u32 data = 0x1234cd78;
    usb_packet p;
    p.token = USB_TOKEN_IN;
    p.addr = 1;
    p.epno = 2;
    p.result = USB_RESULT_SUCCESS;
    p.data = (u8*)&data;
    p.length = sizeof(data);
    EXPECT_EQ(to_string(p),
              "USB_TOKEN_IN @ 1.2 [78 cd 34 12] (USB_RESULT_SUCCESS)");

    p.token = USB_TOKEN_OUT;
    p.addr = 7;
    p.epno = 5;
    p.length = 0;
    p.result = USB_RESULT_NACK;
    EXPECT_EQ(to_string(p),
              "USB_TOKEN_OUT @ 7.5 [<no data>] (USB_RESULT_NACK)");
}
