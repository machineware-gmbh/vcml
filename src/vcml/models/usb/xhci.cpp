/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/usb/xhci.h"

namespace vcml {
namespace usb {

using TRB_TYPE = field<10, 6, u32>;
using TRB_EP_ID = field<16, 5, u32>;
using TRB_SLOT_TYPE = field<16, 5, u32>;
using TRB_SLOT = field<24, 8, u32>;
using TRB_SLOT_ID = field<24, 8, u32>;
using TRB_CCODE = field<24, 8, u32>;
using TRB_PORT_ID = field<24, 8, u64>;
using TRB_DATA_LENGTH = field<0, 17, u32>;
using TRB_INTR = field<22, 10, u32>;

enum trb_bits : u32 {
    TRB_C = bit(0),
    TRB_TC = bit(1),
    TRB_ISP = bit(2),
    TRB_NS = bit(3),
    TRB_CH = bit(4),
    TRB_IOC = bit(5),
    TRB_IDT = bit(6),
    TRB_BSR = bit(9),
    TRB_DC = bit(9),
    TRB_DIR = bit(16),
    TRB_SIZE = 16,
};

static_assert(sizeof(xhci::trb) == TRB_SIZE);

constexpr u32 get_trb_type(const xhci::trb& trb) {
    return get_field<TRB_TYPE>(trb.control);
}

constexpr u32 get_trb_slot_type(const xhci::trb& trb) {
    return get_field<TRB_SLOT_TYPE>(trb.control);
}

constexpr u32 get_trb_slot_id(const xhci::trb& trb) {
    return get_field<TRB_SLOT_ID>(trb.control);
}

constexpr u32 get_trb_ep_id(const xhci::trb& trb) {
    return get_field<TRB_EP_ID>(trb.control);
}

constexpr u32 get_trb_data_length(const xhci::trb& trb) {
    return get_field<TRB_DATA_LENGTH>(trb.status);
}

constexpr u32 get_trb_intr(const xhci::trb& trb) {
    return get_field<TRB_INTR>(trb.status);
}

constexpr u32 get_trb_ccode(const xhci::trb& trb) {
    return get_field<TRB_CCODE>(trb.status);
}

constexpr void set_trb_ccode(xhci::trb& trb, u32 ccode) {
    set_field<TRB_CCODE>(trb.status, ccode);
}

enum trb_completion_code : u32 {
    TRB_CC_INVALID = 0,
    TRB_CC_SUCCESS = 1,
    TRB_CC_DATA_BUFFER_ERROR = 2,
    TRB_CC_BABBLE_DETECTED_ERROR = 3,
    TRB_CC_USB_TRANSACTION_ERROR = 4,
    TRB_CC_TRB_ERROR = 5,
    TRB_CC_STALL_ERROR = 6,
    TRB_CC_RESOURCE_ERROR = 7,
    TRB_CC_BANDWIDTH_ERROR = 8,
    TRB_CC_NO_SLOTS_AVAILABLE_ERROR = 9,
    TRB_CC_INVALID_STREAM_TYPE_ERROR = 10,
    TRB_CC_SLOT_NOT_ENABLED_ERROR = 11,
    TRB_CC_ENDPOINT_NOT_ENABLED_ERROR = 12,
    TRB_CC_SHORT_PACKET = 13,
    TRB_CC_RING_UNDERRUN = 14,
    TRB_CC_RING_OVERRUN = 15,
    TRB_CC_VF_EVENT_RING_FULL_ERROR = 16,
    TRB_CC_PARAMETER_ERROR = 17,
    TRB_CC_BANDWIDTH_OVERRUN_ERROR = 18,
    TRB_CC_CONTEXT_STATE_ERROR = 19,
    TRB_CC_NO_PING_RESPONSE_ERROR = 20,
    TRB_CC_EVENT_RING_FULL_ERROR = 21,
    TRB_CC_INCOMPATIBLE_DEVICE_ERROR = 22,
    TRB_CC_MISSED_SERVICE_ERROR = 23,
    TRB_CC_COMMAND_RING_STOPPED = 24,
    TRB_CC_COMMAND_ABORTED = 25,
    TRB_CC_STOPPED = 26,
    TRB_CC_STOPPED_LENGTH_INVALID = 27,
    TRB_CC_STOPPED_SHORT_PACKET = 28,
    TRB_CC_MAX_EXIT_LATENCY_TOO_LARGE_ERROR = 29,
    TRB_CC_ISOCH_BUFFER_OVERRUN = 30,
    TRB_CC_EVENT_LOST_ERROR = 32,
    TRB_CC_UNDEFINED_ERROR = 33,
    TRB_CC_INVALID_STREAM_ID_ERROR = 34,
    TRB_CC_SECONDARY_BANDWIDTH_ERROR = 35,
    TRB_CC_SPLIT_TRANSACTION_ERROR = 36,
};

constexpr const char* trb_cc_str(u32 cc) {
    switch (cc) {
    case TRB_CC_INVALID:
        return "INVALID";
    case TRB_CC_SUCCESS:
        return "SUCCESS";
    case TRB_CC_DATA_BUFFER_ERROR:
        return "DATA_BUFFER_ERROR";
    case TRB_CC_BABBLE_DETECTED_ERROR:
        return "BABBLE_DETECTED_ERROR";
    case TRB_CC_USB_TRANSACTION_ERROR:
        return "USB_TRANSACTION_ERROR";
    case TRB_CC_TRB_ERROR:
        return "TRB_ERROR";
    case TRB_CC_STALL_ERROR:
        return "STALL_ERROR";
    case TRB_CC_RESOURCE_ERROR:
        return "RESOURCE_ERROR";
    case TRB_CC_BANDWIDTH_ERROR:
        return "BANDWIDTH_ERROR";
    case TRB_CC_NO_SLOTS_AVAILABLE_ERROR:
        return "NO_SLOTS_AVAILABLE_ERROR";
    case TRB_CC_INVALID_STREAM_TYPE_ERROR:
        return "INVALID_STREAM_TYPE_ERROR";
    case TRB_CC_SLOT_NOT_ENABLED_ERROR:
        return "SLOT_NOT_ENABLED_ERROR";
    case TRB_CC_ENDPOINT_NOT_ENABLED_ERROR:
        return "ENDPOINT_NOT_ENABLED_ERROR";
    case TRB_CC_SHORT_PACKET:
        return "SHORT_PACKET";
    case TRB_CC_RING_UNDERRUN:
        return "RING_UNDERRUN";
    case TRB_CC_RING_OVERRUN:
        return "RING_OVERRUN";
    case TRB_CC_VF_EVENT_RING_FULL_ERROR:
        return "VF_EVENT_RING_FULL_ERROR";
    case TRB_CC_PARAMETER_ERROR:
        return "PARAMETER_ERROR";
    case TRB_CC_BANDWIDTH_OVERRUN_ERROR:
        return "BANDWIDTH_OVERRUN_ERROR";
    case TRB_CC_CONTEXT_STATE_ERROR:
        return "CONTEXT_STATE_ERROR";
    case TRB_CC_NO_PING_RESPONSE_ERROR:
        return "NO_PING_RESPONSE_ERROR";
    case TRB_CC_EVENT_RING_FULL_ERROR:
        return "EVENT_RING_FULL_ERROR";
    case TRB_CC_INCOMPATIBLE_DEVICE_ERROR:
        return "INCOMPATIBLE_DEVICE_ERROR";
    case TRB_CC_MISSED_SERVICE_ERROR:
        return "MISSED_SERVICE_ERROR";
    case TRB_CC_COMMAND_RING_STOPPED:
        return "COMMAND_RING_STOPPED";
    case TRB_CC_COMMAND_ABORTED:
        return "COMMAND_ABORTED";
    case TRB_CC_STOPPED:
        return "STOPPED";
    case TRB_CC_STOPPED_LENGTH_INVALID:
        return "STOPPED_LENGTH_INVALID";
    case TRB_CC_STOPPED_SHORT_PACKET:
        return "STOPPED_SHORT_PACKET";
    case TRB_CC_MAX_EXIT_LATENCY_TOO_LARGE_ERROR:
        return "MAX_EXIT_LATENCY_TOO_LARGE_ERROR";
    case TRB_CC_ISOCH_BUFFER_OVERRUN:
        return "ISOCH_BUFFER_OVERRUN";
    case TRB_CC_EVENT_LOST_ERROR:
        return "EVENT_LOST_ERROR";
    case TRB_CC_UNDEFINED_ERROR:
        return "UNDEFINED_ERROR";
    case TRB_CC_INVALID_STREAM_ID_ERROR:
        return "INVALID_STREAM_ID_ERROR";
    case TRB_CC_SECONDARY_BANDWIDTH_ERROR:
        return "SECONDARY_BANDWIDTH_ERROR";
    case TRB_CC_SPLIT_TRANSACTION_ERROR:
        return "SPLIT_TRANSACTION_ERROR";
    default:
        return "unknown";
    }
}

enum trb_types : u32 {
    TRB_RESERVED = 0,
    TRB_LINK = 6,

    TRB_TR_NORMAL = 1,
    TRB_TR_SETUP = 2,
    TRB_TR_DATA = 3,
    TRB_TR_STATUS = 4,
    TRB_TR_ISOCH = 5,
    TRB_TR_EVDATA = 7,
    TRB_TR_NOOP = 8,

