/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_SD_CARD_H
#define VCML_SD_CARD_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/component.h"
#include "vcml/core/model.h"

#include "vcml/protocols/sd.h"
#include "vcml/models/block/disk.h"

namespace vcml {
namespace sd {

class card : public component, public sd_host
{
private:
    bool m_spi;
    bool m_do_crc;
    size_t m_blklen;

    u32 m_status; // Card Status Register
    u32 m_hvs;    // Host Voltage Supply
    u16 m_rca;    // Relative Card Address
    u32 m_ocr;    // Operations Conditions Register
    u8 m_cid[16]; // Card Identification Register
    u8 m_csd[16]; // Card-Specific Data Register
    u8 m_scr[8];  // Card Configuration Register
    u8 m_sts[64]; // Status Memory
    u8 m_swf[66]; // Switch-Function Memory

    u8* m_bufptr;
    u8* m_bufend;
    u8 m_buffer[514]; // Block length + CRC16

    u8 m_curcmd;
    size_t m_curoff;
    size_t m_numblk;

    enum state {
        IDLE = 0,
        READY = 1,
        IDENTIFICATION = 2,
        STAND_BY = 3,
        TRANSFER = 4,
        SENDING = 5,
        RECEIVING = 6,
        PROGRAMMING = 7,
        DISCONNECTED = 8,
    };

    state m_state;

    void make_r0(sd_command& tx);
    void make_r1(sd_command& tx);
    void make_r2(sd_command& tx);
    void make_r3(sd_command& tx);
    void make_r6(sd_command& tx);
    void make_r7(sd_command& tx);

    void make_r1_spi(sd_command& tx);
    void make_r2_spi(sd_command& tx);
    void make_r3_spi(sd_command& tx);
    void make_r7_spi(sd_command& tx);

    void setup_tx(u8* data, size_t len);
    void setup_rx(u8* data, size_t len);

    void setup_tx_blk(size_t offset);
    void setup_rx_blk(size_t offset);

    void init_ocr();
    void init_cid();
    void init_csd_sdsc();
    void init_csd_sdhc();
    void init_csd();
    void init_scr();
    void init_sts();

    void update_status();

    void switch_function(u32 arg);

    sd_status do_command(sd_command& tx);
    sd_status do_normal_command(sd_command& tx);
    sd_status do_application_command(sd_command& tx);

    sd_status_tx do_data_read(u8& val);
    sd_status_rx do_data_write(u8 val);

    // disabled
    card();
    card(const card&);

public:
    enum status_bits {
        OUT_OF_RANGE = 1 << 31,
        ADDRESS_ERROR = 1 << 30,
        BLOCK_LEN_ERROR = 1 << 29,
        ERASE_SEQ_ERROR = 1 << 28,
        ERASE_PARAM = 1 << 27,
        WP_VIOLATION = 1 << 26,
        CARD_IS_LOCKED = 1 << 25,
        LOCK_UNLOCK_FAILED = 1 << 24,
        COM_CRC_ERROR = 1 << 23,
        ILLEGAL_COMMAND = 1 << 22,
        CARD_ECC_FAILED = 1 << 21,
        CC_ERROR = 1 << 20,
        ERROR = 1 << 19,
        CSD_OVERWRITE = 1 << 16,
        WP_ERASE_SKIP = 1 << 15,
        CARD_ECC_DISABLED = 1 << 14,
        ERASE_RESET = 1 << 13,
        CURRENT_STATE = 0xf << 9,
        READY_FOR_DATA = 1 << 8,
        FX_EVENT = 1 << 6,
        APP_CMD = 1 << 5,
        AKE_SEQ_ERROR = 1 << 3
    };

    enum spi_status_bits {
        SPI_IN_IDLE = 1 << 0,
        SPI_ERASE_RESET = 1 << 1,
        SPI_ILLEGAL_COMMAND = 1 << 2,
        SPI_COM_CRC_ERROR = 1 << 3,
        SPI_ERASE_SEQ_ERROR = 1 << 4,
        SPI_ADDRESS_ERROR = 1 << 5,
        SPI_PARAMETER_ERROR = 1 << 6,
    };

    enum spi_status2_bits {
        SPI_CARD_IS_LOCKED = 1 << 0,
        SPI_WP_ERASE_SKIP = 1 << 1,
        SPI_ERROR = 1 << 2,
        SPI_CC_ERROR = 1 << 3,
        SPI_CARD_ECC_FAILED = 1 << 4,
        SPI_WP_VIOLATION = 1 << 5,
        SPI_ERASE_PARAM = 1 << 6,
        SPI_OUT_OF_RANGE = 1 << 7,
    };

    enum ocr_bits {
        OCR_VDD_27_28 = 1 << 15,
        OCR_VDD_28_29 = 1 << 16,
        OCR_VDD_29_30 = 1 << 17,
        OCR_VDD_30_31 = 1 << 18,
        OCR_VDD_31_32 = 1 << 19,
        OCR_VDD_32_33 = 1 << 20,
        OCR_VDD_33_34 = 1 << 21,
        OCR_VDD_34_35 = 1 << 22,
        OCR_VDD_35_36 = 1 << 23,
        OCR_S18A = 1 << 24,
        OCR_CCS = 1 << 30,
        OCR_POWERED_UP = 1 << 31,
    };

    property<string> image;
    property<bool> readonly;

    block::disk disk;

    sd_target_socket sd_in;

    card(const sc_module_name& name, const string& image = "",
         bool readonly = false);
    virtual ~card();
    virtual void reset() override;
    VCML_KIND(sd::card);

    bool is_sdhc() const { return m_ocr & OCR_CCS; }
    bool is_sdsc() const { return !is_sdhc(); }

    virtual void sd_transport(const sd_target_socket& socket,
                              sd_command& cmd) override;
    virtual void sd_transport(const sd_target_socket& socket,
                              sd_data& data) override;
};

inline void card::update_status() {
    m_status &= ~(0xf << 9);
    m_status |= m_state << 9;
}

} // namespace sd
} // namespace vcml

#endif
