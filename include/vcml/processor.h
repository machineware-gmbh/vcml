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

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/bitops.h"
#include "vcml/common/range.h"

#include "vcml/logging/logger.h"
#include "vcml/backends/backend.h"
#include "vcml/properties/property.h"

#include "vcml/debugging/target.h"
#include "vcml/debugging/gdbserver.h"

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
                     protected debugging::target {
    private:
        double m_run_time;
        u64    m_cycle_count;

        debugging::gdbserver* m_gdb;

        unordered_map<unsigned int, irq_stats> m_irq_stats;
        unordered_map<u64, property_base*> m_regprops;

        bool cmd_dump(const vector<string>& args, ostream& os);
        bool cmd_read(const vector<string>& args, ostream& os);
        bool cmd_symbols(const vector<string>& args, ostream& os);
        bool cmd_lsym(const vector<string>& args, ostream& os);
        bool cmd_disas(const vector<string>& args, ostream& os);
        bool cmd_v2p(const vector<string>& args, ostream& os);

        using cpureg = debugging::cpureg;
        virtual bool read_cpureg_dbg(const cpureg& r, vcml::u64& val) override;
        virtual bool write_cpureg_dbg(const cpureg& r, vcml::u64 val) override;

        void processor_thread();

        void irq_handler(unsigned int irq);

    public:
        property<string> cpuarch;
        property<string> symbols;

        property<u16>  gdb_port;
        property<bool> gdb_wait;
        property<bool> gdb_sync;
        property<bool> gdb_echo;

        in_port_list<bool> IRQ;

        master_socket INSN;
        master_socket DATA;

        processor(const sc_module_name& name, const string& cpu_arch);
        virtual ~processor();
        VCML_KIND(processor);

        processor() = delete;
        processor(const processor&) = delete;

        virtual void session_suspend() override;
        virtual void session_resume() override;

        virtual u64 cycle_count() const = 0;

        double get_run_time() const { return m_run_time; }
        double get_cps()      const { return cycle_count() / m_run_time; }

        virtual void reset() override;

        bool get_irq_stats(unsigned int irq, irq_stats& stats) const;

        template <typename T>
        inline tlm_response_status fetch (u64 addr, T& data);

        template <typename T>
        inline tlm_response_status read  (u64 addr, T& data);

        template <typename T>
        inline tlm_response_status write (u64 addr, const T& data);

    protected:
        void log_bus_error(const master_socket& socket, vcml_access accss,
                           tlm_response_status rs, u64 addr, u64 size);

        virtual void interrupt(unsigned int irq, bool set);
        virtual void simulate(unsigned int cycles) = 0;
        virtual void update_local_time(sc_time& local_time) override;
        virtual void end_of_elaboration() override;

        virtual void fetch_cpuregs();
        virtual void flush_cpuregs();

        virtual void define_cpuregs(const vector<cpureg>& regs) override;

        virtual bool read_reg_dbg(vcml::u64 idx, vcml::u64& val);
        virtual bool write_reg_dbg(vcml::u64 idx, vcml::u64 val);

        virtual u64 read_pmem_dbg(u64 addr, void* ptr, u64 sz) override;
        virtual u64 write_pmem_dbg(u64 addr, const void* ptr, u64 sz) override;

        virtual const char* arch() override;
    };

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
