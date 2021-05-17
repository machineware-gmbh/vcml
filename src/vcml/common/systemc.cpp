/******************************************************************************
 *                                                                            *
 * Copyright 2020 Jan Henrik Weinstock                                        *
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

#include "vcml/common/systemc.h"
#include "vcml/common/thctl.h"

namespace vcml {

    sc_object* find_object(const string& name) {
        return sc_core::sc_find_object(name.c_str());
    }

    sc_attr_base* find_attribute(const string& name) {
        size_t pos = name.rfind(SC_HIERARCHY_CHAR);
        if (pos == string::npos)
            return nullptr;

        sc_object* parent = find_object(name.substr(0, pos));
        if (parent == nullptr)
            return nullptr;

        return parent->get_attribute(name);
    }

    const sc_time SC_MAX_TIME = time_from_value(~0ull);

    bool is_thread(sc_process_b* proc) {
        if (!thctl_is_sysc_thread())
            return false;
        if (proc == nullptr)
            proc = sc_get_current_process_b();
        if (proc == nullptr)
            return false;
        return proc->proc_kind() == sc_core::SC_THREAD_PROC_;
    }

    bool is_method(sc_process_b* proc) {
        if (!thctl_is_sysc_thread())
            return false;
        if (proc == nullptr)
            proc = sc_get_current_process_b();
        if (proc == nullptr)
            return false;
        return proc->proc_kind() == sc_core::SC_METHOD_PROC_;
    }

    sc_process_b* current_thread() {
        if (!thctl_is_sysc_thread())
            return nullptr;
        sc_process_b* proc = sc_get_current_process_b();
        if (proc == nullptr || proc->proc_kind() != sc_core::SC_THREAD_PROC_)
            return nullptr;
        return proc;
    }

    sc_process_b* current_method() {
        if (!thctl_is_sysc_thread())
            return nullptr;
        sc_process_b* proc = sc_get_current_process_b();
        if (proc == nullptr || proc->proc_kind() != sc_core::SC_METHOD_PROC_)
            return nullptr;
        return proc;
    }

    bool sim_running() {
        sc_simcontext* simc = sc_get_curr_simcontext();
        return simc->get_status() < sc_core::SC_STOPPED;
    }

    void hierarchy_push(sc_module* mod) {
        sc_simcontext* simc = sc_get_curr_simcontext();
        VCML_ERROR_ON(!simc, "no simulation context");
        simc->hierarchy_push(mod);
    }

    sc_module* hierarchy_pop() {
        sc_simcontext* simc = sc_get_curr_simcontext();
        VCML_ERROR_ON(!simc, "no simulation context");
        return simc->hierarchy_pop();
    }

    sc_module* hierarchy_top() {
        sc_simcontext* simc = sc_get_curr_simcontext();
        VCML_ERROR_ON(!simc, "no simulation context");
        return simc->hierarchy_curr();
    }

    const char* tlm_response_to_str(tlm_response_status status) {
        switch (status) {
        case TLM_OK_RESPONSE:
            return "TLM_OK_RESPONSE";

        case TLM_INCOMPLETE_RESPONSE:
            return "TLM_INCOMPLETE_RESPONSE";

        case TLM_GENERIC_ERROR_RESPONSE:
            return "TLM_GENERIC_ERROR_RESPONSE";

        case TLM_ADDRESS_ERROR_RESPONSE:
            return "TLM_ADDRESS_ERROR_RESPONSE";

        case TLM_COMMAND_ERROR_RESPONSE:
            return "TLM_COMMAND_ERROR_RESPONSE";

        case TLM_BURST_ERROR_RESPONSE:
            return "TLM_BURST_ERROR_RESPONSE";

        case TLM_BYTE_ENABLE_ERROR_RESPONSE:
            return "TLM_BYTE_ENABLE_ERROR_RESPONSE";

        default:
            return "TLM_UNKNOWN_RESPONSE";
        }
    }

    string tlm_transaction_to_str(const tlm_generic_payload& tx) {
        stringstream ss;

        // command
        switch (tx.get_command()) {
        case TLM_READ_COMMAND  : ss << "RD "; break;
        case TLM_WRITE_COMMAND : ss << "WR "; break;
        default: ss << "IG "; break;
        }

        // address
        ss << "0x";
        ss << std::hex;
        ss.width(16);
        ss.fill('0');
        ss << tx.get_address();

        // data array
        unsigned int size = tx.get_data_length();
        unsigned char* c = tx.get_data_ptr();

        ss << " [";
        if (size == 0)
            ss << "<no data>";
        for (unsigned int i = 0; i < size; i++) {
            ss.width(2);
            ss.fill('0');
            ss << static_cast<unsigned int>(*c++);
            if (i != (size - 1))
                ss << " ";
        }
        ss << "]";

        // response status
        ss << " (" << tx.get_response_string() << ")";

        // ToDo: byte enable, streaming, etc.
        return ss.str();
    }

    // we just need this class to have something that is called every cycle...
    class cycle_helper: public sc_core::sc_trace_file
    {
    public:
    #define DECL_TRACE_METHOD_A(t) \
        virtual void trace(const t& object, const string& n) override {}

    #define DECL_TRACE_METHOD_B(t) \
        virtual void trace(const t& object, const string& n, int w) override {}

        DECL_TRACE_METHOD_A(sc_event)
        DECL_TRACE_METHOD_A(sc_time)

        DECL_TRACE_METHOD_A(bool)
        DECL_TRACE_METHOD_A(sc_dt::sc_bit)
        DECL_TRACE_METHOD_A(sc_dt::sc_logic)

        DECL_TRACE_METHOD_B(unsigned char)
        DECL_TRACE_METHOD_B(unsigned short)
        DECL_TRACE_METHOD_B(unsigned int)
        DECL_TRACE_METHOD_B(unsigned long)
        DECL_TRACE_METHOD_B(char)
        DECL_TRACE_METHOD_B(short)
        DECL_TRACE_METHOD_B(int)
        DECL_TRACE_METHOD_B(long)
        DECL_TRACE_METHOD_B(sc_dt::int64)
        DECL_TRACE_METHOD_B(sc_dt::uint64)

        DECL_TRACE_METHOD_A(float)
        DECL_TRACE_METHOD_A(double)
        DECL_TRACE_METHOD_A(sc_dt::sc_int_base)
        DECL_TRACE_METHOD_A(sc_dt::sc_uint_base)
        DECL_TRACE_METHOD_A(sc_dt::sc_signed)
        DECL_TRACE_METHOD_A(sc_dt::sc_unsigned)

        DECL_TRACE_METHOD_A(sc_dt::sc_fxval)
        DECL_TRACE_METHOD_A(sc_dt::sc_fxval_fast)
        DECL_TRACE_METHOD_A(sc_dt::sc_fxnum)
        DECL_TRACE_METHOD_A(sc_dt::sc_fxnum_fast)

        DECL_TRACE_METHOD_A(sc_dt::sc_bv_base)
        DECL_TRACE_METHOD_A(sc_dt::sc_lv_base)

    #undef DECL_TRACE_METHOD_A
    #undef DECL_TRACE_METHOD_B

        virtual void trace(const unsigned int& object,
                           const std::string& name,
                           const char** enum_literals ) override {}
        virtual void write_comment(const std::string& comment) override {}
        virtual void set_time_unit(double v, 
			   sc_core::sc_time_unit tu) override {}

        vector<function<void(void)>> deltas;
        vector<function<void(void)>> tsteps;

        cycle_helper(): deltas(), tsteps() {
            sc_get_curr_simcontext()->add_trace_file(this);
        }

        virtual ~cycle_helper() {
    #if SYSTEMC_VERSION >= 20140417
            sc_get_curr_simcontext()->remove_trace_file(this);
    #endif
        }

    protected:
        virtual void cycle(bool delta_cycle) override;
    };

    void cycle_helper::cycle(bool delta_cycle) {
        if (delta_cycle) {
            for (auto func : deltas)
                func();
        } else {
            for (auto func : tsteps)
                func();
        }
    }

    static cycle_helper* g_cycle_helper = nullptr;

    void on_each_delta_cycle(function<void(void)> callback) {
        if (g_cycle_helper == nullptr)
            g_cycle_helper = new cycle_helper();

        g_cycle_helper->deltas.push_back(callback);
    }

    void on_each_time_step(function<void(void)> callback) {
        if (g_cycle_helper == nullptr)
            g_cycle_helper = new cycle_helper();

        g_cycle_helper->tsteps.push_back(callback);
    }

    struct async_info {
        atomic<bool> done;
        atomic<u64> progress;
        mutex jobmtx;
        condition_variable_any jobvar;
        queue<function<void(void)>> jobs;
    };

    __thread async_info* g_async = nullptr;

    void sc_async(const function<void(void)>& job) {
        async_info info;
        info.done = false;
        info.progress = 0;

        // ToDo: reuse threads
        thread t([&](void) -> void {
            g_async = &info;
            job();
            g_async->done = true;
            g_async = nullptr;
        });

        while (!info.done) {
            u64 p = info.progress.exchange(0);
            sc_core::wait(time_from_value(p));

            if (info.jobmtx.try_lock()) {
                while (!info.jobs.empty()) {
                    info.jobs.front()();
                    info.jobs.pop();
                }

                info.jobmtx.unlock();
                info.jobvar.notify_all();
            }
        }

        u64 p = info.progress.exchange(0);
        if (p > 0)
            sc_core::wait(time_from_value(p));

        t.join();
    }

    void sc_progress(const sc_time& delta) {
        VCML_ERROR_ON(!g_async, "no async thread");
        g_async->progress += delta.value();
    }

    void sc_sync(const function<void(void)>& job) {
        VCML_ERROR_ON(!g_async, "no async thread");
        g_async->jobs.push(job);
        g_async->jobvar.wait(g_async->jobmtx);
    }

}

namespace sc_core {
std::istream& operator >> (std::istream& is, sc_core::sc_time& t) {
    std::string str; is >> str;
    str = vcml::to_lower(str);

    char* endptr = nullptr;
    sc_dt::uint64 value = strtoul(str.c_str(), &endptr, 0);
    double fval = value;

    if (strcmp(endptr, "ps") == 0)
        t = sc_core::sc_time(fval, sc_core::SC_PS);
    else if (strcmp(endptr, "ns") == 0)
        t = sc_core::sc_time(fval, sc_core::SC_NS);
    else if (strcmp(endptr, "us") == 0)
        t = sc_core::sc_time(fval, sc_core::SC_US);
    else if (strcmp(endptr, "ms") == 0)
        t = sc_core::sc_time(fval, sc_core::SC_MS);
    else if (strcmp(endptr, "s") == 0)
        t = sc_core::sc_time(fval, sc_core::SC_SEC);
    else // raw value, not part of ieee1666!
        t = sc_core::sc_time(value, true);

    return is;
}
}
