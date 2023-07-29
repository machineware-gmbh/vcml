/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/timers/rtc1742.h"

namespace vcml {
namespace timers {

static u8 bin2bcd(u8 val) {
    return (val / 10) << 4 | (val % 10);
}

static u8 bcd2bin(u8 val) {
    return (val >> 4) * 10 + (val & 0xF);
}

static char* strtime(struct tm* t) {
    char* str = asctime(t);
    str[strlen(str) - 1] = '\0';
    return str;
}

void rtc1742::load_time() {
    // If SystemC time is chosen (default), calculate the current time
    // stamp based on the real time when the simulation was started plus
    // the amount of SystemC seconds elapsed since simulation start.
    // Otherwise we just fetch the current real time stamp from host.
    time_t now = sctime ? sysc_timestamp() : real_timestamp();
    struct tm* timeinfo = gmtime(&now);
    log_debug("loading current time %lu (%s)", now, strtime(timeinfo));

    u8 osc = seconds & SECONDS_OSC; /* oscillator stop bit */
    u8 btb = day & DAY_BF;          /* battery indicator */
    u8 ftb = day & DAY_FT;          /* frequency test bit */

    seconds = (bin2bcd(timeinfo->tm_sec) & 0x7F) | osc;
    minutes = (bin2bcd(timeinfo->tm_min) & 0x7F);
    hour = (bin2bcd(timeinfo->tm_hour) & 0x3F);
    day = (bin2bcd(timeinfo->tm_wday) & 0x03) | btb | ftb;
    date = (bin2bcd(timeinfo->tm_mday) & 0x3F);
    month = (bin2bcd(timeinfo->tm_mon + 1) & 0x1F);
    year = (bin2bcd(timeinfo->tm_year % 100) & 0xFF);

    u32 century = (timeinfo->tm_year + 1900) / 100;
    control = (bin2bcd((u8)century) & 0x3F);
}

void rtc1742::save_time() {
    struct tm tinfo = {};
    tinfo.tm_sec = bcd2bin(seconds & 0x7F);
    tinfo.tm_min = bcd2bin(minutes & 0x7F);
    tinfo.tm_hour = bcd2bin(hour & 0x3F);
    tinfo.tm_wday = bcd2bin(day & 0x03);
    tinfo.tm_mday = bcd2bin(date & 0x3F);
    tinfo.tm_mon = bcd2bin(month & 0x1F) - 1;
    tinfo.tm_year = bcd2bin(year) + bcd2bin(control & 0x3F) * 100 - 1900;

    // Set the new offset time. Future calculations will use the specified
    // real time stamp as the base on to which the elapsed SystemC time
    // since this change is added. This is only relevant if the clock
    // operates based on SystemC time. If hose time is used, this change
    // is ineffectual.
    m_real_timestamp = timegm(&tinfo);
    m_sysc_timestamp = sc_time_stamp();

    log_debug("saving current time %lu (%s)", m_real_timestamp,
              strtime(&tinfo));
}

void rtc1742::load_nvram(const string& filename) {
    log_debug("loading binary file %s into nvram", filename.c_str());
    ifstream file(filename.c_str(), std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        log_warn("cannot open file '%s'", filename.c_str());
        return;
    }

    u64 nbytes = file.tellg();
    u64 size = m_addr.length() - control.get_address();

    if (nbytes > size) {
        nbytes = size;
        log_warn("image file '%s' to big, truncating after %llu bytes",
                 filename.c_str(), nbytes);
    }

    file.seekg(0, std::ios::beg);
    file.read((char*)m_nvmem, nbytes);
}

void rtc1742::save_nvram(const string& filename) {
    log_debug("storing nvram into binary file %s", filename.c_str());
    ofstream file(filename.c_str(), std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        log_warn("cannot open file '%s'", filename.c_str());
        return;
    }

    u64 size = m_addr.length() - control.get_address();
    file.write((char*)m_nvmem, size);
}

bool rtc1742::cmd_load(const vector<string>& args, ostream& os) {
    const string& filename = args[0];
    load_nvram(filename);
    os << "loaded " << filename << " into nvram";
    return true;
}

bool rtc1742::cmd_save(const vector<string>& args, ostream& os) {
    const string& filename = args[0];
    save_nvram(filename);
    os << "saved nvram to " << filename;
    return true;
}

bool rtc1742::cmd_battery(const vector<string>& args, ostream& os) {
    if (day & DAY_BF) {
        os << "clearing battery bit (battery low)";
        day &= ~DAY_BF;
    } else {
        os << "setting battery bit (battery good)";
        day |= DAY_BF;
    }

    return true;
}

tlm_response_status rtc1742::read(const range& addr, void* ptr,
                                  const tlm_sbi& info) {
    if (!addr.inside(m_addr))
        return TLM_ADDRESS_ERROR_RESPONSE;
    memcpy(ptr, m_nvmem + addr.start, addr.length());
    return TLM_OK_RESPONSE;
}

tlm_response_status rtc1742::write(const range& addr, const void* ptr,
                                   const tlm_sbi& info) {
    if (!addr.inside(m_addr))
        return TLM_ADDRESS_ERROR_RESPONSE;
    memcpy(m_nvmem + addr.start, ptr, addr.length());
    return TLM_OK_RESPONSE;
}

void rtc1742::write_control(u8 val) {
    VCML_LOG_REG_BIT_CHANGE(CONTROL_R, control, val);
    VCML_LOG_REG_BIT_CHANGE(CONTROL_W, control, val);

    if ((val & CONTROL_R) && !(control & CONTROL_R)) {
        load_time(); // CONTROL_R set
        control |= CONTROL_R;
    } else if ((val & CONTROL_W) && !(control & CONTROL_W)) {
        load_time(); // CONTROL_W set
        control |= CONTROL_W;
    } else if (!(val & CONTROL_W) && (control & CONTROL_W)) {
        save_time(); // CONTROL_W cleared
        control &= ~CONTROL_W;
    }
}

rtc1742::rtc1742(const sc_module_name& nm, u32 nvmemsz):
    peripheral(nm),
    m_nvmem(nullptr),
    m_addr(0, nvmemsz - 9), // need 8 bytes at the end for registers
    m_real_timestamp(time(NULL)),
    m_sysc_timestamp(sc_time_stamp()),
    control("control", nvmemsz - 8, 0),
    seconds("seconds", nvmemsz - 7, 0),
    minutes("minutes", nvmemsz - 6, 0),
    hour("hour", nvmemsz - 5, 0),
    day("day", nvmemsz - 4, DAY_BF),
    date("date", nvmemsz - 3, 0),
    month("month", nvmemsz - 2, 0),
    year("year", nvmemsz - 1, 0),
    in("in"),
    sctime("sctime", true),
    nvmem("nvmem", "") {
    m_nvmem = new u8[nvmemsz];
    if (!nvmem.get().empty())
        load_nvram(nvmem);

    control.allow_read_write();
    control.on_write(&rtc1742::write_control);

    seconds.allow_read_write();
    minutes.allow_read_write();
    hour.allow_read_write();
    day.allow_read_write();
    date.allow_read_write();
    month.allow_read_write();
    year.allow_read_write();

    register_command("load", 1, &rtc1742::cmd_load,
                     "loads <file> into the nvmem");
    register_command("save", 1, &rtc1742::cmd_save,
                     "stores the contents of the nvmem into <file>");
    register_command("battery", 0, &rtc1742::cmd_battery,
                     "toggle battery bit");
}

rtc1742::~rtc1742() {
    if (!nvmem.get().empty())
        save_nvram(nvmem);
    delete[] m_nvmem;
}

void rtc1742::reset() {
    peripheral::reset();
}

VCML_EXPORT_MODEL(vcml::timers::rtc1742, name, args) {
    return new rtc1742(name);
}

} // namespace timers
} // namespace vcml
