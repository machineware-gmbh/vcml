/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

enum : size_t {
    REG_T0 = 5,
    REG_PC = 32,
    NREGS = 33,
};

enum : u64 {
    MEM_SIZE = 0x1000,
    WR_BASE = 0x80000,
    RD_BASE = 0x90000,
    NO_ADDR = ~0ull,
};

class test_processor : public processor
{
public:
    const u32 base;
    u32 regs[NREGS];
    atomic<size_t> cycles;
    u64 stopped;
    std::set<u64> breakpoints;
    vector<pair<range, vcml_access>> watchpoints;

    test_processor(const sc_module_name& nm, u32 pc):
        processor(nm, "riscv"),
        base(pc),
        regs(),
        cycles(),
        stopped(NO_ADDR),
        breakpoints(),
        watchpoints() {
        const char* names[NREGS] = {
            "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2", "s0",
            "s1",   "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7",
            "s2",   "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10",
            "s11",  "t3", "t4", "t5", "t6", "pc",
        };

        for (size_t i = 0; i < NREGS; i++)
            define_cpureg_rw(i, names[i], 4);
        regs[REG_PC] = base;
        set_little_endian();
    }

    virtual void reset() override {
        cycles = 0;
        processor::reset();
        memset(regs, 0, sizeof(regs));
        regs[REG_PC] = base;
        stopped = NO_ADDR;
    }

    bool watched(u64 addr, vcml_access acs) const {
        for (const auto& wp : watchpoints)
            if (wp.second == acs && wp.first.includes(addr))
                return true;
        return false;
    }

    virtual void simulate(size_t n) override {
        for (size_t i = 0; i < n; i++) {
            u32 pc = regs[REG_PC];
            cycles++;

            if (breakpoints.count(pc) && stopped != pc) {
                stopped = pc;
                notify_breakpoint_hit(pc, local_time_stamp());
                return;
            }

            stopped = NO_ADDR;
            regs[REG_T0]++;
            regs[REG_PC] = base + ((pc + 4) & 0xfff);

            if (watched(WR_BASE + pc, VCML_ACCESS_WRITE)) {
                range addr(WR_BASE + pc, WR_BASE + pc + 3);
                notify_watchpoint_write(addr, &pc, local_time_stamp());
                return;
            }

            if (watched(RD_BASE + pc, VCML_ACCESS_READ)) {
                range addr(RD_BASE + pc, RD_BASE + pc + 3);
                notify_watchpoint_read(addr, local_time_stamp());
                return;
            }
        }
    }

    virtual u64 cycle_count() const override { return cycles; }

    virtual bool read_reg_dbg(size_t regno, void* buf, size_t len) override {
        if (regno >= NREGS || len != sizeof(u32))
            return false;
        memcpy(buf, &regs[regno], len);
        return true;
    }

    virtual bool write_reg_dbg(size_t regno, const void* buf,
                               size_t len) override {
        if (regno >= NREGS || len != sizeof(u32))
            return false;
        memcpy(&regs[regno], buf, len);
        return true;
    }

    virtual bool insert_breakpoint(u64 addr) override {
        return breakpoints.insert(addr).second;
    }

    virtual bool remove_breakpoint(u64 addr) override {
        return breakpoints.erase(addr) > 0;
    }

    virtual bool insert_watchpoint(const range& addr,
                                   vcml_access acs) override {
        watchpoints.emplace_back(addr, acs);
        return true;
    }

    virtual bool remove_watchpoint(const range& addr,
                                   vcml_access acs) override {
        for (auto it = watchpoints.begin(); it != watchpoints.end(); it++) {
            if (it->first == addr && it->second == acs) {
                watchpoints.erase(it);
                return true;
            }
        }

        return false;
    }
};

class rsp_client
{
private:
    mwr::socket m_sock;
    const atomic<bool>& m_suspended;

    static bool needs_escape(char c) {
        return c == '$' || c == '#' || c == '}' || c == '*';
    }

    char recv_char() {
        if (!m_sock.peek(5000))
            VCML_REPORT("timeout waiting for gdbserver");
        return (char)m_sock.recv_char();
    }

public:
    rsp_client(u16 port, const atomic<bool>& suspended):
        m_sock("localhost", port), m_suspended(suspended) {}

