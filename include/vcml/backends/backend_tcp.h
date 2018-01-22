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

#ifndef VCML_BACKEND_TCP_H
#define VCML_BACKEND_TCP_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"
#include "vcml/common/aio.h"

#include "vcml/logging/logger.h"
#include "vcml/properties/property.h"
#include "vcml/backends/backend.h"

namespace vcml {

    class backend_tcp: public backend
    {
    private:
        int m_fd;
        int m_fd_server;

        struct sockaddr_in m_server;
        struct sockaddr_in m_client;

        void handle_accept(int fd, int events);

    public:
        property<u16> port;

        static u16 default_port;

        inline bool is_connected() const { return m_fd > -1; }
        inline bool is_listening() const { return m_fd_server > -1; }

        backend_tcp(const sc_module_name& nm = "backend", u16 port = 0);
        virtual ~backend_tcp();

        VCML_KIND(backend_tcp);

        void listen();
        void listen_async();

        void disconnect();

        virtual size_t peek();
        virtual size_t read(void* buf, size_t len);
        virtual size_t write(const void* buf, size_t len);

        static backend* create(const string& name);
    };

}

#endif
