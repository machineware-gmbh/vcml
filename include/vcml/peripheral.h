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

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"
#include "vcml/common/range.h"

#include "vcml/logging/logger.h"
#include "vcml/properties/property.h"
#include "vcml/protocols/tlm.h"

#include "vcml/component.h"
#include "vcml/register.h"

namespace vcml {

    class reg_base;

    class peripheral: public component
    {
    private:
        int m_current_cpu;
        endianess m_endian;
        vector<reg_base*> m_registers;

        bool cmd_mmap(const vector<string>& args, ostream& os);

    public:
        property<unsigned int> read_latency;
        property<unsigned int> write_latency;

        endianess get_endian() const { return m_endian; }
        void set_endian(endianess e) { m_endian = e; }

        void set_little_endian() { m_endian = ENDIAN_LITTLE; }
        void set_big_endian()    { m_endian = ENDIAN_BIG; }

        bool is_little_endian() const;
        bool is_big_endian() const;
        bool is_host_endian() const;

        template <typename T> T to_host_endian(T val) const;
        template <typename T> T from_host_endian(T val) const;

        int  current_cpu() const      { return m_current_cpu; }
        void set_current_cpu(int cpu) { m_current_cpu = cpu; }

        peripheral(const sc_module_name& nm, endianess e = host_endian(),
                   unsigned int read_latency = 0,
                   unsigned int write_latency = 0);
        virtual ~peripheral();

        peripheral() = delete;
        peripheral(const peripheral&) = delete;

        VCML_KIND(peripheral);

        virtual void reset() override;

        void add_register(reg_base* reg);
        void remove_register(reg_base* reg);

        vector<reg_base*> get_registers() const { return m_registers; }

        void map_dmi(unsigned char* ptr, u64 start, u64 end, vcml_access a);

        virtual unsigned int transport(tlm_generic_payload& tx,
                                       const tlm_sbi& info,
                                       address_space as) override;

        virtual unsigned int receive(tlm_generic_payload& tx,
                                     const tlm_sbi& info,
                                     address_space as);

        virtual tlm_response_status read(const range& addr, void* data,
                                         const tlm_sbi& info, address_space);
        virtual tlm_response_status read(const range& addr, void* data,
                                         const tlm_sbi& info);

        virtual tlm_response_status write(const range& addr, const void* data,
                                          const tlm_sbi& info, address_space);
        virtual tlm_response_status write(const range& addr, const void* data,
                                          const tlm_sbi& info);

        virtual void handle_clock_update(clock_t oldclk,
                                         clock_t newclk) override;
    };

    inline bool peripheral::is_little_endian() const {
        return m_endian == ENDIAN_LITTLE;
    }

    inline bool peripheral::is_big_endian() const {
        return m_endian == ENDIAN_BIG;
    }

    inline bool peripheral::is_host_endian() const {
        return m_endian == host_endian();
    }

    template <typename T>
    inline T peripheral::to_host_endian(T val) const {
        return is_host_endian() ? val : bswap(val);
    }

    template <typename T>
    inline T peripheral::from_host_endian(T val) const {
        return is_host_endian() ? val : bswap(val);
    }

}

#endif
