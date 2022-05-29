/******************************************************************************
 *                                                                            *
 * Copyright 2020 Jan Henrik Weinstock                                        *
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

#include "testing.h"

TEST(virtio, msgcopy) {
    const char* s1 = "abc";
    const char* s2 = "def";

    char* s3 = strdup("abcdefg");

    vq_message msg;
    msg.dmi = [](u64 addr, u32 size, vcml_access a) -> u8* {
        return (u8*)addr; // guest addr == host addr for this test
    };

    msg.append((uintptr_t)s1, strlen(s1), false);
    msg.append((uintptr_t)s2, strlen(s2), false);
    msg.append((uintptr_t)s3, strlen(s3), true);

    size_t n = 0;
    char s4[7] = { 0 };

    n = msg.copy_in(s4, 5, 1);
    EXPECT_EQ(n, 5);
    EXPECT_STREQ(s4, "bcdef");

    n = msg.copy_out("EFG", 3, 4);
    EXPECT_EQ(n, 3);
    EXPECT_STREQ(s3, "abcdEFG");

    free(s3);
}

class virtio_harness : public test_base,
                       public virtio_controller,
                       public virtio_device
{
public:
    // virtio controller interface
    MOCK_METHOD(bool, put, (u32, vq_message&));
    MOCK_METHOD(bool, get, (u32, vq_message&));
    MOCK_METHOD(bool, notify, ());

    // virtio device interface
    MOCK_METHOD(void, identify, (virtio_device_desc&));
    MOCK_METHOD(bool, notify, (u32));
    MOCK_METHOD(void, read_features, (u64&));
    MOCK_METHOD(bool, write_features, (u64));
    MOCK_METHOD(bool, read_config, (const range&, void*));
    MOCK_METHOD(bool, write_config, (const range&, const void*));

    virtio_initiator_socket virtio_out;
    virtio_target_socket virtio_in;

    virtio_base_initiator_socket virtio_out_h;
    virtio_base_target_socket virtio_in_h;

    virtio_initiator_socket virtio_out2;
    virtio_target_socket virtio_in2;

    virtio_harness(const sc_module_name& nm):
        test_base(nm),
        virtio_controller(),
        virtio_device(),
        virtio_out("virtio_out"),
        virtio_in("virtio_in"),
        virtio_out_h("virtio_out_h"),
        virtio_in_h("virtio_in_h"),
        virtio_out2("virtio_out2"),
        virtio_in2("virtio_in2") {
        // test hierarchy binding
        virtio_out.bind(virtio_out_h);
        virtio_in_h.bind(virtio_in);
        virtio_out_h.bind(virtio_in_h);

        // test stubbing
        virtio_out2.stub();
        virtio_in2.stub();

        EXPECT_TRUE(find_object("virtio.virtio_out2_stub"));
        EXPECT_TRUE(find_object("virtio.virtio_in2_stub"));
    }

    virtual void run_test() override {
        // check forward interface
        virtio_device_desc desc = {};
        EXPECT_CALL(*this, identify(_)).Times(1);
        virtio_out->identify(desc);

        u64 features = 0;
        EXPECT_CALL(*this, read_features(features))
            .Times(1)
            .WillOnce(SetArgReferee<0>(7));
        virtio_out->read_features(features);
        EXPECT_EQ(features, 7);

        // check backward interface
        EXPECT_CALL(*this, notify()).Times(1).WillOnce(Return(true));
        EXPECT_TRUE(virtio_in->notify());
        EXPECT_CALL(*this, notify()).Times(1).WillOnce(Return(false));
        EXPECT_FALSE(virtio_in->notify());

        // notifying a stubbed socket should return false
        EXPECT_FALSE(virtio_in2->notify());

        // reading features from a stub clear all bits
        features = 123;
        virtio_out2->read_features(features);
        EXPECT_EQ(features, 0);

        // test identifying a stubbed device
        virtio_out2->identify(desc);
        EXPECT_EQ(desc.device_id, VIRTIO_DEVICE_NONE);
    }
};

TEST(virtio, sockets) {
    virtio_harness test("virtio");
    sc_core::sc_start();
}
