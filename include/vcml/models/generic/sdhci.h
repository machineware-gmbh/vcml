// written by Lasse Urban

#ifndef VCML_GENERIC_SDHCI
#define VCML_GENERIC_SDHCI

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"

#include "vcml/ports.h"
#include "vcml/slave_socket.h"
#include "vcml/peripheral.h"
#include "vcml/register.h"
#include "vcml/sd.h"
#include "vcml/common/bitops.h"

#define CAPABILITY_VALUES_0 0x01000A8A

namespace vcml { namespace generic {

    class sdhci: public peripheral, public sd_bw_transport_if
    {
    private:
        enum kind_of_reset {
            RESET_ALL       = 1 << 0,
            RESET_CMD_LINE  = 1 << 1,
            RESET_DAT_LINE  = 1 << 2
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
            INT_BLOCK_GAP_EVENT     = 1 << 2,
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

        sd_command m_cmd;
        sd_status  m_status;

        u16 m_bufptr;
        u8 m_buffer[514]; // Block length specified in the CAPABILITIES register in Bits 17-16 -> 00 means 512 bytes, two more bytes for the CRC code

        u8 calc_crc7();
        void reset_RESPONSE(int response_reg_nr);
        void write_sd_resp_to_RESPONSE();
        void set_PRESENT_STATE(unsigned int act_state);
        void transfer_data_from_sd_buffer_to_sdhci_buffer();
        void transfer_data_from_sdhci_buffer_to_BUFFER_DATA_PORT();
        void transfer_data_from_BUFFER_DATA_PORT_to_sdhci_buffer();
        void transfer_data_from_sdhci_buffer_to_sd_buffer();

        u16 read_CMD();
        u16 write_CMD(u16 val);

        u32 read_BUFFER_DATA_PORT();
        u32 write_BUFFER_DATA_PORT(u32 val);

        u16 read_CLOCK_CTRL();
        u16 write_CLOCK_CTRL(u16 val);

        u8 read_SOFTWARE_RESET();
        u8 write_SOFTWARE_RESET(u8 val);

        u16 read_NORMAL_INT_STAT();
        u16 write_NORMAL_INT_STAT(u16 val);
        u16 read_ERROR_INT_STAT();
        u16 write_ERROR_INT_STAT(u16 val);

        // disabled
        sdhci();
        sdhci(const sdhci&);


    public:
        // registers implemented as shown in the SD Host Controller Simplified Specification page 32

        // SD Command Generation (00F-000h)
        reg<sdhci, u16> BLOCK_SIZE;
        reg<sdhci, u16> BLOCK_COUNT_16BIT;

        reg<sdhci, u32> ARG;
        reg<sdhci, u16> TRANSFER_MODE;
        reg<sdhci, u16> CMD;

        // Response (01F-010h)
        reg<sdhci, u32, 4> RESPONSE;

        // Buffer Data Port (023-020h)
        reg<sdhci, u32> BUFFER_DATA_PORT;

        // Host Control 1 and Others (03D-030h)
        reg<sdhci, u32> PRESENT_STATE;
        reg<sdhci, u8>  HOST_CONTROL_1;
        reg<sdhci, u8>  POWER_CTRL;
        reg<sdhci, u16> CLOCK_CTRL;
        reg<sdhci, u8>  TIMEOUT_CTRL;
        reg<sdhci, u8>  SOFTWARE_RESET;

        // Interrupt Controls (03D-030h)
        reg<sdhci, u16> NORMAL_INT_STAT;                // Normal Interrupt Status
        reg<sdhci, u16> ERROR_INT_STAT;
        reg<sdhci, u16> NORMAL_INT_STAT_ENABLE;         // Normal Interrupt Status Enable
        reg<sdhci, u16> ERROR_INT_STAT_ENABLE;
        reg<sdhci, u16> NORMAL_INT_SIG_ENABLE;          // Normal Interrupt Signal Enable
        reg<sdhci, u16> ERROR_INT_SIG_ENABLE;

        // Capabilities (04F-040h)
        reg<sdhci, u32, 2> CAPABILITIES;
        reg<sdhci, u32> MAX_CURR_CAP;

        // Common Area (0FF-0F0h), common for SD host controllers with multiple slots (contains version number)
        reg<sdhci, u16> HOST_CONTROLLER_VERSION;

        // Controller specific registers
        reg<sdhci, u16> F_SDH30_AHB_CONFIG;
        reg<sdhci, u32> F_SDH30_ESD_CONTROL;

        out_port IRQ;
        slave_socket IN;
        sd_initiator_socket SD_OUT;

        sdhci(const sc_module_name& name);
        virtual ~sdhci();
        VCML_KIND(sdhci);
        };
}}

#endif
