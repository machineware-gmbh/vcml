/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/gpio/sifive.h"

namespace vcml {
namespace gpio {

bool sifive::cmd_status(const vector<string>& args, ostream& os) {
    for (size_t i = 0; i < ngpios; i++) {
        if (m_gpios[i].is_output)
            os << "GPIO" << i << " output " << (int)gpio_out[i] << std::endl;
        else if (m_gpios[i].is_input)
            os << "GPIO" << i << " input " << (int)gpio_in[i] << std::endl;
        else
            os << "GPIO" << i << " disconnected" << std::endl;
    }

    return true;
}

bool sifive::cmd_set(const vector<string>& args, ostream& os) {
    size_t idx = from_string<size_t>(args[0]);
    if (idx >= ngpios || !m_gpios[idx].is_output) {
        os << "GPIO" << idx << " is not connected";
        return false;
    }

    os << "GPIO" << idx << " set";
    gpio_out[idx] = true;
    return true;
}

bool sifive::cmd_clear(const vector<string>& args, ostream& os) {
    size_t idx = from_string<size_t>(args[0]);
    if (idx >= ngpios || !m_gpios[idx].is_output) {
        os << "GPIO" << idx << " is not connected";
        return false;
    }

    os << "GPIO" << idx << " cleared";
    gpio_out[idx] = false;
    return true;
}

void sifive::update() {
    for (size_t i = 0; i < ngpios; i++) {
        u32 mask = bit(i);

        if (m_gpios[i].is_output) {
            u32 out = output_en & (output_val ^ out_xor);
            gpio_out[i] = out & mask;
        }

        if (m_gpios[i].is_input) {
            bool en = input_en & mask;
            bool prev_val = m_gpios[i].prev_val;
            bool curr_val = m_gpios[i].curr_val && en;
            if (!prev_val && curr_val)
                rise_ip |= mask;
            if (prev_val && !curr_val)
                fall_ip |= mask;
            if (curr_val)
                high_ip |= mask;
            if (!curr_val)
                low_ip |= mask;
        }
    }

    update_irq();
}

void sifive::update_irq() {
    u32 pending = 0;
    pending |= rise_ie & rise_ip;
    pending |= fall_ie & fall_ip;
    pending |= high_ie & high_ip;
    pending |= low_ie & low_ip;

    for (size_t i = 0; i < ngpios; i++)
        if (irq.exists(i))
            irq[i] = pending & bit(i);
}

u32 sifive::read_input_val() {
    u32 input = 0;
    for (size_t i = 0; i < ngpios; i++)
        if (gpio_in.exists(i) && gpio_in[i])
            input |= bit(i);

    return input & input_en;
}

void sifive::write_input_en(u32 val) {
    input_en = val;
    update();
}

void sifive::write_output_en(u32 val) {
    output_en = val;
    update();
}

void sifive::write_output_val(u32 val) {
    output_val = val;
    update();
}

void sifive::write_rise_ie(u32 val) {
    rise_ie = val;
    update();
}

void sifive::write_rise_ip(u32 val) {
    rise_ip &= ~val;
    update_irq();
}

void sifive::write_fall_ie(u32 val) {
    fall_ie = val;
    update();
}

void sifive::write_fall_ip(u32 val) {
    fall_ip &= ~val;
    update_irq();
}

void sifive::write_high_ie(u32 val) {
    high_ie = val;
    update();
}

void sifive::write_high_ip(u32 val) {
    high_ip &= ~val;
    update_irq();
}

void sifive::write_low_ie(u32 val) {
    low_ie = val;
    update();
}

void sifive::write_low_ip(u32 val) {
    low_ip &= ~val;
    update_irq();
}

void sifive::write_out_xor(u32 val) {
    out_xor = val;
    update();
}

sifive::sifive(const sc_module_name& nm, size_t n):
    peripheral(nm),
    m_gpios(),
    ngpios("ngpios", n),
    input_val("input_val", 0x00),
    input_en("input_en", 0x04),
    output_en("output_en", 0x08),
    output_val("output_val", 0x0c),
    pue("pue", 0x10),
    ds("ds", 0x14),
    rise_ie("rise_ie", 0x18),
    rise_ip("rise_ip", 0x1c),
    fall_ie("fall_ie", 0x20),
    fall_ip("fall_ip", 0x24),
    high_ie("high_ie", 0x28),
    high_ip("high_ip", 0x2c),
    low_ie("low_ie", 0x30),
    low_ip("low_ip", 0x34),
    out_xor("out_xor", 0x40),
    irq("irq"),
    gpio_out("gpio_out"),
    gpio_in("gpio_in"),
    in("in") {
    VCML_ERROR_ON(ngpios > 32u, "ngpios exceeds limit of 32");
    register_command("status", 0, &sifive::cmd_status,
                     "reports the status of all GPIO lines");
    register_command("set", 1, &sifive::cmd_set, "sets the given GPIO line");
    register_command("clear", 1, &sifive::cmd_clear,
                     "clears the given GPIO line");

    for (auto& info : m_gpios) {
        info.is_output = false;
        info.is_input = false;
        info.curr_val = false;
        info.prev_val = false;
    }

    input_val.allow_read_only();
    input_val.sync_always();
    input_val.on_read(&sifive::read_input_val);

    input_en.allow_read_write();
    input_en.sync_always();
    input_en.on_write(&sifive::write_input_en);

    output_en.allow_read_write();
    output_en.sync_always();
    output_en.on_write(&sifive::write_output_en);

    output_val.allow_read_write();
    output_val.sync_always();
    output_val.on_write(&sifive::write_output_val);

    pue.allow_read_write();
    pue.sync_never();

    ds.allow_read_write();
    ds.sync_never();

    rise_ie.allow_read_write();
    rise_ie.sync_always();
    rise_ie.on_write(&sifive::write_rise_ie);

    rise_ip.allow_read_write();
    rise_ip.sync_always();
    rise_ip.on_write(&sifive::write_rise_ip);

    fall_ie.allow_read_write();
    fall_ie.sync_always();
    fall_ie.on_write(&sifive::write_fall_ie);

    fall_ip.allow_read_write();
    fall_ip.sync_always();
    fall_ip.on_write(&sifive::write_fall_ip);

    high_ie.allow_read_write();
    high_ie.sync_always();
    high_ie.on_write(&sifive::write_high_ie);

    high_ip.allow_read_write();
    high_ip.sync_always();
    high_ip.on_write(&sifive::write_high_ip);

    low_ie.allow_read_write();
    low_ie.sync_always();
    low_ie.on_write(&sifive::write_low_ie);

    low_ip.allow_read_write();
    low_ip.sync_always();
    low_ip.on_write(&sifive::write_low_ip);

    out_xor.allow_read_write();
    out_xor.sync_always();
    out_xor.on_write(&sifive::write_out_xor);
}

void sifive::reset() {
    peripheral::reset();

    for (auto& info : m_gpios) {
        info.curr_val = false;
        info.prev_val = false;
    }
}

void sifive::gpio_notify(const gpio_target_socket& socket) {
    size_t i = gpio_in.index_of(socket);
    m_gpios[i].prev_val = m_gpios[i].curr_val;
    m_gpios[i].curr_val = socket.read();
    update();
}

void sifive::end_of_elaboration() {
    peripheral::end_of_elaboration();

    for (size_t i = 0; i < ngpios; i++) {
        m_gpios[i].is_output = gpio_out.exists(i);
        m_gpios[i].is_input = gpio_in.exists(i);
        if (m_gpios[i].is_output && m_gpios[i].is_input)
            VCML_ERROR("gpio%zu wired as both input and output", i);
    }
}

VCML_EXPORT_MODEL(vcml::gpio::sifive, name, args) {
    return new sifive(name);
}

} // namespace gpio
} // namespace vcml
