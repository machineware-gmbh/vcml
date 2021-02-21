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
                  << std::dec << std::setfill(' ')

namespace vcml {

    bool processor::cmd_dump(const vector<string>& args, ostream& os) {
        os << "Registers:" << std::endl
           << "  PC 0x" << HEX(program_counter(), 16) << std::endl
           << "  LR 0x" << HEX(link_register(), 16) << std::endl
           << "  SP 0x" << HEX(stack_pointer(), 16) << std::endl
           << "  ID 0x" << HEX(core_id(), 16) << std::endl;

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
        if (!file_exists(args[0])) {
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
        if (!disassemble({vstart, vend}, disas)) {
            os << "Disassembler reported error";
            return false;
        }

        os << "Disassembly of " << HEX(vstart, 16) << ".." << HEX(vend, 16);

        u64 pgsz;
        bool virt = page_size(pgsz);
        if (virt)
            os << " (virtual)";

        for (auto insn : disas) {
            os << "\n" << (insn.addr == program_counter() ? " > " : "   ");
            if (insn.sym != nullptr) {
                u64 offset = insn.addr - insn.sym->virt_addr();
                if (offset <= insn.sym->size()) {
                    os << "[" << insn.sym->name() << "+"
                       << HEX(offset, 4) << "] ";
                }
            }

            os << HEX(insn.addr, 16) << " ";

            u64 phys = insn.addr;
            if (virt) {
                if (virt_to_phys(insn.addr, phys))
                    os << HEX(phys, 16) << " ";
                else
                    os << "????????????????";
            }

            os << HEX(insn.insn, insn.size * 2) << " " << insn.code;
        }

        return true;
    }

    bool processor::cmd_v2p(const vector<string>& args, ostream& os) {
        u64 phys = -1;
        u64 virt = strtoull(args[0].c_str(), NULL, 0);
        bool success = virt_to_phys(virt, phys);
        if (!success) {
            os << "cannot translate virtual address 0x" << HEX(virt, 16);
            return false;
        } else {
            os << "0x" << HEX(virt, 16) << " -> 0x" << HEX(phys, 16);
            return true;
        }
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

    processor::processor(const sc_module_name& nm, const string& cpuarch):
        component(nm),
        target(name()),
        m_run_time(0),
        m_cycle_count(0),
        m_gdb(nullptr),
        m_irq_stats(),
        m_regprops(),
        cpuarch("arch", cpuarch),
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
            vector<string> symfiles = split(trim(symbols), ';');
            for (auto symfile : symfiles) {
                symfile = trim(symfile);
                if (symfile.empty())
                    continue;

                if (!file_exists(symfile)) {
                    log_warn("cannot open file '%s'", symfile.c_str());
                    continue;
                }

                u64 n = load_symbols_from_elf(symfile);
                log_debug("loaded %lu symbols from '%s'", n, symfile.c_str());
            }
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

    bool processor::get_irq_stats(unsigned int irq, irq_stats& stats) const {
        if (m_irq_stats.find(irq) == m_irq_stats.end())
            return false;

        stats = m_irq_stats.at(irq);
        return true;
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
        log_debug("  addr = 0x%016lx", addr);
        log_debug("  pc   = 0x%016lx", program_counter());
        log_debug("  lr   = 0x%016lx", link_register());
        log_debug("  sp   = 0x%016lx", stack_pointer());
        log_debug("  size = %zu bytes", size);
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
    }

    void processor::fetch_cpuregs() {
        for (auto it : m_regprops) {
            const debugging::cpureg* reg = find_cpureg(it.first);
            property_base* prop = it.second;

            VCML_ERROR_ON(!reg, "no cpureg %lu", it.first);
            VCML_ERROR_ON(!prop, "no propery for cpureg %lu", it.first);

            u64 val = 0;
            if (reg->is_readable())
                val = reg->read();

            switch (reg->size) {
            case 1: dynamic_cast<property< u8>*>(prop)->set(( u8)val); break;
            case 2: dynamic_cast<property<u16>*>(prop)->set((u16)val); break;
            case 4: dynamic_cast<property<u32>*>(prop)->set((u32)val); break;
            case 8: dynamic_cast<property<u64>*>(prop)->set((u64)val); break;
            default:
                VCML_ERROR("register %s has illegal size: %lu bytes",
                           reg->name.c_str(), reg->size);
            }
        }
    }

    void processor::flush_cpuregs() {
        for (auto it : m_regprops) {
            const debugging::cpureg* reg = find_cpureg(it.first);
            property_base* prop = it.second;

            VCML_ERROR_ON(!reg, "no cpureg %lu", it.first);
            VCML_ERROR_ON(!prop, "no propery for cpureg %lu", it.first);

            if (!reg->is_writeable())
                continue;

            u64 val = 0;
            switch (reg->size) {
            case 1: val = dynamic_cast<property< u8>*>(prop)->get(); break;
            case 2: val = dynamic_cast<property<u16>*>(prop)->get(); break;
            case 4: val = dynamic_cast<property<u32>*>(prop)->get(); break;
            case 8: val = dynamic_cast<property<u64>*>(prop)->get(); break;
            default:
                VCML_ERROR("register %s has illegal size: %lu bytes",
                           reg->name.c_str(), reg->size);
            }

            const u64 mask = (1ul << reg->width()) - 1;
            if (reg->size < 8 && val > mask) {
                log_warn("truncating value 0x%lx for %lu bit cpu register %s",
                         val, reg->width(), reg->name.c_str());
                val &= mask;
            }

            reg->write(val);
        }
    }

    void processor::define_cpuregs(const vector<debugging::cpureg>& regs) {
        target::define_cpuregs(regs);

        for (auto reg : regs) {
            u64 defval = 0;
            const char* regnm = reg.name.c_str();

            if (reg.is_readable()) {
                if (!read_reg_dbg(reg.regno, defval))
                    VCML_ERROR("cannot read cpureg %s", regnm);
            }

            property_base*& prop = m_regprops[reg.regno];
            VCML_ERROR_ON(prop, "property %s already exists", regnm);

            switch (reg.size) {
            case 1: prop = new property< u8>(regnm, defval); break;
            case 2: prop = new property<u16>(regnm, defval); break;
            case 4: prop = new property<u32>(regnm, defval); break;
            case 8: prop = new property<u64>(regnm, defval); break;
            default:
                VCML_ERROR("cpureg %s has illegal size %lu", regnm, reg.size);
            }
        }

        log_debug("defined %zu cpu registers", regs.size());
        flush_cpuregs();
    }

    bool processor::read_reg_dbg(vcml::u64 idx, vcml::u64& val) {
        return false; // to be overloaded
    }

    bool processor::write_reg_dbg(vcml::u64 idx, vcml::u64 val) {
        return false; // to be overloaded
    }

    bool processor::read_cpureg_dbg(const cpureg& reg, vcml::u64& val) {
        if (!reg.is_readable())
            return false;

        return read_reg_dbg(reg.regno, val);
    }

    bool processor::write_cpureg_dbg(const cpureg& reg, vcml::u64 val) {
        if (!reg.is_writeable())
            return false;

        if (!write_reg_dbg(reg.regno, val))
            return false;

        auto it = m_regprops.find(reg.regno);
        if (it == m_regprops.end())
            VCML_ERROR("no propery for cpureg %s", reg.name.c_str());
        property_base* prop = it->second;

        switch (reg.size) {
        case 1: dynamic_cast<property< u8>*>(prop)->set(( u8)val); break;
        case 2: dynamic_cast<property<u16>*>(prop)->set((u16)val); break;
        case 4: dynamic_cast<property<u32>*>(prop)->set((u32)val); break;
        case 8: dynamic_cast<property<u64>*>(prop)->set((u64)val); break;
        default:
            VCML_ERROR("register %s has illegal size: %lu bytes",
                       reg.name.c_str(), reg.size);
        }

        return true;
    }

    u64 processor::read_pmem_dbg(u64 addr, void* buffer, u64 size) {
        try {
            if (success(DATA.read(addr, buffer, size, SBI_DEBUG)))
                return size;
            if (success(INSN.read(addr, buffer, size, SBI_DEBUG)))
                return size;
        } catch (report& r) {
            log_warn("error reading %lu bytes to memory at address 0x%lx: %s",
                     size, addr, r.message());
        }

        return 0;
    }

    u64 processor::write_pmem_dbg(u64 addr, const void* buffer, u64 size) {
        try {
            if (success(DATA.write(addr, buffer, size, SBI_DEBUG)))
                return size;
            if (success(INSN.write(addr, buffer, size, SBI_DEBUG)))
                return size;
        } catch (report& r) {
            log_warn("error writing %lu bytes to memory at address 0x%lx: %s",
                     size, addr, r.message());
        }

        return 0;
    }

    const char* processor::arch() {
        return cpuarch.get().c_str();
    }

    bool processor::gdb_command(const string& command, string& response) {
        vector<string> args = split(command, ' ');
        string cmdname = args[0];
        args.erase(args.begin());

        stringstream ss;
        if (!execute(cmdname, args, ss))
            return false;

        response = ss.str();
        return true;
    }

    void processor::gdb_simulate(unsigned int cycles) {
        simulate(cycles);
    }

}
