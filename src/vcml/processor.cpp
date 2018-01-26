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

#define HEX(x, w) std::setfill('0') << std::setw(w) << \
                  std::hex << x << std::dec

namespace vcml {

    bool processor::cmd_dump(const vector<string>& args, ostream& os) {
        os << "Registers:" << std::endl
           << "  PC 0x" << HEX(get_program_counter(), 16) << std::endl
           << "  SP 0x" << HEX(get_stack_pointer(), 16) << std::endl
           << "  ID 0x" << HEX(get_core_id(), 16) << std::endl;

        os << "Interrupts:" << std::endl;
        in_port_list::iterator it;
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

    bool processor::cmd_reset(const vector<string>& args, ostream& os) {
        reset();
        return true;
    }

    bool processor::cmd_read(const vector<string>& args, ostream& os) {
        if (args.size() < 1) {
            os << "Usage: read <INSN|DATA> <start> <end>";
            return false;
        }

        master_socket& socket = (args[0] == "INSN") ? INSN : DATA;
        u64 start = strtoull(args[1].c_str(), NULL, 0);
        u64 end   = strtoull(args[2].c_str(), NULL, 0);
        u64 size  = end - start;

        tlm_response_status rs;
        unsigned char* data = new unsigned char [size];
        if (failed(rs = socket.read(start, data, size, VCML_FLAG_DEBUG))) {
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

    bool processor::cmd_bp(const vector<string>& args, ostream& os) {
        u64 addr = strtoul(args[0].c_str(), NULL, 0);
        if (m_symbols) {
            elf_symbol* sym = m_symbols->get_symbol(args[0]);
            if (sym)
                addr = sym->get_virt_addr();
        }

        if (!insert_breakpoint(addr)) { // implementation specific
            os << "Failed to insert breakpoint at 0x" << HEX(addr, 16);
            return false;
        }

        m_breakpoints.push_back(addr);
        os << "Inserted breakpoint at 0x" << HEX(addr, 16);
        return true;
    }

    bool processor::cmd_rmbp(const vector<string>& args, ostream& os) {
        u64 addr = strtoul(args[0].c_str(), NULL, 0);
        if (m_symbols) {
            elf_symbol* sym = m_symbols->get_symbol(args[0]);
            if (sym)
                addr = sym->get_virt_addr();
        }

        if (!stl_contains(m_breakpoints, addr)) {
            os << "No breakpoint at 0x" << HEX(addr, 16);
            return false;
        }

        if (!remove_breakpoint(addr)) { // implementation specific
            os << "Failed to remove breakpoint at 0x" << HEX(addr, 16);
            return false;
        }

        stl_remove_erase(m_breakpoints, addr);
        os << "Removed breakpoint at 0x" << HEX(addr, 16);
        return true;
    }

    bool processor::cmd_lsbp(const vector<string>& args, ostream& os) {
        if (m_breakpoints.empty()) {
            os << "No breakpoints";
            return true;
        }

        os << "Showing breakpoints:";
        for (unsigned int i = 0; i < m_breakpoints.size(); i++) {
            u64 addr = m_breakpoints[i];
            os << "\n" << i << ": 0x" << HEX(addr, 16);
            if (m_symbols) {
                elf_symbol* sym = m_symbols->get_symbol(addr);
                if (sym && sym->is_function()) {
                    os << " [" << sym->get_name();
                    u64 offset = addr - sym->get_virt_addr();
                    if (offset)
                       os << "+"<< HEX(offset, 4);
                    os << "]";
                }
            }
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

        bool virt = virt_to_phys(vstart, pstart) && virt_to_phys(vend, pend) &&
                    (pstart != vstart) && (pend != vend);

        os << "Disassembly of " << HEX(vstart, 16) << ".." << HEX(vend, 16);
        if (virt)
            os << " (virtual)";

        unsigned char insn[8];
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
                if (virt_to_phys(addr, phys))
                    os << HEX(phys, 8) << " ";
                else
                    os << "????????";
            }

            u64 prev = addr;
            if (success(INSN.read(phys, insn, VCML_FLAG_DEBUG))) {
                string disas = disassemble(addr, insn);
                VCML_ERROR_ON(addr == prev, "disassembly address stuck");
                os << HEX(insn, (addr - prev) * 2) << " " << disas;
            } else {
                os << "????????";
                addr += 4;
            }
        }

        return true;
    }

    SC_HAS_PROCESS(processor);

    void processor::processor_thread() {
        wait(SC_ZERO_TIME);

        while (true) {
            sc_time quantum = tlm_global_quantum::instance().get();

            unsigned int num_cycles = 1;
            if (quantum != SC_ZERO_TIME)
                num_cycles = quantum.to_seconds() * clock;
            if (num_cycles == 0)
                num_cycles = 1;

            timeval t1, t2;
            gettimeofday(&t1, NULL);
            simulate(num_cycles);
            gettimeofday(&t2, NULL);

            m_num_cycles += num_cycles;
            m_run_time   += (t2.tv_sec  - t1.tv_sec) +
                            (t2.tv_usec - t1.tv_usec) * 1e-6;

            sc_time delay((double)num_cycles / (double)clock, SC_SEC);
            wait(delay + offset());
            offset() = SC_ZERO_TIME;
        }
    }

    void processor::irq_handler(unsigned int irq) {
        irq_stats& stats = m_irq_stats[irq];
        bool irq_up = IRQ[irq].read();

        if (irq_up == stats.irq_status) {
            log_warning("irq %d already %s", irq, irq_up ? "set" : "cleared");
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

        interrupt(irq, irq_up);
    }

    processor::processor(const sc_module_name& nm, clock_t clk):
        component(nm),
        m_run_time(0),
        m_num_cycles(0),
        m_symbols(NULL),
        m_irq_stats(),
        m_breakpoints(),
        clock("clock", clk),
        symbols("symbols"),
        IRQ("IRQ"),
        INSN("INSN"),
        DATA("DATA") {
        SC_THREAD(processor_thread);

        if (!symbols.get().empty())
            m_symbols = new elf(symbols);

        register_command("dump", 0, this, &processor::cmd_dump,
            "dump internal state of the processor");
        register_command("reset", 0, this, &processor::cmd_reset,
            "reset statistic counters of the processor");
        register_command("read", 3, this, &processor::cmd_read,
            "read memory from INSN or DATA ports");
        register_command("symbols", 1, this, &processor::cmd_symbols,
            "load a symbol file for use in disassembly");
        register_command("lsym", 0, this, &processor::cmd_lsym,
            "show a list of all available symbols");
        register_command("bp", 1, this, &processor::cmd_bp,
            "installs a breakpoint at the given address or symbol");
        register_command("rmbp", 1, this, &processor::cmd_rmbp,
            "removes a given breakpoint");
        register_command("lsbp", 0, this, &processor::cmd_lsbp,
            "lists all currently installed breakpoints");
        register_command("disas", 0, this, &processor::cmd_disas,
            "disassemble instructions from memory");
    }

    processor::~processor() {
       if (m_symbols)
           delete m_symbols;
    }

    bool processor::get_irq_stats(unsigned int irq, irq_stats& stats) const {
        if (m_irq_stats.find(irq) == m_irq_stats.end())
            return false;

        stats = m_irq_stats.at(irq);
        return true;
    }

    void processor::end_of_elaboration() {
        in_port_list::iterator it;
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

    void processor::interrupt(unsigned int irq, bool set)
    {
        /* interrupt ignored by default */
    }

}
