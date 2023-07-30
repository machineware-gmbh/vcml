/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/core/processor.h"
#include <sysc/kernel/sc_simcontext.h>
#include <chrono>
#include <thread>

#define HEX(x, w)                                                  \
    std::setfill('0') << std::setw(w) << std::hex << x << std::dec \
                      << std::setfill(' ')

namespace vcml {

bool processor::cmd_dump(const vector<string>& args, ostream& os) {
    os << "Registers:" << std::endl
       << "  PC 0x" << HEX(program_counter(), 16) << std::endl
       << "  LR 0x" << HEX(link_register(), 16) << std::endl
       << "  SP 0x" << HEX(stack_pointer(), 16) << std::endl
       << "  ID 0x" << HEX(core_id(), 16) << std::endl;

    os << "Interrupts:" << std::endl;
    for (auto it : irq) {
        irq_stats stats;
        if (get_irq_stats(it.first, stats)) {
            os << "  IRQ" << it.first << ": ";

            if (!stats.irq_count) {
                os << "no events" << std::endl;
                continue;
            }

            double max = stats.irq_longest.to_seconds();
            double avg = stats.irq_uptime.to_seconds() / stats.irq_count;

            os << stats.irq_count << " events, "
               << "avg " << avg * 1e6 << "us\\, "
               << "max " << max * 1e6 << "us" << std::endl;
        }
    }

    return true;
}

bool processor::cmd_read(const vector<string>& args, ostream& os) {
    u64 start = strtoull(args[1].c_str(), NULL, 0);
    u64 end = strtoull(args[2].c_str(), NULL, 0);
    u64 size = end - start;
    if (end <= start) {
        os << "Usage: read <INSN|DATA> <start> <end>";
        return false;
    }

    auto& socket = (to_lower(args[0]) == "insn") ? insn : data;

    tlm_response_status rs;
    auto data = std::make_unique<u8[]>(size);
    if (failed(rs = socket.read(start, data.get(), size, SBI_DEBUG))) {
        os << "Read request failed: " << tlm_response_to_str(rs);
        return false;
    }

    os << "reading range 0x" << HEX(start, 16) << " .. 0x" << HEX(end, 16);

    u64 addr = start & ~0xf;
    while (addr < end) {
        if ((addr % 16) == 0)
            os << "\n" << HEX(addr, 16) << ":";
        if ((addr % 4) == 0)
            os << " ";
        if (addr >= start)
            os << HEX((unsigned int)data[addr - start], 2);
        else
            os << "  ";
        addr++;
    }

    return true;
}

bool processor::cmd_symbols(const vector<string>& args, ostream& os) {
    if (!mwr::file_exists(args[0])) {
        os << "File not found: " << args[0];
        return false;
    }

    try {
        u64 n = load_symbols_from_elf(args[0]);
        os << "Found " << n << " symbols in file '" << args[0] << "'";
        return true;
    } catch (std::exception& e) {
        os << e.what();
        return false;
    }
}

bool processor::cmd_lsym(const vector<string>& args, ostream& os) {
    const debugging::symtab& syms = target::symbols();
    if (syms.empty()) {
        os << "No symbols loaded";
        return true;
    }

    os << "Listing symbols:";
    for (const auto& obj : syms.objects())
        os << "\nO " << HEX(obj.virt_addr(), 16) << " " << obj.name();
    for (const auto& func : syms.functions())
        os << "\nF " << HEX(func.virt_addr(), 16) << " " << func.name();

    return true;
}

bool processor::cmd_disas(const vector<string>& args, ostream& os) {
    u64 vstart = program_counter();
    if (args.size() > 0)
        vstart = strtoull(args[0].c_str(), NULL, 0);

    u64 vend = vstart + 40; // disassemble 5/10 instructions by default
    if (args.size() > 1)
        vend = strtoull(args[1].c_str(), NULL, 0);

    if (vstart > vend) {
        os << "Invalid range specified";
        return false;
    }

    vector<debugging::disassembly> disas;
    if (!disassemble({ vstart, vend }, disas)) {
        os << "Disassembler reported error";
        return false;
    }

    os << "Disassembly of " << HEX(vstart, vstart > ~0u ? 16 : 8) << ".."
       << HEX(vend, vend > ~0u ? 16 : 8);

    u64 pgsz;
    bool virt = page_size(pgsz);
    if (virt)
        os << " (virtual)";

    u64 maxsz = 0;
    for (const auto& insn : disas)
        maxsz = max(maxsz, insn.size);

    for (const auto& insn : disas) {
        os << "\n" << (insn.addr == program_counter() ? " > " : "   ");
        if (insn.sym != nullptr) {
            u64 offset = insn.addr - insn.sym->virt_addr();
            if (offset <= insn.sym->size()) {
                os << "[" << insn.sym->name() << "+" << HEX(offset, 4) << "] ";
            }
        }

        os << HEX(insn.addr, insn.addr > ~0u ? 16 : 8);

        u64 phys = insn.addr;
        if (virt) {
            if (virt_to_phys(insn.addr, phys))
                os << " " << HEX(phys, phys > ~0u ? 16 : 8);
            else
                os << "????????????????";
        }

        os << ": [";
        for (u64 i = 0; i < insn.size; i++) {
            os << HEX((int)insn.insn[i], 2);
            if (i < insn.size - 1)
                os << " ";
        }
        os << "]";

        for (u64 i = insn.size; i < maxsz; i++)
            os << "   ";

        os << " " << escape(insn.code, ",");
    }

    return true;
}

bool processor::cmd_v2p(const vector<string>& args, ostream& os) {
    u64 phys = -1;
    u64 virt = strtoull(args[0].c_str(), NULL, 0);
    bool ret = virt_to_phys(virt, phys);
    if (!ret) {
        os << "cannot translate virtual address 0x" << HEX(virt, 16);
        return false;
    } else {
        os << "0x" << HEX(virt, 16) << " -> 0x" << HEX(phys, 16);
        return true;
    }
}

bool processor::cmd_stack(const vector<string>& args, ostream& os) {
    stream_guard guard(os);
    vector<debugging::stackframe> frames;
    stacktrace(frames);

    for (const auto& frame : frames) {
        os << "[" << HEX(frame.program_counter, 16) << "]";
        if (frame.sym != nullptr) {
            os << " " << frame.sym->name() << " +0x" << std::hex
               << frame.program_counter - frame.sym->virt_addr() << "/0x"
               << std::hex << frame.sym->size();
        }

        os << "\n";
    }

    return true;
}

bool processor::cmd_gdb(const vector<string>& args, ostream& os) {
    if (!mwr::file_exists(gdb_term)) {
        os << "gdbterm not found at " << gdb_term.str() << std::endl;
        return false;
    }

    stringstream ss;
    ss << gdb_term.str() << " -n " << name() << " -p " << gdb_port.str();
    vector<string> symfiles = split(symbols);
    for (const auto& symfile : symfiles)
        ss << " -s " << symfile;

    log_debug("gdbterm command line:");
    log_debug("'%s'", ss.str().c_str());

    int res = system(ss.str().c_str());
    if (res == -1) {
        os << "failed to start gdb shell";
        return false;
    }

    os << "started gdb shell on port " << gdb_port.str();
    return true;
}

u64 processor::simulate_cycles(size_t cycles) {
    u64 count = cycle_count();
    double start = mwr::timestamp();
    set_suspendable(false);
    simulate(cycles);
    set_suspendable(true);
    m_run_time += mwr::timestamp() - start;
    return cycle_count() - count;
}

void processor::processor_thread() {
    wait(SC_ZERO_TIME);

    bool running = true;
    while (running) {
        sc_time now = local_time_stamp();

        // check for standby requests
        wait_clock_reset();

        if (async && !is_stepping()) {
            vcml::sc_async([&]() { running = processor_thread_async(); });
        } else {
            running = processor_thread_sync();
        }

        // check that local time advanced beyond quantum start time
        // if we fail here, we most likely have a broken cycle_count()
        if (local_time_stamp() == now)
            VCML_ERROR("processor %s is stuck in time", name());
    }
}

bool processor::processor_thread_async() {
    if (async && async_rate > 10u)
        log_warn("async_rate is larger than 10 - value: %u", async_rate.get());

    sc_time& lt = local_time();
    const sc_time& quantum = tlm::tlm_global_quantum::instance().get();

    sc_progress(lt);
    lt = SC_ZERO_TIME;

    while (true) {
        if (!sim_running())
            return false;

        for (sc_time offset = async_time_offset(); offset < quantum;
             offset = async_time_offset()) {
            u64 step_size = (quantum / clock_cycle()) / async_rate;
            u64 cycles_left = (quantum - offset) / clock_cycle();

            // fall back to sequential simulation when single-stepping
            if (is_stepping())
                return true;

            if (!is_running())
                return true;

            // do not execute a single cycle to avoid tb flushes
            if (cycles_left == 1)
                break;

            simulate_cycles(min(cycles_left, step_size));
            update_local_time(lt, current_process());
            sc_progress(lt);
            lt = SC_ZERO_TIME;
        }

        while (sim_running() && async_time_offset() >= quantum)
            mwr::cpu_yield();
    }
}

bool processor::processor_thread_sync() {
    do {
        debugging::suspender::handle_requests();
        if (!sim_running())
            return false;

        unsigned int num_cycles = 1;
        sc_time quantum = tlm_global_quantum::instance().get();
        if (quantum > clock_cycle() && quantum > local_time()) {
            sc_time time_left = quantum - local_time();
            num_cycles = time_left / clock_cycle();

            // if there will be less than one clock_cycle left in the current
            // quantum -> do one more instruction
            if (time_left % clock_cycle() != SC_ZERO_TIME)
                ++num_cycles;
        }

        if (is_stepping())
            num_cycles = 1;

        if (is_running())
            num_cycles = simulate_cycles(num_cycles);
        else
            wait(num_cycles * clock_cycle());

        if (is_stepping() && num_cycles > 0)
            notify_singlestep();
        if (is_running() && num_cycles == 0)
            wait(quantum - local_time());
    } while (!needs_sync());

    sync();

    return true;
}

processor::processor(const sc_module_name& nm, const string& cpuarch):
    component(nm),
    target(),
    m_run_time(0),
    m_cycle_count(0),
    m_gdb(nullptr),
    m_irq_stats(),
    m_regprops(),
    cpuarch("arch", cpuarch),
    symbols("symbols"),
    gdb_wait("gdb_wait", false),
    gdb_echo("gdb_echo", false),
    gdb_port("gdb_port", gdb_wait ? 0 : -1),
    gdb_term("gdb_term", "gdbterm"),
    async("async", false),
    async_rate("async_rate", 5),
    irq("irq"),
    insn("insn"),
    data("data") {
    SC_HAS_PROCESS(processor);
    SC_THREAD(processor_thread);

    if (!symbols.get().empty()) {
        vector<string> symfiles = split(symbols);
        for (auto symfile : symfiles) {
            symfile = trim(symfile);
            if (symfile.empty())
                continue;

            if (!mwr::file_exists(symfile)) {
                log_warn("cannot open file '%s'", symfile.c_str());
                continue;
            }

            u64 n = load_symbols_from_elf(symfile);
            log_debug("loaded %llu symbols from '%s'", n, symfile.c_str());
        }
    }

    register_command("dump", 0, &processor::cmd_dump,
                     "dump internal state of the processor");
    register_command("read", 3, &processor::cmd_read,
                     "read memory from INSN or DATA ports");
    register_command("symbols", 1, &processor::cmd_symbols,
                     "load a symbol file for use in disassembly");
    register_command("lsym", 0, &processor::cmd_lsym,
                     "show a list of all available symbols");
    register_command("disas", 0, &processor::cmd_disas,
                     "disassemble instructions from memory");
    register_command("v2p", 1, &processor::cmd_v2p,
                     "translate a given virtual address to physical");
    register_command("stack", 0, &processor::cmd_stack,
                     "generates a stack trace for the current function");
    register_command("gdb", 0, &processor::cmd_gdb,
                     "opens a new gdb debug session");
}

processor::~processor() {
    if (m_gdb)
        delete m_gdb;
    for (auto reg : m_regprops)
        delete reg.second;
}

void processor::reset() {
    component::reset();

    m_cycle_count = 0;
    m_run_time = 0.0;

    for (auto reg : m_regprops)
        reg.second->reset();

    flush_cpuregs();
}

void processor::session_suspend() {
    component::session_suspend();
    fetch_cpuregs();
}

void processor::session_resume() {
    component::session_resume();
    flush_cpuregs();
}

bool processor::get_irq_stats(size_t irq, irq_stats& stats) const {
    if (m_irq_stats.find(irq) == m_irq_stats.end())
        return false;

    stats = m_irq_stats.at(irq);
    return true;
}

void processor::log_bus_error(const tlm_initiator_socket& socket,
                              vcml_access rwx, tlm_response_status rs,
                              u64 addr, u64 size) {
    string op;
    switch (rwx) {
    case VCML_ACCESS_READ:
        op = (&socket == &insn) ? "fetch" : "read";
        break;
    case VCML_ACCESS_WRITE:
        op = "write";
        break;
    default:
        op = "memory";
        break;
    }

    string status = tlm_response_to_str(rs);
    log_debug("detected bus error during %s operation", op.c_str());
    log_debug("  addr = 0x%016llx", addr);
    log_debug("  pc   = 0x%016llx", program_counter());
    log_debug("  lr   = 0x%016llx", link_register());
    log_debug("  sp   = 0x%016llx", stack_pointer());
    log_debug("  size = %llu bytes", size);
    log_debug("  port = %s", data.name());
    log_debug("  code = %s", status.c_str());
}

void processor::gpio_notify(const gpio_target_socket& socket, bool state,
                            gpio_vector vector) {
    size_t irqno = irq.index_of(socket);
    irq_stats& stats = m_irq_stats[irqno];

    if (state == stats.irq_status) {
        log_warn("irq %zu already %s", irqno, state ? "set" : "cleared");
        return;
    }

    stats.irq_status = state;

    if (state) {
        stats.irq_count++;
        stats.irq_last = sc_time_stamp();
    } else {
        sc_time delta = sc_time_stamp() - stats.irq_last;
        if (delta > stats.irq_longest)
            stats.irq_longest = delta;
        stats.irq_uptime += delta;
    }

    log_debug("%sing IRQ %zu", state ? "sett" : "clear", irqno);
    interrupt(irqno, state, vector);
}

void processor::interrupt(size_t irq, bool set, gpio_vector vector) {
    interrupt(irq, set);
}

void processor::interrupt(size_t irq, bool set) {
    // to be overloaded
}

void processor::update_local_time(sc_time& local_time, sc_process_b* proc) {
    if (is_local_process(proc)) {
        u64 cycles = cycle_count();
        VCML_ERROR_ON(cycles < m_cycle_count, "cycle count goes down");
        local_time += clock_cycles(cycles - m_cycle_count);
        m_cycle_count = cycles;
    }
}

void processor::end_of_elaboration() {
    component::end_of_elaboration();

    for (auto it : irq) {
        irq_stats& stats = m_irq_stats[it.first];
        stats.irq = it.first;
        stats.irq_count = 0;
        stats.irq_status = false;
        stats.irq_last = SC_ZERO_TIME;
        stats.irq_uptime = SC_ZERO_TIME;
        stats.irq_longest = SC_ZERO_TIME;
    }

    if (gdb_port >= 0) {
        auto run = gdb_wait ? debugging::GDB_STOPPED : debugging::GDB_RUNNING;
        m_gdb = new debugging::gdbserver(gdb_port, *this, run);
        m_gdb->echo(gdb_echo);

        if (gdb_port == 0)
            gdb_port = m_gdb->port();

        log_info("%s for GDB connection on port %hu",
                 gdb_wait ? "waiting" : "listening", m_gdb->port());
    }
}

void processor::fetch_cpuregs() {
    for (auto it : m_regprops) {
        const debugging::cpureg* reg = find_cpureg(it.first);
        VCML_ERROR_ON(!reg, "no cpureg %llu", it.first);

        auto* prop = it.second;
        VCML_ERROR_ON(!prop, "no propery for cpureg %llu", it.first);

        if (!reg->is_readable())
            continue;

        if (!reg->read(prop->raw_ptr(), prop->raw_len()))
            log_warn("failed to fetch cpureg %s", reg->name.c_str());
    }
}

void processor::flush_cpuregs() {
    for (auto it : m_regprops) {
        const debugging::cpureg* reg = find_cpureg(it.first);
        VCML_ERROR_ON(!reg, "no cpureg %llu", it.first);

        auto* prop = it.second;
        VCML_ERROR_ON(!prop, "no propery for cpureg %llu", it.first);

        if (!reg->is_writeable())
            continue;

        if (!reg->write(prop->raw_ptr(), prop->raw_len()))
            log_warn("failed to flush cpureg %s", reg->name.c_str());
    }
}

void processor::define_cpureg(size_t regno, const string& name, size_t size,
                              size_t nelem, int prot) {
    target::define_cpureg(regno, name, size, nelem, prot);

    u64 defval = 0;
    if (is_read_allowed(prot)) {
        if (!read_reg_dbg(regno, &defval, min(size, sizeof(defval))))
            VCML_ERROR("failed to initialize cpureg %s", name.c_str());
    }

    auto*& prop = m_regprops[regno];
    VCML_ERROR_ON(prop, "property %s already exists", name.c_str());
    prop = new property<void>(name.c_str(), size, nelem, defval);
}

void processor::define_cpureg_r(size_t regno, const string& name, size_t size,
                                size_t count) {
    define_cpureg(regno, name, size, count, VCML_ACCESS_READ);
}

void processor::define_cpureg_w(size_t regno, const string& name, size_t size,
                                size_t count) {
    define_cpureg(regno, name, size, count, VCML_ACCESS_WRITE);
}

void processor::define_cpureg_rw(size_t regno, const string& name, size_t size,
                                 size_t count) {
    define_cpureg(regno, name, size, count, VCML_ACCESS_READ_WRITE);
}

bool processor::read_reg_dbg(size_t regno, void* buf, size_t len) {
    return false; // to be overloaded
}

bool processor::write_reg_dbg(size_t regno, const void* buf, size_t len) {
    return false; // to be overloaded
}

bool processor::read_cpureg_dbg(const debugging::cpureg& reg, void* buf,
                                size_t len) {
    return read_reg_dbg(reg.regno, buf, len);
}

bool processor::write_cpureg_dbg(const debugging::cpureg& reg, const void* buf,
                                 size_t len) {
    if (!write_reg_dbg(reg.regno, buf, len))
        return false;

    auto it = m_regprops.find(reg.regno);
    if (it != m_regprops.end()) {
        auto* prop = it->second;
        memcpy(prop->raw_ptr(), buf, min(prop->raw_len(), len));
    }

    return true;
}

u64 processor::read_pmem_dbg(u64 addr, void* buffer, u64 size) {
    try {
        if (success(data.read(addr, buffer, size, SBI_DEBUG)))
            return size;
        if (success(insn.read(addr, buffer, size, SBI_DEBUG)))
            return size;
    } catch (report& r) {
        log_warn("error reading %llu bytes to memory at address 0x%llx: %s",
                 size, addr, r.message());
    }

    return 0;
}

u64 processor::write_pmem_dbg(u64 addr, const void* buffer, u64 size) {
    try {
        if (success(data.write(addr, buffer, size, SBI_DEBUG)))
            return size;
        if (success(insn.write(addr, buffer, size, SBI_DEBUG)))
            return size;
    } catch (report& r) {
        log_warn("error writing %llu bytes to memory at address 0x%llx: %s",
                 size, addr, r.message());
    }

    return 0;
}

const char* processor::arch() {
    return cpuarch.get().c_str();
}

} // namespace vcml
