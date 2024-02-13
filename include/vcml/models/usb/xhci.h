/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_USB_XHCI_H
#define VCML_USB_XHCI_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/peripheral.h"
#include "vcml/core/model.h"

#include "vcml/protocols/usb.h"
#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"

namespace vcml {
namespace usb {

class xhci : public peripheral
{
public:
    static constexpr size_t MAX_SLOTS = 64;
    static constexpr size_t MAX_PORTS = 16;
    static constexpr size_t MAX_INTRS = 15;

    struct trb {
        u64 parameter;
        u32 status;
        u32 control;
    };

    struct ring {
        u64 dequeue;
        bool ccs;
    };

    struct slot {
        u64 context;
        size_t irq;
        size_t port;
        bool enabled;
        bool addressed;
    };

    struct port_regs {
        reg<u32> portsc;
        reg<u32> portpmsc;
        reg<u32> portli;
        reg<u32> porthlpmc;

        port_regs(size_t i);
        ~port_regs() = default;
    };

    struct runtime_regs {
        reg<u32> iman;
        reg<u32> imod;
        reg<u32> erstsz;
        reg<u64> erstba;
        reg<u64> erdp;

        size_t eridx;
        bool erpcs;

        runtime_regs(size_t i);
        ~runtime_regs() = default;
    };

private:
    sc_time m_mfstart;

    sc_event m_cmdev;
    ring m_cmdring;

    slot m_slots[MAX_SLOTS];

    u32 read_hcsparams1();
    u32 read_extcaps(size_t idx);

    void write_usbcmd(u32 val);
    void write_usbsts(u32 val);
    void write_crcrlo(u32 val);
    void write_crcrhi(u32 val);
    void write_config(u32 val);

    void write_portsc(u32 val, size_t idx);

    u32 read_mfindex();

    void write_iman(u32 val, size_t idx);
    void write_erdp(u64 val, size_t idx);
    void write_doorbell(u32 val, size_t idx);

    void start();
    void stop();
    void update();
    void update_irq(size_t idx);

    void send_event(size_t intr, trb& event);
    void send_cc_event(size_t intr, u32 ccode, u32 slotid, u64 addr);
    void send_port_event(size_t intr, u32 ccode, u64 portid);

    bool fetch_command(trb& cmd, u64& addr);
    void execute_command(trb& cmd, u64 addr);
    void process_commands();
    void command_thread();

    void do_noop(trb& cmd, u64 addr);
    void do_enable_slot(trb& cmd, u64 addr);
    void do_disable_slot(trb& cmd, u64 addr);
    void do_address_device(trb& cmd, u64 addr);

    void port_notify(size_t port, u32 mask);
    void port_reset(size_t port, bool warm);
    void port_update(size_t port, bool attach);

public:
    property<size_t> num_slots;
    property<size_t> num_ports;
    property<size_t> num_intrs;

    reg<u32> hciversion;
    reg<u32> hcsparams1;
    reg<u32> hcsparams2;
    reg<u32> hcsparams3;
    reg<u32> hccparams1;
    reg<u32> dboff;
    reg<u32> rtsoff;
    reg<u32> hccparams2;
    reg<u32, 8> extcaps;

    reg<u32> usbcmd;
    reg<u32> usbsts;
    reg<u32> pagesize;
    reg<u32> dnctrl;
    reg<u32> crcrlo;
    reg<u32> crcrhi;
    reg<u64> dcbaap;
    reg<u32> config;
    sc_vector<port_regs> ports;

    reg<u32> mfindex;
    sc_vector<runtime_regs> runtime;

    reg<u32, MAX_SLOTS> doorbell;

    tlm_target_socket in;
    tlm_initiator_socket dma;
    gpio_initiator_socket irq;

    xhci(const sc_module_name& name);
    virtual ~xhci();
    VCML_KIND(usb::xhci);

    virtual void reset() override;
};

} // namespace usb
} // namespace vcml

#endif