    void wait_suspended() {
        for (size_t i = 0; !m_suspended; i++) {
            if (i > 50000)
                VCML_REPORT("timeout waiting for simulation to suspend");
            mwr::usleep(100);
        }
    }

    void send_raw(const string& s) { m_sock.send(s); }

    void send_packet(const string& payload, u8 checksum_error = 0) {
        wait_suspended();
        string esc;
        for (char c : payload) {
            if (needs_escape(c)) {
                esc += '}';
                esc += (char)(c ^ 0x20);
            } else {
                esc += c;
            }
        }

        u8 sum = checksum_error;
        for (char c : esc)
            sum += (u8)c;
        send_raw("$" + esc + mkstr("#%02hhx", sum));
    }

    char recv_ack() { return recv_char(); }

    string recv_packet() {
        while (recv_char() != '$')
            continue;

        u8 sum = 0;
        string payload;
        for (char c = recv_char(); c != '#'; c = recv_char()) {
            sum += (u8)c;
            if (c == '}') {
                c = recv_char();
                sum += (u8)c;
                c ^= 0x20;
            }

            payload += c;
        }

        string ref = { recv_char(), recv_char() };
        EXPECT_EQ(mkstr("%02hhx", sum), ref) << "checksum of " << payload;
        send_raw("+");
        return payload;
    }

    string command(const string& payload) {
        send_packet(payload);
        EXPECT_EQ(recv_ack(), '+') << "no ack for " << payload;
        return recv_packet();
    }
};

class gdbserver_test : public test_base
{
public:
    test_processor cpu0;
    test_processor cpu1;
    test_processor cpu2;
    test_processor cpu3;

    generic::memory mem0;
    generic::memory mem1;
    generic::memory mem2;
    generic::memory mem3;

    model gdb;

    atomic<bool> suspended;

    virtual void session_suspend() override {
        test_base::session_suspend();
        suspended = true;
    }

    virtual void session_resume() override {
        suspended = false;
        test_base::session_resume();
    }

    gdbserver_test(const sc_module_name& nm):
        test_base(nm),
        cpu0("cpu0", 0x1000),
        cpu1("cpu1", 0x2000),
        cpu2("cpu2", 0x3000),
        cpu3("cpu3", 0x4000),
        mem0("mem0", MEM_SIZE),
        mem1("mem1", MEM_SIZE),
        mem2("mem2", MEM_SIZE),
        mem3("mem3", MEM_SIZE),
        gdb("gdb",
            "vcml::meta::gdbserver test.cpu0 test.cpu1 test.cpu2 test.cpu3"),
        suspended(false) {
        test_processor* cpus[] = { &cpu0, &cpu1, &cpu2, &cpu3 };
        generic::memory* mems[] = { &mem0, &mem1, &mem2, &mem3 };
        for (size_t i = 0; i < 4; i++) {
            rst.bind(cpus[i]->rst);
            clk.bind(cpus[i]->clk);
            rst.bind(mems[i]->rst);
            clk.bind(mems[i]->clk);
            cpus[i]->insn.stub();
            cpus[i]->data.bind(mems[i]->in);
        }

        add_test("strings", &gdbserver_test::test_strings);
        add_test("targets", &gdbserver_test::test_targets);
        add_test("properties", &gdbserver_test::test_properties);
        add_test("queries", &gdbserver_test::test_queries);
        add_test("threads", &gdbserver_test::test_threads);
        add_test("registers", &gdbserver_test::test_registers);
        add_test("memory", &gdbserver_test::test_memory);
        add_test("breakpoints", &gdbserver_test::test_breakpoints);
        add_test("watchpoints", &gdbserver_test::test_watchpoints);
        add_test("stepping", &gdbserver_test::test_stepping);
        add_test("interrupt", &gdbserver_test::test_interrupt);
        add_test("protocol", &gdbserver_test::test_protocol);
    }

    void test_strings() {
        EXPECT_STREQ(gdb->kind(), "vcml::meta::gdbserver");
        EXPECT_STREQ(gdb->version(), VCML_VERSION_STRING);
    }

