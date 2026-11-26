/******************************************************************************
 *                                                                            *
 * Copyright (C) 2024 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

using trb = usb::xhci::trb;

enum : u64 {
    MEM_SIZE = 0x100000,
    XHCI_BASE = 0x100000,
    XHCI_SIZE = 0x1000,
    BAD_ADDR = 0x40000000,
    DCBAA = 0x1000,
    CMD_RING = 0x2000,
    CMD_RING2 = 0x2800,
    ERST = 0x3000,
    EVT_RING = 0x4000,
    HEAP = 0x10000,
};

enum : size_t {
    RING_SIZE = 16,
    NUM_PORTS = 6,
    NUM_INTRS = 2,
};

enum : u64 {
    CAP_HCIVERSION = 0x00,
    CAP_HCSPARAMS1 = 0x04,
    CAP_HCSPARAMS2 = 0x08,
    CAP_HCSPARAMS3 = 0x0c,
    CAP_HCCPARAMS1 = 0x10,
    CAP_DBOFF = 0x14,
    CAP_RTSOFF = 0x18,
    CAP_HCCPARAMS2 = 0x1c,
    OP_USBCMD = 0x00,
    OP_USBSTS = 0x04,
    OP_PAGESIZE = 0x08,
    OP_DNCTRL = 0x14,
    OP_CRCR = 0x18,
    OP_DCBAAP = 0x30,
    OP_CONFIG = 0x38,
    OP_PORTSC = 0x400,
    OP_PORTPMSC = 0x404,
    OP_PORTLI = 0x408,
    OP_PORTHLPMC = 0x40c,
    RT_MFINDEX = 0x00,
    RT_IMAN = 0x20,
    RT_IMOD = 0x24,
    RT_ERSTSZ = 0x28,
    RT_ERSTBA = 0x30,
    RT_ERDP = 0x38,
};

enum : u32 {
    USBCMD_RS = bit(0),
    USBCMD_HCRST = bit(1),
    USBCMD_INTE = bit(2),
    USBCMD_LHCRST = bit(7),
    USBCMD_CSS = bit(8),
    USBCMD_CRS = bit(9),
    USBSTS_HCH = bit(0),
    USBSTS_EINT = bit(3),
    USBSTS_SRE = bit(10),
    USBSTS_CNR = bit(11),
    CRCR_RCS = bit(0),
    CRCR_CS = bit(1),
    CRCR_CA = bit(2),
    CRCR_CRR = bit(3),
    IMAN_IP = bit(0),
    IMAN_IE = bit(1),
    ERDP_EHB = bit(3),
    PORTSC_CCS = bit(0),
    PORTSC_PED = bit(1),
    PORTSC_PR = bit(4),
    PORTSC_PP = bit(9),
    PORTSC_LWS = bit(16),
    PORTSC_CSC = bit(17),
    PORTSC_WRC = bit(19),
    PORTSC_PRC = bit(21),
    PORTSC_WPR = bit(31),
};

enum : u32 {
    TRB_C = bit(0),
    TRB_TC = bit(1),
    TRB_ED = bit(2),
    TRB_CH = bit(4),
    TRB_IOC = bit(5),
    TRB_IDT = bit(6),
    TRB_BSR = bit(9),
    TRB_DC = bit(9),
    TRB_DIR_IN = bit(16),
};

enum : u32 {
    TRB_NORMAL = 1,
    TRB_SETUP = 2,
    TRB_DATA = 3,
    TRB_STATUS = 4,
    TRB_ISOCH = 5,
    TRB_LINK = 6,
    TRB_EVDATA = 7,
    TRB_NOOP = 8,
    CMD_ENABLE_SLOT = 9,
    CMD_DISABLE_SLOT = 10,
    CMD_ADDRESS_DEVICE = 11,
    CMD_CONFIGURE_EP = 12,
    CMD_EVALUATE_CTX = 13,
    CMD_RESET_EP = 14,
    CMD_STOP_EP = 15,
    CMD_SET_TR_DEQ = 16,
    CMD_RESET_DEVICE = 17,
    CMD_FORCE_EVENT = 18,
    CMD_NOOP = 23,
    EV_TRANSFER = 32,
    EV_CMD_COMPLETE = 33,
    EV_PORT_STATUS = 34,
    EV_HOST_CONTROLLER = 37,
};

enum : u32 {
    CC_SUCCESS = 1,
    CC_DATA_BUFFER = 2,
    CC_TRANSACTION = 4,
    CC_TRB = 5,
    CC_STALL = 6,
    CC_NO_SLOTS = 9,
    CC_SLOT_NOT_ENABLED = 11,
    CC_CONTEXT_STATE = 19,
    CC_EVENT_RING_FULL = 21,
    CC_CMD_RING_STOPPED = 24,
};

enum : u32 {
    SLOT_DEFAULT = 1,
    SLOT_ADDRESSED = 2,
    SLOT_CONFIGURED = 3,
    EPS_DISABLED = 0,
    EPS_RUNNING = 1,
    EPS_HALTED = 2,
    EPS_STOPPED = 3,
    EPS_ERROR = 4,
};

enum : u32 {
    EPT_ISOCH_OUT = 1,
    EPT_BULK_OUT = 2,
    EPT_CONTROL = 4,
    EPT_ISOCH_IN = 5,
    EPT_BULK_IN = 6,
    EPT_INT_IN = 7,
};

enum : u32 {
    PORT_KEYBOARD2 = 1,
    PORT_EMPTY = 2,
    PORT_DRIVE2 = 3,
    PORT_HEADSET = 5,
    PORT_KEYBOARD3 = 8,
};

static u32 trb_type(const trb& t) {
    return (t.control >> 10) & 0x3f;
}

static u32 trb_cc(const trb& t) {
    return t.status >> 24;
}

static u32 trb_len(const trb& t) {
    return t.status & 0xffffff;
}

static u32 trb_slot(const trb& t) {
    return t.control >> 24;
}

static u32 trb_epid(const trb& t) {
    return (t.control >> 16) & 0x1f;
}

static u32 trb_port(const trb& t) {
    return (t.parameter >> 24) & 0xff;
}

static u32 trb_ctrl(u32 type, u32 flags = 0) {
    return type << 10 | flags;
}

struct xring {
    u64 base;
    size_t size;
    size_t idx;
    bool cycle;

    u64 enq() const { return base + idx * sizeof(trb); }
    u64 deq() const { return enq() | (cycle ? 1 : 0); }
};

struct xdev {
    u32 slot;
    u32 port;
    u32 speed;
    u32 mps0;
    u64 octx;
    u64 ictx;
    xring ep[32];
};

struct xep {
    u32 dci;
    u32 type;
    u32 mps;
    u32 interval;
};

class xhci_test : public test_base
{
public:
    generic::bus bus;
    generic::memory mem;

    usb::xhci xhci;
    usb::keyboard keyboard2;
    usb::keyboard keyboard3;
    usb::drive drive2;
    usb::hostdev hostdev;
    usb::headset headset;

    tlm_initiator_socket out;
    gpio_target_socket irq;

    u64 m_op;
    u64 m_rt;
    u64 m_db;
    u64 m_heap;
    xring m_cmd;
    xring m_er[NUM_INTRS];
    u32 m_cmd_slot;
    trb m_last;
    bool m_in_unknown_command;

    xhci_test(const sc_module_name& nm):
        test_base(nm),
        bus("bus"),
        mem("mem", MEM_SIZE),
        xhci("xhci"),
        keyboard2("keyboard2"),
        keyboard3("keyboard3"),
        drive2("drive2"),
        hostdev("hostdev"),
        headset("headset"),
        out("out"),
        irq("irq"),
        m_op(0x80),
        m_rt(0x600),
        m_db(0x800),
        m_heap(HEAP),
        m_cmd(),
        m_er(),
        m_cmd_slot(),
        m_last(),
        m_in_unknown_command(false) {
        xhci.usb_out[0].bind(keyboard2.usb_in);
        xhci.usb_out[1].bind(keyboard3.usb_in);
        xhci.usb_out[2].bind(drive2.usb_in);
        xhci.usb_out[3].bind(hostdev.usb_in);
        xhci.usb_out[4].bind(headset.usb_in);

        bus.bind(mem.in, 0, MEM_SIZE - 1);
        bus.bind(xhci.in, XHCI_BASE, XHCI_BASE + XHCI_SIZE - 1);

        bus.bind(out);
        bus.bind(xhci.dma);

        xhci.irq.bind(irq);

        clk.bind(bus.clk);
        clk.bind(mem.clk);
        clk.bind(xhci.clk);

        rst.bind(bus.rst);
        rst.bind(mem.rst);
        rst.bind(xhci.rst);

        add_test("strings", &xhci_test::test_strings);
        add_test("capabilities", &xhci_test::test_capabilities);
        add_test("ports", &xhci_test::test_ports);
        add_test("microframes", &xhci_test::test_microframes);
        add_test("bringup", &xhci_test::test_bringup);
        add_test("register_access", &xhci_test::test_register_access);
        add_test("noop", &xhci_test::test_noop);
        add_test("interrupts", &xhci_test::test_interrupts);
        add_test("command_ring", &xhci_test::test_command_ring);
        add_test("command_stop", &xhci_test::test_command_stop);
        add_test("unknown_command", &xhci_test::test_unknown_command);
        add_test("slots", &xhci_test::test_slots);
        add_test("command_errors", &xhci_test::test_command_errors);
        add_test("port_reset", &xhci_test::test_port_reset);
        add_test("hotplug", &xhci_test::test_hotplug);
        add_test("address", &xhci_test::test_address);
        add_test("address_bsr", &xhci_test::test_address_bsr);
        add_test("control", &xhci_test::test_control);
        add_test("stall", &xhci_test::test_stall);
        add_test("transfer_errors", &xhci_test::test_transfer_errors);
        add_test("evaluate_context", &xhci_test::test_evaluate_context);
        add_test("configure", &xhci_test::test_configure);
        add_test("configure_errors", &xhci_test::test_configure_errors);
        add_test("interrupt_in", &xhci_test::test_interrupt_in);
        add_test("stop_endpoint", &xhci_test::test_stop_endpoint);
        add_test("bulk", &xhci_test::test_bulk);
        add_test("isochronous", &xhci_test::test_isochronous);
        add_test("reset_device", &xhci_test::test_reset_device);
        add_test("event_ring_full", &xhci_test::test_event_ring_full);
        add_test("interrupter", &xhci_test::test_interrupter);
        add_test("halt", &xhci_test::test_halt);
        add_test("hcrst", &xhci_test::test_hcrst);
    }

    virtual void end_of_simulation() override {
        test_base::end_of_simulation();
        EXPECT_FALSE(m_in_unknown_command) << "simulation stopped early";
    }

    u8* ptr(u64 addr) { return mem.data() + addr; }

    template <typename T>
    T get(u64 addr) {
        T val;
        memcpy(&val, ptr(addr), sizeof(T));
        return val;
    }

    template <typename T>
    void put(u64 addr, const T& val) {
        memcpy(ptr(addr), &val, sizeof(T));
    }

    u64 alloc(size_t size, size_t align = 64) {
        m_heap = (m_heap + align - 1) & ~(u64)(align - 1);
        u64 addr = m_heap;
        m_heap += size;
        EXPECT_LE(m_heap, MEM_SIZE) << "out of guest memory";
        memset(ptr(addr), 0, size);
        return addr;
    }

    xring new_ring(size_t size) {
        xring r{};
        r.base = alloc(size * sizeof(trb));
        r.size = size;
        r.idx = 0;
        r.cycle = true;
        return r;
    }

    u32 cap(u64 off) {
        u32 val = 0;
        EXPECT_OK(out.readw(XHCI_BASE + off, val));
        return val;
    }

    u32 op_read(u64 off) { return cap(m_op + off); }

    void op_write(u64 off, u32 val) {
        EXPECT_OK(out.writew(XHCI_BASE + m_op + off, val));
    }

    u64 op_read64(u64 off) {
        u64 val = 0;
        EXPECT_OK(out.readw(XHCI_BASE + m_op + off, val));
        return val;
    }

    void op_write64(u64 off, u64 val) {
        EXPECT_OK(out.writew(XHCI_BASE + m_op + off, val));
    }

    u32 rt_read(u64 off) { return cap(m_rt + off); }

    u32 ir_read(size_t intr, u64 off) { return rt_read(0x20 * intr + off); }

    void ir_write(size_t intr, u64 off, u32 val) {
        EXPECT_OK(out.writew(XHCI_BASE + m_rt + 0x20 * intr + off, val));
    }

    u64 ir_read64(size_t intr, u64 off) {
        u64 val = 0;
        EXPECT_OK(out.readw(XHCI_BASE + m_rt + 0x20 * intr + off, val));
        return val;
    }

    void ir_write64(size_t intr, u64 off, u64 val) {
        EXPECT_OK(out.writew(XHCI_BASE + m_rt + 0x20 * intr + off, val));
    }

    u32 portsc(u32 port) { return op_read(OP_PORTSC + 0x10 * (port - 1)); }

    void write_portsc(u32 port, u32 val) {
        op_write(OP_PORTSC + 0x10 * (port - 1), val);
    }

    void doorbell(u32 slot, u32 target) {
        EXPECT_OK(out.writew(XHCI_BASE + m_db + 4 * slot, target));
    }

    bool wait_op(u64 off, u32 mask, u32 val) {
        for (int i = 0; i < 1000; i++) {
            if ((op_read(off) & mask) == val)
                return true;
            wait(1, SC_US);
        }

        return false;
    }

    void setup_interrupter(size_t intr) {
        u64 erst = ERST + 0x40 * intr;
        u64 ring = EVT_RING + 0x1000 * intr;
        put<u64>(erst, ring);
        put<u32>(erst + 8, RING_SIZE);
        put<u32>(erst + 12, 0);
        m_er[intr] = { ring, RING_SIZE, 0, true };
        ir_write(intr, RT_ERSTSZ, 1);
        ir_write64(intr, RT_ERSTBA, erst);
        ir_write64(intr, RT_ERDP, ring);
        ir_write(intr, RT_IMOD, 4000);
        ir_write(intr, RT_IMAN, IMAN_IE);
    }

    void start_xhci(u32 max_slots = 0) {
        memset(mem.data(), 0, MEM_SIZE);
        m_heap = HEAP;

        m_op = cap(CAP_HCIVERSION) & 0xff;
        m_rt = cap(CAP_RTSOFF) & ~0x1fu;
        m_db = cap(CAP_DBOFF) & ~0x3u;

        u32 hcs1 = cap(CAP_HCSPARAMS1);
        u32 hcs2 = cap(CAP_HCSPARAMS2);
        u32 nscratch = ((hcs2 >> 21) & 0x1f) << 5 | ((hcs2 >> 27) & 0x1f);

        op_write(OP_USBCMD, op_read(OP_USBCMD) & ~USBCMD_RS);
        EXPECT_TRUE(wait_op(OP_USBSTS, USBSTS_HCH, USBSTS_HCH));
        op_write(OP_USBCMD, USBCMD_HCRST);
        EXPECT_TRUE(wait_op(OP_USBCMD, USBCMD_HCRST, 0));
        EXPECT_TRUE(wait_op(OP_USBSTS, USBSTS_CNR, 0));

        op_write(OP_CONFIG, max_slots ? max_slots : hcs1 & 0xff);

        if (nscratch > 0) {
            u64 array = alloc(8 * nscratch);
            for (u32 i = 0; i < nscratch; i++)
                put<u64>(array + 8 * i, alloc(4096, 4096));
            put<u64>(DCBAA, array);
        }

        op_write64(OP_DCBAAP, DCBAA);

        m_cmd = { CMD_RING, RING_SIZE, 0, true };
        op_write64(OP_CRCR, CMD_RING | CRCR_RCS);

        for (size_t i = 0; i < NUM_INTRS; i++)
            setup_interrupter(i);

        op_write(OP_USBCMD, USBCMD_RS | USBCMD_INTE);
        EXPECT_TRUE(wait_op(OP_USBSTS, USBSTS_HCH, 0));
    }

    u64 push(xring& r, u64 param, u32 status, u32 control) {
        u64 addr = r.enq();
        trb t{ param, status, (control & ~TRB_C) | (r.cycle ? TRB_C : 0) };
        put(addr, t);
        if (++r.idx == r.size - 1) {
            u32 ctrl = trb_ctrl(TRB_LINK, TRB_TC) | (r.cycle ? TRB_C : 0);
            put(r.enq(), trb{ r.base, 0, ctrl });
            r.idx = 0;
            r.cycle = !r.cycle;
        }

        return addr;
    }

    bool peek_event(size_t intr, trb& ev) {
        ev = get<trb>(m_er[intr].enq());
        return ((ev.control & TRB_C) != 0) == m_er[intr].cycle;
    }

    void ack_event(size_t intr) {
        ir_write(intr, RT_IMAN, IMAN_IP | IMAN_IE);
        op_write(OP_USBSTS, USBSTS_EINT);
        ir_write64(intr, RT_ERDP, m_er[intr].enq() | ERDP_EHB);
    }

    trb next_event(size_t intr = 0, bool ack = true,
                   const sc_time& timeout = sc_time(10, SC_MS)) {
        trb ev{};
        sc_time deadline = sc_time_stamp() + timeout;
        while (!peek_event(intr, ev)) {
            if (sc_time_stamp() >= deadline) {
                ADD_FAILURE() << "timeout waiting for event on intr" << intr;
                return trb{};
            }

            wait(1, SC_US);
        }

        auto& r = m_er[intr];
        if (++r.idx == r.size) {
            r.idx = 0;
            r.cycle = !r.cycle;
        }

        if (ack)
            ack_event(intr);

        m_last = ev;
        return ev;
    }

    bool no_event(size_t intr = 0, const sc_time& t = sc_time(2, SC_MS)) {
        wait(t);
        trb ev;
        return !peek_event(intr, ev);
    }

    u32 command(u32 type, u32 slot = 0, u32 dci = 0, u64 param = 0,
                u32 flags = 0) {
        u32 ctrl = trb_ctrl(type, flags) | slot << 24 | dci << 16;
        u64 addr = push(m_cmd, param, 0, ctrl);
        doorbell(0, 0);
        trb ev = next_event();
        EXPECT_EQ(trb_type(ev), EV_CMD_COMPLETE);
        EXPECT_EQ(ev.parameter, addr);
        m_cmd_slot = trb_slot(ev);
        return trb_cc(ev);
    }

    u32 enable_slot() {
        EXPECT_EQ(command(CMD_ENABLE_SLOT), CC_SUCCESS);
        return m_cmd_slot;
    }

    void reset_port(u32 port) {
        write_portsc(port, PORTSC_PP | PORTSC_PR);
        trb ev = next_event();
        EXPECT_EQ(trb_type(ev), EV_PORT_STATUS);
        EXPECT_EQ(trb_port(ev), port);
        EXPECT_EQ(trb_cc(ev), CC_SUCCESS);
        u32 sc = portsc(port);
        EXPECT_TRUE(sc & PORTSC_PED);
        EXPECT_TRUE(sc & PORTSC_PRC);
        EXPECT_FALSE(sc & PORTSC_PR);
        write_portsc(port, PORTSC_PP | PORTSC_PRC | PORTSC_CSC);
        EXPECT_FALSE(portsc(port) & (PORTSC_PRC | PORTSC_CSC));
    }

    void fill_address_input(xdev& d) {
        memset(ptr(d.ictx), 0, 33 * 32);
        put<u32>(d.ictx + 4, 3);
        put<u32>(d.ictx + 32, 1u << 27 | d.speed << 20);
        put<u32>(d.ictx + 36, d.port << 16);
        put<u32>(d.ictx + 64 + 4, d.mps0 << 16 | EPT_CONTROL << 3 | 3 << 1);
        put<u64>(d.ictx + 64 + 8, d.ep[1].deq());
        put<u32>(d.ictx + 64 + 16, 8);
    }

    xdev setup_device(u32 port, u32 mps0, size_t ringsz = RING_SIZE) {
        xdev d{};
        d.port = port;
        reset_port(port);
        d.speed = (portsc(port) >> 10) & 0xf;
        d.slot = enable_slot();
        d.mps0 = mps0;
        d.octx = alloc(32 * 32);
        d.ictx = alloc(33 * 32);
        d.ep[1] = new_ring(ringsz);
        put<u64>(DCBAA + 8 * d.slot, d.octx);
        fill_address_input(d);
        return d;
    }

    xdev address(u32 port, u32 mps0, bool bsr = false,
                 size_t ringsz = RING_SIZE) {
        xdev d = setup_device(port, mps0, ringsz);
        u32 flags = bsr ? TRB_BSR : 0;
        EXPECT_EQ(command(CMD_ADDRESS_DEVICE, d.slot, 0, d.ictx, flags),
                  CC_SUCCESS);
        return d;
    }

    u32 slot_state(const xdev& d) { return get<u32>(d.octx + 12) >> 27; }
    u32 dev_addr(const xdev& d) { return get<u32>(d.octx + 12) & 0xff; }
    u32 slot_entries(const xdev& d) { return get<u32>(d.octx) >> 27; }

    u32 ep_state(const xdev& d, u32 dci) {
        return get<u32>(d.octx + 32 * dci) & 7;
    }

    u64 ep_deq(const xdev& d, u32 dci) {
        return get<u64>(d.octx + 32 * dci + 8);
    }

    u32 ep_mps(const xdev& d, u32 dci) {
        return get<u32>(d.octx + 32 * dci + 4) >> 16;
    }

    u32 control(xdev& d, u8 type, u8 req, u16 val, u16 idx, u16 len, u64 buf,
                u32 flags = 0) {
        xring& r = d.ep[1];
        bool in = type & 0x80;
        u64 setup = (u64)type | (u64)req << 8 | (u64)val << 16 |
                    (u64)idx << 32 | (u64)len << 48;
        u32 trt = len == 0 ? 0 : (in ? 3 : 2);
        push(r, setup, 8, trb_ctrl(TRB_SETUP, TRB_IDT | trt << 16));
        if (len > 0) {
            u32 dir = in ? TRB_DIR_IN : 0;
            push(r, buf, len, trb_ctrl(TRB_DATA, dir | flags));
        }

        bool status_in = len == 0 || !in;
        u32 sflags = TRB_IOC | (status_in ? TRB_DIR_IN : 0);
        u64 status = push(r, 0, 0, trb_ctrl(TRB_STATUS, sflags));
        doorbell(d.slot, 1);

        trb ev = next_event();
        EXPECT_EQ(trb_type(ev), EV_TRANSFER);
        EXPECT_EQ(trb_slot(ev), d.slot);
        EXPECT_EQ(trb_epid(ev), 1u);
        if (trb_cc(ev) == CC_SUCCESS) {
            EXPECT_EQ(ev.parameter, status);
            EXPECT_EQ(trb_len(ev), 0u);
        }

        return trb_cc(ev);
    }

    vector<u8> get_descriptor(xdev& d, u8 type, u8 idx, u16 len) {
        u64 buf = alloc(len);
        u32 cc = control(d, 0x80, 6, type << 8 | idx, 0, len, buf);
        EXPECT_EQ(cc, CC_SUCCESS) << "get_descriptor " << (int)type;
        return vector<u8>(ptr(buf), ptr(buf) + len);
    }

    u8 config_value(xdev& d) { return get_descriptor(d, 2, 0, 9)[5]; }

    void set_configuration(xdev& d, u8 value) {
        EXPECT_EQ(control(d, 0x00, 9, value, 0, 0, 0), CC_SUCCESS);
    }

    u32 configure(xdev& d, const vector<xep>& eps, u32 drop = 0,
                  u32 flags = 0) {
        memset(ptr(d.ictx), 0, 33 * 32);
        u32 acf = bit(0);
        u32 entries = 1;
        for (const xep& ep : eps) {
            acf |= bit(ep.dci);
            entries = max(entries, ep.dci);
            d.ep[ep.dci] = new_ring(RING_SIZE);
            u64 ctx = d.ictx + 32 * (ep.dci + 1);
            put<u32>(ctx, ep.interval << 16);
            put<u32>(ctx + 4, ep.mps << 16 | ep.type << 3 | 3 << 1);
            put<u64>(ctx + 8, d.ep[ep.dci].deq());
            put<u32>(ctx + 16, ep.mps);
        }

        memcpy(ptr(d.ictx + 32), ptr(d.octx), 32);
        u32 dw0 = get<u32>(d.ictx + 32) & ~(0x1fu << 27);
        put<u32>(d.ictx + 32, dw0 | entries << 27);
        put<u32>(d.ictx, drop);
        put<u32>(d.ictx + 4, acf);
        return command(CMD_CONFIGURE_EP, d.slot, 0, d.ictx, flags);
    }

    trb transfer(xdev& d, u32 dci, u64 buf, u32 len, u32 flags = TRB_IOC,
                 u32 type = TRB_NORMAL) {
        u64 addr = push(d.ep[dci], buf, len, trb_ctrl(type, flags));
        doorbell(d.slot, dci);
        trb ev = next_event();
        EXPECT_EQ(trb_type(ev), EV_TRANSFER);
        EXPECT_EQ(trb_slot(ev), d.slot);
        EXPECT_EQ(trb_epid(ev), dci);
        EXPECT_EQ(ev.parameter, addr);
        return ev;
    }

    void attach(usb::device& dev, bool attach) {
        std::stringstream ss;
        EXPECT_TRUE(dev.execute(attach ? "usb_attach" : "usb_detach", ss))
            << ss.str();
    }

    void test_strings() {
        EXPECT_STREQ(xhci.kind(), "vcml::usb::xhci");
        EXPECT_STREQ(xhci.version(), VCML_VERSION_STRING);
        EXPECT_STREQ(keyboard2.kind(), "vcml::usb::keyboard");
        EXPECT_STREQ(keyboard2.version(), VCML_VERSION_STRING);
        EXPECT_STREQ(keyboard3.kind(), "vcml::usb::keyboard");
        EXPECT_STREQ(keyboard3.version(), VCML_VERSION_STRING);
        EXPECT_STREQ(drive2.kind(), "vcml::usb::drive");
        EXPECT_STREQ(drive2.version(), VCML_VERSION_STRING);
        EXPECT_STREQ(hostdev.kind(), "vcml::usb::hostdev");
        EXPECT_STREQ(hostdev.version(), VCML_VERSION_STRING);
        EXPECT_STREQ(headset.kind(), "vcml::usb::headset");
        EXPECT_STREQ(headset.version(), VCML_VERSION_STRING);
    }

    void test_capabilities() {
        u32 hciversion = cap(CAP_HCIVERSION);
        EXPECT_EQ(hciversion >> 16, 0x100);
        EXPECT_EQ(hciversion & 0xff, 0x80);

        u32 hcsparams1 = cap(CAP_HCSPARAMS1);
        EXPECT_EQ(hcsparams1, (2 * NUM_PORTS) << 24 | NUM_INTRS << 8 | 64);

        u32 hcsparams2 = cap(CAP_HCSPARAMS2);
        EXPECT_EQ((hcsparams2 >> 4) & 0xf, 0u);
        EXPECT_EQ(hcsparams2 >> 21, 0u);
        EXPECT_EQ(cap(CAP_HCSPARAMS3), 0u);

        u32 hccparams1 = cap(CAP_HCCPARAMS1);
        EXPECT_TRUE(hccparams1 & bit(0));
        EXPECT_FALSE(hccparams1 & bit(2));
        EXPECT_EQ(hccparams1 >> 16, 8u);
        EXPECT_EQ(cap(CAP_DBOFF), 0x800u);
        EXPECT_EQ(cap(CAP_RTSOFF), 0x600u);
        EXPECT_EQ(cap(CAP_HCCPARAMS2), 0u);

        u32 usb2[4], usb3[4];
        for (u64 i = 0; i < 4; i++) {
            usb2[i] = cap(0x20 + 4 * i);
            usb3[i] = cap(0x30 + 4 * i);
        }

        EXPECT_EQ(usb2[0] & 0xff, 2u);
        EXPECT_EQ((usb2[0] >> 8) & 0xff, 4u);
        EXPECT_EQ(usb2[0] >> 24, 0x02);
        EXPECT_EQ(usb2[1], fourcc("usb "));
        EXPECT_EQ(usb2[2] & 0xff, 1u);
        EXPECT_EQ((usb2[2] >> 8) & 0xff, NUM_PORTS);
        EXPECT_EQ(usb2[3], 0u);

        EXPECT_EQ(usb3[0] & 0xff, 2u);
        EXPECT_EQ((usb3[0] >> 8) & 0xff, 0u);
        EXPECT_EQ(usb3[0] >> 24, 0x03);
        EXPECT_EQ(usb3[1], fourcc("usb "));
        EXPECT_EQ(usb3[2] & 0xff, NUM_PORTS + 1);
        EXPECT_EQ((usb3[2] >> 8) & 0xff, NUM_PORTS);
        EXPECT_EQ(usb3[3], 0u);

        EXPECT_CE(out.writew(XHCI_BASE + CAP_HCSPARAMS1, 0u));
        EXPECT_CE(out.writew(XHCI_BASE + 0x20, 0u));
        EXPECT_EQ(cap(CAP_HCSPARAMS1), hcsparams1);
    }

    void test_ports() {
        u32 sc[6];
        sc[0] = portsc(1);
        sc[1] = portsc(2);
        sc[2] = portsc(3);
        sc[3] = portsc(7);
        sc[4] = portsc(8);
        sc[5] = portsc(9);

        EXPECT_EQ((sc[0] >> 10) & 0xf, 3);
        EXPECT_EQ((sc[1] >> 10) & 0xf, 0);
        EXPECT_EQ((sc[2] >> 10) & 0xf, 3);

        EXPECT_EQ((sc[3] >> 10) & 0xf, 0);
        EXPECT_EQ((sc[4] >> 10) & 0xf, 4);
        EXPECT_EQ((sc[5] >> 10) & 0xf, 0);
    }

    void test_microframes() {
        u32 start = rt_read(RT_MFINDEX);
        wait(125, SC_US);
        u32 end = rt_read(RT_MFINDEX);
        EXPECT_EQ(end - start, 1);
    }

    void test_bringup() {
        EXPECT_TRUE(op_read(OP_USBSTS) & USBSTS_HCH);
        EXPECT_EQ(op_read(OP_USBCMD), 0u);

        start_xhci();
        EXPECT_EQ(m_op, 0x80u);
        EXPECT_EQ(m_rt, 0x600u);
        EXPECT_EQ(m_db, 0x800u);

        EXPECT_EQ(op_read(OP_PAGESIZE), 1u);
        EXPECT_EQ(op_read(OP_CONFIG), 64u);
        EXPECT_EQ(op_read64(OP_DCBAAP), DCBAA);
        EXPECT_EQ(op_read(OP_CRCR), 0u);
        EXPECT_EQ(op_read(OP_CRCR + 4), 0u);
        EXPECT_EQ(op_read(OP_USBCMD), USBCMD_RS | USBCMD_INTE);
        EXPECT_FALSE(op_read(OP_USBSTS) & USBSTS_HCH);
        EXPECT_FALSE(op_read(OP_USBSTS) & USBSTS_CNR);

        EXPECT_EQ(ir_read(0, RT_ERSTSZ), 1u);
        EXPECT_EQ(ir_read64(0, RT_ERSTBA), ERST);
        EXPECT_EQ(ir_read64(0, RT_ERDP), EVT_RING);
        EXPECT_EQ(ir_read(0, RT_IMOD), 4000u);
        EXPECT_EQ(ir_read(0, RT_IMAN), IMAN_IE);
        EXPECT_EQ(ir_read64(1, RT_ERSTBA), ERST + 0x40);

        u32 mf0 = rt_read(RT_MFINDEX);
        wait(1, SC_MS);
        u32 mf1 = rt_read(RT_MFINDEX);
        EXPECT_EQ(mf1 - mf0, 8u);

        op_write(OP_DNCTRL, 0xffff);
        EXPECT_EQ(op_read(OP_DNCTRL), 0xffffu);

        op_write(OP_CONFIG, ~0u);
        EXPECT_EQ(op_read(OP_CONFIG), 0x3ffu);
        op_write(OP_CONFIG, 64);

        op_write(OP_USBCMD, USBCMD_INTE);
        EXPECT_TRUE(op_read(OP_USBSTS) & USBSTS_HCH);

        op_write(OP_USBCMD, USBCMD_CRS);
        EXPECT_TRUE(op_read(OP_USBSTS) & USBSTS_SRE);
        op_write(OP_USBSTS, USBSTS_SRE);
        EXPECT_FALSE(op_read(OP_USBSTS) & USBSTS_SRE);
        op_write(OP_USBCMD, USBCMD_CRS);
        op_write(OP_USBCMD, USBCMD_CSS);
        EXPECT_FALSE(op_read(OP_USBSTS) & USBSTS_SRE);

        op_write(OP_USBCMD, USBCMD_LHCRST);
        EXPECT_EQ(op_read(OP_USBCMD), 0u);
        EXPECT_EQ(op_read(OP_CONFIG), 0u);
        EXPECT_EQ(op_read64(OP_DCBAAP), 0u);
        EXPECT_EQ(op_read(OP_DNCTRL), 0u);
        EXPECT_TRUE(op_read(OP_USBSTS) & USBSTS_HCH);
    }

    void test_register_access() {
        EXPECT_EQ(cap(CAP_HCIVERSION) & 0xff, 0x80u);

        op_write(OP_DCBAAP, 0x12345000);
        op_write(OP_DCBAAP + 4, 0x9);
        EXPECT_EQ(op_read64(OP_DCBAAP), 0x912345000ull);
        EXPECT_EQ(op_read(OP_DCBAAP), 0x12345000u);
        EXPECT_EQ(op_read(OP_DCBAAP + 4), 0x9u);

        ir_write(0, RT_ERSTBA, 0xabcdef7f);
        ir_write(0, RT_ERSTBA + 4, 0x1);
        EXPECT_EQ(ir_read64(0, RT_ERSTBA), 0x1abcdef40ull);

        ir_write(0, RT_ERSTSZ, 0xffffffff);
        EXPECT_EQ(ir_read(0, RT_ERSTSZ), 0xffffu);

        ir_write64(0, RT_ERDP, 0x1234567full);
        EXPECT_EQ(ir_read64(0, RT_ERDP), 0x12345677ull);

        ir_write(0, RT_IMAN, ~0u);
        EXPECT_EQ(ir_read(0, RT_IMAN), IMAN_IE);
        ir_write(0, RT_IMAN, 0);
        EXPECT_EQ(ir_read(0, RT_IMAN), 0u);

        EXPECT_CE(out.writew(XHCI_BASE + m_op + OP_PAGESIZE, 0xffu));
        EXPECT_CE(out.writew(XHCI_BASE + m_rt + RT_MFINDEX, 0xffu));
        EXPECT_EQ(op_read(OP_PAGESIZE), 1u);

        u32 db = 0xffffffff;
        EXPECT_OK(out.readw(XHCI_BASE + m_db, db));
        EXPECT_EQ(db, 0u);

        op_write(OP_CRCR, 0xfffff040 | CRCR_RCS);
        EXPECT_EQ(op_read(OP_CRCR), 0u);
        EXPECT_EQ(op_read(OP_CRCR + 4), 0u);

        for (u32 port = 1; port <= 2 * NUM_PORTS; port++) {
            u64 off = 0x10 * (port - 1);
            op_write(OP_PORTPMSC + off, 0x1ff00);
            op_write(OP_PORTLI + off, 0x123);
            op_write(OP_PORTHLPMC + off, 0x456);
            EXPECT_EQ(op_read(OP_PORTPMSC + off), 0x1ff00u);
            EXPECT_EQ(op_read(OP_PORTLI + off), 0x123u);
            EXPECT_EQ(op_read(OP_PORTHLPMC + off), 0x456u);
        }
    }

    void test_noop() {
        start_xhci();
        EXPECT_FALSE(irq);

        u64 addr = push(m_cmd, 0, 0, trb_ctrl(CMD_NOOP));
        EXPECT_TRUE(no_event(0, sc_time(10, SC_US)));
        doorbell(0, 0);
        trb ev = next_event(0, false);
        EXPECT_EQ(trb_type(ev), EV_CMD_COMPLETE);
        EXPECT_EQ(trb_cc(ev), CC_SUCCESS);
        EXPECT_EQ(trb_slot(ev), 0u);
        EXPECT_EQ(ev.parameter, addr);

        EXPECT_TRUE(irq);
        EXPECT_TRUE(op_read(OP_USBSTS) & USBSTS_EINT);
        EXPECT_TRUE(ir_read(0, RT_IMAN) & IMAN_IP);
        EXPECT_TRUE(ir_read64(0, RT_ERDP) & ERDP_EHB);
        EXPECT_TRUE(op_read(OP_CRCR) & CRCR_CRR);

        ir_write64(0, RT_ERDP, m_er[0].enq());
        EXPECT_TRUE(ir_read64(0, RT_ERDP) & ERDP_EHB);
        EXPECT_TRUE(irq);

        ack_event(0);
        EXPECT_FALSE(irq);
        EXPECT_FALSE(op_read(OP_USBSTS) & USBSTS_EINT);
        EXPECT_FALSE(ir_read(0, RT_IMAN) & IMAN_IP);
        EXPECT_EQ(ir_read64(0, RT_ERDP), m_er[0].enq());

        EXPECT_EQ(command(CMD_NOOP), CC_SUCCESS);
        EXPECT_FALSE(irq);
    }

    void test_interrupts() {
        start_xhci();

        ir_write(0, RT_IMAN, 0);
        push(m_cmd, 0, 0, trb_ctrl(CMD_NOOP));
        doorbell(0, 0);
        next_event(0, false);
        EXPECT_TRUE(ir_read(0, RT_IMAN) & IMAN_IP);
        EXPECT_FALSE(irq);
        ir_write(0, RT_IMAN, IMAN_IE);
        EXPECT_TRUE(irq);
        ack_event(0);
        EXPECT_FALSE(irq);

        op_write(OP_USBCMD, USBCMD_RS);
        push(m_cmd, 0, 0, trb_ctrl(CMD_NOOP));
        doorbell(0, 0);
        next_event(0, false);
        EXPECT_TRUE(ir_read(0, RT_IMAN) & IMAN_IP);
        EXPECT_FALSE(irq);
        op_write(OP_USBCMD, USBCMD_RS | USBCMD_INTE);
        EXPECT_TRUE(irq);
        op_write(OP_USBCMD, USBCMD_RS);
        EXPECT_FALSE(irq);
        op_write(OP_USBCMD, USBCMD_RS | USBCMD_INTE);
        EXPECT_TRUE(irq);
        ack_event(0);
        EXPECT_FALSE(irq);
    }

    void test_command_ring() {
        start_xhci();
        for (int i = 0; i < 3 * (int)RING_SIZE; i++)
            EXPECT_EQ(command(CMD_NOOP), CC_SUCCESS) << "noop " << i;
        EXPECT_FALSE(irq);

        vector<u64> addrs(10);
        for (u64& addr : addrs)
            addr = push(m_cmd, 0, 0, trb_ctrl(CMD_NOOP));
        doorbell(0, 0);
        for (u64 addr : addrs) {
            trb ev = next_event();
            EXPECT_EQ(trb_type(ev), EV_CMD_COMPLETE);
            EXPECT_EQ(trb_cc(ev), CC_SUCCESS);
            EXPECT_EQ(ev.parameter, addr);
        }

        EXPECT_TRUE(no_event());
    }

    void test_command_stop() {
        start_xhci();
        EXPECT_FALSE(op_read(OP_CRCR) & CRCR_CRR);
        EXPECT_EQ(command(CMD_NOOP), CC_SUCCESS);
        EXPECT_TRUE(op_read(OP_CRCR) & CRCR_CRR);

        op_write(OP_CRCR, CRCR_CS);
        trb ev = next_event();
        EXPECT_EQ(trb_type(ev), EV_CMD_COMPLETE);
        EXPECT_EQ(trb_cc(ev), CC_CMD_RING_STOPPED);
        EXPECT_EQ(ev.parameter, m_cmd.enq());
        EXPECT_FALSE(op_read(OP_CRCR) & CRCR_CRR);

        EXPECT_EQ(command(CMD_NOOP), CC_SUCCESS);
        EXPECT_TRUE(op_read(OP_CRCR) & CRCR_CRR);

        op_write(OP_CRCR, CRCR_CA);
        ev = next_event();
        EXPECT_EQ(trb_cc(ev), CC_CMD_RING_STOPPED);
        EXPECT_EQ(ev.parameter, m_cmd.enq());
        EXPECT_FALSE(op_read(OP_CRCR) & CRCR_CRR);

        m_cmd = { CMD_RING2, RING_SIZE, 0, true };
        op_write(OP_CRCR + 4, 0);
        op_write(OP_CRCR, CMD_RING2 | CRCR_RCS);
        EXPECT_EQ(command(CMD_NOOP), CC_SUCCESS);
        EXPECT_EQ(m_last.parameter, CMD_RING2);
        EXPECT_EQ(command(CMD_NOOP), CC_SUCCESS);
        EXPECT_EQ(m_last.parameter, CMD_RING2 + 16);

        op_write(OP_CRCR, CMD_RING | CRCR_RCS);
        EXPECT_EQ(command(CMD_NOOP), CC_SUCCESS);
        EXPECT_EQ(m_last.parameter, CMD_RING2 + 32);

        op_write(OP_USBCMD, USBCMD_INTE);
        EXPECT_FALSE(op_read(OP_CRCR) & CRCR_CRR);
        m_cmd = { CMD_RING, RING_SIZE, 0, true };
        memset(ptr(CMD_RING), 0, RING_SIZE * sizeof(trb));
        op_write(OP_CRCR, CMD_RING | CRCR_RCS);
        op_write(OP_USBCMD, USBCMD_RS | USBCMD_INTE);
        EXPECT_EQ(command(CMD_NOOP), CC_SUCCESS);
        EXPECT_EQ(m_last.parameter, CMD_RING);

        op_write(OP_USBCMD, USBCMD_INTE);
        op_write(OP_CRCR + 4, BAD_ADDR >> 32);
        op_write(OP_CRCR, (u32)BAD_ADDR | CRCR_RCS);
        op_write(OP_USBCMD, USBCMD_RS | USBCMD_INTE);
        doorbell(0, 0);
        EXPECT_TRUE(no_event());
    }

    void test_unknown_command() {
        start_xhci();
        m_in_unknown_command = true;
        EXPECT_EQ(command(CMD_FORCE_EVENT), CC_TRB);
        EXPECT_EQ(command(TRB_NORMAL), CC_TRB);
        EXPECT_EQ(command(TRB_LINK + 0x30), CC_TRB);
        EXPECT_EQ(command(EV_TRANSFER), CC_TRB);
        EXPECT_EQ(command(CMD_NOOP), CC_SUCCESS);
        m_in_unknown_command = false;
    }

    void test_slots() {
        start_xhci();
        EXPECT_EQ(enable_slot(), 1u);
        EXPECT_EQ(enable_slot(), 2u);
        EXPECT_EQ(command(CMD_DISABLE_SLOT, 1), CC_SUCCESS);
        EXPECT_EQ(m_cmd_slot, 1u);
        EXPECT_EQ(command(CMD_DISABLE_SLOT, 1), CC_SLOT_NOT_ENABLED);
        EXPECT_EQ(command(CMD_DISABLE_SLOT, 0), CC_TRB);
        EXPECT_EQ(command(CMD_DISABLE_SLOT, 64), CC_TRB);
        EXPECT_EQ(enable_slot(), 1u);
        EXPECT_EQ(enable_slot(), 3u);

        for (u32 i = 4; i < 64; i++)
            EXPECT_EQ(enable_slot(), i);
        EXPECT_EQ(command(CMD_ENABLE_SLOT), CC_NO_SLOTS);

        reset_port(PORT_KEYBOARD2);
        xdev d{};
        d.slot = 40;
        d.port = PORT_KEYBOARD2;
        d.speed = 3;
        d.mps0 = 64;
        d.octx = alloc(32 * 32);
        d.ictx = alloc(33 * 32);
        d.ep[1] = new_ring(RING_SIZE);
        put<u64>(DCBAA + 8 * d.slot, d.octx);
        fill_address_input(d);
        EXPECT_EQ(command(CMD_ADDRESS_DEVICE, d.slot, 0, d.ictx), CC_SUCCESS);
        EXPECT_EQ(slot_state(d), SLOT_ADDRESSED);
        EXPECT_EQ(dev_addr(d), 40u);
        EXPECT_EQ(command(CMD_STOP_EP, d.slot, 1), CC_SUCCESS);
        EXPECT_EQ(ep_state(d, 1), EPS_STOPPED);
        EXPECT_EQ(get_descriptor(d, 1, 0, 18)[1], 1);

        start_xhci(2);
        EXPECT_EQ(enable_slot(), 1u);
        EXPECT_EQ(enable_slot(), 2u);
        EXPECT_EQ(command(CMD_ENABLE_SLOT), CC_NO_SLOTS);
        EXPECT_EQ(command(CMD_DISABLE_SLOT, 2), CC_SUCCESS);
        EXPECT_EQ(enable_slot(), 2u);
    }

    void test_command_errors() {
        start_xhci();
        u64 ictx = alloc(33 * 32);
        const u32 slot_cmds[] = {
            CMD_ADDRESS_DEVICE, CMD_CONFIGURE_EP, CMD_EVALUATE_CTX,
            CMD_RESET_EP,       CMD_STOP_EP,      CMD_SET_TR_DEQ,
            CMD_RESET_DEVICE,
        };

        for (u32 type : slot_cmds) {
            EXPECT_EQ(command(type, 0, 1, ictx), CC_TRB) << type;
            EXPECT_EQ(command(type, 64, 1, ictx), CC_TRB) << type;
            EXPECT_EQ(command(type, 5, 1, ictx), CC_SLOT_NOT_ENABLED) << type;
        }

        u32 slot = enable_slot();
        EXPECT_EQ(command(CMD_CONFIGURE_EP, slot, 0, ictx), CC_CONTEXT_STATE);
        EXPECT_EQ(command(CMD_EVALUATE_CTX, slot, 0, ictx), CC_CONTEXT_STATE);
        EXPECT_EQ(command(CMD_RESET_DEVICE, slot), CC_CONTEXT_STATE);
        EXPECT_EQ(command(CMD_RESET_EP, slot, 0), CC_TRB);
        EXPECT_EQ(command(CMD_STOP_EP, slot, 0), CC_TRB);
        EXPECT_EQ(command(CMD_SET_TR_DEQ, slot, 0), CC_TRB);
        EXPECT_EQ(command(CMD_STOP_EP, slot, 1), CC_CONTEXT_STATE);
        EXPECT_EQ(command(CMD_RESET_EP, slot, 1), CC_CONTEXT_STATE);

        xdev d = setup_device(PORT_KEYBOARD2, 64);
        put<u32>(d.ictx + 4, 1);
        EXPECT_EQ(command(CMD_ADDRESS_DEVICE, d.slot, 0, d.ictx), CC_TRB);
        put<u32>(d.ictx, 4);
        put<u32>(d.ictx + 4, 3);
        EXPECT_EQ(command(CMD_ADDRESS_DEVICE, d.slot, 0, d.ictx), CC_TRB);
        put<u32>(d.ictx, 0);

        const u32 bad_ports[] = {
            0, PORT_EMPTY, NUM_PORTS, 2 * NUM_PORTS, 2 * NUM_PORTS + 1, 255,
        };
        for (u32 port : bad_ports) {
            put<u32>(d.ictx + 36, port << 16);
            EXPECT_EQ(command(CMD_ADDRESS_DEVICE, d.slot, 0, d.ictx), CC_TRB)
                << "port " << port;
        }

        EXPECT_EQ(command(CMD_ADDRESS_DEVICE, d.slot, 0, BAD_ADDR), CC_TRB);
        put<u64>(DCBAA + 8 * d.slot, BAD_ADDR);
        fill_address_input(d);
        EXPECT_EQ(command(CMD_ADDRESS_DEVICE, d.slot, 0, d.ictx), CC_TRB);
        put<u64>(DCBAA + 8 * d.slot, d.octx);
        reset_port(PORT_KEYBOARD2);
        EXPECT_EQ(command(CMD_ADDRESS_DEVICE, d.slot, 0, d.ictx), CC_SUCCESS);
        EXPECT_EQ(command(CMD_CONFIGURE_EP, d.slot, 0, BAD_ADDR), CC_TRB);
        EXPECT_EQ(command(CMD_EVALUATE_CTX, d.slot, 0, BAD_ADDR), CC_TRB);
        EXPECT_EQ(command(CMD_DISABLE_SLOT, d.slot), CC_SUCCESS);
        xdev d2 = setup_device(PORT_DRIVE2, 64);
        EXPECT_EQ(command(CMD_ADDRESS_DEVICE, d2.slot, 0, d2.ictx),
                  CC_SUCCESS);
        EXPECT_EQ(command(CMD_DISABLE_SLOT, d2.slot), CC_SUCCESS);
        d2.slot = enable_slot();
        put<u64>(DCBAA + 8 * d2.slot, d2.octx);
        EXPECT_EQ(command(CMD_ADDRESS_DEVICE, d2.slot, 0, d2.ictx),
                  CC_TRANSACTION);
    }

    void test_port_reset() {
        start_xhci();

        u32 sc = portsc(PORT_KEYBOARD2);
        EXPECT_TRUE(sc & PORTSC_CCS);
        EXPECT_TRUE(sc & PORTSC_PP);
        EXPECT_TRUE(sc & PORTSC_CSC);
        EXPECT_FALSE(sc & PORTSC_PED);
        EXPECT_EQ((sc >> 5) & 0xf, 7u);
        EXPECT_EQ((sc >> 10) & 0xf, 3u);

        sc = portsc(PORT_KEYBOARD3);
        EXPECT_TRUE(sc & PORTSC_CCS);
        EXPECT_TRUE(sc & PORTSC_PED);
        EXPECT_EQ((sc >> 5) & 0xf, 0u);
        EXPECT_EQ((sc >> 10) & 0xf, 4u);

        sc = portsc(PORT_EMPTY);
        EXPECT_FALSE(sc & PORTSC_CCS);
        EXPECT_TRUE(sc & PORTSC_PP);
        EXPECT_EQ((sc >> 5) & 0xf, 5u);

        sc = portsc(PORT_HEADSET);
        EXPECT_TRUE(sc & PORTSC_CCS);
        EXPECT_EQ((sc >> 10) & 0xf, 3u);

        reset_port(PORT_KEYBOARD2);
        sc = portsc(PORT_KEYBOARD2);
        EXPECT_EQ((sc >> 5) & 0xf, 0u);
        EXPECT_TRUE(sc & PORTSC_CCS);

        write_portsc(PORT_KEYBOARD2, PORTSC_PP);
        sc = portsc(PORT_KEYBOARD2);
        EXPECT_TRUE(sc & PORTSC_PED);
        EXPECT_TRUE(sc & PORTSC_CCS);
        EXPECT_EQ((sc >> 5) & 0xf, 0u);
        EXPECT_EQ((sc >> 10) & 0xf, 3u);

        write_portsc(PORT_KEYBOARD2, PORTSC_PP | 3 << 5 | PORTSC_LWS);
        sc = portsc(PORT_KEYBOARD2);
        EXPECT_EQ((sc >> 5) & 0xf, 3u);
        EXPECT_FALSE(sc & PORTSC_LWS);
        write_portsc(PORT_KEYBOARD2, PORTSC_PP);
        EXPECT_EQ((portsc(PORT_KEYBOARD2) >> 5) & 0xf, 3u);
        write_portsc(PORT_KEYBOARD2, PORTSC_PP | PORTSC_LWS);
        EXPECT_EQ((portsc(PORT_KEYBOARD2) >> 5) & 0xf, 0u);

        write_portsc(PORT_KEYBOARD2, PORTSC_PP | PORTSC_PED);
        EXPECT_FALSE(portsc(PORT_KEYBOARD2) & PORTSC_PED);
        EXPECT_TRUE(portsc(PORT_KEYBOARD2) & PORTSC_CCS);

        write_portsc(PORT_EMPTY, PORTSC_PP | PORTSC_PR);
        EXPECT_TRUE(no_event());
        EXPECT_FALSE(portsc(PORT_EMPTY) & (PORTSC_PED | PORTSC_PRC));

        write_portsc(PORT_KEYBOARD3, PORTSC_PP | PORTSC_WPR);
        trb ev = next_event();
        EXPECT_EQ(trb_type(ev), EV_PORT_STATUS);
        EXPECT_EQ(trb_port(ev), PORT_KEYBOARD3);
        sc = portsc(PORT_KEYBOARD3);
        EXPECT_TRUE(sc & PORTSC_WRC);
        EXPECT_TRUE(sc & PORTSC_PRC);
        EXPECT_TRUE(sc & PORTSC_PED);
        EXPECT_FALSE(sc & PORTSC_WPR);
        write_portsc(PORT_KEYBOARD3, PORTSC_PP | PORTSC_WRC | PORTSC_PRC);
        sc = portsc(PORT_KEYBOARD3);
        EXPECT_FALSE(sc & (PORTSC_WRC | PORTSC_PRC));

        reset_port(PORT_KEYBOARD3);
        reset_port(PORT_DRIVE2);
        EXPECT_TRUE(no_event());
    }

    void test_hotplug() {
        start_xhci();
        EXPECT_FALSE(portsc(4) & PORTSC_CCS);

        attach(keyboard2, false);
        trb ev = next_event();
        EXPECT_EQ(trb_type(ev), EV_PORT_STATUS);
        EXPECT_EQ(trb_port(ev), PORT_KEYBOARD2);
        u32 sc = portsc(PORT_KEYBOARD2);
        EXPECT_FALSE(sc & PORTSC_CCS);
        EXPECT_FALSE(sc & PORTSC_PED);
        EXPECT_TRUE(sc & PORTSC_CSC);
        EXPECT_EQ((sc >> 10) & 0xf, 0u);
        write_portsc(PORT_KEYBOARD2, PORTSC_PP | PORTSC_CSC);
        write_portsc(PORT_KEYBOARD2, PORTSC_PP | PORTSC_PR);
        EXPECT_TRUE(no_event());

        attach(keyboard2, true);
        ev = next_event();
        EXPECT_EQ(trb_port(ev), PORT_KEYBOARD2);
        sc = portsc(PORT_KEYBOARD2);
        EXPECT_TRUE(sc & PORTSC_CCS);
        EXPECT_TRUE(sc & PORTSC_CSC);
        EXPECT_EQ((sc >> 10) & 0xf, 3u);
        write_portsc(PORT_KEYBOARD2, PORTSC_PP | PORTSC_CSC);

        attach(keyboard3, false);
        ev = next_event();
        EXPECT_EQ(trb_port(ev), PORT_KEYBOARD3);
        EXPECT_FALSE(portsc(PORT_KEYBOARD3) & PORTSC_CCS);
        write_portsc(PORT_KEYBOARD3, PORTSC_PP | PORTSC_CSC);
        attach(keyboard3, true);
        ev = next_event();
        EXPECT_EQ(trb_port(ev), PORT_KEYBOARD3);
        sc = portsc(PORT_KEYBOARD3);
        EXPECT_TRUE(sc & PORTSC_CCS);
        EXPECT_TRUE(sc & PORTSC_PED);
        EXPECT_EQ((sc >> 10) & 0xf, 4u);

        op_write(OP_USBCMD, USBCMD_INTE);
        attach(drive2, false);
        EXPECT_FALSE(portsc(PORT_DRIVE2) & PORTSC_CCS);
        EXPECT_TRUE(portsc(PORT_DRIVE2) & PORTSC_CSC);
        attach(drive2, true);
        EXPECT_TRUE(portsc(PORT_DRIVE2) & PORTSC_CCS);
        op_write(OP_USBCMD, USBCMD_RS | USBCMD_INTE);
        EXPECT_TRUE(no_event());

        xdev d = address(PORT_KEYBOARD2, 64);
        attach(keyboard2, false);
        ev = next_event();
        EXPECT_EQ(trb_port(ev), PORT_KEYBOARD2);
        u64 buf = alloc(18);
        EXPECT_EQ(control(d, 0x80, 6, 0x100, 0, 18, buf), CC_TRANSACTION);
        EXPECT_EQ(ep_state(d, 1), EPS_HALTED);
        attach(keyboard2, true);
        ev = next_event();
        EXPECT_EQ(trb_port(ev), PORT_KEYBOARD2);
    }

    void test_address() {
        start_xhci();
        xdev d = address(PORT_KEYBOARD2, 64);
        EXPECT_EQ(d.slot, 1u);
        EXPECT_EQ(slot_state(d), SLOT_ADDRESSED);
        EXPECT_EQ(dev_addr(d), d.slot);
        EXPECT_EQ((get<u32>(d.octx + 4) >> 16) & 0xff, PORT_KEYBOARD2);
        EXPECT_EQ(ep_state(d, 1), EPS_RUNNING);
        EXPECT_EQ(ep_deq(d, 1), d.ep[1].deq());
        EXPECT_EQ(ep_mps(d, 1), 64u);

        vector<u8> desc = get_descriptor(d, 1, 0, 18);
        EXPECT_EQ(desc[0], 18);
        EXPECT_EQ(desc[1], 1);
        EXPECT_EQ(desc[2] | desc[3] << 8, 0x200);
        EXPECT_EQ(desc[7], 8);
        EXPECT_EQ(desc[8] | desc[9] << 8, USB_VENDOR_VCML);

        EXPECT_EQ(get_descriptor(d, 3, 0, 4),
                  vector<u8>({ 4, 3, 0x09, 0x04 }));
        string product = "VCML Keyboard";
        vector<u8> str = get_descriptor(d, 3, desc[15],
                                        (u16)(2 + 2 * product.size()));
        EXPECT_EQ(str[0], 2 + 2 * product.size());
        EXPECT_EQ(str[1], 3);
        for (size_t i = 0; i < product.size(); i++)
            EXPECT_EQ(str[2 + 2 * i], product[i]);

        vector<u8> cfg = get_descriptor(d, 2, 0, 9);
        EXPECT_EQ(cfg[1], 2);
        EXPECT_EQ(cfg[5], 42);
        u16 total = cfg[2] | cfg[3] << 8;
        cfg = get_descriptor(d, 2, 0, total);
        EXPECT_EQ(cfg[9 + 1], 4);
        EXPECT_EQ(cfg[9 + 5], 3);

        set_configuration(d, 42);
        u64 buf = alloc(2);
        EXPECT_EQ(control(d, 0x80, 8, 0, 0, 1, buf), CC_SUCCESS);
        EXPECT_EQ(get<u8>(buf), 42);
        EXPECT_EQ(control(d, 0x80, 0, 0, 0, 2, buf), CC_SUCCESS);
        EXPECT_EQ(get<u16>(buf), 2u);

        xdev d3 = address(PORT_KEYBOARD3, 512);
        EXPECT_EQ(d3.slot, 2u);
        EXPECT_EQ(d3.speed, 4u);
        EXPECT_EQ(slot_state(d3), SLOT_ADDRESSED);
        desc = get_descriptor(d3, 1, 0, 18);
        EXPECT_EQ(desc[2] | desc[3] << 8, 0x300);
        EXPECT_EQ(desc[7], 9);
    }

    void test_address_bsr() {
        start_xhci();
        xdev d = address(PORT_DRIVE2, 64, true);
        EXPECT_EQ(slot_state(d), SLOT_DEFAULT);
        EXPECT_EQ(dev_addr(d), 0u);
        EXPECT_EQ(ep_state(d, 1), EPS_RUNNING);

        vector<u8> desc = get_descriptor(d, 1, 0, 8);
        EXPECT_EQ(desc[0], 18);
        EXPECT_EQ(desc[1], 1);
        EXPECT_TRUE(desc[7] == 8 || desc[7] == 64) << (int)desc[7];

        fill_address_input(d);
        EXPECT_EQ(command(CMD_ADDRESS_DEVICE, d.slot, 0, d.ictx), CC_SUCCESS);
        EXPECT_EQ(slot_state(d), SLOT_ADDRESSED);
        EXPECT_EQ(dev_addr(d), d.slot);

        desc = get_descriptor(d, 1, 0, 18);
        EXPECT_EQ(desc[0], 18);
        EXPECT_EQ(desc[8] | desc[9] << 8, USB_VENDOR_VCML);
    }

    void test_control() {
        start_xhci();
        xdev d = address(PORT_KEYBOARD3, 8, false, 8);

        for (int i = 0; i < 4; i++) {
            vector<u8> cfg = get_descriptor(d, 2, 0, 9);
            u16 total = cfg[2] | cfg[3] << 8;
            EXPECT_GT(total, 32);
            cfg = get_descriptor(d, 2, 0, total);
            EXPECT_EQ(cfg[0], 9);
            EXPECT_EQ(cfg[5], 42);
            EXPECT_EQ(cfg[total - 6 + 1], USB_DT_ENDPOINT_COMPANION);
        }

        set_configuration(d, 42);

        u64 buf = alloc(32);
        put<u8>(buf, 0x02);
        EXPECT_EQ(control(d, 0x21, 9, 0x200, 0, 1, buf), CC_SUCCESS);

        xring& r = d.ep[1];
        push(r, 0x0001000002000921ull, 8,
             trb_ctrl(TRB_SETUP, TRB_IDT | 2 << 16));
        push(r, 0x03, 1, trb_ctrl(TRB_DATA, TRB_IDT));
        u64 status = push(r, 0, 0, trb_ctrl(TRB_STATUS, TRB_DIR_IN | TRB_IOC));
        doorbell(d.slot, 1);
        trb ev = next_event();
        EXPECT_EQ(trb_cc(ev), CC_SUCCESS);
        EXPECT_EQ(ev.parameter, status);

        push(r, 0x0012000001000680ull, 8,
             trb_ctrl(TRB_SETUP, TRB_IDT | 3 << 16));
        u64 data = push(r, buf, 8, trb_ctrl(TRB_DATA, TRB_DIR_IN | TRB_IOC));
        u64 noop = push(r, 0, 0, trb_ctrl(TRB_NOOP, TRB_IOC));
        push(r, buf + 16, 10, trb_ctrl(TRB_DATA, TRB_DIR_IN));
        status = push(r, 0, 0, trb_ctrl(TRB_STATUS, TRB_IOC));
        doorbell(d.slot, 1);
        ev = next_event();
        EXPECT_EQ(trb_cc(ev), CC_SUCCESS);
        EXPECT_EQ(ev.parameter, data);
        EXPECT_EQ(get<u8>(buf), 18);
        ev = next_event();
        EXPECT_EQ(trb_cc(ev), CC_SUCCESS);
        EXPECT_EQ(ev.parameter, noop);
        ev = next_event();
        EXPECT_EQ(trb_cc(ev), CC_SUCCESS);
        EXPECT_EQ(ev.parameter, status);
        EXPECT_TRUE(no_event());
    }

    void test_stall() {
        start_xhci();
        xdev d = address(PORT_KEYBOARD2, 64);

        u64 buf = alloc(64);
        u64 setup = d.ep[1].enq();
        EXPECT_EQ(control(d, 0x80, 6, 0x0a00, 0, 64, buf), CC_STALL);
        EXPECT_EQ(m_last.parameter, setup);
        EXPECT_EQ(ep_state(d, 1), EPS_HALTED);

        doorbell(d.slot, 1);
        EXPECT_TRUE(no_event());

        EXPECT_EQ(command(CMD_RESET_EP, d.slot, 1), CC_SUCCESS);
        EXPECT_EQ(ep_state(d, 1), EPS_STOPPED);
        EXPECT_EQ(command(CMD_RESET_EP, d.slot, 1), CC_CONTEXT_STATE);
        EXPECT_EQ(command(CMD_SET_TR_DEQ, d.slot, 1, d.ep[1].deq()),
                  CC_SUCCESS);
        EXPECT_EQ(ep_state(d, 1), EPS_STOPPED);
        EXPECT_EQ(ep_deq(d, 1), d.ep[1].deq());

        EXPECT_EQ(get_descriptor(d, 1, 0, 18)[0], 18);
        EXPECT_EQ(ep_state(d, 1), EPS_RUNNING);
        EXPECT_EQ(command(CMD_SET_TR_DEQ, d.slot, 1, d.ep[1].deq()),
                  CC_CONTEXT_STATE);

        put<u16>(buf, 0x0303);
        u64 status = d.ep[1].enq() + 2 * sizeof(trb);
        EXPECT_EQ(control(d, 0x21, 9, 0x200, 0, 2, buf), CC_STALL);
        EXPECT_EQ(m_last.parameter, status);
        EXPECT_EQ(ep_state(d, 1), EPS_HALTED);
        EXPECT_EQ(command(CMD_RESET_EP, d.slot, 1), CC_SUCCESS);
        EXPECT_EQ(command(CMD_SET_TR_DEQ, d.slot, 1, d.ep[1].deq()),
                  CC_SUCCESS);
        EXPECT_EQ(get_descriptor(d, 1, 0, 18)[1], 1);
    }

    void test_transfer_errors() {
        start_xhci();
        xdev d = address(PORT_KEYBOARD2, 64);

        xring& r = d.ep[1];
        push(r, 0x0012000001000680ull, 8,
             trb_ctrl(TRB_SETUP, TRB_IDT | 3 << 16));
        u64 data = push(r, BAD_ADDR, 18, trb_ctrl(TRB_DATA, TRB_DIR_IN));
        u64 status = push(r, 0, 0, trb_ctrl(TRB_STATUS, TRB_IOC));
        doorbell(d.slot, 1);
        trb ev = next_event();
        EXPECT_EQ(trb_cc(ev), CC_DATA_BUFFER);
        EXPECT_EQ(ev.parameter, data);
        ev = next_event();
        EXPECT_EQ(trb_cc(ev), CC_SUCCESS);
        EXPECT_EQ(ev.parameter, status);
        EXPECT_EQ(ep_state(d, 1), EPS_RUNNING);

        EXPECT_EQ(command(CMD_STOP_EP, d.slot, 1), CC_SUCCESS);
        EXPECT_EQ(ep_state(d, 1), EPS_STOPPED);
        EXPECT_EQ(ep_deq(d, 1), d.ep[1].deq());
        EXPECT_EQ(command(CMD_SET_TR_DEQ, d.slot, 1, BAD_ADDR | 1),
                  CC_SUCCESS);
        EXPECT_EQ(ep_deq(d, 1), BAD_ADDR | 1);
        doorbell(d.slot, 1);
        ev = next_event();
        EXPECT_EQ(trb_type(ev), EV_TRANSFER);
        EXPECT_EQ(trb_cc(ev), CC_DATA_BUFFER);
        EXPECT_EQ(ev.parameter, BAD_ADDR);
        EXPECT_EQ(ep_state(d, 1), EPS_ERROR);

        EXPECT_EQ(command(CMD_SET_TR_DEQ, d.slot, 1, d.ep[1].deq()),
                  CC_SUCCESS);
        EXPECT_EQ(ep_state(d, 1), EPS_STOPPED);
        EXPECT_EQ(get_descriptor(d, 1, 0, 18)[0], 18);

        doorbell(d.slot, 0);
        doorbell(d.slot, 3);
        doorbell(d.slot, 40);
        doorbell(7, 1);
        EXPECT_TRUE(no_event());

        EXPECT_EQ(get_descriptor(d, 1, 0, 18)[0], 18);
    }

    void test_evaluate_context() {
        start_xhci();
        xdev d = address(PORT_KEYBOARD2, 8);
        EXPECT_EQ(ep_mps(d, 1), 8u);

        memset(ptr(d.ictx), 0, 33 * 32);
        put<u32>(d.ictx + 4, 3);
        memcpy(ptr(d.ictx + 32), ptr(d.octx), 64);
        u32 dw1 = get<u32>(d.ictx + 32 + 4);
        put<u32>(d.ictx + 32 + 4, (dw1 & ~0xffffu) | 0x123);
        put<u32>(d.ictx + 32 + 8, 1u << 22);
        u32 epdw1 = get<u32>(d.ictx + 64 + 4);
        put<u32>(d.ictx + 64 + 4, (epdw1 & 0xffff) | 64u << 16);
        put<u32>(d.ictx + 64 + 8, 0xdead0);
        u64 deq = ep_deq(d, 1);

        EXPECT_EQ(command(CMD_EVALUATE_CTX, d.slot, 0, d.ictx), CC_SUCCESS);
        EXPECT_EQ(get<u32>(d.octx + 4) & 0xffff, 0x123u);
        EXPECT_EQ(get<u32>(d.octx + 8) >> 22, 1u);
        EXPECT_EQ(slot_state(d), SLOT_ADDRESSED);
        EXPECT_EQ(dev_addr(d), d.slot);
        EXPECT_EQ((get<u32>(d.octx + 4) >> 16) & 0xff, PORT_KEYBOARD2);
        EXPECT_EQ(ep_mps(d, 1), 64u);
        EXPECT_EQ(ep_state(d, 1), EPS_RUNNING);
        EXPECT_EQ(ep_deq(d, 1), deq);

        EXPECT_EQ(get_descriptor(d, 1, 0, 18)[0], 18);

        put<u32>(d.ictx + 4, bit(5));
        EXPECT_EQ(command(CMD_EVALUATE_CTX, d.slot, 0, d.ictx), CC_SUCCESS);
        EXPECT_EQ(ep_state(d, 4), EPS_DISABLED);
    }

    void test_configure() {
        start_xhci();
        xdev d = address(PORT_KEYBOARD2, 64);
        set_configuration(d, config_value(d));

        EXPECT_EQ(configure(d, { { 3, EPT_INT_IN, 8, 3 } }), CC_SUCCESS);
        EXPECT_EQ(slot_state(d), SLOT_CONFIGURED);
        EXPECT_EQ(slot_entries(d), 3u);
        EXPECT_EQ(ep_state(d, 3), EPS_RUNNING);
        EXPECT_EQ(ep_deq(d, 3), d.ep[3].deq());
        EXPECT_EQ(ep_mps(d, 3), 8u);

        xring ring = d.ep[3];
        EXPECT_EQ(configure(d, { { 3, EPT_INT_IN, 8, 3 } }), CC_CONTEXT_STATE);
        d.ep[3] = ring;

        u64 buf = alloc(8);
        EXPECT_EQ(trb_cc(transfer(d, 3, buf, 8)), CC_SUCCESS);

        EXPECT_EQ(configure(d, {}, bit(3)), CC_SUCCESS);
        EXPECT_EQ(ep_state(d, 3), EPS_DISABLED);
        doorbell(d.slot, 3);
        EXPECT_TRUE(no_event());

        EXPECT_EQ(configure(d, {}, bit(3)), CC_SUCCESS);
        EXPECT_EQ(configure(d, { { 3, EPT_INT_IN, 8, 3 } }), CC_SUCCESS);
        EXPECT_EQ(ep_state(d, 3), EPS_RUNNING);
        EXPECT_EQ(trb_cc(transfer(d, 3, buf, 8)), CC_SUCCESS);

        EXPECT_EQ(configure(d, { { 3, EPT_INT_IN, 8, 3 } }, bit(3)),
                  CC_SUCCESS);
        EXPECT_EQ(ep_state(d, 3), EPS_RUNNING);
        EXPECT_EQ(ep_deq(d, 3), d.ep[3].deq());
        EXPECT_EQ(trb_cc(transfer(d, 3, buf, 8)), CC_SUCCESS);

        EXPECT_EQ(configure(d, {}, bit(1)), CC_TRB);
        EXPECT_EQ(configure(d, {}, bit(0)), CC_TRB);

        EXPECT_EQ(configure(d, { { 4, EPT_BULK_OUT, 0, 0 } }), CC_SUCCESS);
        EXPECT_EQ(ep_state(d, 4), EPS_RUNNING);
        EXPECT_EQ(ep_state(d, 3), EPS_RUNNING);
        EXPECT_EQ(trb_cc(transfer(d, 4, buf, 8)), CC_TRB);

        EXPECT_EQ(configure(d, {}, 0, TRB_DC), CC_SUCCESS);
        EXPECT_EQ(slot_state(d), SLOT_ADDRESSED);
        EXPECT_EQ(ep_state(d, 3), EPS_DISABLED);
        EXPECT_EQ(ep_state(d, 4), EPS_DISABLED);
        EXPECT_EQ(ep_state(d, 1), EPS_RUNNING);
        EXPECT_EQ(get_descriptor(d, 1, 0, 18)[0], 18);
    }

    void test_configure_errors() {
        start_xhci();
        xdev d = setup_device(PORT_KEYBOARD3, 512);
        d.octx = MEM_SIZE - 64;
        put<u64>(DCBAA + 8 * d.slot, d.octx);
        EXPECT_EQ(command(CMD_ADDRESS_DEVICE, d.slot, 0, d.ictx), CC_SUCCESS);
        EXPECT_EQ(slot_state(d), SLOT_ADDRESSED);
        EXPECT_EQ(configure(d, { { 3, EPT_INT_IN, 8, 3 } }), CC_TRB);
        EXPECT_EQ(slot_state(d), SLOT_ADDRESSED);
    }

    ui::keyboard& keys(usb::keyboard& dev) {
        string nm = mkstr("%s.keyboard", dev.name());
        ui::keyboard* kb = ui::input::find<ui::keyboard>(nm);
        VCML_ERROR_ON(!kb, "keyboard %s not found", nm.c_str());
        return *kb;
    }

    void test_interrupt_in() {
        start_xhci();
        xdev d = address(PORT_KEYBOARD2, 64);
        set_configuration(d, config_value(d));
        EXPECT_EQ(configure(d, { { 3, EPT_INT_IN, 8, 3 } }), CC_SUCCESS);

        u64 buf = alloc(8);
        memset(ptr(buf), 0xff, 8);
        trb ev = transfer(d, 3, buf, 8);
        EXPECT_EQ(trb_cc(ev), CC_SUCCESS);
        EXPECT_EQ(trb_len(ev), 0u);
        EXPECT_EQ(get<u64>(buf), 0u);
        sc_time t1 = sc_time_stamp();

        keys(keyboard2).notify_key(ui::KEYSYM_a, true);
        ev = transfer(d, 3, buf, 8);
        sc_time t2 = sc_time_stamp();
        EXPECT_EQ(trb_cc(ev), CC_SUCCESS);
        EXPECT_GE(t2 - t1, sc_time(875, SC_US));
        EXPECT_EQ(get<u8>(buf + 2), 0x04);

        keys(keyboard2).notify_key(ui::KEYSYM_a, false);
        while (keys(keyboard2).has_events())
            EXPECT_EQ(trb_cc(transfer(d, 3, buf, 8)), CC_SUCCESS);
        EXPECT_EQ(trb_cc(transfer(d, 3, buf, 8)), CC_SUCCESS);
        EXPECT_EQ(get<u8>(buf + 2), 0x00);

        vector<u64> addrs(3);
        for (u64& addr : addrs)
            addr = push(d.ep[3], buf, 8, trb_ctrl(TRB_NORMAL, 0));
        u64 last = push(d.ep[3], buf, 8, trb_ctrl(TRB_NORMAL, TRB_IOC));
        doorbell(d.slot, 3);
        ev = next_event();
        EXPECT_EQ(trb_cc(ev), CC_SUCCESS);
        EXPECT_EQ(ev.parameter, last);
        EXPECT_TRUE(no_event());

        ev = transfer(d, 3, 0, 0, TRB_IOC, TRB_NOOP);
        EXPECT_EQ(trb_cc(ev), CC_SUCCESS);
    }

    void test_stop_endpoint() {
        start_xhci();
        xdev d = address(PORT_KEYBOARD2, 64);
        set_configuration(d, config_value(d));
        EXPECT_EQ(configure(d, { { 3, EPT_INT_IN, 8, 3 } }), CC_SUCCESS);

        u64 buf = alloc(8);
        EXPECT_EQ(trb_cc(transfer(d, 3, buf, 8)), CC_SUCCESS);

        u64 pending = push(d.ep[3], buf, 8, trb_ctrl(TRB_NORMAL, TRB_IOC));
        doorbell(d.slot, 3);
        EXPECT_EQ(command(CMD_STOP_EP, d.slot, 3), CC_SUCCESS);
        EXPECT_EQ(ep_state(d, 3), EPS_STOPPED);
        EXPECT_EQ(ep_deq(d, 3), pending | 1);
        EXPECT_TRUE(no_event(0, sc_time(3, SC_MS)));

        doorbell(d.slot, 3);
        trb ev = next_event();
        EXPECT_EQ(trb_type(ev), EV_TRANSFER);
        EXPECT_EQ(trb_cc(ev), CC_SUCCESS);
        EXPECT_EQ(ev.parameter, pending);
        EXPECT_EQ(ep_state(d, 3), EPS_RUNNING);

        EXPECT_EQ(command(CMD_STOP_EP, d.slot, 3), CC_SUCCESS);
        EXPECT_EQ(command(CMD_SET_TR_DEQ, d.slot, 3, d.ep[3].deq()),
                  CC_SUCCESS);
        EXPECT_EQ(command(CMD_STOP_EP, d.slot, 5), CC_CONTEXT_STATE);
        EXPECT_EQ(trb_cc(transfer(d, 3, buf, 8)), CC_SUCCESS);
    }

    void msd_cbw(u64 addr, u32 tag, u32 len, bool in, const vector<u8>& cdb) {
        memset(ptr(addr), 0, 31);
        memcpy(ptr(addr), "USBC", 4);
        put<u32>(addr + 4, tag);
        put<u32>(addr + 8, len);
        put<u8>(addr + 12, in ? 0x80 : 0x00);
        put<u8>(addr + 14, (u8)cdb.size());
        memcpy(ptr(addr + 15), cdb.data(), cdb.size());
    }

    void msd_csw(xdev& d, u32 tag) {
        u64 csw = alloc(13);
        EXPECT_EQ(trb_cc(transfer(d, 3, csw, 13)), CC_SUCCESS);
        EXPECT_EQ(memcmp(ptr(csw), "USBS", 4), 0);
        EXPECT_EQ(get<u32>(csw + 4), tag);
        EXPECT_EQ(get<u8>(csw + 12), 0);
    }

    void msd_command(xdev& d, u32 tag, u64 data, u32 len, bool in,
                     const vector<u8>& cdb) {
        u64 cbw = alloc(31);
        msd_cbw(cbw, tag, len, in, cdb);
        EXPECT_EQ(trb_cc(transfer(d, 4, cbw, 31)), CC_SUCCESS);
        if (len > 0) {
            trb ev = transfer(d, in ? 3 : 4, data, len);
            EXPECT_EQ(trb_cc(ev), CC_SUCCESS);
        }
        msd_csw(d, tag);
    }

    void test_bulk() {
        start_xhci();
        xdev d = address(PORT_DRIVE2, 64);
        set_configuration(d, config_value(d));
        EXPECT_EQ(configure(d, { { 3, EPT_BULK_IN, 512, 0 },
                                 { 4, EPT_BULK_OUT, 512, 0 } }),
                  CC_SUCCESS);
        EXPECT_EQ(slot_entries(d), 4u);

        u64 inq = alloc(36);
        msd_command(d, 1, inq, 36, true, { 0x12, 0, 0, 0, 36, 0 });
        EXPECT_EQ(get<u8>(inq) & 0x1f, 0);

        u64 cap = alloc(8);
        msd_command(d, 2, cap, 8, true, { 0x25, 0, 0, 0, 0, 0, 0, 0, 0, 0 });
        EXPECT_EQ(bswap(get<u32>(cap)), 512 * MiB / 512 - 1);
        EXPECT_EQ(bswap(get<u32>(cap + 4)), 512u);

        u64 wdata = alloc(1024);
        for (u32 i = 0; i < 1024; i++)
            put<u8>(wdata + i, (u8)(i * 7 + 3));

        u64 cbw = alloc(31);
        msd_cbw(cbw, 3, 1024, false, { 0x2a, 0, 0, 0, 0, 4, 0, 0, 2, 0 });
        EXPECT_EQ(trb_cc(transfer(d, 4, cbw, 31)), CC_SUCCESS);
        push(d.ep[4], wdata, 512, trb_ctrl(TRB_NORMAL, TRB_CH));
        u64 last = push(d.ep[4], wdata + 512, 512,
                        trb_ctrl(TRB_NORMAL, TRB_IOC));
        doorbell(d.slot, 4);
        trb ev = next_event();
        EXPECT_EQ(trb_cc(ev), CC_SUCCESS);
        EXPECT_EQ(ev.parameter, last);
        msd_csw(d, 3);

        u64 rdata = alloc(1024);
        cbw = alloc(31);
        msd_cbw(cbw, 4, 1024, true, { 0x28, 0, 0, 0, 0, 4, 0, 0, 2, 0 });
        EXPECT_EQ(trb_cc(transfer(d, 4, cbw, 31)), CC_SUCCESS);
        push(d.ep[3], rdata, 512, trb_ctrl(TRB_NORMAL, TRB_CH));
        push(d.ep[3], rdata + 512, 512, trb_ctrl(TRB_NORMAL, TRB_CH));
        push(d.ep[3], 0x1234abcd, 0, trb_ctrl(TRB_EVDATA, TRB_IOC));
        doorbell(d.slot, 3);
        ev = next_event();
        EXPECT_EQ(trb_type(ev), EV_TRANSFER);
        EXPECT_EQ(trb_cc(ev), CC_SUCCESS);
        EXPECT_TRUE(ev.control & TRB_ED);
        EXPECT_EQ(ev.parameter, 0x1234abcdu);
        EXPECT_EQ(trb_len(ev), 1024u);
        EXPECT_EQ(trb_epid(ev), 3u);
        msd_csw(d, 4);
        EXPECT_EQ(memcmp(ptr(rdata), ptr(wdata), 1024), 0);

        ev = transfer(d, 4, BAD_ADDR, 31);
        EXPECT_EQ(trb_cc(ev), CC_DATA_BUFFER);
        EXPECT_EQ(ep_state(d, 4), EPS_RUNNING);

        ev = transfer(d, 4, 0, 0, TRB_IOC, CMD_ENABLE_SLOT);
        EXPECT_EQ(trb_cc(ev), CC_TRB);
        EXPECT_EQ(ep_state(d, 4), EPS_RUNNING);

        ev = transfer(d, 4, 0x0102030405060708ull, 9, TRB_IOC | TRB_IDT);
        EXPECT_EQ(trb_cc(ev), CC_TRB);
        EXPECT_EQ(ep_state(d, 4), EPS_RUNNING);

        msd_cbw(cbw, 5, 0, false, { 0x00, 0, 0, 0, 0, 0 });
        memcpy(ptr(cbw), "XXXX", 4);
        ev = transfer(d, 4, cbw, 31);
        EXPECT_EQ(trb_cc(ev), CC_STALL);
        EXPECT_EQ(ep_state(d, 4), EPS_HALTED);
        EXPECT_EQ(command(CMD_RESET_EP, d.slot, 4), CC_SUCCESS);
        EXPECT_EQ(command(CMD_SET_TR_DEQ, d.slot, 4, d.ep[4].deq()),
                  CC_SUCCESS);

        msd_command(d, 6, 0, 0, false, { 0x00, 0, 0, 0, 0, 0 });
    }

    void test_isochronous() {
        start_xhci();
        xdev d = address(PORT_HEADSET, 64);
        set_configuration(d, config_value(d));
        EXPECT_EQ(configure(d, { { 2, EPT_ISOCH_OUT, 192, 0 },
                                 { 5, EPT_ISOCH_IN, 96, 0 } }),
                  CC_SUCCESS);
        EXPECT_EQ(ep_state(d, 2), EPS_RUNNING);
        EXPECT_EQ(ep_state(d, 5), EPS_RUNNING);

        u64 buf = alloc(3 * 96);
        memset(ptr(buf), 0x55, 3 * 96);
        vector<u64> addrs;
        for (int i = 0; i < 3; i++) {
            u64 a = buf + 96 * i;
            addrs.push_back(
                push(d.ep[5], a, 96, trb_ctrl(TRB_ISOCH, TRB_IOC)));
        }

        doorbell(d.slot, 5);
        sc_time prev = SC_ZERO_TIME;
        for (int i = 0; i < 3; i++) {
            trb ev = next_event();
            EXPECT_EQ(trb_type(ev), EV_TRANSFER);
            EXPECT_EQ(trb_cc(ev), CC_SUCCESS);
            EXPECT_EQ(trb_epid(ev), 5u);
            EXPECT_EQ(ev.parameter, addrs[i]);
            if (i > 0) {
                EXPECT_GT(sc_time_stamp(), prev);
            }
            prev = sc_time_stamp();
        }

        for (int i = 0; i < 3 * 96; i++)
            EXPECT_EQ(get<u8>(buf + i), 0) << "byte " << i;

        u64 out = alloc(192);
        trb ev = transfer(d, 2, out, 192, TRB_IOC, TRB_ISOCH);
        EXPECT_EQ(trb_cc(ev), CC_SUCCESS);
        ev = transfer(d, 2, out, 192, TRB_IOC, TRB_ISOCH);
        EXPECT_EQ(trb_cc(ev), CC_SUCCESS);
    }

    void test_reset_device() {
        start_xhci();
        xdev d = address(PORT_KEYBOARD2, 64);
        set_configuration(d, config_value(d));
        EXPECT_EQ(configure(d, { { 3, EPT_INT_IN, 8, 3 } }), CC_SUCCESS);
        EXPECT_EQ(slot_state(d), SLOT_CONFIGURED);

        EXPECT_EQ(command(CMD_RESET_DEVICE, d.slot), CC_SUCCESS);
        EXPECT_EQ(slot_state(d), SLOT_DEFAULT);
        EXPECT_EQ(dev_addr(d), 0u);
        EXPECT_EQ(slot_entries(d), 1u);
        EXPECT_EQ(ep_state(d, 3), EPS_DISABLED);
        EXPECT_EQ(ep_state(d, 1), EPS_RUNNING);
        doorbell(d.slot, 3);
        EXPECT_TRUE(no_event());

        fill_address_input(d);
        EXPECT_EQ(command(CMD_ADDRESS_DEVICE, d.slot, 0, d.ictx), CC_SUCCESS);
        EXPECT_EQ(slot_state(d), SLOT_ADDRESSED);
        EXPECT_EQ(dev_addr(d), d.slot);
        EXPECT_EQ(get_descriptor(d, 1, 0, 18)[0], 18);

        EXPECT_EQ(command(CMD_DISABLE_SLOT, d.slot), CC_SUCCESS);
        EXPECT_EQ(command(CMD_RESET_DEVICE, d.slot), CC_SLOT_NOT_ENABLED);
        EXPECT_EQ(command(CMD_ADDRESS_DEVICE, d.slot, 0, d.ictx),
                  CC_SLOT_NOT_ENABLED);
        doorbell(d.slot, 1);
        EXPECT_TRUE(no_event());
    }

    void test_event_ring_full() {
        start_xhci();
        vector<u64> addrs(RING_SIZE - 1);
        for (u64& addr : addrs)
            addr = push(m_cmd, 0, 0, trb_ctrl(CMD_NOOP));
        doorbell(0, 0);

        for (size_t i = 0; i < RING_SIZE - 2; i++) {
            trb ev = next_event(0, false);
            EXPECT_EQ(trb_type(ev), EV_CMD_COMPLETE) << i;
            EXPECT_EQ(ev.parameter, addrs[i]) << i;
        }

        trb ev = next_event(0, false);
        EXPECT_EQ(trb_type(ev), EV_HOST_CONTROLLER);
        EXPECT_EQ(trb_cc(ev), CC_EVENT_RING_FULL);
        EXPECT_TRUE(no_event());

        addrs.push_back(push(m_cmd, 0, 0, trb_ctrl(CMD_NOOP)));
        doorbell(0, 0);
        ir_write64(0, RT_ERDP, EVT_RING);
        EXPECT_TRUE(no_event());

        ack_event(0);
        for (size_t i = RING_SIZE - 2; i < RING_SIZE; i++) {
            ev = next_event();
            EXPECT_EQ(trb_type(ev), EV_CMD_COMPLETE) << i;
            EXPECT_EQ(trb_cc(ev), CC_SUCCESS) << i;
            EXPECT_EQ(ev.parameter, addrs[i]) << i;
        }

        EXPECT_TRUE(no_event());
        EXPECT_EQ(command(CMD_NOOP), CC_SUCCESS);

        ir_write64(0, RT_ERDP, BAD_ADDR);
        push(m_cmd, 0, 0, trb_ctrl(CMD_NOOP));
        doorbell(0, 0);
        EXPECT_TRUE(no_event());
        ir_write64(0, RT_ERDP, m_er[0].enq());
        EXPECT_TRUE(no_event());
        EXPECT_EQ(command(CMD_NOOP), CC_SUCCESS);
    }

    void test_interrupter() {
        start_xhci();
        xdev d = address(PORT_KEYBOARD2, 64);

        u64 buf = alloc(18);
        xring& r = d.ep[1];
        push(r, 0x0012000001000680ull, 8,
             trb_ctrl(TRB_SETUP, TRB_IDT | 3 << 16));
        push(r, buf, 18, trb_ctrl(TRB_DATA, TRB_DIR_IN));
        u64 status = push(r, 0, 1u << 22, trb_ctrl(TRB_STATUS, TRB_IOC));
        doorbell(d.slot, 1);

        trb ev = next_event(1, false);
        EXPECT_EQ(trb_type(ev), EV_TRANSFER);
        EXPECT_EQ(trb_cc(ev), CC_SUCCESS);
        EXPECT_EQ(ev.parameter, status);
        EXPECT_TRUE(ir_read(1, RT_IMAN) & IMAN_IP);
        EXPECT_FALSE(ir_read(0, RT_IMAN) & IMAN_IP);
        EXPECT_TRUE(irq);

        push(m_cmd, 0, 0, trb_ctrl(CMD_NOOP));
        doorbell(0, 0);
        next_event(0, false);
        EXPECT_TRUE(irq);
        ack_event(0);
        EXPECT_TRUE(irq);
        ack_event(1);
        EXPECT_FALSE(irq);

        push(r, 0x0012000001000680ull, 8,
             trb_ctrl(TRB_SETUP, TRB_IDT | 3 << 16));
        push(r, buf, 18, trb_ctrl(TRB_DATA, TRB_DIR_IN));
        push(r, 0, 9u << 22, trb_ctrl(TRB_STATUS, TRB_IOC));
        doorbell(d.slot, 1);
        EXPECT_TRUE(no_event(0));
        EXPECT_TRUE(no_event(1));

        ir_write64(1, RT_ERSTBA, BAD_ADDR);
        EXPECT_EQ(get_descriptor_intr1(d), CC_SUCCESS);
        EXPECT_FALSE(ir_read(1, RT_IMAN) & IMAN_IP);

        put<u64>(ERST + 0x40, BAD_ADDR);
        ir_write64(1, RT_ERSTBA, ERST + 0x40);
        ir_write64(1, RT_ERDP, BAD_ADDR);
        EXPECT_EQ(get_descriptor_intr1(d), CC_SUCCESS);
        EXPECT_FALSE(ir_read(1, RT_IMAN) & IMAN_IP);
        EXPECT_EQ(command(CMD_NOOP), CC_SUCCESS);
    }

    u32 get_descriptor_intr1(xdev& d) {
        xring& r = d.ep[1];
        u64 buf = alloc(18);
        push(r, 0x0012000001000680ull, 8,
             trb_ctrl(TRB_SETUP, TRB_IDT | 3 << 16));
        push(r, buf, 18, trb_ctrl(TRB_DATA, TRB_DIR_IN));
        push(r, 0, 1u << 22, trb_ctrl(TRB_STATUS, TRB_IOC));
        doorbell(d.slot, 1);
        wait(1, SC_MS);
        EXPECT_EQ(ep_state(d, 1), EPS_RUNNING);
        return get<u8>(buf) == 18 ? CC_SUCCESS : CC_TRB;
    }

    void test_halt() {
        start_xhci();
        xdev d = address(PORT_KEYBOARD2, 64);
        set_configuration(d, config_value(d));
        EXPECT_EQ(configure(d, { { 3, EPT_INT_IN, 8, 3 } }), CC_SUCCESS);

        u64 buf = alloc(8);
        for (int i = 0; i < 20; i++)
            EXPECT_EQ(trb_cc(transfer(d, 3, buf, 8)), CC_SUCCESS);

        u64 pending = push(d.ep[3], buf, 8, trb_ctrl(TRB_NORMAL, TRB_IOC));
        doorbell(d.slot, 3);
        op_write(OP_USBCMD, USBCMD_INTE);
        EXPECT_TRUE(wait_op(OP_USBSTS, USBSTS_HCH, USBSTS_HCH));
        EXPECT_TRUE(no_event(0, sc_time(5, SC_MS)));

        u64 cmd = push(m_cmd, 0, 0, trb_ctrl(CMD_NOOP));
        doorbell(0, 0);
        EXPECT_TRUE(no_event());

        op_write(OP_USBCMD, USBCMD_RS | USBCMD_INTE);
        EXPECT_TRUE(wait_op(OP_USBSTS, USBSTS_HCH, 0));
        doorbell(d.slot, 3);
        trb ev = next_event(0, true, sc_time(2, SC_MS));
        EXPECT_EQ(trb_type(ev), EV_TRANSFER);
        EXPECT_EQ(ev.parameter, pending);

        doorbell(0, 0);
        ev = next_event();
        EXPECT_EQ(trb_type(ev), EV_CMD_COMPLETE);
        EXPECT_EQ(ev.parameter, cmd);

        op_write(OP_USBCMD, USBCMD_INTE);
        attach(keyboard3, false);
        attach(keyboard3, true);
        op_write(OP_USBCMD, USBCMD_RS | USBCMD_INTE);
        EXPECT_TRUE(no_event());
        EXPECT_TRUE(portsc(PORT_KEYBOARD3) & PORTSC_CSC);
    }

    void test_hcrst() {
        start_xhci();
        xdev d = address(PORT_KEYBOARD2, 64);
        set_configuration(d, config_value(d));
        EXPECT_EQ(configure(d, { { 3, EPT_INT_IN, 8, 3 } }), CC_SUCCESS);
        u64 buf = alloc(8);
        EXPECT_EQ(trb_cc(transfer(d, 3, buf, 8)), CC_SUCCESS);
        for (int i = 0; i < 5; i++)
            EXPECT_EQ(command(CMD_NOOP), CC_SUCCESS);

        push(d.ep[3], buf, 8, trb_ctrl(TRB_NORMAL, TRB_IOC));
        doorbell(d.slot, 3);
        push(m_cmd, 0, 0, trb_ctrl(CMD_NOOP));
        doorbell(0, 0);
        next_event(0, false);
        EXPECT_TRUE(irq);
        xhci.reset();
        EXPECT_FALSE(irq);
        EXPECT_TRUE(op_read(OP_USBSTS) & USBSTS_HCH);
        EXPECT_EQ(op_read(OP_USBCMD), 0u);
        EXPECT_EQ(op_read(OP_CONFIG), 0u);
        EXPECT_EQ(op_read64(OP_DCBAAP), 0u);
        EXPECT_EQ(ir_read64(0, RT_ERSTBA), 0u);
        EXPECT_TRUE(portsc(PORT_KEYBOARD2) & PORTSC_CCS);
        EXPECT_TRUE(portsc(PORT_KEYBOARD2) & PORTSC_CSC);
        EXPECT_FALSE(portsc(PORT_KEYBOARD2) & PORTSC_PED);
        EXPECT_TRUE(no_event());

        start_xhci();
        EXPECT_EQ(command(CMD_STOP_EP, d.slot, 3), CC_SLOT_NOT_ENABLED);
        EXPECT_EQ(m_last.parameter, CMD_RING);

        d = address(PORT_KEYBOARD2, 64);
        EXPECT_EQ(d.slot, 1u);
        EXPECT_EQ(get_descriptor(d, 1, 0, 18)[0], 18);

        EXPECT_EQ(command(CMD_NOOP), CC_SUCCESS);
        op_write(OP_USBCMD, USBCMD_INTE);
        EXPECT_TRUE(wait_op(OP_USBSTS, USBSTS_HCH, USBSTS_HCH));
        op_write(OP_USBCMD, USBCMD_HCRST);
        EXPECT_TRUE(wait_op(OP_USBCMD, USBCMD_HCRST, 0));
        EXPECT_EQ(op_read(OP_CONFIG), 0u);
        start_xhci();
        EXPECT_EQ(command(CMD_NOOP), CC_SUCCESS);
        EXPECT_EQ(m_last.parameter, CMD_RING);
        EXPECT_EQ(enable_slot(), 1u);
    }
};

TEST(usb, xhci) {
    broker brkr("brkr");
    brkr.define("system.xhci.num_ports", NUM_PORTS);
    brkr.define("system.xhci.num_intrs", NUM_INTRS);
    brkr.define("system.keyboard2.usb3", false);
    brkr.define("system.keyboard3.usb3", true);
    brkr.define("system.drive2.usb3", false);
    xhci_test test("system");
    sc_core::sc_start();
}
