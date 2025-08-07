/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/i2c/ads1015.h"

namespace vcml {
namespace i2c {

using ADS1015_COMP_QUE = field<0, 2, u16>;
using ADS1015_DR = field<5, 3, u16>;
using ADS1015_PGA = field<9, 3, u16>;
using ADS1015_MUX = field<12, 3, u16>;

enum ads1015_comp_que : u16 {
    ADS1015_COMP_QUE_ONE = 0,
    ADS1015_COMP_QUE_TWO = 1,
    ADS1015_COMP_QUE_FOUR = 2,
    ADS1015_COMP_QUE_DISABLED = 3,
};

static constexpr size_t ads1015_get_comp_que(u16 config) {
    switch (get_field<ADS1015_COMP_QUE>(config)) {
    case ADS1015_COMP_QUE_ONE:
        return 1;
    case ADS1015_COMP_QUE_TWO:
        return 2;
    case ADS1015_COMP_QUE_FOUR:
        return 4;
    case ADS1015_COMP_QUE_DISABLED:
    default:
        return 0;
    }
}

enum ads1015_data_rate : u16 {
    ADS1015_128SPS = 0,
    ADS1015_250SPS = 1,
    ADS1015_490SPS = 2,
    ADS1015_920SPS = 3,
    ADS1015_1600SPS = 4,
    ADS1015_2400SPS = 5,
    ADS1015_3300SPS = 6,
};

static constexpr int ads1015_data_rate(u16 config) {
    switch (get_field<ADS1015_DR>(config)) {
    case ADS1015_128SPS:
        return 128;
    case ADS1015_250SPS:
        return 250;
    case ADS1015_490SPS:
        return 490;
    case ADS1015_920SPS:
        return 920;
    case ADS1015_1600SPS:
        return 1600;
    case ADS1015_2400SPS:
        return 2400;
    case ADS1015_3300SPS:
    default:
        return 3300;
    }
}

static sc_time ads1015_sample_delta(u16 config) {
    return sc_time(1.0 / ads1015_data_rate(config), SC_SEC);
}

enum ads1015_pga_fsr : u16 {
    ADS1015_FSR_6_144V = 0,
    ADS1015_FSR_4_096V = 1,
    ADS1015_FSR_2_048V = 2,
    ADS1015_FSR_1_024V = 3,
    ADS1015_FSR_0_512V = 4,
    ADS1015_FSR_0_256V = 5,
};

static constexpr int ads1015_fsr(u16 config) {
    switch (get_field<ADS1015_PGA>(config)) {
    case ADS1015_FSR_6_144V:
        return 6144;
    case ADS1015_FSR_4_096V:
        return 4096;
    case ADS1015_FSR_2_048V:
        return 2048;
    case ADS1015_FSR_1_024V:
        return 1024;
    case ADS1015_FSR_0_512V:
        return 512;
    case ADS1015_FSR_0_256V:
    default:
        return 256;
    }
}

enum ads1015_bits : u16 {
    ADS1015_COMP_LAT = bit(2),
    ADS1015_COMP_POL = bit(3),
    ADS1015_COMP_MODE = bit(4),
    ADS1015_MODE = bit(8),
    ADS1015_OS = bit(15),

    ADS1015_CONFIG_RESET = ADS1015_COMP_QUE::set(ADS1015_COMP_QUE_DISABLED) |
                           ADS1015_DR::set(ADS1015_1600SPS) | ADS1015_MODE |
                           ADS1015_PGA::set(ADS1015_FSR_2_048V) | ADS1015_OS,