    void test_targets() {
        module& m = gdb;
        auto* model = dynamic_cast<vcml::meta::gdbserver*>(&m);
        ASSERT_NE(model, nullptr);
        EXPECT_FALSE(model->targets().empty());
        ASSERT_EQ(model->targets().size(), 4);
        EXPECT_STREQ(model->targets()[0]->target_name(), "test.cpu0");
        EXPECT_STREQ(model->targets()[1]->target_name(), "test.cpu1");
        EXPECT_STREQ(model->targets()[2]->target_name(), "test.cpu2");
        EXPECT_STREQ(model->targets()[3]->target_name(), "test.cpu3");
    }

    template <typename T>
    property<T>& find_property(const string& full_name) {
        auto* attr = vcml::find_attribute(full_name);
        VCML_ERROR_ON(!attr, "property '%s' not found", full_name.c_str());
        auto* prop = dynamic_cast<property<T>*>(attr);
        VCML_ERROR_ON(!prop, "property '%s' invalid", full_name.c_str());
        return *prop;
    }

    void test_properties() {
        auto& gdb_host = find_property<string>("test.gdb.host");
        auto& gdb_port = find_property<int>("test.gdb.port");
        auto& gdb_wait = find_property<bool>("test.gdb.wait");
        auto& gdb_echo = find_property<bool>("test.gdb.echo");

        EXPECT_EQ(gdb_host.get(), "localhost");
        EXPECT_NE(gdb_port, 0u);
        EXPECT_FALSE(gdb_wait);
        EXPECT_FALSE(gdb_echo);
    }

    void run_session(function<void(rsp_client&)> session) {
        u16 port = find_property<int>("test.gdb.port").get();
        atomic<bool> done(false);
        thread client([&]() {
            try {
                rsp_client rsp(port, suspended);
                session(rsp);
                rsp.send_packet("D");
                EXPECT_EQ(rsp.recv_ack(), '+');
            } catch (std::exception& ex) {
                ADD_FAILURE() << ex.what();
            }

            done = true;
        });

        while (!done)
            wait(clock_cycles(100));
        client.join();
    }

    static string hex32(u32 val) {
        return mkstr("%02x%02x%02x%02x", val & 0xff, (val >> 8) & 0xff,
                     (val >> 16) & 0xff, val >> 24);
    }

    static string hexstr(const string& s) {
        string hex;
        for (char c : s)
            hex += mkstr("%02hhx", c);
        return hex;
    }

    void test_queries() {
        run_session([&](rsp_client& rsp) {
            EXPECT_EQ(rsp.command("qSupported:multiprocess+"),
                      "PacketSize=800000;qXfer:features:read+;"
                      "vContSupported+;");
            EXPECT_EQ(rsp.command("qAttached"), "1");
            EXPECT_EQ(rsp.command("qOffsets"), "Text=0;Data=0;Bss=0");
            EXPECT_EQ(rsp.command("qC"), "QC1");
            EXPECT_EQ(rsp.command("qTStatus"), "");
            EXPECT_EQ(rsp.command("?"), "T05thread:1;");
            EXPECT_EQ(rsp.command("vCont?"), "s;S;c;C");
            EXPECT_EQ(rsp.command("vMustReplyEmpty"), "");
            EXPECT_EQ(rsp.command("!"), "");

            string xml = rsp.command("qXfer:features:read:target.xml:0,10000");
            ASSERT_FALSE(xml.empty());
            EXPECT_EQ(xml[0], 'l');
            EXPECT_THAT(xml, HasSubstr("<architecture>riscv</architecture>"));
            EXPECT_THAT(xml, HasSubstr("org.gnu.gdb.riscv.cpu"));
            EXPECT_THAT(xml, HasSubstr("<reg name=\"pc\" regnum=\"32\" "
                                       "bitsize=\"32\" />"));

            string part0 = rsp.command("qXfer:features:read:target.xml:0,10");
            string part1 = rsp.command("qXfer:features:read:target.xml:10,8");
            EXPECT_EQ(part0, "m" + xml.substr(1, 0x10));
            EXPECT_EQ(part1, "m" + xml.substr(0x11, 8));

            EXPECT_EQ(rsp.command(mkstr("qXfer:features:read:target.xml:%zx,8",
                                        xml.length() - 5)),
                      "l" + xml.substr(xml.length() - 4));
            EXPECT_EQ(rsp.command("qXfer:features:read:target.xml:fffff,8"),
                      "E16");
            EXPECT_EQ(rsp.command("qXfer:features:write:target.xml:0,8"),
                      "E16");
            EXPECT_EQ(rsp.command("qXfer:features:read:target.xml:zz"), "E16");
            EXPECT_EQ(rsp.command("qXfer:features:read"), "E16");
            EXPECT_EQ(rsp.command("qXfer:auxv:read:x:0,8"), "");
        });
    }

