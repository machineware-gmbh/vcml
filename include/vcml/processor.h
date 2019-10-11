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

#ifndef VCML_PROCESSOR_H
#define VCML_PROCESSOR_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"

#include "vcml/logging/logger.h"
#include "vcml/backends/backend.h"
#include "vcml/properties/property.h"

#include "vcml/debugging/gdbstub.h"
#include "vcml/debugging/gdbserver.h"

#include "vcml/elf.h"
#include "vcml/range.h"
#include "vcml/ports.h"
#include "vcml/component.h"
#include "vcml/master_socket.h"

namespace vcml {

    struct irq_stats {
        unsigned int irq;
        unsigned int irq_count;
        bool         irq_status;
        sc_time      irq_last;
        sc_time      irq_uptime;
        sc_time      irq_longest;
    };

    class processor: public component,
                     public debugging::gdbstub {
    private:
        double m_run_time;
        u64    m_curr_cycle_count;
        u64    m_prev_cycle_count;
        elf*   m_symbols;

        debugging::gdbserver* m_gdb;

        std::map<unsigned int, irq_stats> m_irq_stats;

        bool cmd_dump(const vector<string>& args, ostream& os);
        bool cmd_read(const vector<string>& args, ostream& os);
        bool cmd_symbols(const vector<string>& args, ostream& os);
        bool cmd_lsym(const vector<string>& args, ostream& os);
        bool cmd_disas(const vector<string>& args, ostream& os);
        bool cmd_v2p(const vector<string>& args, ostream& os);

        void processor_thread();

        void irq_handler(unsigned int irq);

        void simulate_debug(unsigned int cycles);

    public:
        property<string> symbols;

        property<u16>  gdb_port;
        property<bool> gdb_wait;
        property<bool> gdb_sync;
        property<bool> gdb_echo;

        in_port_list<bool> IRQ;

        master_socket INSN;
        master_socket DATA;

        processor(const sc_module_name& name);
        virtual ~processor();
        VCML_KIND(processor);

        processor() = delete;
        processor(const processor&) = delete;

        virtual string disassemble(u64& addr, unsigned char* insn);

        virtual u64 get_program_counter() { return 0; }
        virtual u64 get_stack_pointer()   { return 0; }
        virtual u64 get_core_id()         { return 0; }

        virtual void set_program_counter(u64 val) {}
        virtual void set_stack_pointer(u64 val)   {}
        virtual void set_core_id(u64 val)         {}

        virtual u64 cycle_count() const { return m_curr_cycle_count; }

        double get_run_time() const { return m_run_time; }
        double get_cps()      const { return cycle_count() / m_run_time; }

        virtual void reset() override;
        virtual sc_time& local_time(sc_process_b* proc = nullptr) override;

        bool get_irq_stats(unsigned int irq, irq_stats& stats) const;

        virtual void end_of_elaboration();

        virtual void interrupt(unsigned int irq, bool set);
        virtual void simulate(unsigned int&) = 0;

        void log_bus_error(const master_socket& socket, vcml_access accss,
                           tlm_response_status rs, u64 addr, u64 size);

        template <typename T>
        inline tlm_response_status fetch (u64 addr, T& data);

        template <typename T>
        inline tlm_response_status read  (u64 addr, T& data);

        template <typename T>
        inline tlm_response_status write (u64 addr, const T& data);

        virtual u64  gdb_num_registers() override;
        virtual u64  gdb_register_width(u64 idx) override;

        virtual bool gdb_read_reg(u64 idx, void* buf, u64 sz) override;
        virtual bool gdb_write_reg(u64 idx, const void* buf, u64 sz) override;

        virtual bool gdb_page_size(u64& size) override;
        virtual bool gdb_virt_to_phys(u64 vaddr, u64& paddr) override;

        virtual bool gdb_read_mem(u64 addr, void* buf, u64 sz) override;
        virtual bool gdb_write_mem(u64 addr, const void* buf, u64 sz) override;

        virtual bool gdb_insert_breakpoint(u64 addr) override;
        virtual bool gdb_remove_breakpoint(u64 addr) override;

        virtual bool gdb_insert_watchpoint(const range& mem,
                                           vcml_access acs) override;
        virtual bool gdb_remove_watchpoint(const range& mem,
                                           vcml_access a) override;

        virtual string gdb_handle_rcmd(const string& command) override;

        virtual void gdb_simulate(unsigned int& cycles) override;
        virtual void gdb_notify(int signal) override;
    };

    inline string processor::disassemble(u64& addr, unsigned char* insn) {
        addr += 4;
        return "n/a";
    }

    template <typename T>
    inline tlm_response_status processor::fetch(u64 addr, T& data) {
        tlm_response_status rs = INSN.readw(addr, data);
        if (failed(rs))
            log_bus_error(INSN, VCML_ACCESS_READ, rs, addr, sizeof(T));
        return rs;
    }

    template <typename T>
    inline tlm_response_status processor::read(u64 addr, T& data) {
        tlm_response_status rs = DATA.readw(addr, data);
        if (failed(rs))
            log_bus_error(DATA, VCML_ACCESS_READ, rs, addr, sizeof(T));
        return rs;
    }

    template <typename T>
    inline tlm_response_status processor::write(u64 addr, const T& data) {
        tlm_response_status rs = DATA.writew(addr, data);
        if (failed(rs))
            log_bus_error(DATA, VCML_ACCESS_WRITE, rs, addr, sizeof(T));
        return rs;
    }

}

#endif
