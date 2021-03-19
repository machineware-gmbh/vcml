/******************************************************************************
 *                                                                            *
 * Copyright 2021 Jan Henrik Weinstock                                        *
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

#include <arpa/inet.h>

TEST(socket, server) {
    vcml::socket server(12345);
    EXPECT_EQ(server.port(), 12345);
}


TEST(socket, port_select) {
    vcml::socket server(0);
    EXPECT_NE(server.port(), 0);
}

TEST(socket, rehost) {
    vcml::socket server(0);
    EXPECT_GT(server.port(), 0);
    int port = server.port();
    server.unlisten();
    EXPECT_EQ(server.port(), 0);
    server.listen(port);
    EXPECT_EQ(server.port(), port);
}

TEST(socket, connect) {
    vcml::socket server(0);
    vcml::socket client(server.host(), server.port());

    server.accept();
    client.send_char('x');
    EXPECT_GT(server.peek(), 0);
    EXPECT_EQ(server.recv_char(), 'x');
    server.send_char('y');
    EXPECT_GT(client.peek(), 0);
    EXPECT_EQ(client.recv_char(), 'y');
}

TEST(socket, send) {
    const char* str = "Hello World";
    char buf[strlen(str) + 1] = {};

    vcml::socket server(0);
    vcml::socket client(server.host(), server.port());

    server.accept();
    server.send(str);
    client.recv(buf, sizeof(buf) - 1);

    EXPECT_EQ(strcmp(str, buf), 0);
}

TEST(socket, async) {
    vcml::socket server;
    vcml::socket client;

    for (auto i : {1, 2, 3}) {
        const char* str = "Hello World";
        char buf[strlen(str) + 1] = {};

        server.listen(0);
        server.accept_async();
        client.connect(server.host(), server.port());

        server.send(str);
        client.recv(buf, sizeof(buf) - 1);
        EXPECT_EQ(strcmp(str, buf), 0);

        server.disconnect();
        client.disconnect();
        server.unlisten();
    }
}

TEST(socket, unlisten) {
    vcml::socket sock(0);
    sock.unlisten();
    EXPECT_FALSE(sock.is_listening());

    sock.listen(0);
    EXPECT_TRUE(sock.is_listening());

    sock.accept_async();
    sock.unlisten();

    EXPECT_THROW(sock.send("test"), vcml::report);
}