    void test_threads() {
        run_session([&](rsp_client& rsp) {
            EXPECT_EQ(rsp.command("qfThreadInfo"), "m1,2,3,4,");
            EXPECT_EQ(rsp.command("qsThreadInfo"), "l");
            EXPECT_EQ(rsp.command("qThreadExtraInfo,1"), hexstr("test.cpu0"));
            EXPECT_EQ(rsp.command("qThreadExtraInfo,p1.4"),
                      hexstr("test.cpu3"));
            EXPECT_EQ(rsp.command("qThreadExtraInfo,9"), "E02");
            EXPECT_EQ(rsp.command("qThreadExtraInfo,zz"), "E16");

            for (int tid = 1; tid <= 4; tid++)
                EXPECT_EQ(rsp.command(mkstr("T%d", tid)), "OK");
            EXPECT_EQ(rsp.command("Tp1.2"), "OK");
            EXPECT_EQ(rsp.command("T5"), "E02");
            EXPECT_EQ(rsp.command("Tp2.1"), "E02");
            EXPECT_EQ(rsp.command("Tzz"), "E16");

            EXPECT_EQ(rsp.command("Hg3"), "OK");
            EXPECT_EQ(rsp.command("p20"), hex32(cpu2.regs[REG_PC]));
            EXPECT_EQ(rsp.command("Hgp1.2"), "OK");
            EXPECT_EQ(rsp.command("p20"), hex32(cpu1.regs[REG_PC]));
            EXPECT_EQ(rsp.command("Hg0"), "OK");
            EXPECT_EQ(rsp.command("p20"), hex32(cpu0.regs[REG_PC]));

            EXPECT_EQ(rsp.command("Hg4"), "OK");
            EXPECT_EQ(rsp.command("Hg-1"), "OK");
            EXPECT_EQ(rsp.command("p20"), hex32(cpu3.regs[REG_PC]));

            EXPECT_EQ(rsp.command("Hc2"), "OK");
            EXPECT_EQ(rsp.command("?"), "T05thread:2;");
            EXPECT_EQ(rsp.command("Hc-1"), "OK");
            EXPECT_EQ(rsp.command("?"), "T05thread:2;");

            EXPECT_EQ(rsp.command("Hg9"), "E16");
            EXPECT_EQ(rsp.command("Hc9"), "E16");
            EXPECT_EQ(rsp.command("Hgzz"), "E16");
            EXPECT_EQ(rsp.command("Hx1"), "E16");

            EXPECT_EQ(rsp.command("Hg1"), "OK");
            EXPECT_EQ(rsp.command("Hc1"), "OK");
        });
    }

