/******************************************************************************
 *                                                                            *
 * Copyright 2022 Jan Henrik Weinstock                                        *
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

#include "vcml/models/generic/lm75.h"

namespace vcml {
namespace generic {

bool lm75::cmd_set_temp(const vector<string>& args, ostream& os) {
    double val = 0.0;
    if (sscanf(args[0].c_str(), "%lf", &val) != 1)
        return false;

    temp = to_temp9(val);
    log_info("setting temperature to %.1fC", from_temp9(temp));
    return true;
}

bool lm75::cmd_set_high(const vector<string>& args, ostream& os) {
    double val = 0.0;
    if (sscanf(args[0].c_str(), "%lf", &val) != 1)
        return false;

    high = to_temp9(val);
    log_info("setting high temp threshold to %.1fC", from_temp9(high));
    return true;
}

bool lm75::cmd_set_hyst(const vector<string>& args, ostream& os) {
    double val = 0.0;
    if (sscanf(args[0].c_str(), "%lf", &val) != 1)
        return false;

    hyst = to_temp9(val);
    log_info("setting low temp threshold to %.1fC", from_temp9(hyst));
    return true;
}

void lm75::poll_temp() {
    bool event = false;
    while (true) {
        wait(tlm_global_quantum::instance().get());

        if (config & CFG_SHUTDOWN) {
            alarm = false;
            continue;
        }

        if (config & CFG_INT) { // interrupt mode
            if (!event && temp > high) {
                event = true;
                alarm = true;
            } else if (event && temp < hyst) {
                event = false;
                alarm = true;
            }
        } else { // comparator mode
            if (temp > high)
                alarm = true;
            if (temp < hyst)
                alarm = false;
        }
    }
}

void lm75::load_buffer() {
    switch (pointer & 3) {
    case REG_TEMP:
        log_debug("reading temp 0x%hx (%.1lfC)", temp.get(), from_temp9(temp));
        m_buf[0] = temp >> 1;
        m_buf[1] = temp << 7;
        break;

    case REG_CONF:
        m_buf[0] = config;
        m_buf[1] = 0xff;
        break;

    case REG_HIGH:
        log_debug("reading high 0x%hx (%.1lfC)", high.get(), from_temp9(high));
        m_buf[0] = high >> 1;
        m_buf[1] = high << 7;
        break;

    case REG_HYST:
        log_debug("reading hyst 0x%hx (%.1lfC)", hyst.get(), from_temp9(hyst));
        m_buf[0] = hyst >> 1;
        m_buf[1] = hyst << 7;
        break;
    }
}

void lm75::save_buffer() {
    switch (pointer & 3) {
    case REG_TEMP:
        temp = (u16)m_buf[0] << 1 | m_buf[1] >> 7;
        log_debug("setting temp 0x%hx (%.1lfC)", temp.get(), from_temp9(temp));
        break;

    case REG_CONF:
        config = m_buf[0];
        log_debug("config updated: %s mode, %s interrupt polarity, %u faults",
                  (config & CFG_INT) ? "interrupt" : "comparator",
                  (config & CFG_POL) ? "positive" : "negative",
                  1u << ((config & CFG_FQUEUE) >> 3));
        if (config & CFG_SHUTDOWN)
            log_info("sensor disabled");
        break;

    case REG_HIGH:
        high = (u16)m_buf[0] << 1 | m_buf[1] >> 7;
        log_debug("setting high 0x%hx (%.1lfC)", high.get(), from_temp9(high));
        break;

    case REG_HYST:
        hyst = (u16)m_buf[0] << 1 | m_buf[1] >> 7;
        log_debug("setting hyst 0x%hx (%.1lfC)", hyst.get(), from_temp9(hyst));
        break;
    }
}

u16 lm75::to_temp9(double t) {
    if (t < -55.0)
        return 0b110010010;
    if (t > 125.0)
        return 0b011111010;

    i16 x = t * 2.0;
    return (u16)x & 0x1ff;
}

double lm75::from_temp9(u16 t9) {
    if (t9 > 0xff)
        t9 |= 0xfe00;
    return (i16)t9 / 2.0;
}

lm75::lm75(const sc_module_name& nm, u8 address):
    module(nm),
    i2c_host(),
    m_buf(),
    m_len(),
    pointer("pointer", 0),
    config("config", 0),
    temp("temp", to_temp9(22.5)),
    high("temp_hi", to_temp9(80.0)),
    hyst("temp_lo", to_temp9(75.0)),
    i2c_addr("i2c_addr", address),
    i2c("i2c"),
    alarm("alarm") {
    i2c.set_address(i2c_addr);
    register_command("set_temp", 1, &lm75::cmd_set_temp,
                     "sets the temperature reported by the sensor in C");
    register_command("set_temp_hi", 1, &lm75::cmd_set_high,
                     "sets the temperature threshold for the alarm signal");
    register_command("set_temp_lo", 1, &lm75::cmd_set_hyst,
                     "sets the temperature threshold for clearing alarm");

    SC_HAS_PROCESS(lm75);
    SC_THREAD(poll_temp);
}

void lm75::reset() {
    m_len = 0;

    m_buf[0] = 0xff;
    m_buf[1] = 0xff;
}

i2c_response lm75::i2c_start(const i2c_target_socket& socket,
                             tlm_command command) {
    if (command == TLM_READ_COMMAND) {
        load_buffer();

        if (config & CFG_INT)
            alarm = false;
    }

    m_len = 0;
    return I2C_ACK;
}

i2c_response lm75::i2c_stop(const i2c_target_socket& socket) {
    return I2C_ACK;
}

i2c_response lm75::i2c_read(const i2c_target_socket& socket, u8& data) {
    if (m_len < sizeof(m_buf))
        data = m_buf[m_len++];
    else
        data = 0xff;
    return I2C_ACK;
}

i2c_response lm75::i2c_write(const i2c_target_socket& socket, u8 data) {
    if (m_len == 0) {
        pointer = data;
    } else {
        m_buf[m_len - 1] = data;
        save_buffer();
    }

    m_len++;
    return I2C_ACK;
}

} // namespace generic
} // namespace vcml
