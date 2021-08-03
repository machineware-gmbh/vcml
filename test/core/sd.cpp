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

#include "testing.h"

enum : address_space {
    VCML_AS_TEST = VCML_AS_DEFAULT + 1,
};

class sd_harness: public test_base, public sd_host
{
public:
    sd_initiator_socket SD_OUT;
    sd_target_socket SD_IN;

    sd_initiator_socket SD_OUT2;
    sd_target_socket SD_IN2;

   sd_harness(const sc_module_name& nm):
        test_base(nm),
        sd_host(),
        SD_OUT("SPI_OUT"),
        SD_IN("SPI_IN", VCML_AS_TEST),
        SD_OUT2("SPI_OUT2"),
        SD_IN2("SPI_IN2") {
        SD_OUT.bind(SD_IN);
        SD_OUT2.stub();
        SD_IN2.stub();

        auto initiators = get_sd_initiator_sockets();
        auto targets = get_sd_target_sockets();
        auto sockets = get_sd_target_sockets(VCML_AS_TEST);

        EXPECT_EQ(initiators.size(), 2) << "sd initiators did not register";
        EXPECT_EQ(targets.size(), 2) << "sd targets did not register";
        EXPECT_EQ(sockets.size(), 1) << "sd targets in wrong address space";
    }

    virtual void sd_transport(const sd_target_socket& socket,
                              sd_command& cmd) override {
        EXPECT_EQ(socket.as, VCML_AS_TEST);
        cmd.status = SD_OK;
        cmd.response[0] = cmd.argument * 10;
    }

    virtual void sd_transport(const sd_target_socket& socket,
                              sd_data& data) {
        EXPECT_EQ(socket.as, VCML_AS_TEST);
        EXPECT_EQ(data.mode, SD_READ);
        data.data *= 10;
        data.status.read = SDTX_OK;
    }

    virtual void run_test() override {
        for (vcml::u8 i = 0; i < 10; i++) {
            wait(1, sc_core::SC_SEC);
            sd_command cmd = {};
            cmd.opcode = 0;
            cmd.argument = i;
            cmd.status = SD_INCOMPLETE;
            SD_OUT->sd_transport(cmd);
            EXPECT_TRUE(success(cmd));
            EXPECT_EQ(cmd.response[0], i * 10);

            sd_data data = {};
            data.mode = SD_READ;
            data.data = i;
            data.status.read = SDTX_INCOMPLETE;
            SD_OUT->sd_transport(data);
            EXPECT_TRUE(success(data));
            EXPECT_EQ(data.data, i * 10);
        }
    }
};

TEST(sd, sockets) {
    sd_harness test("test");
    sc_core::sc_start();
}