    void test_registers() {
        run_session([&](rsp_client& rsp) {
            EXPECT_EQ(rsp.command("Hg2"), "OK");

            EXPECT_EQ(rsp.command("P5=78563412"), "OK");
            EXPECT_EQ(cpu1.regs[REG_T0], 0x12345678);
            EXPECT_EQ(rsp.command("p5"), "78563412");
            EXPECT_EQ(rsp.command("P20=00240000"), "OK");
            EXPECT_EQ(cpu1.regs[REG_PC], 0x2400);
            EXPECT_EQ(rsp.command("p20"), "00240000");

            EXPECT_EQ(rsp.command("p21"), "xxxxxxxx");
            EXPECT_EQ(rsp.command("P21=00000000"), "OK");
            EXPECT_EQ(rsp.command("pzz"), "E16");
            EXPECT_EQ(rsp.command("Pzz"), "E16");
            EXPECT_EQ(rsp.command("P5=7856"), "E16");
            EXPECT_EQ(rsp.command("P5"), "E16");
            EXPECT_EQ(rsp.command("P5=7856zz12"), "E16");
            EXPECT_EQ(cpu1.regs[REG_T0], 0x12345678);

            string all;
            for (u32 reg : cpu1.regs)
                all += hex32(reg);
            EXPECT_EQ(rsp.command("g"), all);

            all.clear();
            for (u32 i = 0; i < NREGS; i++)
                all += hex32(i == REG_PC ? 0x2800 : 0x11110000 + i);
            EXPECT_EQ(rsp.command("G" + all), "OK");
            EXPECT_EQ(rsp.command("g"), all);
            for (u32 i = 0; i < REG_PC; i++)
                EXPECT_EQ(cpu1.regs[i], 0x11110000 + i) << "register " << i;
            EXPECT_EQ(cpu1.regs[REG_PC], 0x2800);
            EXPECT_EQ(rsp.command("G" + all.substr(8)), "E16");

            EXPECT_EQ(rsp.command("Hg1"), "OK");
            EXPECT_NE(rsp.command("g"), all);
        });
    }

    void test_memory() {
        run_session([&](rsp_client& rsp) {
            EXPECT_EQ(rsp.command("Hg2"), "OK");

            EXPECT_EQ(rsp.command("M100,4:deadbeef"), "OK");
            EXPECT_EQ(rsp.command("m100,4"), "deadbeef");
            EXPECT_EQ(rsp.command("mfe,8"), "0000deadbeef0000");

            u32 data = 0;
            EXPECT_OK(cpu1.data.readw(0x100, data, SBI_DEBUG));
            EXPECT_EQ(data, 0xefbeadde);
            EXPECT_OK(cpu0.data.readw(0x100, data, SBI_DEBUG));
            EXPECT_EQ(data, 0);

            EXPECT_EQ(rsp.command("X200,6:a$b#}*"), "OK");
            EXPECT_EQ(rsp.command("m200,6"), hexstr("a$b#}*"));
            EXPECT_EQ(rsp.command("Xffc,4:wxyz"), "OK");
            EXPECT_EQ(rsp.command("mffc,4"), hexstr("wxyz"));

            EXPECT_EQ(rsp.command("m100,0"), "OK");
            EXPECT_EQ(rsp.command("M100,0:"), "OK");
            EXPECT_EQ(rsp.command("X100,0:"), "OK");

            EXPECT_EQ(rsp.command("mffe,4"), "E0e");
            EXPECT_EQ(rsp.command("Mffe,4:11223344"), "E0e");
            EXPECT_EQ(rsp.command("Xffe,4:abcd"), "E0e");
            EXPECT_EQ(rsp.command("m2000,4"), "E0e");

            EXPECT_EQ(rsp.command("mzz"), "E16");
            EXPECT_EQ(rsp.command("m100"), "E16");
            EXPECT_EQ(rsp.command("Mzz"), "E16");
            EXPECT_EQ(rsp.command("M100,4"), "E16");
            EXPECT_EQ(rsp.command("Xzz"), "E16");
            EXPECT_EQ(rsp.command("X100,4:ab"), "E16");
            EXPECT_EQ(rsp.command("m0,400001"), "E16");
            EXPECT_EQ(rsp.command("M0,400001:00"), "E16");
            EXPECT_EQ(rsp.command("X0,800001:00"), "E16");

            EXPECT_EQ(rsp.command("M100,4:00000000"), "OK");
            EXPECT_EQ(rsp.command("X200,6:" + string(6, '\0')), "OK");
            EXPECT_EQ(rsp.command("Xffc,4:" + string(4, '\0')), "OK");
            EXPECT_EQ(rsp.command("Hg1"), "OK");
        });
    }