    ADS1015_THRESH_LO_RESET = 0x8000,
    ADS1015_THRESH_HI_RESET = 0x7fff,
};

static constexpr bool ads1015_is_single(u16 config) {
    return config & ADS1015_MODE;
}

static constexpr bool ads1015_is_continuous(u16 config) {
    return !ads1015_is_single(config);
}

static constexpr bool ads1015_is_window_comp(u16 config) {
    return config & ADS1015_COMP_MODE;
}

static constexpr bool ads1015_alert_latched(u16 config) {
    return config & ADS1015_COMP_LAT;
}

static constexpr bool ads1015_alert_active(u16 config) {
    return config & ADS1015_COMP_POL;
}

static constexpr bool ads1015_alert_inactive(u16 config) {
    return !ads1015_alert_active(config);
}

double ads1015::read_voltage(int channel) {
    switch (channel) {
    case 0:
        return ain0 - ain1;
    case 1:
        return ain0 - ain3;
    case 2:
        return ain1 - ain3;
    case 3:
        return ain2 - ain3;
    case 4:
        return ain0;
    case 5:
        return ain1;
    case 6:
        return ain2;
    case 7:
        return ain3;
    default:
        return INFINITY;
    }
}

void ads1015::sample_data() {
    u16 config = m_regs[CONFIG];
    int channel = get_field<ADS1015_MUX>(config);
    int fsr = ads1015_fsr(config);
    int voltage = read_voltage(channel) * 1000.0; // mV
    if (voltage > fsr)
        voltage = fsr - 1;
    if (voltage < -fsr)
        voltage = -fsr;

    u16 convert = voltage * 2048 / fsr;
    m_regs[CONVERT] = deposit(m_regs[CONVERT], 4, 12, convert);

    int comp_que = ads1015_get_comp_que(m_regs[CONFIG]);
    if (comp_que == 0) {
        alert = ads1015_alert_inactive(config);
        return;
    }

    u16 thresh_lo = extract(m_regs[THRESH_LO], 4, 12);
    u16 thresh_hi = extract(m_regs[THRESH_HI], 4, 12);

    if (thresh_lo > thresh_hi) {
        log_warn("lower threshold (%hu) exceeds upper (%hu)", thresh_lo,
                 thresh_hi);
    }

    if (ads1015_is_window_comp(config)) {
        if (convert > thresh_hi || convert < thresh_lo)
            m_comp_que++;
        else
            m_comp_que = 0;
    } else {
        if (convert > thresh_hi || m_comp_que > 0)
            m_comp_que++;
        if (convert < thresh_lo)
            m_comp_que = 0;
    }

    m_comp_que = min(m_comp_que, comp_que);

    if (m_comp_que >= comp_que)
        alert = ads1015_alert_active(config);
    else if (!ads1015_alert_latched(config))
        alert = ads1015_alert_inactive(config);
}

void ads1015::sample_thread() {
    while (true) {
        sc_time quantum = tlm_global_quantum::instance().get();
        sc_time tsample = ads1015_sample_delta(m_regs[CONFIG]);

        wait(max(quantum, tsample));

        if (ads1015_is_continuous(m_regs[CONFIG]))
            sample_data();
    }
}

void ads1015::update_config() {
    u16& config = m_regs[CONFIG];

    log_debug("configuration updated:");
    if (ads1015_is_continuous(config))
        log_debug("      mode: continuous");
    else
        log_debug("      mode: single-shot");
    log_debug("   channel: %hu", get_field<ADS1015_MUX>(config));
    log_debug("     range: +-%umV", ads1015_fsr(config));
    log_debug(" data rate: %uSPS", ads1015_data_rate(config));

    if (int compq = ads1015_get_comp_que(config)) {
        log_debug("     alert: after %u samples", compq);
        log_debug("     latch: %s", config & ADS1015_COMP_LAT ? "yes" : "no");
        log_debug("  polartiy: active %s",
                  config & ADS1015_COMP_POL ? "high" : "low");
        log_debug("      mode: %s",
                  config & ADS1015_COMP_MODE ? "window" : "traditional");
    } else {
        log_debug("     alert: disabled");
    }

    if (config & ADS1015_OS) {
        config &= ~ADS1015_OS;
        sample_data();
    } else if (!polling && ads1015_is_continuous(config)) {
        sample_data();
    }
}

void ads1015::post_read() {
    switch (m_reg_ptr) {
    case CONFIG: {
        u16 config = m_regs[CONFIG];
        if (ads1015_get_comp_que(config) && ads1015_alert_latched(config))
            alert = ads1015_alert_inactive(config);
        break;
    }

    default:
        break;
    }
}

void ads1015::post_write() {
    u32 regval = m_regs[m_reg_ptr];
    switch (m_reg_ptr) {
    case CONFIG:
        update_config();
        break;
    case THRESH_LO:
        log_debug("setup lower threshold %u", regval >> 4);
        break;
    case THRESH_HI:
        log_debug("setup upper threshold %u", regval >> 4);
        break;
    }
}

bool ads1015::cmd_get_voltage(const vector<string>& args, ostream& os) {
    if (args.empty()) {
        os << mkstr("ain0: %.3f\n", ain0.get());
        os << mkstr("ain1: %.3f\n", ain1.get());
        os << mkstr("ain2: %.3f\n", ain2.get());
        os << mkstr("ain3: %.3f", ain3.get());
        return true;
    }

    string channel = to_lower(args[0]);
    if (channel == "ain0") {
        os << mkstr("ain0: %.3f", ain0.get());
        return true;
    }

    if (channel == "ain1") {
        os << mkstr("ain1: %.3f", ain1.get());
        return true;
    }

    if (channel == "ain2") {
        os << mkstr("ain2: %.3f", ain2.get());
        return true;
    }

    if (channel == "ain3") {
        os << mkstr("ain3: %.3f", ain3.get());
        return true;
    }

    os << "unknown channel: " << channel << "\n"
       << "use: ain0, ain1, ain2, or ain3";
    return false;
}

bool ads1015::cmd_set_voltage(const vector<string>& args, ostream& os) {
    string channel = to_lower(args[0]);
    if (channel == "ain0") {
        ain0 = from_string<double>(args[1]);
        return true;
    }

    if (channel == "ain1") {
        ain1 = from_string<double>(args[1]);
        return true;
    }

    if (channel == "ain2") {
        ain2 = from_string<double>(args[1]);
        return true;
    }

    if (channel == "ain3") {
        ain3 = from_string<double>(args[1]);
        return true;
    }

    os << "unknown channel: " << channel << "\n"
       << "use: ain0, ain1, ain2, or ain3";
    return false;
}

ads1015::ads1015(const sc_module_name& nm, u8 addr):
    module(nm),
    i2c_host(),
    m_state(IDLE),
    m_reg_ptr(),
    m_regs(),
    m_comp_que(),
    polling("polling", false),
    ain0("ain0", 0.5),
    ain1("ain1", 1.0),
    ain2("ain2", 1.3),
    ain3("ain3", -0.7),
    i2c_addr("i2c_addr", addr),
    i2c_in("i2c_in"),
    alert("alert") {
    i2c_in.set_address(i2c_addr);

    m_regs[CONVERT] = 0;
    m_regs[CONFIG] = ADS1015_CONFIG_RESET;
    m_regs[THRESH_LO] = ADS1015_THRESH_LO_RESET;
    m_regs[THRESH_HI] = ADS1015_THRESH_HI_RESET;

    if (polling) {
        SC_HAS_PROCESS(ads1015);
        SC_THREAD(sample_thread);
    } else {
        sample_data();
    }

    register_command("get_voltage", 0, this, &ads1015::cmd_get_voltage,
                     "returns the voltage on a given channel");
    register_command("set_voltage", 1, this, &ads1015::cmd_set_voltage,
                     "sets the voltage on a given channel");
}

ads1015::~ads1015() {
    // nothing to do
}

void ads1015::before_end_of_elaboration() {
    if (!alert.is_bound())
        alert.stub();
}

void ads1015::start_of_simulation() {
    u16 config = m_regs[CONFIG];
    alert = ads1015_alert_inactive(config);
}

void ads1015::session_resume() {
    if (!polling && ads1015_is_continuous(m_regs[CONFIG]))
        sample_data();
}

i2c_response ads1015::i2c_start(const i2c_target_socket& socket,
                                tlm_command command) {
    if (m_state == IDLE)
        m_state = STARTED;
    return I2C_ACK;
}

i2c_response ads1015::i2c_stop(const i2c_target_socket& socket) {
    m_state = IDLE;
    return I2C_ACK;
}

i2c_response ads1015::i2c_read(const i2c_target_socket& socket, u8& data) {
    switch (m_state) {
    case STARTED:
        data = m_reg_ptr;
        m_state = MSB;
        return I2C_ACK;
    case MSB:
        data = m_regs[m_reg_ptr] >> 8;
        m_state = LSB;
        return I2C_ACK;
    case LSB:
        data = m_regs[m_reg_ptr];
        if (m_reg_ptr == CONVERT)
            post_read();
        m_state = MSB;
        return I2C_ACK;
    default:
        return I2C_NACK;
    }
}

i2c_response ads1015::i2c_write(const i2c_target_socket& socket, u8 data) {
    switch (m_state) {
    case STARTED:
        m_reg_ptr = data & 0x3;
        m_state = MSB;
        return I2C_ACK;
    case MSB:
        m_regs[m_reg_ptr] = deposit(m_regs[m_reg_ptr], 8, 8, data);
        m_state = LSB;
        return I2C_ACK;
    case LSB:
        m_regs[m_reg_ptr] = deposit(m_regs[m_reg_ptr], 0, 8, data);
        m_state = MSB;
        post_write();
        return I2C_ACK;
    default:
        return I2C_NACK;
    }
}

VCML_EXPORT_MODEL(vcml::i2c::ads1015, name, args) {
    u8 addr = 0x48;
    if (!args.empty())
        addr = from_string<u8>(args[0]);
    return new ads1015(name, addr);
}

} // namespace i2c
} // namespace vcml
