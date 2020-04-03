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

#ifndef VCML_BACKEND_TAP_H
#define VCML_BACKEND_TAP_H

#include "vcml/backends/backend.h"

namespace vcml {

    class backend_tap: public backend
    {
    private:
        int m_fd;

    public:
        property<int> devno;

        backend_tap(const sc_module_name& name = "backend", int devno = -1);
        virtual ~backend_tap();
        VCML_KIND(backend_tap);

        virtual size_t peek();
        virtual size_t read(void* buf, size_t len);
        virtual size_t write(const void* buf, size_t len);

        static backend* create(const string& name);
    };

}

#endif
