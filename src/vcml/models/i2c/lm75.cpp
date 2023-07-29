/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/i2c/lm75.h"

namespace vcml {
namespace i2c {

constexpr u16 to_temp9(double t) {
    if (t < -55.0)
        return 0b110010010;
    if (t > 125.0)
        return 0b011111010;

    i16 x = t * 2.0;
    return (u16)x & 0x1ff;
}

constexpr double from_temp9(u16 t9) {
    if (t9 > 0xff)
        t9 |= 0xfe00;
    return (i16)t9 / 2.0;
}

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
    while (true) {
        sc_time quantum = tlm_global_quantum::instance().get();
        sc_time i2c_clk = sc_time(1.0 / (100 * kHz), SC_SEC);
        wait(max(quantum, i2c_clk));
        irq_update();
    }
}

void lm75::irq_update() {
    if (config & CFG_SHUTDOWN) {
        alarm = false;
        return;
    }

    if (config & CFG_INT) { // interrupt mode
        if (!m_evt && temp > high) {
            m_evt = true;
            alarm = true;
        } else if (m_evt && temp < hyst) {
            m_evt = false;
            alarm = true;
        }
    } else { // comparator mode
        if (temp > high)
            alarm = true;
        if (temp < hyst)
            alarm = false;
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

    irq_update();
}

lm75::lm75(const sc_module_name& nm, u8 address):
    module(nm),
    i2c_host(),
    m_buf(),
    m_len(),
    m_evt(false),
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

VCML_EXPORT_MODEL(vcml::i2c::lm75, name, args) {
    return new lm75(name);
}

} // namespace i2c
} // namespace vcml
