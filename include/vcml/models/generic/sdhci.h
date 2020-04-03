/******************************************************************************
 *                                                                            *
 * Copyright 2020 Lasse Urban, Lukas Juenger, Jan Henrik Weinstock            *
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

#ifndef VCML_GENERIC_SDHCI_H
#define VCML_GENERIC_SDHCI_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"
#include "vcml/common/bitops.h"
#include "vcml/common/systemc.h"

#include "vcml/ports.h"
#include "vcml/master_socket.h"
#include "vcml/slave_socket.h"
#include "vcml/register.h"
#include "vcml/peripheral.h"
#include "vcml/sd.h"

namespace vcml { namespace generic {

    class sdhci: public peripheral, public sd_bw_transport_if
    {
    private:
        enum reset_kind {
            RESET_ALL      = 1 << 0,
            RESET_CMD_LINE = 1 << 1,
            RESET_DAT_LINE = 1 << 2
        };

        enum present_state : unsigned int {
            COMMAND_INHIBIT_CMD     = 1 << 0,
            COMMAND_INHIBIT_DAT     = 1 << 1,
            DAT_LINE_ACTIVE         = 1 << 2,
            WRITE_TRANSFER_ACTIVE   = 1 << 8,
            READ_TRANSFER_ACTIVE    = 1 << 9,
            BUFFER_WRITE_ENABLE     = 1 << 10,
            BUFFER_READ_ENABLE      = 1 << 11,
            CARD_INSERTED           = 1 << 16
        };

        enum normal_interrupts {
            INT_COMMAND_COMPLETE    = 1 << 0,
            INT_TRANSFER_COMPLETE   = 1 << 1,
            INT_DMA_INTERRUPT       = 1 << 3,
            INT_BUFFER_WRITE_READY  = 1 << 4,
            INT_BUFFER_READ_READY   = 1 << 5,
            INT_ERROR               = 1 << 15
        };

        enum error_interrupts {
            ERR_CMD_TIMEOUT         = 1 << 0,
            ERR_CMD_CRC             = 1 << 1,
            ERR_CMD_END_BIT         = 1 << 2,
            ERR_CMD_INDEX           = 1 << 3,
            ERR_DATA_TIMEOUT        = 1 << 4,
            ERR_DATA_CRC            = 1 << 5,
            ERR_DATA_END_BIT        = 1 << 6
        };

        enum capabilities : u32 {
            CAPABILITY_VALUES_0 = 0x01000A8A,
        };

        sd_command m_cmd;
        sd_status  m_status;

        u16 m_bufptr;
        u8 m_buffer[514]; // 512 block length + 2 bytes CRC

        u8 calc_crc7() const;

        void reset_response(int response_reg_nr);
        void store_response();
        void set_present_state(unsigned int state);

        void transfer_data_from_sd();
        void transfer_data_to_sd();
        void transfer_data_from_port();
        void transfer_data_to_port();

        u16 write_CMD(u16 val);
        u32 read_BUFFER_DATA_PORT();
        u32 write_BUFFER_DATA_PORT(u32 val);
        u16 write_CLOCK_CTRL(u16 val);
        u8  write_SOFTWARE_RESET(u8 val);
        u16 write_NORMAL_INT_STAT(u16 val);
        u16 write_ERROR_INT_STAT(u16 val);
        u32 read_CAPABILITIES();

        void dma_thread();

        tlm_response_status dma_read(u32 boundary);
        tlm_response_status dma_write(u32 boundary);

        sc_event m_dma_start;

    public:
        // Common SDHCI registers
        reg<sdhci, u32> SDMA_SYSTEM_ADDRESS;
        reg<sdhci, u16> BLOCK_SIZE;
        reg<sdhci, u16> BLOCK_COUNT_16BIT;

        reg<sdhci, u32> ARG;
        reg<sdhci, u16> TRANSFER_MODE;
        reg<sdhci, u16> CMD;

        reg<sdhci, u32, 4> RESPONSE;

        reg<sdhci, u32> BUFFER_DATA_PORT;

        reg<sdhci, u32> PRESENT_STATE;
        reg<sdhci, u8>  HOST_CONTROL_1;
        reg<sdhci, u8>  POWER_CTRL;
        reg<sdhci, u16> CLOCK_CTRL;
        reg<sdhci, u8>  TIMEOUT_CTRL;
        reg<sdhci, u8>  SOFTWARE_RESET;

        reg<sdhci, u16> NORMAL_INT_STAT;
        reg<sdhci, u16> ERROR_INT_STAT;
        reg<sdhci, u16> NORMAL_INT_STAT_ENABLE;
        reg<sdhci, u16> ERROR_INT_STAT_ENABLE;
        reg<sdhci, u16> NORMAL_INT_SIG_ENABLE;
        reg<sdhci, u16> ERROR_INT_SIG_ENABLE;

        reg<sdhci, u32, 2> CAPABILITIES;
        reg<sdhci, u32> MAX_CURR_CAP;

        reg<sdhci, u16> HOST_CONTROLLER_VERSION;

        // Controller specific registers
        reg<sdhci, u16> F_SDH30_AHB_CONFIG;
        reg<sdhci, u32> F_SDH30_ESD_CONTROL;

        property<bool> dma_enabled;

        out_port<bool> IRQ;

        slave_socket IN;
        master_socket OUT;

        sd_initiator_socket SD_OUT;

        sdhci() = delete;
        sdhci(const sc_module_name& name);
        virtual ~sdhci();
        VCML_KIND(sdhci);

        virtual void reset() override;
    };

}}

#endif