    void test_breakpoints() {
        run_session([&](rsp_client& rsp) {
            EXPECT_EQ(rsp.command("Hg2"), "OK");
            EXPECT_EQ(rsp.command("P20=" + hex32(0x2000)), "OK");
            EXPECT_EQ(rsp.command("P5=" + hex32(0)), "OK");

            EXPECT_EQ(rsp.command("Z0,2010,4"), "OK");
            EXPECT_EQ(rsp.command("c"), "T05thread:2;");
            EXPECT_EQ(rsp.command("p20"), hex32(0x2010));
            EXPECT_EQ(rsp.command("p5"), hex32(4));
            EXPECT_EQ(rsp.command("?"), "T05thread:2;");

            EXPECT_EQ(rsp.command("c"), "T05thread:2;");
            EXPECT_EQ(rsp.command("p20"), hex32(0x2010));
            EXPECT_EQ(rsp.command("p5"), hex32(4 + 0x1000 / 4));

            EXPECT_EQ(rsp.command("Hg3"), "OK");
            EXPECT_EQ(rsp.command("P20=" + hex32(0x3000)), "OK");
            EXPECT_EQ(rsp.command("Z1,3020,4"), "OK");
            EXPECT_EQ(rsp.command("z0,2010,4"), "OK");
            EXPECT_EQ(rsp.command("z0,2010,4"), "E05");
            EXPECT_EQ(rsp.command("vCont;c"), "T05thread:3;");
            EXPECT_EQ(rsp.command("p20"), hex32(0x3020));
            EXPECT_EQ(rsp.command("qC"), "QC1");
            EXPECT_EQ(rsp.command("z1,3020,4"), "OK");
            EXPECT_EQ(rsp.command("z1,3020,4"), "E05");

            EXPECT_EQ(rsp.command("Z0,2010,4"), "OK");
            EXPECT_EQ(rsp.command("Z0,2010,4"), "OK");
            EXPECT_EQ(rsp.command("z0,2010,4"), "OK");
            EXPECT_TRUE(cpu1.breakpoints.empty());

            EXPECT_EQ(rsp.command("Z9,2010,4"), "E16");
            EXPECT_EQ(rsp.command("z9,2010,4"), "E16");
            EXPECT_EQ(rsp.command("Zzz"), "E16");
            EXPECT_EQ(rsp.command("zzz"), "E16");

            EXPECT_EQ(rsp.command("Hg1"), "OK");
            EXPECT_EQ(rsp.command("Hc1"), "OK");
        });
    }

    void test_watchpoints() {
        run_session([&](rsp_client& rsp) {
            EXPECT_EQ(rsp.command("Hg2"), "OK");
            EXPECT_EQ(rsp.command("P20=" + hex32(0x2000)), "OK");

            EXPECT_EQ(rsp.command("Z2,82008,4"), "OK");
            string reply = rsp.command("c");
            EXPECT_THAT(reply, StartsWith("T05thread:2;"));
            EXPECT_THAT(reply, EndsWith("watch:82008;"));
            EXPECT_EQ(rsp.command("p20"), hex32(0x200c));
            EXPECT_EQ(rsp.command("?"), "T05thread:2;");
            EXPECT_EQ(rsp.command("z2,82008,4"), "OK");
            EXPECT_EQ(rsp.command("z2,82008,4"), "E05");

            EXPECT_EQ(rsp.command("Z3,92010,4"), "OK");
            EXPECT_EQ(rsp.command("c"), "T05thread:2;rwatch:92010;");
            EXPECT_EQ(rsp.command("p20"), hex32(0x2014));
            EXPECT_EQ(rsp.command("z3,92010,4"), "OK");
            EXPECT_EQ(rsp.command("z3,92010,4"), "E05");

            EXPECT_EQ(rsp.command("Hg3"), "OK");
            EXPECT_EQ(rsp.command("P20=" + hex32(0x3000)), "OK");
            EXPECT_EQ(rsp.command("Z4,93020,8"), "OK");
            EXPECT_EQ(rsp.command("c"), "T05thread:3;rwatch:93020;");
            EXPECT_EQ(rsp.command("p20"), hex32(0x3024));
            EXPECT_EQ(rsp.command("z4,93020,8"), "OK");
            EXPECT_EQ(rsp.command("z4,93020,8"), "E05");

            for (auto* cpu : { &cpu0, &cpu1, &cpu2, &cpu3 })
                EXPECT_TRUE(cpu->watchpoints.empty()) << cpu->name();

            EXPECT_EQ(rsp.command("Hg1"), "OK");
            EXPECT_EQ(rsp.command("Hc1"), "OK");
        });
    }