    TRB_CR_ENABLE_SLOT = 9,
    TRB_CR_DISABLE_SLOT = 10,
    TRB_CR_ADDRESS_DEVICE = 11,
    TRB_CR_CONFIGURE_ENDPOINT = 12,
    TRB_CR_EVALUATE_CONTEXT = 13,
    TRB_CR_RESET_ENDPOINT = 14,
    TRB_CR_STOP_ENDPOINT = 15,
    TRB_CR_SET_TR_DEQUEUE_POINTER = 16,
    TRB_CR_RESET_DEVICE = 17,
    TRB_CR_FORCE_EVENT = 18,
    TRB_CR_NEGOTIATE_BANDWIDTH = 19,
    TRB_CR_SET_LATENCY_TOLERANCE = 20,
    TRB_CR_GET_PORT_BANDWIDTH = 21,
    TRB_CR_FORCE_HEADER = 22,
    TRB_CR_NOOP = 23,
    TRB_CR_GET_EXT_PROPERTY = 24,
    TRB_CR_SET_EXT_PROPERTY = 25,

    TRB_EV_TRANSFER_EVENT = 32,
    TRB_EV_COMMAND_COMPLETE = 33,
    TRB_EV_PORT_STATUS_CHANGE = 34,
    TRB_EV_BANDWIDTH_REQUEST = 35,
    TRB_EV_DOORBELL = 36,
    TRB_EV_HOST_CONTROLLER = 37,
    TRB_EV_DEVICE_NOTIFICATION = 38,
    TRB_EV_MFINDEX_WRAP = 39,
};

constexpr const char* trb_type_str(u32 type) {
    switch (type) {
    case TRB_RESERVED:
        return "RESERVED";
    case TRB_LINK:
        return "LINK";
    case TRB_TR_NORMAL:
        return "NORMAL";
    case TRB_TR_SETUP:
        return "SETUP";
    case TRB_TR_DATA:
        return "DATA";
    case TRB_TR_STATUS:
        return "STATUS";
    case TRB_TR_ISOCH:
        return "ISOCH";
    case TRB_TR_EVDATA:
        return "EVDATA";
    case TRB_TR_NOOP:
        return "NOOP";
    case TRB_CR_ENABLE_SLOT:
        return "ENABLE_SLOT";
    case TRB_CR_DISABLE_SLOT:
        return "DISABLE_SLOT";
    case TRB_CR_ADDRESS_DEVICE:
        return "ADDRESS_DEVICE";
    case TRB_CR_CONFIGURE_ENDPOINT:
        return "CONFIGURE_ENDPOINT";
    case TRB_CR_EVALUATE_CONTEXT:
        return "EVALUATE_CONTEXT";
    case TRB_CR_RESET_ENDPOINT:
        return "RESET_ENDPOINT";
    case TRB_CR_STOP_ENDPOINT:
        return "STOP_ENDPOINT";
    case TRB_CR_SET_TR_DEQUEUE_POINTER:
        return "SET_TR_DEQUEUE_POINTER";
    case TRB_CR_RESET_DEVICE:
        return "RESET_DEVICE";
    case TRB_CR_FORCE_EVENT:
        return "FORCE_EVENT";
    case TRB_CR_NEGOTIATE_BANDWIDTH:
        return "NEGOTIATE_BANDWIDTH";
    case TRB_CR_SET_LATENCY_TOLERANCE:
        return "SET_LATENCY_TOLERANCE";
    case TRB_CR_GET_PORT_BANDWIDTH:
        return "GET_PORT_BANDWIDTH";
    case TRB_CR_FORCE_HEADER:
        return "FORCE_HEADER";
    case TRB_CR_NOOP:
        return "NOOP";
    case TRB_CR_GET_EXT_PROPERTY:
        return "GET_EXT_PROPERTY";
    case TRB_CR_SET_EXT_PROPERTY:
        return "SET_EXT_PROPERTY";
    case TRB_EV_TRANSFER_EVENT:
        return "TRANSFER_EVENT";
    case TRB_EV_COMMAND_COMPLETE:
        return "COMMAND_COMPLETE";
    case TRB_EV_PORT_STATUS_CHANGE:
        return "PORT_STATUS_CHANGE";
    case TRB_EV_BANDWIDTH_REQUEST:
        return "BANDWIDTH_REQUEST";
    case TRB_EV_DOORBELL:
        return "DOORBELL";
    case TRB_EV_HOST_CONTROLLER:
        return "HOST_CONTROLLER";
    case TRB_EV_DEVICE_NOTIFICATION:
        return "DEVICE_NOTIFICATION";
    case TRB_EV_MFINDEX_WRAP:
        return "MFINDEX_WRAP";
    default:
        return "unknown";
    }
}

struct input_context {
    u32 dcf;
    u32 acf;
};

enum slot_state : u32 {
    SLOT_ENABLED = 0,
    SLOT_DEFAULT = 1,
    SLOT_ADDRESSED = 2,
    SLOT_CONFIGURED = 3,
};

struct slot_context {
    u32 dwords[4];

    using DW0_ENTRIES = field<27, 4, u32>;
    using DW1_PORTNO = field<16, 8, u32>;
    using DW2_INTR = field<24, 8, u32>;
    using DW3_DEVADDR = field<0, 8, u32>;
    using DW3_STATE = field<27, 4, u32>;

    constexpr u32 entries() const { return get_field<DW0_ENTRIES>(dwords[0]); }
    constexpr u32 portno() const { return get_field<DW1_PORTNO>(dwords[1]); }
    constexpr u32 intr() const { return get_field<DW2_INTR>(dwords[2]); }
    constexpr u32 devaddr() const { return get_field<DW3_DEVADDR>(dwords[3]); }

    constexpr void set_entries(u32 entries) {
        set_field<DW0_ENTRIES>(dwords[0], entries);
    }

    constexpr void set_portno(u32 portno) {
        set_field<DW1_PORTNO>(dwords[1], portno);
    }

    constexpr void set_intr(u32 interrupter) {
        set_field<DW2_INTR>(dwords[2], interrupter);
    }

    constexpr void set_devaddr(u32 addr) {
        set_field<DW3_DEVADDR>(dwords[3], addr);
    }

    constexpr void set_state(slot_state newstate) {
        set_field<DW3_STATE>(dwords[3], newstate);
    }
};

enum ep_state : u32 {
    EP_DISABLED = 0,
    EP_RUNNING = 1,
    EP_HALTED = 2,
    EP_STOPPED = 3,
    EP_ERROR = 4,
};

struct ep_context {
    u32 dwords[5];

    using DW0_STATE = field<0, 3, u32>;
    using DW0_MAX_PSTREAMS = field<10, 5, u32>;
    using DW0_INTERVAL = field<16, 8, u32>;
    using DW1_TYPE = field<3, 3, u32>;
    using DW1_MAX_BURST_SIZE = field<8, 8, u32>;
    using DW1_MAX_PACKET_SIZE = field<16, 16, u32>;

    enum epctx_bits : u32 {
        DW0_LSA = bit(15),
        DW1_HID = bit(7),
        DW2_DCS = bit(0),
    };

    constexpr u32 state() { return get_field<DW0_STATE>(dwords[0]); }
    constexpr u32 type() const { return get_field<DW1_TYPE>(dwords[1]); }
    constexpr bool dcs() const { return dwords[2] & DW2_DCS; }

    constexpr u32 max_pstreams() const {
        return get_field<DW0_MAX_PSTREAMS>(dwords[0]);
    }

    constexpr u32 interval() const {
        return 1 << get_field<DW0_INTERVAL>(dwords[0]);
    }

    constexpr u32 max_psize() const {
        return get_field<DW1_MAX_PACKET_SIZE>(dwords[1]) *
               (1 + get_field<DW1_MAX_BURST_SIZE>(dwords[1]));
    }

    constexpr u64 dequeue_ptr() const {
        return ((u64)dwords[3] << 32) | (dwords[2] & bitmask(28, 4));
    }

