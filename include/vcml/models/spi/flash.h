/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_SPI_FLASH_H
#define VCML_SPI_FLASH_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/component.h"
#include "vcml/core/model.h"

#include "vcml/protocols/spi.h"
#include "vcml/protocols/gpio.h"

#include "vcml/models/block/disk.h"

namespace vcml {
namespace spi {

struct flash_info {
    const char* name;
    u32 jedec_id;
    u32 jedec_ex;
    u64 sector_size;
    u64 num_sectors;
};

class flash : public component, public spi_host
{
private:
    enum command : u8 {
        CMD_NOP = 0x00,
        CMD_WRITE_ENABLE = 0x06,
        CMD_WRITE_DISABLE = 0x04,
        CMD_READ_IDENT = 0x9f,
        CMD_READ_STATUS = 0x05,
        CMD_WRITE_STATUS = 0x01,
        CMD_READ_DATA = 0x03,
        CMD_READ_DATA_FAST = 0x0b,
        CMD_PAGE_PROGRAM = 0x02,
        CMD_SECTOR_ERASE = 0xd8,
        CMD_BULK_ERASE = 0xc7,
        CMD_POWER_DOWN = 0xb9,
        CMD_POWER_UP = 0xab,
    };

    enum state {
        STATE_IDLE,
        STATE_COLLECTING,
        STATE_READING_BUFFER,
        STATE_READING_STORAGE,
        STATE_PROGRAMMING,
    };

    flash_info m_info;

    command m_command;
    state m_state;

    int m_pos;
    int m_len;
    int m_needed;

    bool m_write_enable;
    u64 m_address;

    u8 m_buffer[16];

    void decode(u8 val);
    void complete();
    void process(spi_payload& tx);

    virtual void spi_transport(const spi_target_socket& socket,
                               spi_payload& tx) override;

public:
    property<string> device;
    property<string> image;
    property<bool> readonly;

    block::disk disk;

    spi_target_socket spi_in;
    gpio_target_socket cs_in;

    size_t sector_size() const { return m_info.sector_size; }
    size_t sector_count() const { return m_info.num_sectors; }
    size_t size() const { return sector_count() * sector_size(); }

    flash(const sc_module_name& name, const string& device = "m25p80");
    virtual ~flash();
    VCML_KIND(spi::flash);
    virtual void reset() override;
};

} // namespace spi
} // namespace vcml

#endif
