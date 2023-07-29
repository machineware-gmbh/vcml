/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_TIMERS_RTC1742_H
#define VCML_TIMERS_RTC1742_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/range.h"
#include "vcml/core/peripheral.h"
#include "vcml/core/model.h"

#include "vcml/protocols/tlm.h"

namespace vcml {
namespace timers {

class rtc1742 : public peripheral
{
private:
    u8* m_nvmem;
    range m_addr;

    time_t m_real_timestamp;
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
                                     const tlm_sbi& info) override;
    virtual tlm_response_status write(const range& addr, const void* ptr,
                                      const tlm_sbi& info) override;

    void write_control(u8 val);

    // disabled
    rtc1742();
    rtc1742(const rtc1742&);

public:
    enum nvram_size {
        NVMEM_2K = 2 * 1024, // original ds1742
        NVMEM_8K = 8 * 1024, // model ds1743
    };

    enum control_bits {
        CONTROL_W = 1 << 7, // write bit
        CONTROL_R = 1 << 6, // read bit
    };

    enum seconds_bits {
        SECONDS_OSC = 1 << 7, // stop bit
    };

    enum day_bits {
        DAY_BF = 1 << 7, // battery flag
        DAY_FT = 1 << 6, // frequency test
    };

    reg<u8> control;
    reg<u8> seconds;
    reg<u8> minutes;
    reg<u8> hour;
    reg<u8> day;
    reg<u8> date;
    reg<u8> month;
    reg<u8> year;

    tlm_target_socket in;

    property<bool> sctime;
    property<string> nvmem;

    rtc1742(const sc_module_name& nm, u32 nvramsz = NVMEM_2K);
    virtual ~rtc1742();
    VCML_KIND(timers::rtc1742);

    virtual void reset() override;
};

inline time_t rtc1742::real_timestamp() const {
    return time(NULL);
}

inline time_t rtc1742::sysc_timestamp() const {
    sc_time delta = sc_time_stamp() - m_sysc_timestamp;
    return m_real_timestamp + (time_t)delta.to_seconds();
}

} // namespace timers
} // namespace vcml

#endif