    constexpr void set_state(u32 newstate) {
        set_field<DW0_STATE>(dwords[0], newstate);
    }
};

constexpr u32 usb_packet_ccode(const usb_packet& p) {
    switch (p.result) {
    case USB_RESULT_SUCCESS:
        return TRB_CC_SUCCESS;
    case USB_RESULT_STALL:
        return TRB_CC_STALL_ERROR;
    case USB_RESULT_NODEV:
    case USB_RESULT_NACK:
    case USB_RESULT_BABBLE:
    case USB_RESULT_IOERROR:
    case USB_RESULT_INCOMPLETE:
    default:
        return TRB_CC_USB_TRANSACTION_ERROR;
    }
}

enum xhci_params : u32 {
    XHCI_OP_OFF = 0x80,
    XHCI_OP_LEN = 0x400 + 0x10 * xhci::MAX_PORTS,
    XHCI_RT_OFF = 0x600,
    XHCI_RT_LEN = 0x20 + 0x20 * xhci::MAX_INTRS,
    XHCI_DB_OFF = 0x800,
    XHCI_DB_LEN = 0x4 * xhci::MAX_SLOTS,
};

static_assert(XHCI_OP_OFF < 0x100);
static_assert(XHCI_RT_OFF >= XHCI_OP_OFF + XHCI_OP_LEN);
static_assert(XHCI_DB_OFF >= XHCI_RT_OFF + XHCI_RT_LEN);

enum hciversion : u32 {
    HCIVERSION_1_0_0 = 0x100 << 16,
    HCIVERSION_1_1_0 = 0x110 << 16,
    HCIVERSION_RESET = HCIVERSION_1_0_0 | XHCI_OP_OFF,
};

using HCSPARAMS1_MAX_SLOTS = field<0, 8, u32>;
using HCSPARAMS1_MAX_INTRS = field<8, 11, u32>;
using HCSPARAMS1_MAX_PORTS = field<24, 8, u32>;

using HCSPARAMS2_IST = field<0, 4, u32>;
using HCSPARAMS2_ERSTMAX = field<4, 4, u32>;
using HCSPARAMS2_MSCPDHI = field<21, 5, u32>;
using HCSPARAMS2_MSCPDLO = field<27, 5, u32>;

enum hcsparams2_bits : u32 {
    HCSPARAMS2_SPR = bit(26),
    HCSPARAMS2_RESET = HCSPARAMS2_IST::set(0xf),
};

using HCCPARAMS1_MAXPSASIZE = field<12, 4, u32>;
using HCCPARAMS1_EXTCAPPTR = field<16, 16, u32>;

enum hccparams1_bits : u32 {
    HCCPARAMS1_AC64 = bit(0),
    HCCPARAMS1_BNC = bit(1),
    HCCPARAMS1_CSZ = bit(2),
    HCCPARAMS1_PPC = bit(3),
    HCCPARAMS1_PIND = bit(4),
    HCCPARAMS1_LHRC = bit(5),
    HCCPARAMS1_LTC = bit(6),
    HCCPARAMS1_NSS = bit(7),
    HCCPARAMS1_PAE = bit(8),
    HCCPARAMS1_SPC = bit(9),
    HCCPARAMS1_SEC = bit(10),
    HCCPARAMS1_CFC = bit(11),
    HCCPARAMS1_RESET = HCCPARAMS1_AC64 | HCCPARAMS1_EXTCAPPTR::set(8),
};

enum usbcmd_bits : u32 {
    USBCMD_RS = bit(0),
    USBCMD_HCRST = bit(1),
    USBCMD_INTE = bit(2),
    USBCMD_HSEE = bit(3),
    USBCMD_LHCRST = bit(7),
    USBCMD_CSS = bit(8),
    USBCMD_CRS = bit(9),
    USBCMD_EWE = bit(10),
    USBCMD_EU3S = bit(11),
    USBCMD_CME = bit(13),
    USBCMD_ETE = bit(14),
    USBCMD_TSCEN = bit(15),
    USBCMD_VTIOEN = bit(16),
    USBCMD_MASK = USBCMD_RS | USBCMD_HCRST | USBCMD_INTE | USBCMD_HSEE |
                  USBCMD_LHCRST | USBCMD_CSS | USBCMD_CRS | USBCMD_EWE |
                  USBCMD_EU3S | USBCMD_CME | USBCMD_ETE | USBCMD_TSCEN |
                  USBCMD_VTIOEN,
};

enum usbsts_bits : u32 {
    USBSTS_HCH = bit(0),
    USBSTS_HSE = bit(2),
    USBSTS_EINT = bit(3),
    USBSTS_PCD = bit(4),
    USBSTS_SSS = bit(8),
    USBSTS_RSS = bit(9),
    USBSTS_SRE = bit(10),
    USBSTS_CNR = bit(11),
    USBSTS_HCE = bit(12),
    USBSTS_RESET = USBSTS_HCH,
    USBSTS_RW1C = USBSTS_HSE | USBSTS_EINT | USBSTS_PCD | USBSTS_SRE,
    USBSTS_MASK = USBSTS_HCH | USBSTS_HSE | USBSTS_EINT | USBSTS_PCD |
                  USBSTS_SSS | USBSTS_RSS | USBSTS_SRE | USBSTS_CNR |
                  USBSTS_HCE,
};

constexpr bool xhci_halted(u32 usbsts) {
    return usbsts & USBSTS_HCH;
}

constexpr bool xhci_running(u32 usbsts) {
    return !xhci_halted(usbsts);
}

enum pagesize_bits : u32 {
    PAGESIZE_4K = bit(0),
    PAGESIZE_8K = bit(1),
    PAGESIZE_16K = bit(2),
};

enum crcr_bits : u32 {
    CRCR_RCS = bit(0),
    CRCR_CS = bit(1),
    CRCR_CA = bit(2),
    CRCR_CRR = bit(3),
    CRCR_CRPMASK = bitmask(26, 6),
    CRCR_MASK = CRCR_CS | CRCR_CA,
};

constexpr bool cmdring_running(u64 crcr) {
    return crcr & CRCR_CRR;
}

enum dcbaap_bits : u64 {
    DCBAAP_MASK = bitmask(58, 6),
};

using CONFIG_MAXSLOTSEN = field<0, 8, u32>;

enum config_bits : u32 {
    CONFIG_U3E = bit(8),
    CONFIG_CIE = bit(9),
    CONFIG_MASK = CONFIG_MAXSLOTSEN() | CONFIG_U3E | CONFIG_CIE,
};

using PORTSC_PLS = field<5, 4, u32>;
using PORTSC_SPEED = field<10, 4, u32>;
using PORTSC_PIC = field<14, 2, u32>;

enum portsc_bits : u32 {
    PORTSC_CCS = bit(0),
    PORTSC_PED = bit(1),
    PORTSC_OCA = bit(3),
    PORTSC_PR = bit(4),
    PORTSC_PP = bit(9),
    PORTSC_LWS = bit(16),
    PORTSC_CSC = bit(17),
    PORTSC_PEC = bit(18),
    PORTSC_WRC = bit(19),
    PORTSC_OCC = bit(20),
    PORTSC_PRC = bit(21),
    PORTSC_PLC = bit(22),
    PORTSC_CEC = bit(23),
    PORTSC_CAS = bit(24),
    PORTSC_WCE = bit(25),
    PORTSC_WDE = bit(26),
    PORTSC_WOE = bit(27),
    PORTSC_DR = bit(30),
    PORTSC_WPR = bit(31),

    PORTSC_SPEED_FULL = PORTSC_SPEED::set(1),
    PORTSC_SPEED_LOW = PORTSC_SPEED::set(2),
    PORTSC_SPEED_HIGH = PORTSC_SPEED::set(3),
    PORTSC_SPEED_SUPER = PORTSC_SPEED::set(4),

    PORTSC_PIC_OFF = PORTSC_PIC::set(0),
    PORTSC_PIC_AMBER = PORTSC_PIC::set(1),
    PORTSC_PIC_GREEN = PORTSC_PIC::set(2),
    PORTSC_PIC_UNDEF = PORTSC_PIC::set(3),

