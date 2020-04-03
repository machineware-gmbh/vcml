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

#include <testing.h>
#include <arpa/inet.h>

#include "../../src/vcml/backends/backend_tcp.h"

TEST(backend_tcp, connect) {
    vcml::backend* backend = vcml::backend::create("tcp", "name");
    vcml::backend_tcp* tcp = dynamic_cast<vcml::backend_tcp*>(backend);

    ASSERT_TRUE(backend != nullptr);
    ASSERT_TRUE(tcp != nullptr);

    EXPECT_TRUE(tcp->is_listening());
    EXPECT_FALSE(tcp->is_connected());

    vcml::u16 port = tcp->port;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_FALSE(fd < 0);

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    EXPECT_EQ(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr), 1);
    ASSERT_EQ(connect(fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)), 0);

    ASSERT_TRUE(tcp->is_listening());
    ASSERT_TRUE(tcp->is_connected());

    const char msg[] = "Hello World";

    EXPECT_FALSE(backend->peek());
    ASSERT_EQ(vcml::fd_write(fd, msg, sizeof(msg)), sizeof(msg));
    EXPECT_TRUE(backend->peek());

    char buf[12];
    ASSERT_EQ(backend->read(buf, sizeof(buf)), sizeof(buf));
    EXPECT_EQ(memcmp(buf, msg, sizeof(buf)), 0);

    close(fd);

    EXPECT_EQ(tcp->read(buf, 1), 0);
    EXPECT_TRUE(tcp->is_listening());
    EXPECT_FALSE(tcp->is_connected());

    delete backend;
}
