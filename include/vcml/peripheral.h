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

#ifndef VCML_PERIPHERAL_H
#define VCML_PERIPHERAL_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"

#include "vcml/logging/logger.h"
#include "vcml/backends/backend.h"
#include "vcml/properties/property.h"

#include "vcml/range.h"
#include "vcml/txext.h"
#include "vcml/dmi_cache.h"
#include "vcml/component.h"
#include "vcml/register.h"

namespace vcml {

    class reg_base;

    class peripheral: public component
    {
    private:
        vcml_endian m_endian;
        vector<reg_base*> m_registers;
        vector<backend*> m_backends;

        // disabled
        peripheral();
        peripheral(const peripheral&);

    public:
        property<sc_time> read_latency;
        property<sc_time> write_latency;

        property<string> backends;


        inline vcml_endian get_endian() const { return m_endian; }
        inline void set_endian(vcml_endian e) { m_endian = e; }

        inline void set_little_endian() { m_endian = VCML_ENDIAN_LITTLE; }
        inline void set_big_endian() { m_endian = VCML_ENDIAN_BIG; }

        inline bool is_little_endian() const;
        inline bool is_big_endian() const;

        peripheral(const sc_module_name& nm, vcml_endian e = host_endian(),
                   const sc_time& read_latency = SC_ZERO_TIME,
                   const sc_time& write_latency = SC_ZERO_TIME);
        virtual ~peripheral();

        VCML_KIND(peripheral);

        void add_register(reg_base* reg);
        void remove_register(reg_base* reg);

        void map_dmi(unsigned char* ptr, u64 start, u64 end, vcml_access a);

        bool   bepeek();
        size_t beread(void* buffer, size_t size);
        size_t bewrite(const void* buffer, size_t size);

        template <typename T> size_t beread(T& val);
        template <typename T> size_t bewrite(const T& val);

        virtual unsigned int transport(tlm_generic_payload& tx, sc_time& dt,
                                       int flags);
        virtual unsigned int receive(tlm_generic_payload& tx, int flags);
        virtual tlm_response_status read  (const range& addr, void* data,
                                           int flags);
        virtual tlm_response_status write (const range& addr, const void* data,
                                           int flags);
    };

    inline bool peripheral::is_little_endian() const {
        return m_endian == VCML_ENDIAN_LITTLE;
    }

    inline bool peripheral::is_big_endian() const {
        return m_endian == VCML_ENDIAN_BIG;
    }

    template <typename T>
    inline size_t peripheral::beread(T& val) {
        return beread(&val, sizeof(T));
    }

    template <typename T>
    inline size_t peripheral::bewrite(const T& val) {
        return bewrite(&val, sizeof(T));
    }

}

#endif
