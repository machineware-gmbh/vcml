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

#ifndef VCML_GENERIC_MEMORY_H
#define VCML_GENERIC_MEMORY_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"

#include "vcml/range.h"
#include "vcml/peripheral.h"
#include "vcml/slave_socket.h"

namespace vcml { namespace generic {

    class memory: public peripheral
    {
    private:
        unsigned char* m_memory;

        // commands
        bool cmd_load(const vector<string>& args, ostream& os);
        bool cmd_show(const vector<string>& args, ostream& os);

        memory();
        memory(const memory&);

    public:
        property<u64> size;
        property<bool> readonly;
        property<string> images;
        property<u8> poison;

        slave_socket IN;

        inline unsigned char* get_data_ptr() const { return m_memory; }

        memory(const sc_module_name& name, u64 size, bool read_only = false,
               unsigned int read_latency = 0, unsigned int write_latency = 0);
        virtual ~memory();

        VCML_KIND(memory);

        virtual void reset();

        void load(const string& binary, u64 offset = 0);

        virtual tlm_response_status read  (const range& addr, void* data,
                                           const sideband& info) override;
        virtual tlm_response_status write (const range& addr, const void* data,
                                           const sideband& info) override;
    };

}}

#endif
