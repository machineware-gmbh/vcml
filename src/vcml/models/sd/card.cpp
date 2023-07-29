/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/sd/card.h"

#define SDHC_BLKLEN 512

namespace vcml {
namespace sd {

static bool check_crc7(const sd_command& tx) {
    u8 buffer[5] = {
        (u8)(tx.opcode | 0x40), // start bit = 0, transmission bit = 1
        (u8)(tx.argument >> 24), (u8)(tx.argument >> 16),
        (u8)(tx.argument >> 8),  (u8)(tx.argument >> 0),
    };

    return (crc7(buffer, sizeof(buffer)) | 1) == tx.crc;
}

void card::make_r0(sd_command& tx) {
    tx.resp_len = 0;
}

void card::make_r1(sd_command& tx) {
    if (m_spi) {
        make_r1_spi(tx);
        return;
    }

    update_status();
    tx.response[0] = tx.opcode;
    tx.response[1] = m_status >> 24;
    tx.response[2] = m_status >> 16;
    tx.response[3] = m_status >> 8;
    tx.response[4] = m_status >> 0;
    tx.response[5] = crc7(tx.response, 5) | 1;
    tx.resp_len = 6;

    m_status &= ~(OUT_OF_RANGE | ADDRESS_ERROR | BLOCK_LEN_ERROR |
                  ERASE_SEQ_ERROR | ERASE_PARAM | WP_VIOLATION |
                  LOCK_UNLOCK_FAILED | CARD_ECC_FAILED | CC_ERROR | ERROR |
                  CSD_OVERWRITE | WP_ERASE_SKIP | ERASE_RESET | AKE_SEQ_ERROR);
}

void card::make_r2(sd_command& tx) {
    if (m_spi) {
        make_r2_spi(tx);
        return;
    }

    tx.response[0] = 0x3f;
    switch (tx.opcode) {
    case 2:
    case 10:
        memcpy(tx.response + 1, m_cid, sizeof(m_cid));
        break;
    case 9:
        memcpy(tx.response + 1, m_csd, sizeof(m_csd));
        break;
    default:
        VCML_ERROR("invalid response for CMD%u", (unsigned)tx.opcode);
    }

    tx.response[16] |= 1; // stop bit
    tx.resp_len = 17;     // 136 bit
}

void card::make_r3(sd_command& tx) {
    if (m_spi) {
        make_r3_spi(tx);
        return;
    }

    tx.response[0] = 0x3f;
    tx.response[1] = m_ocr >> 24;
    tx.response[2] = m_ocr >> 16;
    tx.response[3] = m_ocr >> 8;
    tx.response[4] = m_ocr >> 0;
    tx.response[5] = 0xff;
    tx.resp_len = 6;
}

void card::make_r6(sd_command& tx) {
    tx.response[0] = 0x03;
    tx.response[1] = m_rca >> 8;
    tx.response[2] = m_rca & 0xff;
    tx.response[3] = (m_status & CURRENT_STATE) << 1;
    if (m_status & COM_CRC_ERROR)
        tx.response[3] |= 1 << 7;
    if (m_status & ILLEGAL_COMMAND)
        tx.response[3] |= 1 << 6;
    if (m_status & ERROR)
        tx.response[3] |= 1 << 5;
    if (m_status & READY_FOR_DATA)
        tx.response[3] |= 1 << 0;
    tx.response[4] = 0;
    if (m_status & FX_EVENT)
        tx.response[4] |= FX_EVENT;
    if (m_status & APP_CMD)
        tx.response[4] |= APP_CMD;
    if (m_status & AKE_SEQ_ERROR)
        tx.response[4] |= AKE_SEQ_ERROR;
    tx.response[5] = crc7(tx.response, 5) | 1;
    tx.resp_len = 6;
}

void card::make_r7(sd_command& tx) {
    if (m_spi) {
        make_r7_spi(tx);
        return;
    }

    tx.response[0] = 0x08;
    tx.response[1] = m_hvs >> 24;
    tx.response[2] = m_hvs >> 16;
    tx.response[3] = m_hvs >> 8;
    tx.response[4] = m_hvs >> 0;
    tx.response[5] = crc7(tx.response, 5) | 1;
    tx.resp_len = 6;
}

void card::make_r1_spi(sd_command& tx) {
    VCML_ERROR_ON(!m_spi, "not in SPI mode");
    update_status();

    tx.resp_len = 1;
    tx.response[0] = 0;

    if (m_state <= IDENTIFICATION)
        tx.response[0] |= SPI_IN_IDLE;
    if (m_status & ERASE_RESET)
        tx.response[0] |= SPI_ERASE_RESET;
    if (m_status & ILLEGAL_COMMAND)
        tx.response[0] |= SPI_ILLEGAL_COMMAND;
    if (m_status & COM_CRC_ERROR)
        tx.response[0] |= SPI_COM_CRC_ERROR;
    if (m_status & ERASE_SEQ_ERROR)
        tx.response[0] |= SPI_ERASE_SEQ_ERROR;
    if (m_status & ADDRESS_ERROR)
        tx.response[0] |= SPI_ADDRESS_ERROR;
    if (m_status & OUT_OF_RANGE)
        tx.response[0] |= SPI_PARAMETER_ERROR;

    m_status &= ~(OUT_OF_RANGE | ADDRESS_ERROR | ERASE_SEQ_ERROR |
                  ERASE_RESET);
}

void card::make_r2_spi(sd_command& tx) {
    VCML_ERROR_ON(!m_spi, "not in SPI mode");

    make_r1_spi(tx);

    tx.resp_len = 2;
    tx.response[1] = 0;

    if (m_status & CARD_IS_LOCKED)
        tx.response[1] |= SPI_CARD_IS_LOCKED;
    if (m_status & (WP_ERASE_SKIP | LOCK_UNLOCK_FAILED))
        tx.response[1] |= SPI_WP_ERASE_SKIP;
    if (m_status & ERROR)
        tx.response[1] |= SPI_ERROR;
    if (m_status & CC_ERROR)
        tx.response[1] |= SPI_CC_ERROR;
    if (m_status & CARD_ECC_FAILED)
        tx.response[1] |= SPI_CARD_ECC_FAILED;
    if (m_status & WP_VIOLATION)
        tx.response[1] |= SPI_WP_VIOLATION;
    if (m_status & ERASE_PARAM)
        tx.response[1] |= SPI_ERASE_PARAM;
    if (m_status & (OUT_OF_RANGE | CSD_OVERWRITE))
        tx.response[1] |= SPI_OUT_OF_RANGE;
}

void card::make_r3_spi(sd_command& tx) {
    VCML_ERROR_ON(!m_spi, "not in SPI mode");

    make_r1_spi(tx);

    tx.response[1] = m_ocr >> 24;
    tx.response[2] = m_ocr >> 16;
    tx.response[3] = m_ocr >> 8;
    tx.response[4] = m_ocr >> 0;
    tx.resp_len = 5;
}

void card::make_r7_spi(sd_command& tx) {
    VCML_ERROR_ON(!m_spi, "not in SPI mode");

    make_r1_spi(tx);

    tx.response[1] = m_hvs >> 24;
    tx.response[2] = m_hvs >> 16;
    tx.response[3] = m_hvs >> 8;
    tx.response[4] = m_hvs >> 0;
    tx.resp_len = 5;
}

void card::setup_tx(u8* data, size_t len) {
    VCML_ERROR_ON(!len, "attempt to transmit zero bytes");

    m_bufptr = data;
    m_bufend = data + len;

    m_state = SENDING;
}

void card::setup_rx(u8* data, size_t len) {
    VCML_ERROR_ON(!len, "attempt to receive zero bytes");

    m_bufptr = data;
    m_bufend = data + len;

    m_state = RECEIVING;
}

void card::setup_tx_blk(size_t offset) {
    size_t blklen = is_sdhc() ? SDHC_BLKLEN : m_blklen;
    if (offset % blklen) {
        m_status |= ADDRESS_ERROR;
        return;
    }

    if ((offset + blklen) > disk.capacity()) {
        m_status |= OUT_OF_RANGE;
        return;
    }

    m_curoff = offset;
    disk.seek(m_curoff);
    disk.read(m_buffer, blklen);

    if (m_do_crc) {
        u16 crc = crc16(m_buffer, m_blklen);
        m_buffer[blklen + 0] = (u8)(crc >> 8);
        m_buffer[blklen + 1] = (u8)(crc >> 0);
    } else {
        m_buffer[blklen + 0] = 0xdc; /* don't care */
        m_buffer[blklen + 1] = 0xdc;
    }

    setup_tx(m_buffer, blklen + 2);
}

void card::setup_rx_blk(size_t offset) {
    size_t blklen = is_sdhc() ? SDHC_BLKLEN : m_blklen;
    if (offset % blklen) {
        m_status |= ADDRESS_ERROR;
        return;
    }

    if ((offset + blklen) > disk.capacity()) {
        m_status |= OUT_OF_RANGE;
        return;
    }

    m_curoff = offset;

    setup_rx(m_buffer, blklen + 2);
}

void card::init_ocr() {
    m_ocr = 0;

    m_ocr |= OCR_VDD_27_28;
    m_ocr |= OCR_VDD_28_29;
    m_ocr |= OCR_VDD_29_30;
    m_ocr |= OCR_VDD_30_31;
    m_ocr |= OCR_VDD_31_32;
    m_ocr |= OCR_VDD_32_33;
    m_ocr |= OCR_VDD_33_34;
    m_ocr |= OCR_VDD_34_35;
    m_ocr |= OCR_VDD_35_36;

    if (disk.capacity() > 1 * GiB)
        m_ocr |= OCR_CCS;
}

void card::init_cid() {
    m_cid[0] = 0xBB; // manufacturer ID

    m_cid[1] = 'J'; // OEM ID
    m_cid[2] = 'W';

    m_cid[3] = 'V'; // product name
    m_cid[4] = 'C';
    m_cid[5] = 'M';
    m_cid[6] = 'L';
    m_cid[7] = ' ';

    m_cid[8] = 0x01; // product revision in BCD

    m_cid[9] = 0x13; // product serial number
    m_cid[10] = 0x37;
    m_cid[11] = 0xBE;
    m_cid[12] = 0xEF;

    m_cid[13] = 0x01; // manufacturing date in format ryym (r stands for
                      // reserved)
    m_cid[14] = 0x21; // year: 0x12 = 18, month: 0x1 = 1

    m_cid[15] = crc7(m_cid, sizeof(m_cid) - 1);
}

void card::init_csd_sdsc() {
    u32 read_bl_len = fls(m_blklen);
    u32 c_size_mult = 7; // 2^(7+2) = 512
    u32 c_size = disk.capacity() / (m_blklen * (1 << (c_size_mult + 2)));
    u32 sector_size = 7; // 128 blocks erasable at once (max)
    u32 wpgrp_size = 7;  // 128 blocks per write-protect group

    // prevent underflow if capacity < 256k
    if (c_size > 0)
        c_size -= 1;

    log_debug("using SDSC specification");
    log_debug("  capacity: %zu bytes", disk.capacity());
    log_debug("  blocklen: %zu bytes", m_blklen);
    log_debug("    c_size: %u bytes", c_size);
    log_debug("    c_mult: %ux", 1 << (c_size_mult + 2));

    m_csd[0] = 0x00; // CSD structure version 1
    m_csd[1] = 0x26; // TAAC (time unit 1ms, time value 1.5)
    m_csd[2] = 0x00; // NSAC
    m_csd[3] = 0x32; // 25MHz TX speed (fixed by spec)
    m_csd[4] = 0x5F; // Card Command Classes (0,2,4,5,6,7,8,10)
    m_csd[5] = 0x50 | (u8)read_bl_len;
    m_csd[6] = 0x80 | ((c_size >> 10) & 3); // no DSR
    m_csd[7] = ((c_size) >> 2) & 0xFF;      // middle part of device size
    m_csd[8] = ((c_size & 3) << 6) | 0x0f;  // VDD 1mA..100mA
    m_csd[9] = 0x3c | ((c_size_mult >> 1) & 3);
    m_csd[10] = ((c_size_mult & 1) << 7) | 1 << 6 | (sector_size >> 1);
    m_csd[11] = (sector_size & 1) << 7 | (wpgrp_size & 0x7F);
    m_csd[12] = (1 << 7) | ((read_bl_len >> 2) & 3); // R2W -> 1:1
    m_csd[13] = ((read_bl_len & 3) << 6) | (1 << 5); // part. WR OK
    m_csd[14] = 0x00; // hard disk type, original data, no protect
    m_csd[15] = crc7(m_csd, sizeof(m_csd) - 1) | 1;
}

void card::init_csd_sdhc() {
    u32 c_size_mult = 8; // 2^(8+2) = 1024, fixed by spec
    u32 c_size = disk.capacity() / (m_blklen * (1 << (c_size_mult + 2)));

    // prevent underflow if capacity < 512k
    if (c_size > 0)
        c_size -= 1;

    log_debug("using SDHC/SDXC specification");
    log_debug("  capacity: %zu bytes", disk.capacity());
    log_debug("  blocklen: %zu bytes", m_blklen);
    log_debug("    c_size: %u bytes", c_size);
    log_debug("    c_mult: %ux", 1 << (c_size_mult + 2));

    m_csd[0] = 0x40; // CSD structure version 2
    m_csd[1] = 0x0e; // TAAC
    m_csd[2] = 0x00; // NSAC
    m_csd[3] = 0x32; // TRAN_SPEED
    m_csd[4] = 0x5b; // CCC 11:4
    m_csd[5] = 0x59; // CCC  3:0 | READ_BL_LEN
    m_csd[6] = 0x00; // READ_BL_P | WR_BLK_MA | RD_BLK_MA | DSR_IMP
    m_csd[7] = (c_size >> 16) & 0x3f;
    m_csd[8] = (c_size >> 8) & 0xff;
    m_csd[9] = (c_size >> 0) & 0xff;
    m_csd[10] = 0x7f; // ERASE_BLK_LEN | SECTOR_SIZE[6:1]
    m_csd[11] = 0x80; // SECTOR_SIZE[0] | WP_GRP_SIZE
    m_csd[12] = 0x0a; // WP_GRP_EN | R2W_FACTOR | WRITE_BL_LEN[3:2]
    m_csd[13] = 0x40; // WRITE_BL_LEN[1:0] | WRITE_BL_PARTIAL
    m_csd[14] = 0x00; // FORMAT_GRP | CPY | PRM_WP | TMP_WP | FORMAT
    m_csd[15] = crc7(m_csd, sizeof(m_csd) - 1) | 1;
}

void card::init_csd() {
    VCML_ERROR_ON(!is_pow2(m_blklen), "invalid block size");

    if (is_sdhc())
        init_csd_sdhc();
    else
        init_csd_sdsc();
}

void card::init_scr() {
    m_scr[0] = 0 << 4 | 2;
    m_scr[1] = 2 << 4 | (1 + 4); // security & bus widths
    m_scr[2] = 0;                // no extended security
    m_scr[3] = 0;                // command support bits

    m_scr[4] = 0; // reserved
    m_scr[5] = 0;
    m_scr[6] = 0;
    m_scr[7] = 0;
}

void card::init_sts() {
    memset(m_sts, 0, sizeof(m_sts));
}

void card::switch_function(u32 arg) {
    bool update = arg & 0x80000000;
    if (update)
        log_debug("function update requested");
    else
        log_debug("function scan requested");

    // 30 .. 63 -> reserved
    // 28 .. 29 -> grp 1 busy status
    // 26 .. 27 -> grp 2 busy status
    // 24 .. 25 -> grp 3 busy status
    // 22 .. 23 -> grp 4 busy status
    // 20 .. 21 -> grp 5 busy status
    // 18 .. 19 -> grp 6 busy status
    // 17       -> structure version
    // 16       -> function selection grp 1 + 2
    // 15       -> function selection grp 3 + 4
    // 14       -> function selection grp 5 + 6
    // 12 .. 13 -> support bits grp 1
    // 10 .. 11 -> support bits grp 2
    //  8 ..  9 -> support bits grp 3
    //  6 ..  7 -> support bits grp 4
    //  4 ..  5 -> support bits grp 5
    //  2 ..  3 -> support bits grp 6
    //  0 ..  1 -> max current/power

    // group 1: bus speed control
    // group 2: command system
    // group 3: UHS-I driver strength
    // group 4: power limit control
    // group 5: reserved
    // group 6: reserved

    memset(m_swf, 0, sizeof(m_swf));

    m_swf[0] = 0x00;
    m_swf[1] = 0x01; // max power */

    m_swf[2] = 0x00;
    m_swf[3] = 0x00; // supported grp6 functions
    m_swf[4] = 0x00;
    m_swf[5] = 0x00; // supported grp5 functions
    m_swf[6] = 0x00;
    m_swf[7] = 0x00; // supported grp4 functions
    m_swf[8] = 0x00;
    m_swf[9] = 0x00; // supported grp3 functions
    m_swf[10] = 0x00;
    m_swf[11] = 0x00; // supported grp2 functions
    m_swf[12] = 0x00;
    m_swf[13] = 0x00; // supported grp1 functions

    m_swf[14] = (0 << 4) | 0; // function selection group 5 + group 6
    m_swf[15] = (0 << 4) | 0; // function selection group 3 + group 4
    m_swf[16] = (0 << 4) | 0; // function selection group 1 + group 2

    m_swf[17] = 0; // 0: structure version (first 17 bytes present)
                   // 1: extended version (12 more bytes present)

    u16 crc = crc16(m_swf, 64);
    m_swf[64] = crc >> 8;
    m_swf[65] = crc & 0xff;
}

sd_status card::do_normal_command(sd_command& tx) {
    switch (tx.opcode) {
    case 0: // GO_IDLE_STATE (SD/SPI)
        m_state = IDLE;
        m_spi = tx.spi;
        log_debug("operation mode is %s", m_spi ? "SPI" : "SD");
        make_r1(tx);
        return SD_OK;

    case 1: // SEND_OP_COND (SPI only)
        if (!m_spi)
            break;

        make_r1_spi(tx);
        return SD_OK;

    case 2: // ALL_SEND_CID (SD only)
        if (m_spi)
            break;

        m_state = IDENTIFICATION;
        make_r2(tx);
        return SD_OK;

    case 3: // SEND_RELATIVE_ADDR (SD only)
        if (m_spi)
            break;

        m_state = STAND_BY;
        make_r6(tx);
        return SD_OK;

    case 4: // SET_DSR (SD only)
        // not implemented
        break;

    case 5: // reserved for SDIO
        break;

    case 6: // SWITCH_FUNC (SD/SPI)
        switch_function(tx.argument);
        setup_tx(m_swf, sizeof(m_swf));
        make_r1(tx);
        return SD_OK_TX_RDY;

    case 7: // SELECT/DESELECT CARD (SD only)
        if (m_spi)
            break;

        m_state = TRANSFER;
        make_r1(tx);
        return SD_OK;

    case 8: // SEND_IF_COND (SD/SPI)
        m_hvs = tx.argument & 0xfff;
        make_r7(tx);
        return SD_OK;

    case 9: // SEND_CSD (SD/SPI)
        if (m_spi) {
            setup_tx(m_csd, sizeof(m_csd));
            make_r1_spi(tx);
            return SD_OK_TX_RDY;
        } else {
            u16 rca = tx.argument >> 16;
            if (rca == m_rca)
                make_r2(tx);
            else
                make_r0(tx);
        }
        return SD_OK;

    case 10: // SEND_CID (SD/SPI)
        if (m_spi) {
            setup_tx(m_cid, sizeof(m_cid));
            make_r1_spi(tx);
            return SD_OK_TX_RDY;
        } else {
            u16 rca = tx.argument >> 16;
            if (rca == m_rca)
                make_r2(tx);
            else
                make_r0(tx);
            return SD_OK;
        }

    case 11: // VOLTAGE_SWITCH (SD only)
        // not implemented
        break;

    case 12: // STOP_TRANSMISSION (SD/SPI)
        log_debug("stopping transmission after %zu blocks", m_numblk);
        m_state = TRANSFER;
        make_r1(tx);
        return SD_OK;

    case 13: // SEND_STATUS (SD/SPI)
        if (m_spi) {
            make_r2_spi(tx);
        } else {
            u16 rca = tx.argument >> 16;
            if (rca == m_rca)
                make_r1(tx);
            else
                make_r0(tx);
        }
        m_status &= ~READY_FOR_DATA;
        return SD_OK;

    case 14: // reserved
        break;

    case 15: // GO_INACTIVE_STATE (SD only)
        // not implemented
        break;

        /*** class 2 commands (reading) ***/

    case 16: // SET_BLOCKLEN (SD/SPI)
        if (tx.argument != m_blklen) {
            m_blklen = tx.argument;
            log_debug("block length changed to %zu bytes", m_blklen);
        }
        make_r1(tx);
        return SD_OK;

    case 17: // READ_SINGLE_BLOCK (SD/SPI)
        m_numblk = 0;
        setup_tx_blk(is_sdhc() ? tx.argument * SDHC_BLKLEN : tx.argument);
        make_r1(tx);
        return SD_OK_TX_RDY;

    case 18: // READ_MULTIPLE_BLOCK (SD/SPI)
        m_numblk = 0;
        setup_tx_blk(is_sdhc() ? tx.argument * SDHC_BLKLEN : tx.argument);
        make_r1(tx);
        return SD_OK_TX_RDY;

    case 19:   // SEND_TUNING_BLOCK (SD only)
        break; // not implemented

    case 20:   // SPEED_CLASS_CONTROL (SD only)
        break; // not implemented

    case 22: // reserved
        break;

    case 23: // SET_BLOCK_COUNT (SD only)
        if (m_spi)
            break;
        // for sdhc cards not necessary
        break;

        /*** class 4 commands (writing) ***/

    case 24: // WRITE_BLOCK (SD/SPI)
        m_numblk = 0;
        setup_rx_blk(is_sdhc() ? tx.argument * SDHC_BLKLEN : tx.argument);
        m_status |= READY_FOR_DATA;
        make_r1(tx);
        return SD_OK_RX_RDY;

    case 25: // WRITE_MULTIPLE_BLOCK (SD/SPI)
        m_numblk = 0;
        setup_rx_blk(is_sdhc() ? tx.argument * SDHC_BLKLEN : tx.argument);
        m_status |= READY_FOR_DATA;
        make_r1(tx);
        return SD_OK_RX_RDY;

    case 26: // reserved for manufacturer (SD only)
        break;

    case 27: // PROGRAM_CSD (SD/SPI)
        // not implemented
        break;

        /*** class 6 commands (protection) ***/

    case 28: // SET_WRITE_PROT (SD/SPI)
        if (is_sdhc())
            return SD_ERR_ILLEGAL;
        // not implemented
        break;

    case 29: // CLR_WRITE_PROT (SD/SPI)
        if (is_sdhc())
            return SD_ERR_ILLEGAL;
        // not implemented
        break;

    case 30: // SEND_WRITE_PROT (SD/SPI)
        if (is_sdhc())
            return SD_ERR_ILLEGAL;
        // not implemented
        break;

    case 31: // reserved
        break;

        /*** class 5 commands (erase) ***/

    case 32: // ERASE_WR_BLK_START (SD/SPI)
        // not implemented
        break;

    case 33: // ERASE_WR_BLK_END (SD/SPI)
        // not implemented
        break;

    case 38: // ERASE (SD/SPI)
        // not implemented
        break;

        /*** class 7 commands (locking) ***/

    case 40: // DPSspec (SD only)
        break;

    case 42: // LOCK_UNLOCK (SD/SPI)
        // not implemented
        break;

        /*** class 8 commands (application specific) ***/

    case 55: // APP_CMD (SD/SPI)
        m_status |= APP_CMD;
        make_r1(tx);
        return SD_OK;

    case 56: // GEN_CMD (SD/SPI)
        // not implemented
        break;

        /*** class 9 commands (I/O mode) ***/

    case 52:
    case 53:
    case 54: // SDIO commands
        break;

    case 58: // READ_OCR (SPI only)
        if (!m_spi)
            break;

        make_r3_spi(tx);
        return SD_OK;

    case 59: // CRC_ON_OFF (SPI only)
        m_do_crc = tx.argument & 0x1;
        log_debug("%sabling CRC", m_do_crc ? "en" : "dis");
        make_r1_spi(tx);
        return SD_OK;

    default:
        break;
    }

    return SD_ERR_ILLEGAL;
}

sd_status card::do_application_command(sd_command& tx) {
    switch (tx.opcode) {
    case 6: // SET_BUS_WIDTH (SD only)
        if (m_spi)
            break;

        switch (tx.argument & 0b11) {
        case 0b00:
        case 0b10:
            log_debug("%u bit bus width", 1u << (tx.argument & 0b11));
            break;

        default:
            log_debug("illegal argument for SET_BUS_WIDTH");
        }

        make_r1(tx);
        return SD_OK;

    case 13: // SD_STATUS (SD/SPI)
        setup_tx(m_sts, sizeof(m_sts));
        if (m_spi)
            make_r2_spi(tx);
        else
            make_r1(tx);
        return SD_OK_TX_RDY;

    case 22: // SEND_NUM_WR_BLOCKS (SD/SPI)
        m_buffer[0] = (u8)(m_numblk >> 24);
        m_buffer[1] = (u8)(m_numblk >> 16);
        m_buffer[2] = (u8)(m_numblk >> 8);
        m_buffer[3] = (u8)(m_numblk >> 0);
        m_buffer[4] = (u8)(crc16(m_buffer, 4) >> 8);
        m_buffer[5] = (u8)(crc16(m_buffer, 4) >> 0);
        setup_tx(m_buffer, 6);
        make_r1(tx);
        return SD_OK_TX_RDY;

    case 23: // SET_WR_BLK_ERASE_COUNT (SD/SPI)
        // not implemented
        break;

    case 41: // SD_SEND_OP_COND (SD/SPI)
        if (m_spi) {
            m_state = TRANSFER;
            make_r1_spi(tx);
            return SD_OK;
        }

        if (m_state != IDLE)
            return SD_ERR_ILLEGAL;

        if (tx.argument) { // make sure its not just an inquiry
            m_state = READY;
            m_ocr |= OCR_POWERED_UP;
        }

        make_r3(tx);
        return SD_OK;

    case 42: // SET_CLR_CARD_DETECT (SD/SPI)
        // not implemented
        break;

    case 51: // SEND_SCR (SD/SPI)
        setup_tx(m_scr, sizeof(m_scr));
        make_r1(tx);
        return SD_OK_TX_RDY;

    default:
        break;
    }

    return SD_ERR_ILLEGAL;
}

sd_status_tx card::do_data_read(u8& val) {
    val = 0xff;

    if (m_state != SENDING) {
        log_debug("attempt to read from card that is not sending");
        return SDTX_ERR_ILLEGAL;
    }

    VCML_ERROR_ON(m_bufptr == nullptr, "buffer not loaded");
    VCML_ERROR_ON(m_bufend == nullptr, "buffer size not set");

    if (m_bufptr >= m_bufend) {
        m_numblk++;
        m_state = TRANSFER;
        m_bufptr = nullptr;
        m_bufend = nullptr;

        if ((m_curcmd != 18))
            return SDTX_OK_COMPLETE;

        size_t blklen = is_sdhc() ? SDHC_BLKLEN : m_blklen;
        size_t offset = m_curoff + blklen;
        if (offset >= disk.capacity())
            return SDTX_OK_COMPLETE;

        setup_tx_blk(m_curoff + blklen);
        return SDTX_OK_BLK_DONE;
    }

    val = *m_bufptr++;
    return SDTX_OK;
}

sd_status_rx card::do_data_write(u8 val) {
    if (m_state != RECEIVING) {
        log_debug("attempt to write to card that is not receiving");
        return SDRX_ERR_ILLEGAL;
    }

    VCML_ERROR_ON(m_bufptr == nullptr, "buffer not loaded");
    VCML_ERROR_ON(m_bufend == nullptr, "buffer size not set");

    if (m_bufptr < m_bufend) {
        *m_bufptr++ = val;
        return SDRX_OK;
    }

    m_state = TRANSFER;
    update_status();
    m_bufptr = nullptr;
    m_bufend = nullptr;

    if ((m_curcmd != 24) && (m_curcmd != 25)) // WRITE or WRITE_MULTIPLE
        VCML_ERROR("unsupported write CMD%hhu", m_curcmd);

    size_t blklen = is_sdhc() ? SDHC_BLKLEN : m_blklen;
    u16 crc = (u16)m_buffer[blklen] << 8 | (u16)m_buffer[blklen + 1];
    if (crc != crc16(m_buffer, blklen)) {
        log_debug("CRC mismatch on received data");
        return SDRX_ERR_CRC;
    }

    disk.seek(m_curoff);
    disk.write(m_buffer, blklen);
    disk.flush();

    m_numblk++;

    if (m_curcmd == 24) // writing only single block?
        return SDRX_OK_COMPLETE;

    size_t offset = m_curoff + blklen;
    if (offset + blklen > disk.capacity()) // reached end of card memory?
        return SDRX_OK_COMPLETE;

    setup_rx_blk(offset); // continue writing
    return SDRX_OK_BLK_DONE;
}

card::card(const sc_module_name& nm, const string& img, bool ro):
    component(nm),
    sd_host(),
    m_spi(false),
    m_do_crc(true),
    m_blklen(SDHC_BLKLEN),
    m_status(0),
    m_hvs(0),
    m_rca(0),
    m_ocr(),
    m_cid(),
    m_csd(),
    m_scr(),
    m_sts(),
    m_swf(),
    m_bufptr(nullptr),
    m_bufend(nullptr),
    m_buffer(),
    m_curcmd(),
    m_curoff(),
    m_numblk(),
    m_state(IDLE),
    image("image", img),
    readonly("readonly", ro),
    disk("disk", image, readonly),
    sd_in("sd_in") {
    if (disk.capacity() % 1024)
        log_warn("image size should be multiples of 1kB");

    init_ocr();
    init_cid();
    init_csd();
    init_scr();
    init_sts();
}

card::~card() {
    // nothing to do
}

void card::reset() {
    m_status = 0;
    m_state = IDLE;

    init_ocr();
    init_cid();
    init_csd();
    init_scr();
    init_sts();

    component::reset();
}

sd_status card::do_command(sd_command& tx) {
    if (!check_crc7(tx))
        return SD_ERR_CRC;

    if (tx.appcmd)
        return do_application_command(tx);
    else
        return do_normal_command(tx);
}

void card::sd_transport(const sd_target_socket& socket, sd_command& tx) {
    tx.appcmd = (m_status & APP_CMD);
    tx.resp_len = 0;

    if (m_state == SENDING || m_state == RECEIVING) {
        m_state = TRANSFER;
        update_status();
    }

    m_status &= ~(COM_CRC_ERROR | ILLEGAL_COMMAND | APP_CMD);
    m_curcmd = tx.opcode;

    tx.status = do_command(tx);

    switch (tx.status) {
    case SD_OK:
        update_status();
        break;

    case SD_OK_TX_RDY:
        update_status();
        VCML_ERROR_ON(tx.resp_len == 0, "invalid response from handler");
        break;

    case SD_OK_RX_RDY:
        update_status();
        VCML_ERROR_ON(tx.resp_len == 0, "invalid response from handler");
        break;

    case SD_ERR_CRC:
        m_status |= COM_CRC_ERROR;
        m_state = m_state < TRANSFER ? IDLE : TRANSFER;
        make_r1(tx);
        update_status();
        log_debug("command checksum error");
        break;

    case SD_ERR_ARG:
        m_status |= OUT_OF_RANGE;
        m_state = m_state < TRANSFER ? IDLE : TRANSFER;
        make_r1(tx);
        update_status();
        log_debug("command argument out of range 0x%08x", tx.argument);
        break;

    case SD_ERR_ILLEGAL:
        m_status |= ILLEGAL_COMMAND;
        m_state = m_state < TRANSFER ? IDLE : TRANSFER;
        make_r1(tx);
        update_status();
        log_debug("illegal command %s", to_string(tx).c_str());
        break;

    default:
        VCML_ERROR("invalid response code from command handler");
        break;
    }
}

void card::sd_transport(const sd_target_socket& socket, sd_data& tx) {
    if (tx.mode == SD_READ)
        tx.status.read = do_data_read(tx.data);
    if (tx.mode == SD_WRITE)
        tx.status.write = do_data_write(tx.data);
}

VCML_EXPORT_MODEL(vcml::sd::card, name, args) {
    if (args.empty())
        return new card(name);
    return new card(name, args[0]);
}

VCML_EXPORT_MODEL(vcml::sd::rocard, name, args) {
    if (args.empty())
        return new card(name, "", true);
    return new card(name, args[0], true);
}

} // namespace sd
} // namespace vcml
