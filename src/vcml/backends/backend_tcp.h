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

#include "vcml/common/socket.h"
#include "vcml/backends/backend.h"

namespace vcml {

    class backend_tcp: public backend
    {
    private:
        socket m_sock;

    public:
        property<u16> port;

        bool is_listening() const { return m_sock.is_listening(); }
        bool is_connected() const { return m_sock.is_connected(); }

        backend_tcp(const sc_module_name& nm = "backend", u16 port = 0);
        virtual ~backend_tcp();
        VCML_KIND(backend_tcp);

        virtual size_t peek() override;
        virtual size_t read(void* buf, size_t len) override;
        virtual size_t write(const void* buf, size_t len) override;

        static backend* create(const string& name);
    };

}

#endif
