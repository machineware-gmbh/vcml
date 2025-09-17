/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_PROCESSOR_H
#define VCML_PROCESSOR_H

#include "vcml/core/types.h"
#include "vcml/core/range.h"
#include "vcml/core/component.h"

#include "vcml/logging/logger.h"
#include "vcml/properties/property.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"

#include "vcml/debugging/target.h"
#include "vcml/debugging/gdbserver.h"

namespace vcml {

struct irq_stats {
    size_t irq;
    size_t irq_count;
    bool irq_status;
    sc_time irq_last;
    sc_time irq_uptime;
    sc_time irq_longest;
};

class processor : public component, public debugging::target
{
private:
    double m_run_time;
    bool m_async;

    std::mutex m_cycle_mtx;
    std::unordered_map<sc_process_b*, u64> m_cycle_count;

    debugging::gdbserver* m_gdb;

    unordered_map<size_t, irq_stats> m_irq_stats;

    bool cmd_dump(const vector<string>& args, ostream& os);
    bool cmd_read(const vector<string>& args, ostream& os);
    bool cmd_gdb(const vector<string>& args, ostream& os);

    void sample_callstack();

    u64 simulate_cycles(size_t cycles);
    void processor_thread();
    bool processor_thread_sync();
    bool processor_thread_async();

public:
    property<string> cpuarch;
    property<vector<string>> symbols;

    property<bool> gdb_wait;
    property<bool> gdb_echo;
    property<int> gdb_port;
    property<string> gdb_host;
    property<string> gdb_term;

    property<bool> async;
    property<unsigned int> async_rate;
    property<int> async_affinity;

    property<bool> trace_callstack;

    gpio_target_array<> irq;

    tlm_initiator_socket insn;
    tlm_initiator_socket data;

    processor(const sc_module_name& name, const string& cpu_arch);
    virtual ~processor();
    VCML_KIND(processor);

    processor() = delete;
    processor(const processor&) = delete;

    virtual void session_suspend() override;
    virtual void session_resume() override;

    virtual u64 cycle_count() const = 0;

    double get_run_time() const { return m_run_time; }
    double get_cps() const { return cycle_count() / m_run_time; }

    virtual void reset() override;

    bool get_irq_stats(size_t irq, irq_stats& stats) const;

    template <typename T>
    inline tlm_response_status fetch(u64 addr, T& data);

    template <typename T>
    inline tlm_response_status read(u64 addr, T& data);

    template <typename T>
    inline tlm_response_status write(u64 addr, const T& data);

protected:
    void log_bus_error(const tlm_initiator_socket& socket, vcml_access rwx,
                       tlm_response_status rs, u64 addr, u64 size);

    virtual void gpio_notify(const gpio_target_socket& socket, bool state,
                             gpio_vector vector) override;

    virtual void interrupt(size_t irq, bool set, gpio_vector vector);
    virtual void interrupt(size_t irq, bool set);

    virtual void simulate(size_t cycles) = 0;
    virtual void update_local_time(sc_time& time, sc_process_b* proc) override;

    virtual void before_end_of_elaboration() override;
    virtual void end_of_elaboration() override;
    virtual void end_of_simulation() override;

    virtual u64 read_pmem_dbg(u64 addr, void* ptr, u64 sz) override;
    virtual u64 write_pmem_dbg(u64 addr, const void* ptr, u64 sz) override;

    virtual const char* arch() override;

    virtual void wait_for_interrupt(sc_event& ev);
};

template <typename T>
inline tlm_response_status processor::fetch(u64 addr, T& data) {
    tlm_response_status rs = insn.readw(addr, data);
    if (failed(rs))
        log_bus_error(insn, VCML_ACCESS_READ, rs, addr, sizeof(T));
    return rs;
}

template <typename T>
inline tlm_response_status processor::read(u64 addr, T& data) {
    tlm_response_status rs = data.readw(addr, data);
    if (failed(rs))
        log_bus_error(data, VCML_ACCESS_READ, rs, addr, sizeof(T));
    return rs;
}

template <typename T>
inline tlm_response_status processor::write(u64 addr, const T& data) {
    tlm_response_status rs = data.writew(addr, data);
    if (failed(rs))
        log_bus_error(data, VCML_ACCESS_WRITE, rs, addr, sizeof(T));
    return rs;
}

} // namespace vcml

#endif
