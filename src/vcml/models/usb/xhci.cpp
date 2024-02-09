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

enum pagesize_bits : u32 {
    PAGESIZE_4K = bit(0),
    PAGESIZE_8K = bit(1),
    PAGESIZE_16K = bit(2),
};

using CONFIG_MAXSLOTSEN = field<0, 8, u32>;

enum config_bits : u32 {
    CONFIG_U3E = bit(8),
    CONFIG_CIE = bit(9),
    CONFIG_MASK = CONFIG_MAXSLOTSEN() | CONFIG_U3E | CONFIG_CIE,
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
using ERDP_ADDR = field<4, 60, u64>;

enum erdp_bits : u64 {
    ERDP_EHB = bit(3),
};

static u32 read_zero() {
    return 0;
}

xhci::port_regs::port_regs(size_t idx):
    portsc(mkstr("portsc%zu", idx), XHCI_OP_OFF + 0x400 + 0x10 * idx),
    portpmsc(mkstr("portpmsc%zu", idx), XHCI_OP_OFF + 0x404 + 0x10 * idx),
    portli(mkstr("portli%zu", idx), XHCI_OP_OFF + 0x408 + 0x10 * idx),
    porthlpmc(mkstr("porthlpmc%zu", idx), XHCI_OP_OFF + 0x40c + 0x10 * idx) {
    portsc.tag = idx;
    portsc.sync_always();
    portsc.allow_read_write();

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
    iman(mkstr("iman%zu", idx), XHCI_RT_OFF + 0x20 * (idx + 1) + 0x0),
    imod(mkstr("imod%zu", idx), XHCI_RT_OFF + 0x20 * (idx + 1) + 0x4),
    erstsz(mkstr("erstsz%zu", idx), XHCI_RT_OFF + 0x20 * (idx + 1) + 0x8),
    erstba(mkstr("erstba%zu", idx), XHCI_RT_OFF + 0x20 * (idx + 1) + 0x10),
    erdp(mkstr("erdp%zu", idx), XHCI_RT_OFF + 0x20 * (idx + 1) + 0x18) {
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

    erdp.tag = idx;
    erdp.sync_always();
    erdp.allow_read_write();
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
    update();
}

void xhci::write_config(u32 val) {
    u32 max_slots = get_field<CONFIG_MAXSLOTSEN>(val);
    log_info("driver wants %u device slots", max_slots);
    config = val & CONFIG_MASK;
}

u32 xhci::read_extcaps(size_t idx) {
    switch (idx) {
    case 0:
        return 0x03000002; // supported protocol usb 3.0
    case 1:
        return fourcc("usb ");
    case 2:
        return num_ports << 8 | 1;
    case 3:
        return 0;
    default:
        VCML_ERROR("invalid array register index: %zu", idx);
    }
}

u32 xhci::read_mfindex() {
    u64 ns = time_to_us(sc_time_stamp() - m_mfstart);
    return (ns / 125000) & MFINDEX_MASK;
}

void xhci::write_iman(u32 val, size_t idx) {
    runtime[idx].iman = val & IMAN_MASK;
    update();
}

void xhci::write_doorbell(u32 val, size_t idx) {
    log_info("doorbell %zu 0x%08x", idx, val);
}

void xhci::start() {
    log_info("started");
    usbsts &= ~USBSTS_HCH;
}

void xhci::stop() {
    log_info("stopped");
    usbsts |= USBSTS_HCH;
}

void xhci::update() {
    // todo
}

xhci::xhci(const sc_module_name& nm):
    peripheral(nm),
    num_slots("num_slots", 64),
    num_ports("num_ports", 1),
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
    crcr("crcr", XHCI_OP_OFF + 0x18),
    dcbaap("dcbaap", XHCI_OP_OFF + 0x30),
    config("config", XHCI_OP_OFF + 0x38),
    port("port", num_ports, create_port_regs),
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

    crcr.sync_always();
    crcr.allow_read_write();

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
    doorbell.on_read(read_zero);
    doorbell.on_write(&xhci::write_doorbell);
}

xhci::~xhci() {
    // nothing to do
}

void xhci::reset() {
    peripheral::reset();
}

VCML_EXPORT_MODEL(vcml::usb::xhci, name, args) {
    return new xhci(name);
}

} // namespace usb
} // namespace vcml
