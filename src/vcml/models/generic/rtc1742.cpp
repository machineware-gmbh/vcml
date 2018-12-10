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

#include <vcml/models/generic/rtc1742.h>

namespace vcml { namespace generic {

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
        struct tm* timeinfo = localtime(&now);
        log_debug("loading current time %llu (%s)", now, strtime(timeinfo));

        u8 osc = SECONDS & SECONDS_OSC; /* oscillator stop bit */
        u8 btb = DAY & DAY_BF;          /* battery indicator */
        u8 ftb = DAY & DAY_FT;          /* frequency test bit */

        SECONDS = (bin2bcd(timeinfo->tm_sec)  & 0x7F) | osc;
        MINUTES = (bin2bcd(timeinfo->tm_min)  & 0x7F);
        HOUR    = (bin2bcd(timeinfo->tm_hour) & 0x3F);
        DAY     = (bin2bcd(timeinfo->tm_wday) & 0x03) | btb | ftb;
        DATE    = (bin2bcd(timeinfo->tm_mday) & 0x3F);
        MONTH   = (bin2bcd(timeinfo->tm_mon + 1)  & 0x1F);
        YEAR    = (bin2bcd(timeinfo->tm_year % 100) & 0xFF);

        u32 century = (timeinfo->tm_year + 1900) / 100;
        CONTROL = (bin2bcd((u8)century) & 0x3F);
    }

    void rtc1742::save_time() {
        struct tm tinfo;
        tinfo.tm_sec  = bcd2bin(SECONDS & 0x7F);
        tinfo.tm_min  = bcd2bin(MINUTES & 0x7F);
        tinfo.tm_hour = bcd2bin(HOUR & 0x3F);
        tinfo.tm_wday = bcd2bin(DAY & 0x03);
        tinfo.tm_mday = bcd2bin(DATE & 0x3F);
        tinfo.tm_mon  = bcd2bin(MONTH & 0x1F) - 1;
        tinfo.tm_year = bcd2bin(YEAR) + bcd2bin(CONTROL & 0x3F) * 100 - 1900;

        // Set the new offset time. Future calculations will use the specified
        // real time stamp as the base on to which the elapsed SystemC time
        // since this change is added. This is only relevant if the clock
        // operates based on SystemC time. If hose time is used, this change
        // is ineffectual.
        m_real_timestamp = mktime(&tinfo);
        m_sysc_timestamp = sc_time_stamp();

        struct tm* check = localtime(&m_real_timestamp);
        VCML_ERROR_ON(check->tm_hour != tinfo.tm_hour, "time zone error");

        log_debug("saving current time %llu (%s)", m_real_timestamp,
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
        u64 size = m_addr.length() - CONTROL.get_address();

        if (nbytes > size) {
            nbytes = size;
            log_warn("image file '%s' to big, truncating after %llu bytes",
                     nbytes, filename.c_str());
        }

        file.seekg(0, std::ios::beg);
        file.read((char*)m_nvram, nbytes);
    }

    void rtc1742::save_nvram(const string& filename) {
        log_debug("storing nvram into binary file %s", filename.c_str());
        ofstream file(filename.c_str(), std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            log_warn("cannot open file '%s'", filename.c_str());
            return;
        }

        u64 size = m_addr.length() - CONTROL.get_address();
        file.write((char*)m_nvram, size);
    }

    bool rtc1742::cmd_load(const vector<string>& args, ostream& os) {
        const string filename = args[0];
        load_nvram(filename);
        os << "loaded " << filename << " into nvram";
        return true;
    }

    bool rtc1742::cmd_save(const vector<string>& args, ostream& os) {
        const string filename = args[0];
        save_nvram(filename);
        os << "saved nvram to " << filename;
        return true;
    }

    bool rtc1742::cmd_battery(const vector<string>& args, ostream& os) {
        if (DAY & DAY_BF) {
            os << "clearing battery bit (battery low)";
            DAY &= ~DAY_BF;
        } else {
            os << "setting battery bit (battery good)";
            DAY |= DAY_BF;
        }

        return true;
    }

    tlm_response_status rtc1742::read(const range& addr, void* ptr,
                                      int flags) {
        if (!addr.inside(m_addr))
            return TLM_ADDRESS_ERROR_RESPONSE;
        memcpy(ptr, m_nvram + addr.start, addr.length());
        return TLM_OK_RESPONSE;
    }

    tlm_response_status rtc1742::write(const range& addr, const void* ptr,
                                       int flags) {
        if (!addr.inside(m_addr))
            return TLM_ADDRESS_ERROR_RESPONSE;
        memcpy(m_nvram + addr.start, ptr, addr.length());
        return TLM_OK_RESPONSE;
    }

    u8 rtc1742::write_CONTROL(u8 val) {
        if ((val & CONTROL_R) && !(CONTROL & CONTROL_R)) {
            load_time();
            return CONTROL | CONTROL_R;
        }

        if (!(val & CONTROL_R) && (CONTROL & CONTROL_R)) {
            save_time();
            return CONTROL & ~CONTROL_R;
        }

        return val;
    }

    rtc1742::rtc1742(const sc_module_name& nm, u32 nvramsz):
        peripheral(nm),
        m_nvram(NULL),
        m_addr(0, nvramsz - 9), // need 8 bytes at the end for registers
        m_real_timestamp(time(NULL)),
        m_sysc_timestamp(sc_time_stamp()),
        CONTROL ("CONTROL", nvramsz - 8, 0),
        SECONDS ("SECONDS", nvramsz - 7, SECONDS_OSC),
        MINUTES ("MINUTES", nvramsz - 6, 0),
        HOUR    ("HOUR",    nvramsz - 5, 0),
        DAY     ("DAY",     nvramsz - 4, DAY_BF),
        DATE    ("DATE",    nvramsz - 3, 0),
        MONTH   ("MONTH",   nvramsz - 2, 0),
        YEAR    ("YEAR",    nvramsz - 1, 0),
        IN("IN"),
        sctime("sctime", true),
        nvmem("nvmem", "") {
        m_nvram = new u8[nvramsz];
        if (!nvmem.get().empty())
            load_nvram(nvmem);

        CONTROL.allow_read_write();
        CONTROL.write = &rtc1742::write_CONTROL;

        SECONDS.allow_read_write();
        MINUTES.allow_read_write();
        HOUR.allow_read_write();
        DAY.allow_read_write();
        DATE.allow_read_write();
        MONTH.allow_read_write();
        YEAR.allow_read_write();

        register_command("load", 1, this, &rtc1742::cmd_load,
                         "loads <file> into nvram");
        register_command("save", 1, this, &rtc1742::cmd_save,
                         "stores the contents of nvram into <file>");
        register_command("battery", 1, this, &rtc1742::cmd_battery,
                         "toggle battery bit");
    }

    rtc1742::~rtc1742() {
        if (!nvmem.get().empty())
            save_nvram(nvmem);
        delete [] m_nvram;
    }

    void rtc1742::reset() {
        peripheral::reset();

        CONTROL = CONTROL.get_default();
        SECONDS = SECONDS.get_default();
        MINUTES = MINUTES.get_default();
        HOUR    = HOUR.get_default();
        DAY     = DAY.get_default();
        DATE    = DATE.get_default();
        MONTH   = MONTH.get_default();
        YEAR    = YEAR.get_default();
    }

}}
