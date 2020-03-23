/******************************************************************************
 *                                                                            *
 * Copyright 2018 Jan Henrik Weinstock                                        *
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

#include "vcml/processor.h"

#define HEX(x, w) std::setfill('0') << std::setw(w) << std::hex << x \
                  << std::dec << std::setfill('0')

namespace vcml {

    u64 processor::gdb_num_registers() {
        return m_num_gdbregs;
    }

    u64 processor::gdb_register_width(u64 idx) {
        cpureg_info reg;
        if (!lookup_gdbreg(idx, reg))
            return 0;
        return reg.size;
    }

    bool processor::gdb_read_reg(u64 idx, void* p, u64 size) {
        cpureg_info reg;
        if (!lookup_gdbreg(idx, reg))
            return false;

        VCML_ERROR_ON(size != reg.size, "invalid register size %lu", size);

        u64 val = get_cpureg_internal(reg);
        memcpy(p, &val, size);

        if (is_big_endian())
            memswap(p, size);

        return true;
    }

    bool processor::gdb_write_reg(u64 idx, const void* p, u64 size) {
        cpureg_info reg;
        if (!lookup_gdbreg(idx, reg))
            return false;
        if (is_write_allowed(reg.perms))
            return false;

        VCML_ERROR_ON(size != reg.size, "invalid register size %lu", size);

        u64 val = 0;
        memcpy(&val, p, size);

        if (is_big_endian())
            memswap(&val, size);

        set_cpureg_internal(reg, val);
        return false;
    }

    bool processor::gdb_page_size(u64& size) {
        size = 0;
        return false;
    }

    bool processor::gdb_virt_to_phys(u64 vaddr, u64& paddr) {
        paddr = vaddr;
        return true;
    }

    bool processor::gdb_read_mem(u64 addr, void* buffer, u64 size) {
        if (success(DATA.read(addr, buffer, size, SBI_DEBUG)))
            return true;
        if (success(INSN.read(addr, buffer, size, SBI_DEBUG)))
            return true;
        return false;
    }

    bool processor::gdb_write_mem(u64 addr, const void* buffer, u64 size) {
        if (success(DATA.write(addr, buffer, size, SBI_DEBUG)))
            return true;
        if (success(INSN.write(addr, buffer, size, SBI_DEBUG)))
            return true;
        return false;
    }

    bool processor::gdb_insert_breakpoint(u64 addr) {
        return false;
    }

    bool processor::gdb_remove_breakpoint(u64 addr) {
        return false;
    }

    bool processor::gdb_insert_watchpoint(const range& mem, vcml_access acs) {
        return false;
    }

    bool processor::gdb_remove_watchpoint(const range& mem, vcml_access acs) {
        return false;
    }

    string processor::gdb_handle_rcmd(const string& command) {
        vector<string> args = split(command, ' ');
        string cmdname = args[0];
        args.erase(args.begin());

        stringstream ss;
        execute(cmdname, args, ss);
        return ss.str();
    }

    void processor::gdb_simulate(unsigned int cycles) {
        simulate(cycles);
    }

    void processor::gdb_notify(int signal) {
        if (m_gdb)
            m_gdb->notify(signal);
    }

    bool processor::cmd_dump(const vector<string>& args, ostream& os) {
        os << "Registers:" << std::endl
           << "  PC 0x" << HEX(get_program_counter(), 16) << std::endl
           << "  SP 0x" << HEX(get_stack_pointer(), 16) << std::endl
           << "  ID 0x" << HEX(get_core_id(), 16) << std::endl;

        os << "Interrupts:" << std::endl;
        in_port_list<bool>::iterator it;
        for (it = IRQ.begin(); it != IRQ.end(); it++) {
            irq_stats stats;
            if (get_irq_stats(it->first, stats)) {
                os << "  IRQ" << it->first << ": ";

                if (!stats.irq_count) {
                    os << "no events" << std::endl;
                    continue;
                }

                double max = stats.irq_longest.to_seconds();
                double avg = stats.irq_uptime.to_seconds() / stats.irq_count;

                os << stats.irq_count << " events, "
                   << "avg " << avg * 1e6 << "us, "
                   << "max " << max * 1e6 << "us"
                   << std::endl;
            }
        }

        return true;
    }

    bool processor::cmd_read(const vector<string>& args, ostream& os) {
        master_socket& socket = (args[0] == "INSN") ? INSN : DATA;
        u64 start = strtoull(args[1].c_str(), NULL, 0);
        u64 end   = strtoull(args[2].c_str(), NULL, 0);
        u64 size  = end - start;
        if (end <= start) {
            os << "Usage: read <INSN|DATA> <start> <end>";
            return false;
        }

        tlm_response_status rs;
        unsigned char* data = new unsigned char [size];
        if (failed(rs = socket.read(start, data, size, SBI_DEBUG))) {
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

        delete [] data;
        return true;
    }

    bool processor::cmd_symbols(const vector<string>& args, ostream& os) {
        if (m_symbols)
            delete m_symbols;

        try {
            symbols = args[0];
            m_symbols = new elf(symbols);
            os << "Found " << m_symbols->get_num_symbols() << " symbols "
               << "in file '" << symbols.str() << "'";
        } catch (std::exception& e) {
            symbols = "";
            m_symbols = NULL;
            os << e.what();
            return false;
        }

        return true;
    }

    bool processor::cmd_lsym(const vector<string>& args, ostream& os) {
        if (!m_symbols) {
            os << "No symbols loaded";
            return false;
        }

        size_t nsym = m_symbols->get_num_symbols();
        if (nsym == 0) {
            os << "No symbols";
            return true;
        }

        os << "Listing symbols:";
        for (size_t i = 0; i < nsym; i++) {
            elf_symbol* sym = m_symbols->get_symbol(i);
            if (sym->is_function())
                os << "\nF ";
            else if (sym->is_object())
                os << "\nO ";
            else
                continue;

            os << HEX(sym->get_virt_addr(), 16) << " "
               << sym->get_name();
        }

        return true;
    }

    bool processor::cmd_disas(const vector<string>& args, ostream& os) {
        u64 vstart = get_program_counter();
        if (args.size() > 0)
            vstart = strtoull(args[0].c_str(), NULL, 0);

        u64 vend = vstart + 40; // disassemble 5/10 instructions by default
        if (args.size() > 1)
            vend = strtoull(args[1].c_str(), NULL, 0);

        vstart &= ~0x3ull;

        if (vstart > vend) {
            os << "Invalid range specified";
            return false;
        }

        u64 pstart = 0;
        u64 pend = 0;

        bool virt = gdb_virt_to_phys(vstart, pstart) &&
                    gdb_virt_to_phys(vend, pend) &&
                    (pstart != vstart) && (pend != vend);

        os << "Disassembly of " << HEX(vstart, 16) << ".." << HEX(vend, 16);
        if (virt)
            os << " (virtual)";

        u64 addr = vstart;
        while (addr < vend) {
            os << "\n" << ((addr == get_program_counter()) ? " > " : "   ");

            if (m_symbols) {
                elf_symbol* sym = m_symbols->find_function(addr);
                if (sym) {
                    os << "[" << sym->get_name();
                    u64 offset = addr - sym->get_virt_addr();
                    os << "+" << HEX(offset, 4) << "] ";
                }
            }

            os << HEX(addr, 16) << " ";

            u64 phys = addr;
            if (virt) {
                if (gdb_virt_to_phys(addr, phys))
                    os << HEX(phys, 16) << " ";
                else
                    os << "????????????????";
            }

            u64 prev = addr;
            unsigned char insn[8];
            if (success(INSN.read(phys, insn, sizeof(insn), SBI_DEBUG))) {
                string disas = disassemble(addr, insn);
                VCML_ERROR_ON(addr == prev, "disassembly address stuck");
                for (unsigned int i = 0; i < (addr - prev); i++)
                    os << HEX((int)insn[i], 2);
                os << " " << disas;
            } else {
                log_warn("debugger failed to read address 0x%016x", phys);
                os << "????????";
                addr += 4;
            }
        }

        return true;
    }

    bool processor::cmd_v2p(const vector<string>& args, ostream& os) {
        u64 phys = -1;
        u64 virt = strtoull(args[0].c_str(), NULL, 0);
        bool success = gdb_virt_to_phys(virt, phys);
        if (!success) {
            os << "cannot translate virtual address 0x" << HEX(virt, 16);
            return false;
        } else {
            os << "0x" << HEX(virt, 16) << " -> 0x" << HEX(phys, 16);
            return true;
        }
    }

    void processor::cpureg_info::create(u64 defval) {
        if (defval > 0 && fls(defval) >= size * 8)
            VCML_ERROR("value 0x%lx is too big for register %s", defval, name);

        switch (size) {
        case 1: prop = new property< u8>(name, defval); break;
        case 2: prop = new property<u16>(name, defval); break;
        case 4: prop = new property<u32>(name, defval); break;
        case 8: prop = new property<u64>(name, defval); break;
        default:
            VCML_ERROR("register %s has illegal size %u bytes", name, size);
        }
    }

    void processor::cpureg_info::reset() {
        switch (size) {
        case 1: set(dynamic_cast<property< u8>*>(prop)->get_default()); break;
        case 2: set(dynamic_cast<property<u16>*>(prop)->get_default()); break;
        case 4: set(dynamic_cast<property<u32>*>(prop)->get_default()); break;
        case 8: set(dynamic_cast<property<u64>*>(prop)->get_default()); break;
        default:
            VCML_ERROR("register %s has illegal size %u bytes", name, size);
        }
    }

    u64 processor::cpureg_info::get() const {
        switch (size) {
        case 1: return dynamic_cast<property< u8>*>(prop)->get();
        case 2: return dynamic_cast<property<u16>*>(prop)->get();
        case 4: return dynamic_cast<property<u32>*>(prop)->get();
        case 8: return dynamic_cast<property<u64>*>(prop)->get();
        default:
            VCML_ERROR("register %s has illegal size %u bytes", name, size);
        }
    }

    void processor::cpureg_info::set(u64 val) {
        if (val > 0 && fls(val) >= size * 8)
            VCML_ERROR("value 0x%lx is too big for register %s", val, name);

        switch (size) {
        case 1: dynamic_cast<property< u8>*>(prop)->set(( u8)val); break;
        case 2: dynamic_cast<property<u16>*>(prop)->set((u16)val); break;
        case 4: dynamic_cast<property<u32>*>(prop)->set((u32)val); break;
        case 8: dynamic_cast<property<u64>*>(prop)->set((u64)val); break;
        default:
            VCML_ERROR("register %s has illegal size %u bytes", name, size);
        }
    }

    bool processor::has_cpureg(int regno) const {
        return m_cpuregs.find(regno) != m_cpuregs.end();
    }

    bool processor::lookup_cpureg(int regno, cpureg_info& info) {
        auto it = m_cpuregs.find(regno);
        if (it == m_cpuregs.end())
            return false;

        info = it->second;
        return true;
    }

    bool processor::lookup_gdbreg(int gdbno, cpureg_info& info) {
        for (auto it : m_cpuregs) {
            if (it.second.gdbno == gdbno) {
                info = it.second;
                return true;
            }
        }

        return false;
    }

    u64  processor::get_cpureg_internal(const cpureg_info& reg) {
        if (!reg.read_allowed())
            return 0;
        return get_cpureg(reg.regno);
    }

    void processor::set_cpureg_internal(const cpureg_info& reg, u64 val) {
        if (!reg.write_allowed())
            return;
        set_cpureg(reg.regno, val);
    }

    void processor::processor_thread() {
        wait(SC_ZERO_TIME);
        while (true) {
            sc_time now = local_time_stamp();

            // check for standby requests
            wait_clock_reset();

            sc_time quantum = tlm_global_quantum::instance().get();

            unsigned int num_cycles = 1;
            if (quantum != SC_ZERO_TIME)
                num_cycles = (quantum - local_time()) / clock_cycle();
            if (num_cycles == 0)
                num_cycles = 1;

            double start = realtime();
            if (m_gdb == nullptr) {
                simulate(num_cycles);
            } else {
                m_gdb->simulate(num_cycles);

                if (m_gdb->is_killed())
                    return;

                if (m_gdb->is_stopped() && !gdb_sync)
                    local_time() += clock_cycles(num_cycles);
            }

            m_run_time += realtime() - start;

            if (needs_sync())
                sync();

            // check that local time advanced beyond quantum start time
            // if we fail here, we most likely have a broken cycle_count()
            if (local_time_stamp() == now)
                VCML_ERROR("processor %s is stuck in time", name());
        }
    }

    void processor::irq_handler(unsigned int irq) {
        irq_stats& stats = m_irq_stats[irq];
        bool irq_up = IRQ[irq].read();

        if (irq_up == stats.irq_status) {
            log_warn("irq %d already %s", irq, irq_up ? "set" : "cleared");
            return;
        }

        stats.irq_status = irq_up;

        if (irq_up) {
            stats.irq_count++;
            stats.irq_last = sc_time_stamp();
        } else {
            sc_time delta = sc_time_stamp() - stats.irq_last;
            if (delta > stats.irq_longest)
                stats.irq_longest = delta;
            stats.irq_uptime += delta;
        }

        log_debug("%sing IRQ %u", irq_up ? "sett" : "clear", irq);
        interrupt(irq, irq_up);
    }

    SC_HAS_PROCESS(processor);

    processor::processor(const sc_module_name& nm):
        component(nm),
        m_run_time(0),
        m_cycle_count(0),
        m_symbols(NULL),
        m_gdb(NULL),
        m_irq_stats(),
        m_endian(VCML_ENDIAN_LITTLE),
        m_cpuregs(),
        m_num_gdbregs(0),
        symbols("symbols"),
        gdb_port("gdb_port", 0),
        gdb_wait("gdb_wait", false),
        gdb_sync("gdb_sync", true),
        gdb_echo("gdb_echo", false),
        IRQ("IRQ"),
        INSN("INSN"),
        DATA("DATA") {
        SC_THREAD(processor_thread);

        if (!symbols.get().empty()) {
            if (file_exists(symbols)) {
                m_symbols = new elf(symbols);
            } else {
                log_warn("cannot open file '%s'", symbols.get().c_str());
            }
        }

        if (gdb_port > 0) {
            debugging::gdb_status status = debugging::GDB_RUNNING;
            if (gdb_wait) {
                status = debugging::GDB_STOPPED;
                log_info("waiting for GDB connection on port %u",
                         (unsigned int)gdb_port.get());
            }

            m_gdb = new debugging::gdbserver(gdb_port, this, status);
            m_gdb->echo(gdb_echo);
            m_gdb->sync(gdb_sync);
        }

        register_command("dump", 0, this, &processor::cmd_dump,
            "dump internal state of the processor");
        register_command("read", 3, this, &processor::cmd_read,
            "read memory from INSN or DATA ports");
        register_command("symbols", 1, this, &processor::cmd_symbols,
            "load a symbol file for use in disassembly");
        register_command("lsym", 0, this, &processor::cmd_lsym,
            "show a list of all available symbols");
        register_command("disas", 0, this, &processor::cmd_disas,
            "disassemble instructions from memory");
        register_command("v2p", 1, this, &processor::cmd_v2p,
            "translate a given virtual address to physical");
    }

    processor::~processor() {
        if (m_gdb)
            delete m_gdb;
        if (m_symbols)
            delete m_symbols;
        for (auto reg : m_cpuregs)
            delete reg.second.prop;
    }

    void processor::reset() {
        component::reset();
        m_cycle_count = 0;
        m_run_time = 0.0;

        for (auto reg : m_cpuregs)
            reg.second.reset();

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

    bool processor::get_irq_stats(unsigned int irq, irq_stats& stats) const {
        if (m_irq_stats.find(irq) == m_irq_stats.end())
            return false;

        stats = m_irq_stats.at(irq);
        return true;
    }

    u64 processor::get_cpureg(int regno) {
        return 0; // to be overloaded
    }

    void processor::set_cpureg(int regno, u64 val) {
        // to be overloaded
    }

    void processor::fetch_cpuregs() {
        for (auto it : m_cpuregs)
            it.second.set(get_cpureg_internal(it.second));
    }

    void processor::flush_cpuregs() {
        for (auto it : m_cpuregs)
            set_cpureg_internal(it.second, it.second.get());
    }

    void processor::define_cpuregs(const vector<cpureg>& regs) {
        for (auto reg : regs) {
            if (has_cpureg(reg.regno))
                VCML_ERROR("register %s already defined", reg.name);

            if (!is_pow2(reg.size) || reg.size > 64)
                VCML_ERROR("register %s has illegal size", reg.name);

            u64 def = is_read_allowed(reg.perms) ? get_cpureg(reg.regno) : 0;
            cpureg_info& info = m_cpuregs[reg.regno];
            info.regno = reg.regno;
            info.gdbno = reg.gdbno;
            info.name  = reg.name;
            info.size  = reg.size;
            info.perms = reg.perms;
            info.create(def);

            if (reg.gdbno >= 0 && (u64)reg.gdbno >= m_num_gdbregs)
                m_num_gdbregs = reg.gdbno + 1;
        }

        log_debug("defined %zu cpu registers", m_cpuregs.size());
        log_debug("defined %lu gdb registers", m_num_gdbregs);

        flush_cpuregs();
    }

    void processor::log_bus_error(const master_socket& socket, vcml_access acs,
                                  tlm_response_status rs, u64 addr, u64 size) {
        string op;
        switch (acs) {
        case VCML_ACCESS_READ  : op = (&socket == &INSN) ? "fetch"
                                                         : "read"; break;
        case VCML_ACCESS_WRITE : op = "write"; break;
        default: op = "memory"; break;
        }

        string status = tlm_response_to_str(rs);
        log_debug("detected bus error during %s operation", op.c_str());
        log_debug("  addr = 0x%016" PRIx64, addr);
        log_debug("  pc   = 0x%016" PRIx64, get_program_counter());
        log_debug("  sp   = 0x%016" PRIx64, get_stack_pointer());
        log_debug("  size = %" PRIu64 " bytes", size);
        log_debug("  port = %s", DATA.name());
        log_debug("  code = %s", status.c_str());
    }

    void processor::interrupt(unsigned int irq, bool set) {
        // to be overloaded
    }

    void processor::update_local_time(sc_time& local_time) {
        u64 cycles = cycle_count();
        VCML_ERROR_ON(cycles < m_cycle_count, "cycle count goes down");
        local_time += clock_cycles(cycles - m_cycle_count);
        m_cycle_count = cycles;
    }

    void processor::end_of_elaboration() {
        for (auto it : IRQ) {
            std::stringstream ss;
            ss << "irq_handler_" << it.first;

            sc_spawn_options opts;
            opts.spawn_method();
            opts.set_sensitivity(it.second);
            opts.dont_initialize();
            sc_spawn(sc_bind(&processor::irq_handler, this, it.first),
                     sc_gen_unique_name(ss.str().c_str()), &opts);

            irq_stats& stats = m_irq_stats[it.first];
            stats.irq = it.first;
            stats.irq_count = 0;
            stats.irq_status = (*it.second)->read();
            stats.irq_last = SC_ZERO_TIME;
            stats.irq_uptime = SC_ZERO_TIME;
            stats.irq_longest = SC_ZERO_TIME;
        }
    }

}
