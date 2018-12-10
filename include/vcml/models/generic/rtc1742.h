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

#ifndef VCML_GENERIC_RTC1742_H
#define VCML_GENERIC_RTC1742_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"

#include "vcml/range.h"
#include "vcml/ports.h"
#include "vcml/command.h"
#include "vcml/register.h"
#include "vcml/peripheral.h"
#include "vcml/slave_socket.h"

namespace vcml { namespace generic {

    class rtc1742: public peripheral
    {
    private:
        u8*     m_nvram;
        range   m_addr;

        time_t  m_real_timestamp;
        sc_time m_sysc_timestamp;

        time_t real_timestamp() const;
        time_t sysc_timestamp() const;

        void load_time();
        void save_time();

        void load_nvram(const string& filename);
        void save_nvram(const string& filename);

        bool cmd_load(const vector<string>& args, ostream& os);
        bool cmd_save(const vector<string>& args, ostream& os);
        bool cmd_battery(const vector<string>& args, ostream& os);

        virtual tlm_response_status read(const range& addr, void* ptr,
                                         int flags) override;
        virtual tlm_response_status write(const range& addr, const void* ptr,
                                         int flags) override;

        u8 write_CONTROL(u8 val);

        // disabled
        rtc1742();
        rtc1742(const rtc1742&);

    public:
        enum nvram_size {
            NVMEM_2K    = 2 * 1024, /* original ds1742 */
            NVMEM_8K    = 8 * 1024, /* model ds1743 */
        };

        enum control_bits {
            CONTROL_W   = 1 << 7,   /* write bit */
            CONTROL_R   = 1 << 6,   /* read bit */
        };

        enum seconds_bits {
            SECONDS_OSC = 1 << 7,   /* stop bit */
        };

        enum day_bits {
            DAY_BF      = 1 << 7,   /* battery flag */
            DAY_FT      = 1 << 6,   /* frequency test */
        };

        reg<rtc1742, u8> CONTROL;
        reg<rtc1742, u8> SECONDS;
        reg<rtc1742, u8> MINUTES;
        reg<rtc1742, u8> HOUR;
        reg<rtc1742, u8> DAY;
        reg<rtc1742, u8> DATE;
        reg<rtc1742, u8> MONTH;
        reg<rtc1742, u8> YEAR;

        slave_socket IN;

        property<bool> sctime;
        property<string> nvmem;

        rtc1742(const sc_module_name& nm, u32 nvramsz = NVMEM_2K);
        virtual ~rtc1742();
        VCML_KIND(rtc1742);

        virtual void reset() override;
    };

    inline time_t rtc1742::real_timestamp() const {
        return time(NULL);
    }

    inline time_t rtc1742::sysc_timestamp() const {
        sc_time delta = sc_time_stamp() - m_sysc_timestamp;
        return m_real_timestamp + (time_t)delta.to_seconds();
    }

}}

#endif
