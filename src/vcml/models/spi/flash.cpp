/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/spi/flash.h"

namespace vcml {
namespace spi {

static flash_info devices[] = {
    { "m25p05", 0x202010, 0, 32 * 1024, 2 },
    { "m25p10", 0x202011, 0, 32 * 1024, 4 },
    { "m25p20", 0x202012, 0, 64 * 1024, 4 },
    { "m25p40", 0x202013, 0, 64 * 1024, 8 },
    { "m25p80", 0x202014, 0, 64 * 1024, 16 },
    { "m25p16", 0x202015, 0, 64 * 1024, 32 },
    { "m25p32", 0x202016, 0, 64 * 1024, 64 },
    { "m25p64", 0x202017, 0, 64 * 1024, 128 },
    { "m25p128", 0x202018, 0, 256 * 1024, 64 },
};

flash_info lookup_device(const string& device) {
    for (const flash_info& info : devices)
        if (device == info.name)
            return info;
    VCML_ERROR("unknown flash device: %s", device.c_str());
}

enum sr_status_bits : u8 {
    SR_WIP = bit(0),  // work in progress
    SR_WEL = bit(1),  // write enable latch
    SR_BP0 = bit(2),  // block protect 0
    SR_BP1 = bit(3),  // block protect 1
    SR_BP2 = bit(4),  // block protect 2
    SR_SRWD = bit(7), // status register write protect
};

void flash::decode(u8 val) {
    m_command = (command)val;
    switch (m_command) {
    case CMD_WRITE_ENABLE:
        m_write_enable = true;
        m_state = STATE_IDLE;
        break;

    case CMD_WRITE_DISABLE:
        m_write_enable = false;
        m_state = STATE_IDLE;
        break;

    case CMD_READ_IDENT:
        m_buffer[0] = m_info.jedec_id >> 16;
        m_buffer[1] = m_info.jedec_id >> 8;
        m_buffer[2] = m_info.jedec_id;
        m_buffer[3] = m_info.jedec_ex >> 8;
        m_buffer[4] = m_info.jedec_ex;
        m_state = STATE_READING_BUFFER;
        m_len = 5;
        m_pos = 0;
        break;

    case CMD_READ_STATUS:
        m_buffer[0] = m_write_enable ? 0 : SR_SRWD;
        m_state = STATE_READING_BUFFER;
        m_len = 1;
        m_pos = 0;
        break;

    case CMD_WRITE_STATUS:
        if (m_write_enable) {
            m_state = STATE_COLLECTING;
            m_len = 0;
            m_pos = 0;
            m_needed = 1;
        } else {
            m_state = STATE_IDLE;
            m_len = 0;
            m_pos = 0;
            m_needed = 0;
        }
        break;

    case CMD_READ_DATA_FAST:
        m_state = STATE_COLLECTING;
        m_len = 0;
        m_pos = 0;
        m_needed = 4;
        break;

    case CMD_READ_DATA:
    case CMD_PAGE_PROGRAM:
    case CMD_SECTOR_ERASE:
        m_state = STATE_COLLECTING;
        m_len = 0;
        m_pos = 0;
        m_needed = 3;
        break;

    case CMD_BULK_ERASE:
        disk.seek(0);
        disk.wzero(size());
        m_state = STATE_IDLE;
        break;

    case CMD_POWER_DOWN:
    case CMD_POWER_UP:
    case CMD_NOP:
        m_state = STATE_IDLE;
        break;

    default:
        log_error("unknown command code (%02hhx)", m_command);
        m_command = CMD_NOP;
        break;
    }
}

void flash::complete() {
    m_address = m_buffer[0] << 16 | m_buffer[1] << 8 | m_buffer[2];

    switch (m_command) {
    case CMD_READ_DATA:
    case CMD_READ_DATA_FAST:
        m_state = STATE_READING_STORAGE;
        break;

    case CMD_PAGE_PROGRAM:
        m_state = STATE_PROGRAMMING;
        break;

    case CMD_SECTOR_ERASE:
        disk.seek(m_address);
        disk.wzero(sector_size());
        m_state = STATE_IDLE;
        break;

    default:
        log_error("unknown command code (0x%02x)", m_command);
        break;
    }
}

void flash::process(spi_payload& tx) {
    switch (m_state) {
    case STATE_IDLE:
        decode(tx.mosi);
        break;

    case STATE_COLLECTING:
        m_buffer[m_len] = tx.mosi;
        m_len++;
        if (m_len == m_needed)
            complete();
        break;

    case STATE_PROGRAMMING:
        if (m_write_enable) {
            disk.seek(m_address);
            disk.write(&tx.mosi, 1);
            m_address = (m_address + 1) % size();
        }
        break;

    case STATE_READING_STORAGE:
        disk.seek(m_address);
        disk.read(&tx.miso, 1);
        m_address = (m_address + 1) % size();
        break;

    case STATE_READING_BUFFER:
        tx.miso = m_buffer[m_pos++];
        if (m_pos == m_len) {
            m_pos = m_len = 0;
            m_state = STATE_IDLE;
        }
        break;

    default:
        VCML_ERROR("unknown device state %u", (int)m_state);
    }
}

void flash::spi_transport(const spi_target_socket& socket, spi_payload& tx) {
    if (cs_in)
        process(tx);
}

flash::flash(const sc_module_name& nm, const string& dev):
    component(nm),
    spi_host(),
    m_info(),
    m_command(CMD_NOP),
    m_state(STATE_IDLE),
    m_pos(),
    m_len(),
    m_needed(),
    m_write_enable(),
    m_address(),
    m_buffer(),
    device("device", dev),
    image("image", ""),
    readonly("readonly", false),
    disk("disk", image, readonly),
    spi_in("spi_in"),
    cs_in("cs_in") {
    m_info = lookup_device(device);
}

flash::~flash() {
    // nothing to do
}

void flash::reset() {
    component::reset();
    m_pos = 0;
    m_len = 0;
    m_needed = 0;
    m_state = STATE_IDLE;
    m_command = CMD_NOP;
    disk.flush();
}

VCML_EXPORT_MODEL(vcml::spi::m25p05, name, args) {
    return new flash(name, "m25p05");
}

VCML_EXPORT_MODEL(vcml::spi::m25p10, name, args) {
    return new flash(name, "m25p10");
}

VCML_EXPORT_MODEL(vcml::spi::m25p20, name, args) {
    return new flash(name, "m25p20");
}

VCML_EXPORT_MODEL(vcml::spi::m25p40, name, args) {
    return new flash(name, "m25p40");
}

VCML_EXPORT_MODEL(vcml::spi::m25p80, name, args) {
    return new flash(name, "m25p80");
}

VCML_EXPORT_MODEL(vcml::spi::m25p16, name, args) {
    return new flash(name, "m25p16");
}

VCML_EXPORT_MODEL(vcml::spi::m25p32, name, args) {
    return new flash(name, "m25p32");
}

VCML_EXPORT_MODEL(vcml::spi::m25p64, name, args) {
    return new flash(name, "m25p64");
}

VCML_EXPORT_MODEL(vcml::spi::m25p128, name, args) {
    return new flash(name, "m25p128");
}

} // namespace spi
} // namespace vcml
