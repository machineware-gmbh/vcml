/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/gpio/pl061.h"

namespace vcml {
namespace gpio {

bool pl061::cmd_status(const vector<string>& args, ostream& os) {
    for (int i = 0; i < 8; i++) {
        os << "GPIO" << i;
        if (gpio_in.exists(i))
            os << " input";
        else if (gpio_out.exists(i))
            os << " output";
        else {
            os << " not connected" << std::endl;
            continue;
        }

        u8 mask = 1 << i;
        if (is & mask)
            os << " interrupt";

        if (m_status & mask)
            os << " is high" << std::endl;
        else
            os << " is low" << std::endl;
    }

    os << "  RIS ";
    for (int i = 7; i >= 0; i--)
        os << ((ris & (1 << i)) ? "1" : "0");
    os << std::endl;

    os << "  MIS ";
    for (int i = 7; i >= 0; i--)
        os << ((mis & (1 << i)) ? "1" : "0");

    return true;
}

bool pl061::cmd_set(const vector<string>& args, ostream& os) {
    size_t idx = from_string<size_t>(args[0]);
    if (!gpio_out.exists(idx)) {
        os << "GPIO" << idx << " is not an output";
        return false;
    }

    u8 mask = 1u << idx;
    m_status |= mask;
    update(true);

    os << "GPIO" << idx << " set";
    return true;
}

bool pl061::cmd_clear(const vector<string>& args, ostream& os) {
    size_t idx = from_string<size_t>(args[0]);
    if (!gpio_out.exists(idx)) {
        os << "GPIO" << idx << " is not an output";
        return false;
    }

    u8 mask = 1u << idx;
    m_status &= ~mask;
    update(true);

    os << "GPIO" << idx << " cleared";
    return true;
}

void pl061::write_dir(u8 val, bool debug) {
    dir = val;
    update(debug);
}

void pl061::write_is(u8 val, bool debug) {
    is = val;
    update(debug);
}

void pl061::write_ibe(u8 val, bool debug) {
    ibe = val;
    update(debug);
}

void pl061::write_iev(u8 val, bool debug) {
    iev = val;
    update(debug);
}

void pl061::write_ie(u8 val, bool debug) {
    ie = val;
    update(debug);
}

void pl061::write_ic(u8 val, bool debug) {
    ris = (ris & is) | (ris & ~is & ~val);
    update(debug);
}

void pl061::update(bool debug) {
    u8 outputs = m_status & dir;
    for (int i = 0; i < 8; i++) {
        u8 mask = 1 << i;
        if (gpio_out.exists(i) && !debug)
            gpio_out[i] = !!(outputs & mask);
    }

    u8 level = ~(m_status ^ iev) & ~dir;
    u8 rise = (~m_prev & m_status) & ~dir;
    u8 fall = (m_prev & ~m_status) & ~dir;
    u8 edge = (rise & iev) | (fall & ~iev) | ((rise | fall) & ibe);

    ris = (level & is) | (edge & ~is) | (ris & ~is);
    mis = ris & ie;

    if (!debug)
        intr = mis != 0;

    m_prev = m_status;
}

pl061::pl061(const sc_module_name& nm):
    peripheral(nm),
    m_status(),
    m_prev(),
    dir("dir", 0x400),
    is("is", 0x404),
    ibe("ibe", 0x408),
    iev("iev", 0x40c),
    ie("ie", 0x410),
    ris("ris", 0x414),
    mis("mis", 0x418),
    ic("ic", 0x41c),
    afsel("afsel", 0x420),
    pid("pid", 0xfe0, { 0x61, 0x10, 0x04, 0x00 }),
    cid("cid", 0xff0, { 0x0d, 0xf0, 0x05, 0xb1 }),
    gpio_out("gpio_out"),
    gpio_in("gpio_in"),
    intr("intr"),
    in("in") {
    dir.sync_always();
    dir.allow_read_write();
    dir.on_write(&pl061::write_dir);

    is.sync_always();
    is.allow_read_write();
    is.on_write(&pl061::write_is);

    ibe.sync_always();
    ibe.allow_read_write();
    ibe.on_write(&pl061::write_ibe);

    iev.sync_always();
    iev.allow_read_write();
    iev.on_write(&pl061::write_iev);

    ie.sync_always();
    ie.allow_read_write();
    ie.on_write(&pl061::write_ie);

    ris.sync_always();
    ris.allow_read_only();

    mis.sync_always();
    mis.allow_read_only();

    ic.sync_always();
    ic.allow_write_only();
    ic.on_write(&pl061::write_ic);

    afsel.sync_always();
    afsel.allow_read_write();

    register_command("status", 0, &pl061::cmd_status,
                     "reports the status of all GPIO lines");
    register_command("set", 1, &pl061::cmd_set, "sets the given GPIO line");
    register_command("clear", 1, &pl061::cmd_clear,
                     "clears the given GPIO line");
}

tlm_response_status pl061::read(const range& addr, void* data,
                                const tlm_sbi& sbi) {
    if (addr.end > 0x3ff)
        return peripheral::read(addr, data, sbi);

    u32 mask = extract(addr.start, 2, 8);
    mwr::write_once<u8>(data, m_status & mask);
    return TLM_OK_RESPONSE;
}

tlm_response_status pl061::write(const range& addr, const void* data,
                                 const tlm_sbi& sbi) {
    if (addr.end > 0x3ff)
        return peripheral::write(addr, data, sbi);

    u8 mask = extract(addr.start, 2, 8) & dir;
    u8 val = mwr::read_once<u8>(data);

    m_status = (m_status & ~mask) | (val & mask);
    update(sbi.is_debug);
    return TLM_OK_RESPONSE;
}

void pl061::gpio_notify(const gpio_target_socket& socket) {
    size_t idx = gpio_in.index_of(socket);

    u8 mask = (1 << idx) & ~dir;
    u8 val = socket.read() ? 1 << idx : 0;
    m_status = (m_status & ~mask) | (val & mask);
    update(false);
}

void pl061::end_of_elaboration() {
    peripheral::end_of_elaboration();

    for (size_t i = 0; i < 8; i++) {
        bool is_output = gpio_out.exists(i);
        bool is_input = gpio_in.exists(i);
        if (is_output && is_input)
            VCML_ERROR("gpio%zu wired as both input and output", i);
    }
}

VCML_EXPORT_MODEL(vcml::gpio::pl061, name, args) {
    return new pl061(name);
}

} // namespace gpio
} // namespace vcml
