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

namespace vcml {

bool processor::cmd_dump(const vector<string>& args, ostream& os) {
    os << "Registers:" << std::endl
       << "  PC 0x" << mkstr("%0*llx", 16, program_counter()) << std::endl
       << "  LR 0x" << mkstr("%0*llx", 16, link_register()) << std::endl
       << "  SP 0x" << mkstr("%0*llx", 16, stack_pointer()) << std::endl
       << "  ID 0x" << mkstr("%0*llx", 16, core_id()) << std::endl;

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

    os << "reading range 0x" << mkstr("%0*llx\n", 16, start) << " .. 0x"
       << mkstr("%0*llx", 16, end);

    u64 addr = start & ~0xf;
    while (addr < end) {
        if ((addr % 16) == 0)
            os << "\n" << mkstr("%0*llx", 16, addr) << ":";
        if ((addr % 4) == 0)
            os << " ";
        if (addr >= start)
            os << mkstr("%0*x", 2, (unsigned int)data[addr - start]);
        else
            os << "  ";
        addr++;
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
    for (const auto& symfile : symbols)
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

void processor::sample_callstack() {
#if defined(HAVE_INSCIGHT) && defined(INSCIGHT_CPU_CALL_STACK)
    if (!trace_callstack)
        return;

    u64 time = time_to_ps(local_time_stamp());
    vector<debugging::stackframe> frames;
    stacktrace(frames);

    for (size_t i = 0; i < frames.size(); i++) {
        u64 addr = frames[i].program_counter;
        string sym = frames[i].sym ? frames[i].sym->name() : "unknown";
        // NOLINTNEXTLINE(clang-analyzer-unix.Malloc)
        INSCIGHT_CPU_CALL_STACK(*this, time, i, addr, sym.c_str());
    }
#endif
}

u64 processor::simulate_cycles(size_t cycles) {
    if (trace_callstack)
        sample_callstack();

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
            m_async = true;
            vcml::sc_async([&]() { running = processor_thread_async(); },
                           async_affinity);
        } else {
            m_async = false;
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

            if (!is_running()) {
                lt += cycles_left * clock_cycle();
                return true;
            }

            // do not execute a single cycle to avoid tb flushes
            if (cycles_left == 1) {
                sc_progress(SC_ZERO_TIME);
                break;
            }

            simulate_cycles(min(cycles_left, step_size));
            update_local_time(lt, current_process());
            sc_progress(lt);
            lt = SC_ZERO_TIME;
        }

        while (sim_running() && async_time_offset() >= quantum) {
            mwr::cpu_yield();
            sc_progress(SC_ZERO_TIME);
        }
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

        if (is_running()) {
            num_cycles = simulate_cycles(num_cycles);
            if (is_stepping() && num_cycles > 0)
                notify_singlestep();
            if (num_cycles == 0)
                wait(quantum - local_time());
        } else
            wait(num_cycles * clock_cycle());
    } while (!needs_sync());

    sync();

    return true;
}

processor::processor(const sc_module_name& nm, const string& cpuarch):
    component(nm),
    target(*(module*)this),
    m_run_time(0),
    m_async(false),
    m_cycle_mtx(),
    m_cycle_count(0),
    m_gdb(nullptr),
    m_irq_stats(),
    cpuarch("arch", cpuarch),
    symbols("symbols"),
    gdb_wait("gdb_wait", false),
    gdb_echo("gdb_echo", false),
    gdb_port("gdb_port", gdb_wait ? 0 : -1),
    gdb_term("gdb_term", "gdbterm"),
    async("async", false),
    async_rate("async_rate", 5),
    async_affinity("async_affinity", -1),
    trace_callstack("trace_callstack", false),
    irq("irq"),
    insn("insn"),
    data("data") {
    SC_HAS_PROCESS(processor);
    SC_THREAD(processor_thread);

    insn.set_insn(true);
    data.set_insn(false);

    for (const string& s : symbols) {
        string symfile = trim(s);
        if (symfile.empty())
            continue;

        if (!mwr::file_exists(symfile)) {
            log_warn("cannot open file '%s'", symfile.c_str());
            continue;
        }

        u64 n = load_symbols_from_elf(symfile);
        log_debug("loaded %llu symbols from '%s'", n, symfile.c_str());
    }

    register_command("dump", 0, &processor::cmd_dump,
                     "dump internal state of the processor");
    register_command("read", 3, &processor::cmd_read,
                     "read memory from INSN or DATA ports");
    register_command("gdb", 0, &processor::cmd_gdb,
                     "opens a new gdb debug session");
}

processor::~processor() {
    if (m_gdb)
        delete m_gdb;
}

void processor::reset() {
    component::reset();

    m_cycle_count.clear();
    m_run_time = 0.0;

    reset_cpuregs();
    flush_cpuregs();
}

void processor::session_suspend() {
    component::session_suspend();
    if (m_async)
        sc_suspend_async(this);

    fetch_cpuregs();
}

void processor::session_resume() {
    component::session_resume();
    flush_cpuregs();
    if (m_async)
        sc_resume_async(this);
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

#if defined(HAVE_INSCIGHT) && defined(INSCIGHT_IRQ_LEVEL)
    INSCIGHT_IRQ_LEVEL(id(), irqno, state);
#endif

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
    if (proc && is_local_process(proc)) {
        lock_guard<mutex> guard(m_cycle_mtx);
        u64 curr_cycles = cycle_count();
        u64& prev_cycles = m_cycle_count[proc];
        VCML_ERROR_ON(curr_cycles < prev_cycles, "cycle count goes down");
        local_time += clock_cycles(curr_cycles - prev_cycles);
        prev_cycles = curr_cycles;
    }
}

void processor::before_end_of_elaboration() {
    component::before_end_of_elaboration();
    flush_cpuregs();
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

void processor::end_of_simulation() {
    if (async)
        sc_join_async();

    component::end_of_simulation();
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

void processor::wait_for_interrupt(sc_event& ev) {
#if defined(HAVE_INSCIGHT) && defined(INSCIGHT_CPU_IDLE_ENTER)
    INSCIGHT_CPU_IDLE_ENTER(*this);
#endif

    if (trace_callstack)
        sample_callstack();

    set_suspendable(true);
    wait(ev);
    set_suspendable(false);

#if defined(HAVE_INSCIGHT) && defined(INSCIGHT_CPU_IDLE_LEAVE)
    INSCIGHT_CPU_IDLE_LEAVE(*this);
#endif
}

} // namespace vcml
