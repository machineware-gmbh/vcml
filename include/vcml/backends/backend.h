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

#ifndef VCML_BACKEND_H
#define VCML_BACKEND_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"

#include "vcml/logging/logger.h"

namespace vcml {


    class backend: public sc_module
    {
    public:
        typedef backend* (*backend_cfn)(const string&);

    private:
        static std::map<string, backend_cfn> types;

    public:
        backend(const sc_module_name& nm = "backend");
        virtual ~backend();

        VCML_KIND(backend);

        virtual size_t peek() = 0;
        virtual size_t read(void* buf, size_t len) = 0;
        virtual size_t write(const void* buf, size_t len) = 0;

        template <typename T>
        inline size_t read(T& val) {
            return read(&val, sizeof(T));
        }

        template <typename T>
        inline size_t write(const T& val) {
            return write(&val, sizeof(T));
        }

        static int register_backend_type(const string& type, backend_cfn fn);

        static backend* create(const string& type,
                               const string& name = "backend");

        static size_t peek(int fd);
        static size_t full_read(int fd, void* buffer, size_t len);
        static size_t full_write(int fd, const void* buffer, size_t len);
    };

}

#endif
