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
using TRB_SLOT_TYPE = field<16, 5, u32>;
using TRB_SLOT = field<24, 8, u32>;
using TRB_SLOT_ID = field<24, 8, u32>;
using TRB_CCODE = field<24, 8, u32>;
using TRB_PORT_ID = field<24, 8, u64>;

enum trb_bits : u32 {
    TRB_C = bit(0),
    TRB_TC = bit(1),
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

    PORTSC_SPEED_FULL = 1,
    PORTSC_SPEED_LOW = 2,
    PORTSC_SPEED_HIGH = 3,
    PORTSC_SPEED_SUPORT = 4,

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

u32 xhci::read_hcsparams1() {
    u32 val = 0;
    set_field<HCSPARAMS1_MAX_SLOTS>(val, num_slots);
    set_field<HCSPARAMS1_MAX_INTRS>(val, num_intrs);
    set_field<HCSPARAMS1_MAX_PORTS>(val, num_ports);
    return val;
}

u32 xhci::read_extcaps(size_t idx) {
    u32 usb3_ports = num_ports / 2;
    u32 usb2_ports = num_ports / 2;

    switch (idx) {
    case 0:
        return 0x02000402; // supported protocol usb 2.0
    case 1:
        return fourcc("usb ");
    case 2:
        return usb2_ports << 8 | 1;
    case 3:
        return 0;
    case 4:
        return 0x03000002; // supported protocol usb 3.0
    case 5:
        return fourcc("usb ");
    case 6:
        return usb3_ports << 8 | (usb2_ports + 1);
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

    update();
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
    VCML_LOG_REG_BIT_CHANGE(PORTSC_PED, ports[idx].portsc, val);
    VCML_LOG_REG_BIT_CHANGE(PORTSC_OCA, ports[idx].portsc, val);
    VCML_LOG_REG_BIT_CHANGE(PORTSC_PR, ports[idx].portsc, val);
    VCML_LOG_REG_FIELD_CHANGE(PORTSC_PLS, ports[idx].portsc, val);
    VCML_LOG_REG_BIT_CHANGE(PORTSC_PP, ports[idx].portsc, val);
    VCML_LOG_REG_FIELD_CHANGE(PORTSC_SPEED, ports[idx].portsc, val);
    VCML_LOG_REG_FIELD_CHANGE(PORTSC_PIC, ports[idx].portsc, val);
    VCML_LOG_REG_BIT_CHANGE(PORTSC_LWS, ports[idx].portsc, val);
    VCML_LOG_REG_BIT_CHANGE(PORTSC_CSC, ports[idx].portsc, val);
    VCML_LOG_REG_BIT_CHANGE(PORTSC_PEC, ports[idx].portsc, val);
    VCML_LOG_REG_BIT_CHANGE(PORTSC_WRC, ports[idx].portsc, val);
    VCML_LOG_REG_BIT_CHANGE(PORTSC_OCC, ports[idx].portsc, val);
    VCML_LOG_REG_BIT_CHANGE(PORTSC_PRC, ports[idx].portsc, val);
    VCML_LOG_REG_BIT_CHANGE(PORTSC_PLC, ports[idx].portsc, val);
    VCML_LOG_REG_BIT_CHANGE(PORTSC_CEC, ports[idx].portsc, val);
    VCML_LOG_REG_BIT_CHANGE(PORTSC_CAS, ports[idx].portsc, val);
    VCML_LOG_REG_BIT_CHANGE(PORTSC_WCE, ports[idx].portsc, val);
    VCML_LOG_REG_BIT_CHANGE(PORTSC_WDE, ports[idx].portsc, val);
    VCML_LOG_REG_BIT_CHANGE(PORTSC_WOE, ports[idx].portsc, val);
    VCML_LOG_REG_BIT_CHANGE(PORTSC_DR, ports[idx].portsc, val);
    VCML_LOG_REG_BIT_CHANGE(PORTSC_WPR, ports[idx].portsc, val);

    if (val & PORTSC_WPR) {
        port_reset(idx, true);
        return;
    }

    if (val & PORTSC_PR) {
        port_reset(idx, false);
        return;
    }

    u32 portsc = ports[idx].portsc;
    portsc &= ~(val & PORTSC_W1CMASK);
    portsc &= ~PORTSC_RWMASK;
    portsc |= (val & PORTSC_RWMASK);
    ports[idx].portsc = portsc;
}

u32 xhci::read_mfindex() {
    u64 ns = time_to_us(sc_time_stamp() - m_mfstart);
    return (ns / 125000) & MFINDEX_MASK;
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
    log_debug("doorbell %zu 0x%08x", idx, val);

    if (idx == 0)
        m_cmdev.notify(SC_ZERO_TIME);
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

void xhci::update() {
    // todo
}

void xhci::update_irq(size_t idx) {
    irq = (usbcmd & USBCMD_INTE) &&
          ((runtime[idx].iman & IMAN_MASK) == IMAN_MASK);
}

void xhci::send_event(size_t intr, trb& event) {
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

void xhci::send_cc_event(size_t intr, u32 ccode, u32 slot, u64 addr) {
    xhci::trb event{};
    set_field<TRB_TYPE>(event.control, TRB_EV_COMMAND_COMPLETE);
    set_field<TRB_SLOT_ID>(event.control, slot);
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
    u32 type = get_field<TRB_TYPE>(cmd.control);
    log_debug("command %s (%u) at 0x%llx", trb_type_str(type), type, addr);
    switch (type) {
    case TRB_CR_NOOP:
        do_noop(cmd, addr);
        break;
    case TRB_CR_ENABLE_SLOT:
        do_enable_slot(cmd, addr);
        break;
    case TRB_CR_DISABLE_SLOT:
        do_disable_slot(cmd, addr);
        break;
    default:
        log_warn("unsupported command %s (%u)", trb_type_str(type), type);
    }
}

void xhci::process_commands() {
    trb cmd;
    u64 addr;

    while (fetch_command(cmd, addr)) {
        execute_command(cmd, addr);

        if (crcrlo & (CRCR_CA | CRCR_CS)) {
            log_debug("stopping command ring on request");
            send_cc_event(0, TRB_CC_COMMAND_RING_STOPPED, 0, addr);
            break;
        }

        if (xhci_halted(usbsts))
            break;
    }
}

void xhci::command_thread() {
    while (true) {
        wait(m_cmdev);

        if (!xhci_halted(usbsts)) {
            crcrlo |= CRCR_CRR;
            process_commands();
            crcrhi &= ~CRCR_CRR;
        }
    }
}

void xhci::do_noop(trb& cmd, u64 addr) {
    send_cc_event(0, TRB_CC_SUCCESS, 0, addr);
}

void xhci::do_enable_slot(trb& cmd, u64 addr) {
    size_t i = 0;
    for (i = 0; i < num_slots; i++) {
        if (!m_slots[i].enabled)
            break;
    }

    if (i >= num_slots) {
        send_cc_event(0, TRB_CC_NO_SLOTS_AVAILABLE_ERROR, 0, addr);
        return;
    }

    size_t slotid = i + 1;

    m_slots[slotid - 1].enabled = true;
    m_slots[slotid - 1].addressed = false;
    m_slots[slotid - 1].context = 0;
    m_slots[slotid - 1].irq = -1;
    m_slots[slotid - 1].port = -1;

    send_cc_event(0, TRB_CC_SUCCESS, slotid, addr);
}

void xhci::do_disable_slot(trb& cmd, u64 addr) {
    size_t slotid = get_trb_slot_id(cmd);
    if (slotid > num_slots) {
        send_cc_event(0, TRB_CC_TRB_ERROR, slotid, addr);
        return;
    }

    if (!m_slots[slotid - 1].enabled) {
        log_warn("slot %zu is not enabled", slotid);
        send_cc_event(0, TRB_CC_SLOT_NOT_ENABLED_ERROR, slotid, addr);
        return;
    }

    m_slots[slotid - 1].enabled = false;
    m_slots[slotid - 1].addressed = false;
    m_slots[slotid - 1].context = 0;
    m_slots[slotid - 1].irq = -1;
    m_slots[slotid - 1].port = -1;

    send_cc_event(0, TRB_CC_SUCCESS, slotid, addr);
}

void xhci::port_notify(size_t port, u32 mask) {
    auto& portsc = ports[port].portsc;
    if ((portsc & mask) == mask)
        return;

    portsc |= mask;
    if (xhci_running(usbsts))
        send_port_event(0, TRB_CC_SUCCESS, port);
}

void xhci::port_reset(size_t port, bool warm) {
    auto& portsc = ports[port].portsc;

    log_debug("usb port %zu %sreset", port, warm ? "warm-" : "");

    // TODO: send reset request to port

    // TODO: get USB speed from device connected to port
    usb_speed speed = USB_SPEED_SUPER;
    if (speed == USB_SPEED_SUPER && warm)
        portsc |= PORTSC_WRC;

    portsc.set_field<PORTSC_PLS>(PLS_U0);
    portsc |= PORTSC_PED;
    portsc &= PORTSC_PR;
    port_notify(port, PORTSC_CSC);
}

xhci::xhci(const sc_module_name& nm):
    peripheral(nm),
    m_mfstart(),
    m_cmdev("cmdev"),
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
    ports("ports", num_ports, create_port_regs),
    mfindex("mfindex", XHCI_RT_OFF),
    runtime("run_regs", num_intrs, create_runtime_regs),
    doorbell("doorbell", XHCI_DB_OFF),
    in("in"),
    dma("dma"),
    irq("irq") {
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
}

xhci::~xhci() {
    // nothing to do
}

void xhci::reset() {
    peripheral::reset();

    // for testing
    ports[2].portsc |= PORTSC_CCS;
}

VCML_EXPORT_MODEL(vcml::usb::xhci, name, args) {
    return new xhci(name);
}

} // namespace usb
} // namespace vcml