    void test_stepping() {
        run_session([&](rsp_client& rsp) {
            EXPECT_EQ(rsp.command("Hg2"), "OK");
            EXPECT_EQ(rsp.command("Hc2"), "OK");
            EXPECT_EQ(rsp.command("P20=" + hex32(0x2000)), "OK");
            EXPECT_EQ(rsp.command("P5=" + hex32(0)), "OK");

            EXPECT_EQ(rsp.command("s"), "T05thread:2;");
            EXPECT_EQ(rsp.command("p20"), hex32(0x2004));
            EXPECT_EQ(rsp.command("p5"), hex32(1));
            EXPECT_EQ(rsp.command("s"), "T05thread:2;");
            EXPECT_EQ(rsp.command("p20"), hex32(0x2008));
            EXPECT_EQ(rsp.command("p5"), hex32(2));

            EXPECT_EQ(rsp.command("vCont;s:2"), "T05thread:2;");
            EXPECT_EQ(rsp.command("p20"), hex32(0x200c));
            EXPECT_EQ(rsp.command("p5"), hex32(3));

            EXPECT_EQ(rsp.command("Hg4"), "OK");
            EXPECT_EQ(rsp.command("P20=" + hex32(0x4000)), "OK");
            EXPECT_EQ(rsp.command("vCont;s:p1.4;c"), "T05thread:4;");
            EXPECT_EQ(rsp.command("p20"), hex32(0x4004));
            EXPECT_EQ(rsp.command("?"), "T05thread:4;");

            EXPECT_EQ(rsp.command("Hg2"), "OK");
            EXPECT_EQ(rsp.command("P20=" + hex32(0x2000)), "OK");
            EXPECT_EQ(rsp.command("vCont;s:4"), "T05thread:4;");
            EXPECT_EQ(rsp.command("p20"), hex32(0x4008));
            EXPECT_EQ(rsp.command("Hg2"), "OK");
            EXPECT_EQ(rsp.command("p20"), hex32(0x2000));

            EXPECT_EQ(rsp.command("Hg1"), "OK");
            EXPECT_EQ(rsp.command("Hc1"), "OK");
        });
    }

    void test_interrupt() {
        run_session([&](rsp_client& rsp) {
            EXPECT_EQ(rsp.command("Hc3"), "OK");
            for (const char* cmd : { "c", "vCont;c" }) {
                size_t cycles = cpu2.cycles;
                rsp.send_packet(cmd);
                EXPECT_EQ(rsp.recv_ack(), '+');
                while (cpu2.cycles < cycles + 10)
                    mwr::usleep(100);
                rsp.send_raw("\x03");
                EXPECT_THAT(rsp.recv_packet(), StartsWith("T05thread:"));
                EXPECT_EQ(rsp.command("Hc3"), "OK");
            }

            EXPECT_EQ(rsp.command("Hc1"), "OK");
        });
    }

    void test_protocol() {
        run_session([&](rsp_client& rsp) {
            rsp.send_packet("qC", 1);
            EXPECT_EQ(rsp.recv_ack(), '-');
            rsp.send_packet("qC");
            EXPECT_EQ(rsp.recv_ack(), '+');
            EXPECT_EQ(rsp.recv_packet(), "QC1");

            rsp.send_raw("$qAtt$qC#b4");
            EXPECT_EQ(rsp.recv_ack(), '+');
            EXPECT_EQ(rsp.recv_packet(), "QC1");
        });

        size_t cycles = cpu0.cycles;
        wait(clock_cycles(100));
        EXPECT_GT(cpu0.cycles, cycles);

        run_session([&](rsp_client& rsp) {
            EXPECT_EQ(rsp.command("?"), "T05thread:1;");
        });
    }
};

TEST(meta, gdbserver) {
    gdbserver_test test("test");
    sc_core::sc_start();
}