    PORTSC_ROMASK = PORTSC_CCS | PORTSC_OCA | PORTSC_SPEED() | PORTSC_CAS |
                    PORTSC_DR,
    PORTSC_RWMASK = PORTSC_PR | PORTSC_PLS() | PORTSC_PP | PORTSC_PIC() |
                    PORTSC_LWS | PORTSC_WCE | PORTSC_WDE | PORTSC_WOE |
                    PORTSC_WPR,
    PORTSC_W1CMASK = PORTSC_PED | PORTSC_CSC | PORTSC_PEC | PORTSC_WRC |
                     PORTSC_OCC | PORTSC_PRC | PORTSC_PLC | PORTSC_CEC,
};

enum portsc_pls : u32 {
    PLS_U0 = 0,
    PLS_U1 = 1,
    PLS_U2 = 2,
    PLS_U3 = 3,
    PLS_DISABLED = 4,
    PLS_RX_DETECT = 5,
    PLS_INACTIVE = 6,
    PLS_POLLING = 7,
    PLS_RECOVERY = 8,
    PLS_HOT_RESET = 9,
    PLS_COMPILANCE = 10,
    PLS_TEST_MODE = 11,
    PLS_RESUME = 15,
};

using PORTPMSC_U1TO = field<0, 8, u32>;
using PORTPMSC_U2TO = field<8, 8, u32>;

enum portpmsc_bits : u32 {
    PORTPMSC_FLA = bit(16),
    PORTPMSC_MASK = PORTPMSC_U1TO() | PORTPMSC_U2TO() | PORTPMSC_FLA,
};

using PORTLI_LEC = field<0, 16, u32>;
using PORTLI_RLC = field<16, 4, u32>;
using PORTLI_TLC = field<20, 4, u32>;

enum portli_bits : u32 {
    PORTLI_MASK = PORTLI_LEC() | PORTLI_RLC() | PORTLI_TLC(),
};

enum mfindex_bits : u32 {
    MFINDEX_MASK = bitmask(14),
};

enum iman_bits : u32 {
    IMAN_IP = bit(0),
    IMAN_IE = bit(1),
    IMAN_MASK = IMAN_IP | IMAN_IE,
};

using IMOD_INTERVAL = field<0, 16, u32>;
using IMOD_COUNTER = field<16, 16, u32>;

enum erstsz_bits : u32 {
    ERSTSZ_MASK = bitmask(16),
};

enum erstba_bits : u64 {
    ERSTBA_MASK = bitmask(58, 6),
};

using ERDP_DESI = field<0, 3, u64>;

enum erdp_bits : u64 {
    ERDP_EHB = bit(3),
    ERDP_MASK = bitmask(60, 4),
};

void xhci::endpoint::reset() {
    context = 0;
    max_psize = 0;
    max_pstreams = 0;
    interval = 0;
    mfindex = 0;
    kicked = false;
    state = EP_DISABLED;
}

void xhci::devslot::reset() {
    enabled = false;
    addressed = false;
    context = 0;
    intr = -1;
    port = -1;

    for (auto& ep : endpoints)
        ep.reset();
}

xhci::port_regs::port_regs(size_t idx):
    portsc(mkstr("portsc%zu", idx), XHCI_OP_OFF + 0x400 + 0x10 * idx),
    portpmsc(mkstr("portpmsc%zu", idx), XHCI_OP_OFF + 0x404 + 0x10 * idx),
    portli(mkstr("portli%zu", idx), XHCI_OP_OFF + 0x408 + 0x10 * idx),
    porthlpmc(mkstr("porthlpmc%zu", idx), XHCI_OP_OFF + 0x40c + 0x10 * idx) {
    portsc.tag = idx;
    portsc.sync_always();
    portsc.allow_read_write();
    portsc.on_write(&xhci::write_portsc);

    portpmsc.tag = idx;
    portpmsc.sync_always();
    portpmsc.allow_read_write();

    portli.tag = idx;
    portli.sync_always();
    portli.allow_read_write();

    porthlpmc.tag = idx;
    porthlpmc.sync_always();
    porthlpmc.allow_read_write();
}

static xhci::port_regs* create_port_regs(const char* name, size_t idx) {
    return new xhci::port_regs(idx);
}

xhci::runtime_regs::runtime_regs(size_t idx):
    iman(mkstr("iman%zu", idx), XHCI_RT_OFF + 0x20 + 0x20 * idx),
    imod(mkstr("imod%zu", idx), XHCI_RT_OFF + 0x24 + 0x20 * idx),
    erstsz(mkstr("erstsz%zu", idx), XHCI_RT_OFF + 0x28 + 0x20 * idx),
    erstba(mkstr("erstba%zu", idx), XHCI_RT_OFF + 0x30 + 0x20 * idx),
    erdp(mkstr("erdp%zu", idx), XHCI_RT_OFF + 0x38 + 0x20 * idx),
    eridx(0),
    erpcs(true) {
    iman.tag = idx;
    iman.sync_always();
    iman.allow_read_write();
    iman.on_write(&xhci::write_iman);

    imod.tag = idx;
    imod.sync_always();
    imod.allow_read_write();

    erstsz.tag = idx;
    erstsz.sync_always();
    erstsz.allow_read_write();
    erstsz.on_write_mask(ERSTSZ_MASK);

    erstba.tag = idx;
    erstba.sync_always();
    erstba.allow_read_write();
    erstba.on_write_mask(ERSTBA_MASK);

    erdp.tag = idx;
    erdp.sync_always();
    erdp.allow_read_write();
    erdp.on_write(&xhci::write_erdp);
}

static xhci::runtime_regs* create_runtime_regs(const char* name, size_t idx) {
    return new xhci::runtime_regs(idx);
}

u64 xhci::get_mfindex() const {
    u64 ns = time_to_ns(sc_time_stamp() - m_mfstart);
    return ns / 125000;
}

u32 xhci::read_hcsparams1() {
    u32 val = 0;
    set_field<HCSPARAMS1_MAX_SLOTS>(val, num_slots);
    set_field<HCSPARAMS1_MAX_INTRS>(val, num_intrs);
    set_field<HCSPARAMS1_MAX_PORTS>(val, num_ports * 2);
    return val;
}

u32 xhci::read_extcaps(size_t idx) {
    switch (idx) {
    case 0:
        return 0x02000402; // supported protocol usb 2.0
    case 1:
        return fourcc("usb ");
    case 2:
        return num_ports << 8 | 1;
    case 3:
        return 0;
    case 4:
        return 0x03000002; // supported protocol usb 3.0
    case 5:
        return fourcc("usb ");
    case 6:
        return num_ports << 8 | (num_ports + 1);
    case 7:
        return 0;
    default:
        VCML_ERROR("invalid array register index: %zu", idx);
    }
}

void xhci::write_usbcmd(u32 val) {
    if (!(usbcmd & USBCMD_RS) && (val & USBCMD_RS))
        start();
    if ((usbcmd & USBCMD_RS) && !(val & USBCMD_RS))
        stop();

    if (val & USBCMD_CSS)
        usbsts &= ~USBSTS_SRE;
    if (val & USBCMD_CRS)
        usbsts |= USBSTS_SRE;

    usbcmd = val & USBCMD_MASK;

    if (val & USBCMD_HCRST)
        reset();
    if (val & USBCMD_LHCRST)
        reset();
}

void xhci::write_usbsts(u32 val) {
    usbsts &= ~(val & USBSTS_RW1C);
    update_irq(0);
}

void xhci::write_crcrlo(u32 val) {
    if (!cmdring_running(crcrlo)) {
        m_cmdring.dequeue = val & CRCR_CRPMASK;
        m_cmdring.ccs = val & CRCR_RCS;
    }

    crcrlo = (crcrlo & ~CRCR_MASK) | (val & CRCR_MASK);
}

void xhci::write_crcrhi(u32 val) {
    if (!cmdring_running(crcrlo)) {
        m_cmdring.dequeue &= bitmask(32);
        m_cmdring.dequeue |= (u64)val << 32;
    }
}

void xhci::write_config(u32 val) {
    u32 max_slots = get_field<CONFIG_MAXSLOTSEN>(val);
    log_debug("driver wants %u device slots", max_slots);
    config = val & CONFIG_MASK;
}

void xhci::write_portsc(u32 val, size_t idx) {
    VCML_LOG_REG_BIT_CHANGE(PORTSC_CCS, ports[idx].portsc, val);
    VCML_LOG_REG_BIT_CHANGE(PORTSC_OCA, ports[idx].portsc, val);
    VCML_LOG_REG_FIELD_CHANGE(PORTSC_PLS, ports[idx].portsc, val);
    VCML_LOG_REG_BIT_CHANGE(PORTSC_PP, ports[idx].portsc, val);
    VCML_LOG_REG_FIELD_CHANGE(PORTSC_SPEED, ports[idx].portsc, val);
    VCML_LOG_REG_FIELD_CHANGE(PORTSC_PIC, ports[idx].portsc, val);
    VCML_LOG_REG_BIT_CHANGE(PORTSC_LWS, ports[idx].portsc, val);
    VCML_LOG_REG_BIT_CHANGE(PORTSC_OCC, ports[idx].portsc, val);
    VCML_LOG_REG_BIT_CHANGE(PORTSC_CAS, ports[idx].portsc, val);
    VCML_LOG_REG_BIT_CHANGE(PORTSC_DR, ports[idx].portsc, val);

    if (val & PORTSC_WPR) {
        port_reset(idx, true);
        return;
    }

    if (val & PORTSC_PR) {
        port_reset(idx, false);
        return;
    }

    if (val & PORTSC_CSC)
        log_debug("clear port %zu connection status event", idx);
    if (val & PORTSC_PEC)
        log_debug("clear port %zu enable/disable event", idx);
    if (val & PORTSC_WRC)
        log_debug("clear port %zu warm reset complete event", idx);
    if (val & PORTSC_PRC)
        log_debug("clear port %zu reset complete event", idx);
    if (val & PORTSC_PLC)
        log_debug("clear port %zu port link change event", idx);
    if (val & PORTSC_CEC)
        log_debug("clear port %zu config error change event", idx);

    auto& portsc = ports[idx].portsc;
    portsc &= ~((val & PORTSC_W1CMASK) | PORTSC_RWMASK);
    portsc |= (val & PORTSC_RWMASK);
}

u32 xhci::read_mfindex() {
    return get_mfindex() & MFINDEX_MASK;
}

void xhci::write_iman(u32 val, size_t idx) {
    if (val & IMAN_IP)
        runtime[idx].iman &= ~IMAN_IP;

    runtime[idx].iman &= ~IMAN_IE;
    runtime[idx].iman |= val & IMAN_IE;
    update_irq(idx);
}

void xhci::write_erdp(u64 val, size_t idx) {
    if (val & ERDP_EHB)
        runtime[idx].erdp &= ~ERDP_EHB;

    runtime[idx].erdp = val & ERDP_MASK;
}

void xhci::write_doorbell(u32 val, size_t idx) {
    if (idx == 0) {
        m_cmdev.notify(SC_ZERO_TIME);
        return;
    }

    u32 slotid = idx;
    u32 epid = (val & 0xff) - 1;

    log_debug("doorbell slot%u.ep%u", slotid, epid);

    if (epid >= 32) {
        log_error("invalid endpoint: %u", epid);
        return;
    }

    kick_endpoint(slotid, epid);
}

void xhci::start() {
    log_debug("host controller started");
    usbsts &= ~USBSTS_HCH;
    m_mfstart = sc_time_stamp();
}

void xhci::stop() {
    log_debug("host controller stopped");
    usbsts |= USBSTS_HCH;
}

void xhci::update_irq(size_t idx) {
    irq = (usbcmd & USBCMD_INTE) &&
          ((runtime[idx].iman & IMAN_MASK) == IMAN_MASK);
}

void xhci::handle_event(size_t intr, trb& event) {
    if (xhci_halted(usbsts)) {
        log_error("cannot send event, xhci is stopped");
        return;
    }

    if (intr >= num_intrs) {
        log_error("invalid interruptor: %zu", intr);
        return;
    }

    trb erst;
    u64 erstba = runtime[intr].erstba & ERSTBA_MASK;
    if (failed(dma.readw(erstba, erst))) {
        log_warn("failed to read event ring segment table");
        return;
    }

    u64 erba = erst.parameter & ERSTBA_MASK;
    u64 ersz = erst.status & ERSTSZ_MASK;
    u64 erdp = runtime[intr].erdp & ERDP_MASK;

    if (erdp < erba || erdp >= erba + ersz * TRB_SIZE) {
        log_warn("ERDP out of bounds: 0x%llx (ER: 0x%llx..0x%llx)", erdp, erba,
                 erba + ersz * TRB_SIZE - 1);
        return;
    }

    size_t dpidx = (erdp - erba) / TRB_SIZE;
    size_t remsz = ersz - (runtime[intr].eridx - dpidx + ersz) % ersz;

    if (remsz == 0) {
        log_warn("event ring %zu full, event dropped", intr);
        return;
    }

    if (remsz == 1)
        set_trb_ccode(event, TRB_CC_EVENT_RING_FULL_ERROR);

    if (runtime[intr].erpcs)
        event.control |= TRB_C;

    u64 addr = erba + runtime[intr].eridx * TRB_SIZE;
    if (failed(dma.writew(addr, event))) {
        log_error("failed to write to event ring at 0x%llx", addr);
        return;
    }

    runtime[intr].eridx++;
    if (runtime[intr].eridx >= ersz) {
        runtime[intr].eridx = 0;
        runtime[intr].erpcs = !runtime[intr].erpcs;
    }

    runtime[intr].erdp |= ERDP_EHB;
    runtime[intr].iman |= IMAN_IP;
    usbsts |= USBSTS_EINT;

    update_irq(intr);
}

void xhci::send_event(size_t intr, trb& event) {
    m_events.push({ event, intr });
    m_devev.notify(SC_ZERO_TIME);
}

void xhci::send_cc_event(size_t intr, u32 ccode, u32 slot, u64 addr) {
    xhci::trb event{};
    set_field<TRB_TYPE>(event.control, TRB_EV_COMMAND_COMPLETE);
    set_field<TRB_SLOT_ID>(event.control, slot);
    set_field<TRB_CCODE>(event.status, ccode);
    event.parameter = addr;
    send_event(intr, event);
}

void xhci::send_tr_event(size_t intr, u32 ccode, u32 slot, u32 ep, u64 addr) {
    xhci::trb event{};
    set_field<TRB_TYPE>(event.control, TRB_EV_TRANSFER_EVENT);
    set_field<TRB_SLOT_ID>(event.control, slot);
    set_field<TRB_EP_ID>(event.control, ep + 1);
    set_field<TRB_CCODE>(event.status, ccode);
    event.parameter = addr;
    send_event(intr, event);
}

void xhci::send_port_event(size_t intr, u32 ccode, u64 portid) {
    xhci::trb event{};
    set_field<TRB_TYPE>(event.control, TRB_EV_PORT_STATUS_CHANGE);
    set_field<TRB_CCODE>(event.status, ccode);
    set_field<TRB_PORT_ID>(event.parameter, portid);
    send_event(intr, event);
}

u32 xhci::handle_transmit(u32 slotid, u32 epid, trb& cmd) {
    auto slot = m_slots + slotid;
    auto ep = slot->endpoints + epid;
    VCML_ERROR_ON(ep->state != EP_RUNNING, "ep%u not running", epid);

    bool dirin = (epid & 1) == 0;
    if (epid == 0)
        dirin = cmd.control & TRB_DIR;

    u64 addr = cmd.parameter;
    u32 size = get_trb_data_length(cmd);
    u32 epno = (epid + 1) / 2;
    for (u32 off = 0; off < size; off += ep->max_psize) {
        vector<u8> buf(min(size - off, ep->max_psize));

        if (dirin) {
            auto packet = usb_packet_in(slotid, epno, buf.data(), buf.size());
            usb_out[slot->port].send(packet);
            if (failed(packet))
                return usb_packet_ccode(packet);
            if (failed(dma.write(addr + off, buf.data(), buf.size())))
                return TRB_CC_DATA_BUFFER_ERROR;
        } else {
            if (cmd.control & TRB_IDT) {
                if (size > sizeof(cmd.parameter))
                    return TRB_CC_TRB_ERROR;
                memcpy(buf.data(), (u8*)&cmd.parameter + off, buf.size());
            } else {
                if (failed(dma.read(addr + off, buf.data(), buf.size())))
                    return TRB_CC_DATA_BUFFER_ERROR;
            }

            auto packet = usb_packet_out(slotid, epno, buf.data(), buf.size());
            usb_out[slot->port].send(packet);
            if (failed(packet))
                return usb_packet_ccode(packet);
        }
    }

    return TRB_CC_SUCCESS;
}

void xhci::schedule_transfers() {
    u32 mfindex = get_mfindex();
    for (size_t i = 1; i < num_slots; i++) {
        auto slot = m_slots + i;
        if (slot->enabled) {
            for (size_t j = 0; j < 32; j++) {
                auto ep = slot->endpoints + j;
                if (ep->state == EP_RUNNING && ep->kicked &&
                    ep->mfindex > mfindex) {
                    m_trev.notify(125 * (ep->mfindex - mfindex), SC_US);
                }
            }
        }
    }
}

bool xhci::get_transfer(u32& slotid, u32& epid) {
    u32 mfindex = get_mfindex();
    for (size_t i = 1; i < num_slots; i++) {
        auto slot = m_slots + i;
        if (slot->enabled) {
            for (size_t j = 0; j < 32; j++) {
                auto ep = slot->endpoints + j;
                if (ep->state == EP_RUNNING && ep->kicked &&
                    ep->mfindex <= mfindex) {
                    ep->kicked = false;
                    slotid = i;
                    epid = j;
                    return true;
                }
            }
        }
    }

    return false;
}

void xhci::run_transfer(u32 slotid, u32 epid) {
    auto* slot = m_slots + slotid;
    auto* ep = slot->endpoints + epid;

    if (!slot->enabled) {
        log_warn("slot%u is not enabled", slotid);
        return;
    }

    if (ep->state == EP_STOPPED)
        update_endpoint(slotid, epid, EP_RUNNING);

    while (ep->state == EP_RUNNING) {
        trb request;
        u64 dequeue = ep->tr.dequeue;
        if (failed(dma.readw(dequeue, request))) {
            log_warn("failed to read transfer data at 0x%llx", dequeue);
            update_endpoint(slotid, epid, EP_ERROR);
            send_tr_event(slot->intr, TRB_CC_DATA_BUFFER_ERROR, slotid, epid,
                          dequeue);
            return;
        }

        bool ccs = request.control & TRB_C;
        if (ccs != ep->tr.ccs)
            return;

        if (get_trb_type(request) == TRB_LINK) {
            ep->tr.dequeue = request.parameter;
            if (request.control & TRB_TC)
                ep->tr.ccs = !ep->tr.ccs;
            continue;
        }

        ep->tr.dequeue += TRB_SIZE;
        u32 type = get_trb_type(request);
        u32 code = TRB_CC_SUCCESS;

        switch (type) {
        case TRB_TR_SETUP: {
            auto packet = usb_packet_setup(slotid, &request.parameter, 8);
            usb_out[slot->port].send(packet);
            code = usb_packet_ccode(packet);
            break;
        }

        case TRB_TR_DATA: {
            code = handle_transmit(slotid, 0, request);
            break;
        }

        case TRB_TR_STATUS: {
            if (request.control & TRB_DIR) {
                auto packet = usb_packet_in(slotid, 0, nullptr, 0);
                usb_out[slot->port].send(packet);
                code = usb_packet_ccode(packet);
            } else {
                auto packet = usb_packet_out(slotid, 0, nullptr, 0);
                usb_out[slot->port].send(packet);
                code = usb_packet_ccode(packet);
            }

            break;
        }

        case TRB_TR_ISOCH:
        case TRB_TR_NORMAL: {
            code = handle_transmit(slotid, epid, request);
            break;
        }

        default:
            log_warn("unexpected trb: %s", trb_type_str(type));
            break;
        }

        if (code == TRB_CC_STALL_ERROR || code == TRB_CC_USB_TRANSACTION_ERROR)
            update_endpoint(slotid, epid, EP_HALTED);

        if (ep->state == EP_RUNNING)
            ep->mfindex = get_mfindex() + ep->interval;

        if (request.control & TRB_IOC || code != TRB_CC_SUCCESS)
            send_tr_event(get_trb_intr(request), code, slotid, epid, dequeue);
    }
}

u32 xhci::enable_endpoint(u32 slotid, u32 epid, u64 context, u64 input) {
    auto ep = m_slots[slotid].endpoints + epid;
    if (ep->state == EP_RUNNING)
        return TRB_CC_SUCCESS;

    ep_context epctx;
    if (failed(dma.readw(input, epctx))) {
        log_error("failed to read ep%u%u input context", slotid, epid);
        return TRB_CC_TRB_ERROR;
    }

    epctx.set_state(EP_RUNNING);

    ep->context = context;
    ep->type = epctx.type();
    ep->state = epctx.state();
    ep->tr.dequeue = epctx.dequeue_ptr();
    ep->tr.ccs = epctx.dcs();
    ep->max_pstreams = 0; // streams not supported
    ep->max_psize = epctx.max_psize();
    ep->interval = epctx.interval();
    ep->mfindex = get_mfindex();

    if (failed(dma.writew(ep->context, epctx)))
        return TRB_CC_TRB_ERROR;

    log_debug("slot%u.ep%u enabled", slotid, epid);
    log_debug("  type: %s (%u)", usb_endpoint_str(ep->type), ep->type);
    log_debug("  context: 0x%llx", ep->context);
    log_debug("  dequeue: 0x%llx", ep->tr.dequeue);
    log_debug("  interval: %uus", ep->interval * 125);
    log_debug("  map-psize:    %u", ep->max_psize);
    log_debug("  max-pstreams: %u", ep->max_pstreams);

    return TRB_CC_SUCCESS;
}

u32 xhci::update_endpoint(u32 slotid, u32 epid, u32 newstate) {
    auto ep = m_slots[slotid].endpoints + epid;
    if (ep->state == newstate)
        return TRB_CC_SUCCESS;

    log_debug("slot%u.ep%u state %u -> %u", slotid, epid, ep->state, newstate);

    ep->state = newstate;

    if (newstate == EP_RUNNING)
        kick_endpoint(slotid, epid);
    else
        ep->kicked = false;

    ep_context epctx;
    if (failed(dma.readw(ep->context, epctx))) {
        log_error("failed to read slot%u.ep%u context", slotid, epid);
        return TRB_CC_TRB_ERROR;
    }

    epctx.set_state(newstate);

    if (failed(dma.writew(ep->context, epctx)))
        return TRB_CC_TRB_ERROR;

    return TRB_CC_SUCCESS;
}

void xhci::kick_endpoint(u32 slotid, u32 epid) {
    auto* slot = m_slots + slotid;
    if (!slot->enabled) {
        log_error("slot%u not enabled", slotid);
        return;
    }

    auto* ep = slot->endpoints + epid;
    if (ep->state != EP_RUNNING) {
        log_error("slot%u.ep%u is not running", slotid, epid);
        return;
    }

    ep->kicked = true;

    u32 mfindex = get_mfindex();
    if (ep->mfindex > mfindex)
        m_trev.notify(125 * (ep->mfindex - mfindex), SC_US);
    else
        m_trev.notify(SC_ZERO_TIME);
}

u32 xhci::cmd_noop(trb& cmd) {
    return TRB_CC_SUCCESS;
}

u32 xhci::cmd_enable_slot(trb& cmd, u32& slotid) {
    for (size_t i = 1; i < num_slots; i++) {
        if (!m_slots[i].enabled) {
            m_slots[i].enabled = true;
            m_slots[i].addressed = false;
            m_slots[i].context = 0;
            m_slots[i].intr = -1;
            m_slots[i].port = -1;
            slotid = i;
            return TRB_CC_SUCCESS;
        }
    }

    return TRB_CC_NO_SLOTS_AVAILABLE_ERROR;
}

u32 xhci::cmd_disable_slot(trb& cmd, u32& slotid) {
    slotid = get_trb_slot_id(cmd);
    if (slotid == 0 || slotid >= num_slots)
        return TRB_CC_TRB_ERROR;

    auto* slot = m_slots + slotid;
    if (!slot->enabled)
        return TRB_CC_SLOT_NOT_ENABLED_ERROR;

    slot->reset();

    return TRB_CC_SUCCESS;
}

u32 xhci::cmd_address_device(trb& cmd, u32& slotid) {
    slotid = get_trb_slot_id(cmd);
    auto slot = m_slots + slotid;

    if (slotid == 0 || slotid >= num_slots)
        return TRB_CC_TRB_ERROR;

    if (!slot->enabled)
        return TRB_CC_SLOT_NOT_ENABLED_ERROR;

    input_context ictx;
    slot_context sctx;

    u64 octx_addr = 0;
    u64 ictx_addr = cmd.parameter;

    if (failed(dma.readw(dcbaap + slotid * 8, octx_addr))) {
        log_error("failed to read slot %u output context address", slotid);
        return TRB_CC_TRB_ERROR;
    }

    log_debug("input context at 0x%llx", ictx_addr);
    log_debug("output context at 0x%llx", octx_addr);

    if (failed(dma.readw(ictx_addr, ictx))) {
        log_error("failed to read input context at 0x%llx", ictx_addr);
        return TRB_CC_TRB_ERROR;
    }

    if (ictx.dcf != 0 || ictx.acf != 3) {
        log_error("invalid input context: %08x %08x", ictx.dcf, ictx.acf);
        return TRB_CC_TRB_ERROR;
    }

    if (failed(dma.readw(ictx_addr + 32, sctx))) {
        log_error("failed to read slot %u input data", slotid);
        return TRB_CC_TRB_ERROR;
    }

    log_debug("slot%u context at 0x%llx [%08x %08x %08x %08x]", slotid,
              octx_addr, sctx.dwords[0], sctx.dwords[1], sctx.dwords[2],
              sctx.dwords[3]);

    size_t socket;
    if (!sctx.portno() || !port_connected(sctx.portno() - 1, socket)) {
        log_error("invalid usb port for slot%u : %d", slotid, sctx.portno());
        return TRB_CC_TRB_ERROR;
    }

    if (cmd.control & TRB_BSR) {
        sctx.set_state(SLOT_DEFAULT);
    } else {
        sctx.set_devaddr(slotid);
        sctx.set_state(SLOT_ADDRESSED);

        u8 payload[8]{};
        payload[0] = USB_REQ_OUT | USB_REQ_DEVICE;
        payload[1] = USB_REQ_SET_ADDRESS;
        payload[2] = (u8)slotid;

        usb_packet setup = usb_packet_setup(0, payload, sizeof(payload));
        usb_out[socket].send(setup);
        if (failed(setup))
            return TRB_CC_USB_TRANSACTION_ERROR;

        usb_packet status = usb_packet_in(0, 0, nullptr, 0);
        usb_out[socket].send(status);
        if (failed(status))
            return TRB_CC_USB_TRANSACTION_ERROR;
    }

    if (failed(dma.writew(octx_addr, sctx)))
        return TRB_CC_TRB_ERROR;

    slot->addressed = true;
    slot->port = socket;
    slot->intr = sctx.intr();
    slot->context = octx_addr;

    log_debug("slot%u addressed", slotid);
    log_debug("  context: 0x%llx", slot->context);
    log_debug("  port: %u", slot->port);
    log_debug("  intr: %u", slot->intr);

    return enable_endpoint(slotid, 0, slot->context + 32, ictx_addr + 64);
}

u32 xhci::cmd_configure_endpoint(trb& cmd, u32& slotid) {
    slotid = get_trb_slot_id(cmd);
    auto slot = m_slots + slotid;

    if (slotid == 0 || slotid >= num_slots)
        return TRB_CC_TRB_ERROR;

    if (!slot->enabled)
        return TRB_CC_SLOT_NOT_ENABLED_ERROR;

    if (!slot->addressed)
        return TRB_CC_CONTEXT_STATE_ERROR;

    u64 octx_addr = slot->context;
    u64 ictx_addr = cmd.parameter;

    log_debug("configure endpoint command (slot: %u)", slotid);
    log_debug(" octx: 0x%llx", octx_addr);

    input_context ictx;
    if (failed(dma.readw(ictx_addr, ictx))) {
        log_error("failed to read input context at 0x%llx", ictx_addr);
        return TRB_CC_TRB_ERROR;
    }

    log_debug(" ictx: 0x%llx [%08x %08x]", ictx_addr, ictx.dcf, ictx.acf);

    if (ictx.dcf & 3) { // deconfiguration of slot or ep0 forbidden
        log_error("invalid input context [%08x %08x]", ictx.dcf, ictx.acf);
        return TRB_CC_TRB_ERROR;
    }

    for (u32 i = 1; i < 32; i++) {
        bool a = ictx.acf & bit(i + 1);
        bool d = ictx.dcf & bit(i + 1) || (cmd.control & TRB_DC);
        auto ep = slot->endpoints + i;
        if (a && !d) {
            if (ep->state != EP_DISABLED)
                return TRB_CC_CONTEXT_STATE_ERROR;
            u64 epctx_addr = slot->context + 32 + 32 * i;
            u64 input_addr = ictx_addr + 32 + 32 + 32 * i;
            auto res = enable_endpoint(slotid, i, epctx_addr, input_addr);
            if (failed(res))
                return res;
        } else if (!a && d) {
            auto res = update_endpoint(slotid, i, EP_DISABLED);
            if (failed(res))
                return res;
        } else if (a && d) {
            if (ep->state != EP_STOPPED)
                return TRB_CC_CONTEXT_STATE_ERROR;
            auto res = update_endpoint(slotid, i, EP_RUNNING);
            if (failed(res))
                return res;
        }
    }

    slot_context input_slot_ctx;
    if (failed(dma.readw(ictx_addr + 32, input_slot_ctx))) {
        log_error("failed to read input slot context at 0x%llx",
                  ictx_addr + 32);
        return TRB_CC_TRB_ERROR;
    }

    slot_context output_slot_ctx;
    if (failed(dma.readw(octx_addr, output_slot_ctx))) {
        log_error("failed to read output slot context at 0x%llx", octx_addr);
        return TRB_CC_TRB_ERROR;
    }

    output_slot_ctx.set_state(SLOT_CONFIGURED);
    output_slot_ctx.set_entries(input_slot_ctx.entries());

    if (failed(dma.writew(octx_addr, output_slot_ctx))) {
        log_error("failed to write slot context at 0x%llx", octx_addr);
        return TRB_CC_TRB_ERROR;
    }

    return TRB_CC_SUCCESS;
}

u32 xhci::cmd_evaluate_context(trb& cmd, u32& slotid) {
    slotid = get_trb_slot_id(cmd);
    auto slot = m_slots + slotid;

    if (slotid == 0 || slotid >= num_slots)
        return TRB_CC_TRB_ERROR;

    if (!slot->enabled)
        return TRB_CC_SLOT_NOT_ENABLED_ERROR;

    if (!slot->addressed)
        return TRB_CC_CONTEXT_STATE_ERROR;

    input_context ictx;
    u64 ictx_addr = cmd.parameter;
    if (failed(dma.readw(ictx_addr, ictx))) {
        log_error("failed to read input context at 0x%llx", ictx_addr);
        return TRB_CC_TRB_ERROR;
    }

    if (ictx.acf & bit(0)) {
        slot_context slotctx;
        if (failed(dma.readw(ictx_addr + 32, slotctx))) {
            log_error("failed to read slot context at 0x%llx", ictx_addr + 32);
            return TRB_CC_TRB_ERROR;
        }

        slot->intr = slotctx.intr();
        slot->port = slotctx.portno() - 1;

        if (failed(dma.writew(slot->context, slotctx))) {
            log_error("failed to write slot context at 0x%llx", slot->context);
            return TRB_CC_TRB_ERROR;
        }

        log_debug("slot%u updated", slotid);
        log_debug("  context: 0x%llx", slot->context);
        log_debug("  port: %u", slot->port);
        log_debug("  intr: %u", slot->intr);
    }

    for (u32 i = 1; i < 32; i++) {
        if (ictx.acf & bit(i + 1)) {
            u64 epctx_addr = slot->context + 32 + 32 * i;
            u64 input_addr = ictx_addr + 32 + 32 + 32 * i;

            ep_context epctx;
            if (failed(dma.readw(input_addr, epctx))) {
                log_error("failed to read slot%u.ep%u input context at 0x%llx",
                          slotid, i, input_addr);
                return TRB_CC_TRB_ERROR;
            }

            auto ep = slot->endpoints + i;
            ep->state = epctx.state();
            ep->tr.dequeue = epctx.dequeue_ptr();
            ep->tr.ccs = epctx.dcs();
            ep->max_pstreams = 0; // streams not supported
            ep->max_psize = epctx.max_psize();
            ep->interval = epctx.interval();

            if (failed(dma.writew(epctx_addr, epctx))) {
                log_error("failed to write slot%u.ep%u context at 0x%llx",
                          slotid, i, epctx_addr);
                return TRB_CC_TRB_ERROR;
            }

            log_debug("slot%u.ep%u updated", slotid, i);
            log_debug("  type: %s (%u)", usb_endpoint_str(ep->type), ep->type);
            log_debug("  context: 0x%llx", ep->context);
            log_debug("  dequeue: 0x%llx", ep->tr.dequeue);
            log_debug("  interval: %uus", ep->interval * 125);
            log_debug("  map-psize:    %u", ep->max_psize);
            log_debug("  max-pstreams: %u", ep->max_pstreams);
        }
    }

    return TRB_CC_TRB_ERROR;
}

u32 xhci::cmd_reset_endpoint(trb& cmd, u32& slotid) {
    slotid = get_trb_slot_id(cmd);
    auto slot = m_slots + slotid;

    if (slotid == 0 || slotid >= num_slots)
        return TRB_CC_TRB_ERROR;

    if (!slot->enabled)
        return TRB_CC_SLOT_NOT_ENABLED_ERROR;

    u32 epid = get_trb_ep_id(cmd);
    if (epid < 1 || epid > 31)
        return TRB_CC_TRB_ERROR;

    auto ep = slot->endpoints + slotid;
    if (ep->state != EP_HALTED)
        return TRB_CC_CONTEXT_STATE_ERROR;

    return update_endpoint(slotid, epid, EP_STOPPED);
}

u32 xhci::cmd_stop_endpoint(trb& cmd, u32& slotid) {
    slotid = get_trb_slot_id(cmd);
    auto slot = m_slots + slotid;

    if (slotid == 0 || slotid >= num_slots)
        return TRB_CC_TRB_ERROR;

    if (!slot->enabled)
        return TRB_CC_SLOT_NOT_ENABLED_ERROR;

    u32 epid = get_trb_ep_id(cmd);
    if (epid == 0 || epid > 31)
        return TRB_CC_TRB_ERROR;

    auto ep = slot->endpoints + epid;
    if (ep->state != EP_RUNNING)
        return TRB_CC_CONTEXT_STATE_ERROR;

    return update_endpoint(slotid, epid, EP_STOPPED);
}

u32 xhci::cmd_set_tr_dequeue_pointer(trb& cmd, u32& slotid) {
    slotid = get_trb_slot_id(cmd);
    auto slot = m_slots + slotid;

    if (slotid == 0 || slotid >= num_slots)
        return TRB_CC_TRB_ERROR;

    if (!slot->enabled)
        return TRB_CC_SLOT_NOT_ENABLED_ERROR;

    u32 epid = get_trb_ep_id(cmd);
    if (epid == 0 || epid > 31)
        return TRB_CC_TRB_ERROR;

    auto ep = slot->endpoints + epid;
    if (ep->state == EP_DISABLED || ep->state == EP_HALTED)
        return TRB_CC_CONTEXT_STATE_ERROR;

    if (ep->state == EP_ERROR)
        update_endpoint(slotid, epid, EP_STOPPED);

    ep->tr.dequeue = cmd.parameter & bitmask(60, 4);
    ep->tr.ccs = cmd.parameter & bit(0);

    log_debug("slot%u.ep%u dequeue: 0x%llx", slotid, epid, ep->tr.dequeue);
    return TRB_CC_SUCCESS;
}

u32 xhci::cmd_reset_device(trb& cmd, u32& slotid) {
    slotid = get_trb_slot_id(cmd);
    auto slot = m_slots + slotid;

    if (slotid == 0 || slotid >= num_slots)
        return TRB_CC_TRB_ERROR;

    if (!slot->enabled)
        return TRB_CC_SLOT_NOT_ENABLED_ERROR;

    if (slot->addressed)
        usb_out[slot->port]->usb_reset(-1);

    for (u32 epid = 0; epid < 32; epid++) {
        if (slot->endpoints[epid].state != EP_DISABLED)
            update_endpoint(slotid, epid, EP_DISABLED);
    }

    slot_context slotctx;
    if (failed(dma.readw(slot->context, slotctx))) {
        log_error("failed to read slot context at 0x%llx", slot->context);
        return TRB_CC_TRB_ERROR;
    }

    slotctx.set_state(SLOT_DEFAULT);

    if (failed(dma.writew(slot->context, slotctx))) {
        log_error("failed to write slot context at 0x%llx", slot->context);
        return TRB_CC_TRB_ERROR;
    }

    return TRB_CC_SUCCESS;
}

bool xhci::fetch_command(trb& cmd, u64& addr) {
    while (true) {
        trb temp;
        u64 dequeue = m_cmdring.dequeue;
        if (failed(dma.readw(dequeue, temp))) {
            log_warn("failed to fetch trb from 0x%llx", dequeue);
            return false;
        }

        bool ccs = temp.control & TRB_C;
        if (ccs != m_cmdring.ccs)
            return false;

        if (get_trb_type(temp) == TRB_LINK) {
            m_cmdring.dequeue = temp.parameter;
            if (temp.control & TRB_TC)
                m_cmdring.ccs = !m_cmdring.ccs;
            continue;
        }

        m_cmdring.dequeue += TRB_SIZE;

        cmd = temp;
        addr = dequeue;
        return true;
    }
}

void xhci::execute_command(trb& cmd, u64 addr) {
    u32 code = 0, slot = 0, type = get_field<TRB_TYPE>(cmd.control);
    log_debug("command %s (%u) at 0x%llx", trb_type_str(type), type, addr);

    switch (type) {
    case TRB_CR_NOOP:
        code = cmd_noop(cmd);
        break;
    case TRB_CR_ENABLE_SLOT:
        code = cmd_enable_slot(cmd, slot);
        break;
    case TRB_CR_DISABLE_SLOT:
        code = cmd_disable_slot(cmd, slot);
        break;
    case TRB_CR_ADDRESS_DEVICE:
        code = cmd_address_device(cmd, slot);
        break;
    case TRB_CR_CONFIGURE_ENDPOINT:
        code = cmd_configure_endpoint(cmd, slot);
        break;
    case TRB_CR_EVALUATE_CONTEXT:
        code = cmd_evaluate_context(cmd, slot);
        break;
    case TRB_CR_RESET_ENDPOINT:
        code = cmd_reset_endpoint(cmd, slot);
        break;
    case TRB_CR_STOP_ENDPOINT:
        code = cmd_stop_endpoint(cmd, slot);
        break;
    case TRB_CR_SET_TR_DEQUEUE_POINTER:
        code = cmd_set_tr_dequeue_pointer(cmd, slot);
        break;
    case TRB_CR_RESET_DEVICE:
        code = cmd_reset_device(cmd, slot);
        break;
    default:
        log_warn("unsupported command %s (%u)", trb_type_str(type), type);
        sc_stop();
    }

    send_cc_event(0, code, slot, addr);
}

void xhci::command_thread() {
    while (true) {
        wait(m_cmdev);

        trb cmd;
        u64 addr;

        while (xhci_running(usbsts) && fetch_command(cmd, addr)) {
            crcrlo |= CRCR_CRR;
            execute_command(cmd, addr);

            if (crcrlo & (CRCR_CA | CRCR_CS)) {
                log_debug("stopping command ring on request");
                send_cc_event(0, TRB_CC_COMMAND_RING_STOPPED, 0, addr);
                break;
            }

            crcrhi &= ~CRCR_CRR;
        }
    }
}

void xhci::transfer_thread() {
    while (true) {
        wait(m_trev);

        u32 slotid, epid;
        while (!xhci_halted(usbsts) && get_transfer(slotid, epid))
            run_transfer(slotid, epid);

        schedule_transfers();
    }
}

void xhci::event_thread() {
    while (true) {
        wait(m_devev);

        while (xhci_running(usbsts) && m_events.size() > 0) {
            handle_event(m_events.front().intr, m_events.front().event);
            m_events.pop();
        }
    }
}

bool xhci::port_connected(size_t port, size_t& socket) {
    if (port > 2 * num_ports)
        return false;

    size_t offset = 0;
    usb_speed min = USB_SPEED_LOW;
    usb_speed max = USB_SPEED_HIGH;

    if (port >= num_ports) {
        offset = num_ports;
        min = USB_SPEED_SUPER;
        max = USB_SPEED_SUPER;
    }

    if (!usb_out.exists(port - offset))
        return false;
    if (usb_out[port - offset].connection_speed() < min)
        return false;
    if (usb_out[port - offset].connection_speed() > max)
        return false;

    socket = port - offset;
    return true;
}

void xhci::port_notify(size_t port, u32 mask) {
    auto& portsc = ports[port].portsc;
    if ((portsc & mask) == mask)
        return;

    portsc |= mask;
    if (xhci_running(usbsts))
        send_port_event(0, TRB_CC_SUCCESS, port + 1);
}

void xhci::port_reset(size_t port, bool warm) {
    size_t socket;
    if (!port_connected(port, socket))
        return;

    log_debug("usb port %zu %sreset", port, warm ? "warm-" : "");

    usb_out[socket].reset_device();
    usb_speed speed = usb_out[socket].connection_speed();
    auto& portsc = ports[port].portsc;

    if (speed == USB_SPEED_SUPER && warm)
        portsc |= PORTSC_WRC;

    portsc.set_field<PORTSC_PLS>(PLS_U0);
    portsc |= PORTSC_PED;
    portsc &= ~PORTSC_PR;
    port_notify(port, PORTSC_PRC);
}

void xhci::port_update(size_t port, bool attach) {
    auto& portsc = ports[port].portsc;

    portsc = PORTSC_PP;
    portsc.set_field<PORTSC_PLS>(PLS_RX_DETECT);

    size_t socket;
    if (attach && port_connected(port, socket)) {
        portsc |= PORTSC_CCS;
        switch (usb_out[socket].connection_speed()) {
        case USB_SPEED_LOW:
            portsc |= PORTSC_SPEED_LOW;
            portsc.set_field<PORTSC_PLS>(PLS_POLLING);
            break;
        case USB_SPEED_FULL:
            portsc |= PORTSC_SPEED_FULL;
            portsc.set_field<PORTSC_PLS>(PLS_POLLING);
            break;
        case USB_SPEED_HIGH:
            portsc |= PORTSC_SPEED_HIGH;
            portsc.set_field<PORTSC_PLS>(PLS_POLLING);
            break;
        case USB_SPEED_SUPER:
            portsc |= PORTSC_SPEED_SUPER | PORTSC_PED;
            portsc.set_field<PORTSC_PLS>(PLS_U0);
            break;
        default:
            VCML_ERROR("invalid usb speed setting");
        }
    }

    port_notify(port, PORTSC_CSC);
}

xhci::xhci(const sc_module_name& nm):
    peripheral(nm),
    usb_host_if(),
    m_mfstart(),
    m_trev("trev"),
    m_cmdev("cmdev"),
    m_devev("devev"),
    m_events(),
    m_cmdring(),
    m_slots(),
    num_slots("num_slots", 64),
    num_ports("num_ports", 4),
    num_intrs("num_intrs", 1),
    hciversion("hciversion", 0x00, HCIVERSION_RESET),
    hcsparams1("hciparams1", 0x04, 0x0),
    hcsparams2("hciparams2", 0x08, HCSPARAMS2_RESET),
    hcsparams3("hciparams3", 0x0c, 0x0),
    hccparams1("hccparams1", 0x10, HCCPARAMS1_RESET),
    dboff("dboff", 0x14, XHCI_DB_OFF),
    rtsoff("rtsoff", 0x18, XHCI_RT_OFF),
    hccparams2("hccparams2", 0x1c),
    extcaps("extcaps", 0x20),
    usbcmd("usbcmd", XHCI_OP_OFF + 0x00, 0x0),
    usbsts("usbsts", XHCI_OP_OFF + 0x04, USBSTS_RESET),
    pagesize("pagesize", XHCI_OP_OFF + 0x08, PAGESIZE_4K),
    dnctrl("dnctrl", XHCI_OP_OFF + 0x14),
    crcrlo("crcrlo", XHCI_OP_OFF + 0x18),
    crcrhi("crcrhi", XHCI_OP_OFF + 0x1c),
    dcbaap("dcbaap", XHCI_OP_OFF + 0x30),
    config("config", XHCI_OP_OFF + 0x38),
    ports("ports", 2 * num_ports, create_port_regs),
    mfindex("mfindex", XHCI_RT_OFF),
    runtime("run_regs", num_intrs, create_runtime_regs),
    doorbell("doorbell", XHCI_DB_OFF),
    in("in"),
    dma("dma"),
    irq("irq"),
    usb_out("usb_out", num_ports) {
    VCML_ERROR_ON(num_slots == 0u, "need non-zero device slots");
    VCML_ERROR_ON(num_ports == 0u, "need non-zero ports");
    VCML_ERROR_ON(num_intrs == 0u, "need non-zero interrupters");
    VCML_ERROR_ON(num_slots > MAX_SLOTS, "too many slots");
    VCML_ERROR_ON(num_ports > MAX_PORTS, "too many ports");
    VCML_ERROR_ON(num_intrs > MAX_INTRS, "too many interrupters");

    hciversion.sync_never();
    hciversion.allow_read_only();

    hcsparams1.sync_never();
    hcsparams1.allow_read_only();
    hcsparams1.on_read(&xhci::read_hcsparams1);

    hcsparams2.sync_never();
    hcsparams2.allow_read_only();

    hcsparams3.sync_never();
    hcsparams3.allow_read_only();

    hccparams1.sync_never();
    hccparams1.allow_read_only();

    dboff.sync_never();
    dboff.allow_read_only();

    rtsoff.sync_never();
    rtsoff.allow_read_only();

    hccparams2.sync_never();
    hccparams2.allow_read_only();

    usbcmd.sync_always();
    usbcmd.allow_read_write();
    usbcmd.on_write(&xhci::write_usbcmd);

    usbsts.sync_always();
    usbsts.allow_read_write();
    usbsts.on_write(&xhci::write_usbsts);

    pagesize.sync_never();
    pagesize.allow_read_only();

    dnctrl.sync_always();
    dnctrl.allow_read_write();

    crcrlo.sync_always();
    crcrlo.allow_read_write();
    crcrlo.on_read([&]() -> u32 { return crcrlo & CRCR_CRR; });
    crcrlo.on_write(&xhci::write_crcrlo);

    crcrhi.sync_always();
    crcrhi.allow_read_write();
    crcrhi.read_zero();
    crcrhi.on_write(&xhci::write_crcrhi);

    dcbaap.sync_always();
    dcbaap.allow_read_write();

    config.sync_always();
    config.allow_read_write();
    config.on_write(&xhci::write_config);

    extcaps.sync_never();
    extcaps.allow_read_only();
    extcaps.on_read(&xhci::read_extcaps);

    mfindex.sync_always();
    mfindex.allow_read_only();
    mfindex.on_read(&xhci::read_mfindex);

    doorbell.sync_on_write();
    doorbell.allow_read_write();
    doorbell.read_zero();
    doorbell.on_write(&xhci::write_doorbell);

    SC_HAS_PROCESS(xhci);
    SC_THREAD(command_thread);
    SC_THREAD(transfer_thread);
    SC_THREAD(event_thread);
}

xhci::~xhci() {
    // nothing to do
}

void xhci::reset() {
    peripheral::reset();

    for (auto& slot : m_slots)
        slot.reset();

    for (size_t i = 0; i < 2 * num_ports; i++)
        port_update(i, true);
}

void xhci::usb_attach(usb_initiator_socket& socket) {
    size_t port = usb_out.index_of(socket);
    if (socket.connection_speed() >= USB_SPEED_SUPER)
        port += num_ports;
    log_debug("port%zu connected", port);
    port_update(port, true);
}

void xhci::usb_detach(usb_initiator_socket& socket) {
    size_t port = usb_out.index_of(socket);
    if (socket.connection_speed() >= USB_SPEED_SUPER)
        port += num_ports;
    log_debug("port%zu disconnected", port);
    port_update(port, false);
}

VCML_EXPORT_MODEL(vcml::usb::xhci, name, args) {
    return new xhci(name);
}

} // namespace usb
} // namespace vcml
