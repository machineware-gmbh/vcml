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

#ifndef VCML_GENERIC_SDCARD_H
#define VCML_GENERIC_SDCARD_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/bitops.h"
#include "vcml/common/systemc.h"

#include "vcml/protocols/sd.h"
#include "vcml/component.h"

namespace vcml { namespace generic {

    class sdcard: public component,
                  public sd_fw_transport_if {
    private:
        bool    m_spi;
        bool    m_do_crc;
        size_t  m_blklen;
        fstream m_image;

        u32     m_status;  // Card Status Register
        u32     m_hvs;     // Host Voltage Supply
        u16     m_rca;     // Relative Card Address
        u32     m_ocr;     // Operations Conditions Register
        u8      m_cid[16]; // Card Identification Register
        u8      m_csd[16]; // Card-Specific Data Register
        u8      m_scr[8];  // Card Configuration Register
        u8      m_sts[64]; // Status Memory
        u8      m_swf[66]; // Switch-Function Memory

        u8*     m_bufptr;
        u8*     m_bufend;
        u8      m_buffer[514]; // Block length + CRC16

        u8      m_curcmd;
        size_t  m_curoff;
        size_t  m_numblk;

        enum state {
            IDLE           = 0,
            READY          = 1,
            IDENTIFICATION = 2,
            STAND_BY       = 3,
            TRANSFER       = 4,
            SENDING        = 5,
            RECEIVING      = 6,
            PROGRAMMING    = 7,
            DISCONNECTED   = 8,
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

        void init_image();

        void switch_function(u32 arg);

        sd_status do_command(sd_command& tx);
        sd_status do_normal_command(sd_command& tx);
        sd_status do_application_command(sd_command& tx);

        // disabled
        sdcard();
        sdcard(const sdcard&);

    public:
        enum status_bits {
            OUT_OF_RANGE       = 1 << 31,
            ADDRESS_ERROR      = 1 << 30,
            BLOCK_LEN_ERROR    = 1 << 29,
            ERASE_SEQ_ERROR    = 1 << 28,
            ERASE_PARAM        = 1 << 27,
            WP_VIOLATION       = 1 << 26,
            CARD_IS_LOCKED     = 1 << 25,
            LOCK_UNLOCK_FAILED = 1 << 24,
            COM_CRC_ERROR      = 1 << 23,
            ILLEGAL_COMMAND    = 1 << 22,
            CARD_ECC_FAILED    = 1 << 21,
            CC_ERROR           = 1 << 20,
            ERROR              = 1 << 19,
            CSD_OVERWRITE      = 1 << 16,
            WP_ERASE_SKIP      = 1 << 15,
            CARD_ECC_DISABLED  = 1 << 14,
            ERASE_RESET        = 1 << 13,
            CURRENT_STATE      = 0xf << 9,
            READY_FOR_DATA     = 1 << 8,
            FX_EVENT           = 1 << 6,
            APP_CMD            = 1 << 5,
            AKE_SEQ_ERROR      = 1 << 3
        };

        enum spi_status_bits {
            SPI_IN_IDLE         = 1 << 0,
            SPI_ERASE_RESET     = 1 << 1,
            SPI_ILLEGAL_COMMAND = 1 << 2,
            SPI_COM_CRC_ERROR   = 1 << 3,
            SPI_ERASE_SEQ_ERROR = 1 << 4,
            SPI_ADDRESS_ERROR   = 1 << 5,
            SPI_PARAMETER_ERROR = 1 << 6,
        };

        enum spi_status2_bits {
            SPI_CARD_IS_LOCKED  = 1 << 0,
            SPI_WP_ERASE_SKIP   = 1 << 1,
            SPI_ERROR           = 1 << 2,
            SPI_CC_ERROR        = 1 << 3,
            SPI_CARD_ECC_FAILED = 1 << 4,
            SPI_WP_VIOLATION    = 1 << 5,
            SPI_ERASE_PARAM     = 1 << 6,
            SPI_OUT_OF_RANGE    = 1 << 7,
        };

        enum ocr_bits {
            OCR_VDD_27_28       = 1 << 15,
            OCR_VDD_28_29       = 1 << 16,
            OCR_VDD_29_30       = 1 << 17,
            OCR_VDD_30_31       = 1 << 18,
            OCR_VDD_31_32       = 1 << 19,
            OCR_VDD_32_33       = 1 << 20,
            OCR_VDD_33_34       = 1 << 21,
            OCR_VDD_34_35       = 1 << 22,
            OCR_VDD_35_36       = 1 << 23,
            OCR_S18A            = 1 << 24,
            OCR_CCS             = 1 << 30,
            OCR_POWERED_UP      = 1 << 31,
        };

        property<size_t> capacity;
        property<string> image;
        property<bool>   readonly;

        sd_target_socket SD_IN;

        sdcard(const sc_module_name& name);
        virtual ~sdcard();
        virtual void reset() override;
        VCML_KIND(sdcard);

        bool is_sdhc() const { return m_ocr & OCR_CCS; }
        bool is_sdsc() const { return !is_sdhc(); }

        virtual sd_status sd_transport(sd_command& tx) override;
        virtual sd_tx_status sd_data_read(u8& val) override;
        virtual sd_rx_status sd_data_write(u8 val) override;
    };

    inline void sdcard::update_status() {
        m_status &= ~(0xf << 9);
        m_status |= m_state << 9;
    }

}}

#endif
