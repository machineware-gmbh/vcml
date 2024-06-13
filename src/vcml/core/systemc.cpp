/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/core/version.h"
#include "vcml/core/systemc.h"
#include "vcml/core/thctl.h"

namespace vcml {

#define SYSC_VERSION_MAJOR SC_VERSION_MAJOR
#define SYSC_VERSION_MINOR SC_VERSION_MINOR
#define SYSC_VERSION_PATCH SC_VERSION_PATCH

#define SYSC_GIT_REV       "n/a"
#define SYSC_GIT_REV_SHORT "n/a"

#define SYSC_VERSION        SYSTEMC_VERSION
#define SYSC_VERSION_STRING SC_VERSION

MWR_DECLARE_MODULE(SYSC, "systemc", "Apache-2.0")

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

    return parent->get_attribute(name.substr(pos + 1));
}

const sc_time SC_MAX_TIME = time_from_value(~0ull);

sc_module* hierarchy_top() {
    sc_simcontext* simc = sc_get_curr_simcontext();
    VCML_ERROR_ON(!simc, "no simulation context");
#if SYSTEMC_VERSION < SYSTEMC_VERSION_3_0_0
    return simc->hierarchy_curr();
#else
    return dynamic_cast<sc_module*>(simc->active_object());
#endif
}

void hierarchy_dump(ostream& os) {
    for (sc_object* obj : sc_core::sc_get_top_level_objects())
        hierarchy_dump(os, obj);
}

void hierarchy_dump(ostream& os, sc_object* obj) {
    if (obj == nullptr)
        return;

    os << obj->name() << std::endl;

    if (sc_module* mod = dynamic_cast<sc_module*>(obj))
        for (sc_object* child : mod->get_child_objects())
            hierarchy_dump(os, child);
}

bool is_parent(const sc_object* obj, const sc_object* child) {
    if (obj == child)
        return false;

    for (sc_object* parent = child->get_parent_object(); parent != nullptr;
         parent = parent->get_parent_object()) {
        if (parent == obj)
            return true;
    }

    return false;
}

bool is_child(const sc_object* obj, const sc_object* parent) {
    return is_parent(parent, obj);
}

sc_object* find_child(const sc_object& parent, const string& name) {
    size_t pos = name.find(SC_HIERARCHY_CHAR);
    string cur = name.substr(0, pos);
    string rem = pos != std::string::npos ? name.substr(pos + 1) : "";

    if (cur.empty())
        return nullptr;

    for (auto child : parent.get_child_objects()) {
        if (cur == child->basename()) {
            if (rem.empty())
                return child;
            return find_child(*child, rem);
        }
    }

    return nullptr;
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
    case TLM_READ_COMMAND:
        ss << "RD ";
        break;
    case TLM_WRITE_COMMAND:
        ss << "WR ";
        break;
    default:
        ss << "IG ";
        break;
    }

    // address
    ss << "0x";
    ss << std::hex;
    ss.width(tx.get_address() > ~0u ? 16 : 8);
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

#if SYSTEMC_VERSION >= SYSTEMC_VERSION_2_3_1a && \
    SYSTEMC_VERSION < SYSTEMC_VERSION_3_0_0
static inline bool test_kernel_has_phase_callbacks() {
    class callbacks_tester : public sc_object
    {
    public:
        sc_actions previous;
        callbacks_tester():
            sc_object("$$$vcml_phase_callback_tester$$$"),
            previous(sc_core::sc_report_handler::set_actions(
                sc_core::SC_ID_PHASE_CALLBACKS_UNSUPPORTED_,
                sc_core::SC_THROW)) {
            register_simulation_phase_callback(sc_core::SC_END_OF_UPDATE);
        }

        ~callbacks_tester() {
            sc_core::sc_report_handler::set_actions(
                sc_core::SC_ID_PHASE_CALLBACKS_UNSUPPORTED_, previous);
        }
    };

    try {
        callbacks_tester t;
        return true;
    } catch (sc_core::sc_report& r) {
        (void)r;
        return false;
    }
}
#endif

bool kernel_has_phase_callbacks() {
#if SYSTEMC_VERSION >= SYSTEMC_VERSION_3_0_0
    return true;
#elif SYSTEMC_VERSION < SYSTEMC_VERSION_2_3_1a
    return false;
#else
    static bool has_callbacks = test_kernel_has_phase_callbacks();
    return has_callbacks;
#endif
}

class helper_module : public sc_core::sc_trace_file,
#if SYSTEMC_VERSION >= SYSTEMC_VERSION_3_0_0
                      public sc_core::sc_stage_callback_if,
#endif
                      public sc_core::sc_prim_channel
{
public:
#define DECL_TRACE_METHOD_A(t) \
    virtual void trace(const t& object, const string& n) override {}

#define DECL_TRACE_METHOD_B(t) \
    virtual void trace(const t& object, const string& n, int w) override {}

#if SYSTEMC_VERSION >= SYSTEMC_VERSION_2_3_2
    DECL_TRACE_METHOD_A(sc_event)
    DECL_TRACE_METHOD_A(sc_time)
#endif

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

    virtual void trace(sc_trace_file* tf) const override {}
    virtual void trace(const unsigned int& object, const std::string& name,
                       const char** enum_literals) override {}
    virtual void write_comment(const std::string& comment) override {}
    virtual void set_time_unit(double v, sc_core::sc_time_unit tu) override {}

    atomic<bool> sim_running;

    const bool use_phase_callbacks;

    mutex mtx;

    vector<function<void(void)>> next_update;

    vector<function<void(void)>> end_of_elab;
    vector<function<void(void)>> start_of_sim;
    vector<function<void(void)>> end_of_sim;

    vector<function<void(void)>> deltas;
    vector<function<void(void)>> tsteps;

    struct timer_compare {
        bool operator()(const async_timer::event* a,
                        const async_timer::event* b) const {
            return a->timeout > b->timeout;
        }
    };

    sc_event timeout_event;
    priority_queue<async_timer::event*, vector<async_timer::event*>,
                   timer_compare>
        timers;

    vector<async_timer::event*> pending_timers() {
        lock_guard<mutex> guard(mtx);
        vector<async_timer::event*> pending;
        sc_time now = sc_time_stamp();

        while (!timers.empty()) {
            async_timer::event* event = timers.top();
            if (event->timeout > now)
                break;

            timers.pop();
            pending.push_back(event);
        }

        return pending;
    }

    void update_timer() {
        lock_guard<mutex> guard(mtx);

        if (timers.empty()) {
            timeout_event.cancel();
            return;
        }

        sc_time next_timeout = timers.top()->timeout;
        if (next_timeout < sc_time_stamp())
            timeout_event.notify(SC_ZERO_TIME);
        else
            timeout_event.notify(next_timeout - sc_time_stamp());
    }

    void run_timer() {
        vector<async_timer::event*> pending = pending_timers();
        for (auto event : pending) {
            if (event->owner)
                event->owner->trigger();
            delete event;
        }

        update_timer();
    }

    void add_timer(async_timer::event* ev) {
        lock_guard<mutex> guard(mtx);
        timers.push(ev);
        async_request_update();
    }

    void update() override {
        update_timer();

        vector<function<void(void)>> curr_update;

        mtx.lock();
        std::swap(curr_update, next_update);
        mtx.unlock();

        for (auto& fn : curr_update)
            fn();
    }

    VCML_KIND(helper_module);

    helper_module(const char* nm):
        sc_core::sc_trace_file(),
#if SYSTEMC_VERSION >= SYSTEMC_VERSION_3_0_0
        sc_core::sc_stage_callback_if(),
#endif
        sc_core::sc_prim_channel(nm),
        sim_running(true),
        use_phase_callbacks(kernel_has_phase_callbacks()),
        mtx(),
        end_of_elab(),
        start_of_sim(),
        end_of_sim(),
        deltas(),
        tsteps(),
        timeout_event("timeout_ev"),
        timers() {
#if SYSTEMC_VERSION >= SYSTEMC_VERSION_2_3_1a && \
    SYSTEMC_VERSION < SYSTEMC_VERSION_3_0_0
        if (use_phase_callbacks) {
            register_simulation_phase_callback(sc_core::SC_END_OF_UPDATE |
                                               sc_core::SC_BEFORE_TIMESTEP);
        }
#endif
#if SYSTEMC_VERSION < SYSTEMC_VERSION_3_0_0
        if (!use_phase_callbacks)
            simcontext()->add_trace_file(this);
#endif

#if SYSTEMC_VERSION >= SYSTEMC_VERSION_3_0_0
        sc_core::sc_register_stage_callback(
            *this, sc_core::SC_POST_UPDATE | sc_core::SC_PRE_TIMESTEP);
#endif

        sc_spawn_options opts;
        opts.spawn_method();
        opts.set_sensitivity(&timeout_event);
        opts.dont_initialize();
        sc_spawn([&]() -> void { run_timer(); }, "$$$$vcml_timer$$$$", &opts);
    }

#if SYSTEMC_VERSION >= SYSTEMC_VERSION_2_3_1a && \
    SYSTEMC_VERSION < SYSTEMC_VERSION_3_0_0
    virtual void simulation_phase_callback() override {
        switch (simcontext()->get_status()) {
        case sc_core::SC_END_OF_UPDATE:
            cycle(true);
            break;
        case sc_core::SC_BEFORE_TIMESTEP:
            cycle(false);
            break;

        default:
            break;
        }
    }
#elif SYSTEMC_VERSION >= SYSTEMC_VERSION_3_0_0
    virtual void stage_callback(const sc_core::sc_stage& stage) override {
        switch (stage) {
        case sc_core::SC_POST_UPDATE:
            cycle(true);
            break;
        case sc_core::SC_PRE_TIMESTEP:
            cycle(false);
            break;

        default:
            break;
        }
    }
#endif

    virtual ~helper_module() {
#if SYSTEMC_VERSION >= SYSTEMC_VERSION_2_3_1a && \
    SYSTEMC_VERSION < SYSTEMC_VERSION_3_0_0
        if (!use_phase_callbacks)
            sc_get_curr_simcontext()->remove_trace_file(this);
#endif
#if SYSTEMC_VERSION >= SYSTEMC_VERSION_3_0_0
        sc_core::sc_unregister_stage_callback(
            *this, sc_core::SC_POST_UPDATE | sc_core::SC_PRE_TIMESTEP);
#endif
        while (!timers.empty()) {
            delete timers.top();
            timers.pop();
        }
    }

    static helper_module& instance() {
        static helper_module helper("$$$$vcml_helper_module$$$$");
        return helper;
    }

protected:
    virtual void cycle(bool delta_cycle) override {
        if (delta_cycle) {
            for (auto& func : deltas)
                func();
        } else {
            for (auto& func : tsteps)
                func();
        }
    }

    virtual void end_of_elaboration() override {
        sim_running = true;

        for (auto& func : end_of_elab)
            func();
    }

    virtual void start_of_simulation() override {
        for (auto& func : start_of_sim)
            func();
    }

    virtual void end_of_simulation() override {
        sim_running = false;

        for (auto& func : end_of_sim)
            func();
    }
};

// just make sure the helper module exists at some point during initialization,
// since we cannot do that anymore after simulation has started
helper_module& g_helper = helper_module::instance();

void on_next_update(function<void(void)> callback) {
    helper_module& helper = helper_module::instance();
    lock_guard<mutex> guard(helper.mtx);
    helper.next_update.push_back(std::move(callback));
    helper.async_request_update();
}

void on_end_of_elaboration(function<void(void)> callback) {
    helper_module& helper = helper_module::instance();
    lock_guard<mutex> guard(helper.mtx);
    helper.end_of_elab.push_back(std::move(callback));
}

void on_start_of_simulation(function<void(void)> callback) {
    helper_module& helper = helper_module::instance();
    lock_guard<mutex> guard(helper.mtx);
    helper.start_of_sim.push_back(std::move(callback));
}

void on_end_of_simulation(function<void(void)> callback) {
    helper_module& helper = helper_module::instance();
    lock_guard<mutex> guard(helper.mtx);
    helper.end_of_sim.push_back(std::move(callback));
}

void on_each_delta_cycle(function<void(void)> callback) {
    helper_module& helper = helper_module::instance();
    lock_guard<mutex> guard(helper.mtx);
    helper.deltas.push_back(std::move(callback));
}

void on_each_time_step(function<void(void)> callback) {
    helper_module& helper = helper_module::instance();
    lock_guard<mutex> guard(helper.mtx);
    helper.tsteps.push_back(std::move(callback));
}

async_timer::async_timer(function<void(async_timer&)> cb):
    m_triggers(0), m_timeout(), m_event(nullptr), m_cb(std::move(cb)) {
}

async_timer::~async_timer() {
    cancel();
}

void async_timer::trigger() {
    m_event = nullptr;
    m_triggers++;
    m_cb(*this);
}

void async_timer::cancel() {
    if (m_event) {
        m_event->owner = nullptr;
        m_event = nullptr;
    }
}

void async_timer::reset(const sc_time& delta) {
    cancel();

    m_event = new event;
    m_event->owner = this;
    m_event->timeout = m_timeout = sc_time_stamp() + delta;

    g_helper.add_timer(m_event);
}

thread_local struct async_worker* g_async = nullptr;

struct async_worker {
    const size_t id;
    sc_process_b* const process;

    atomic<bool> alive;
    atomic<bool> working;
    function<void(void)> task;

    atomic<u64> progress;
    atomic<function<void(void)>*> request;

    mutex mtx;
    condition_variable_any notify;
    thread worker;

    sc_time sc_thread_pos;

    struct sim_terminated_exception {};

    async_worker(size_t worker_id, sc_process_b* worker_proc):
        id(worker_id),
        process(worker_proc),
        alive(true),
        working(false),
        task(),
        progress(0),
        request(nullptr),
        mtx(),
        notify(),
        worker(&async_worker::work, this),
        sc_thread_pos(sc_time_stamp()) {
        VCML_ERROR_ON(!process, "invalid parent process");
    }

    ~async_worker() { kill(); }

    void work() {
        g_async = this;
        mwr::set_thread_name(mkstr("vcml_async:%zu", id));

        mtx.lock();
        while (alive) {
            while (alive && !working)
                notify.wait(mtx);

            if (!alive)
                break;

            try {
                mtx.unlock();
                task();
                mtx.lock();
            } catch (sim_terminated_exception& ex) {
                (void)ex;
                mtx.lock();
                alive = false;
            }

            working = false;
        }

        mtx.unlock();
        g_async = nullptr;
    }

    void kill() {
        if (worker.joinable()) {
            mtx.lock();
            alive = false;
            mtx.unlock();
            notify.notify_all();
            worker.join();
        }
    }

    void run_async(function<void(void)>& job) {
        mtx.lock();
        task = job;
        working = true;
        mtx.unlock();
        notify.notify_one();

        while (working) {
            u64 p = progress.exchange(0);
            sc_thread_pos = sc_time_stamp() + time_from_value(p);
            sc_core::wait(time_from_value(p));

            if (request) {
                p = progress.exchange(0);
                if (p > 0) {
                    sc_thread_pos = sc_time_stamp() + time_from_value(p);
                    sc_core::wait(time_from_value(p));
                }

                (*request)();
                request = nullptr;
            }
        }

        u64 p = progress.exchange(0);
        if (p > 0)
            sc_core::wait(time_from_value(p));
    }

    void run_sync(function<void(void)> job) {
        g_async->request = &job;
        while (g_async->request) {
            if (!g_async->alive || !sim_running())
                throw sim_terminated_exception();
            mwr::cpu_yield();
        }
    }

    sc_time timestamp() { return sc_thread_pos + time_from_value(progress); }

    typedef unordered_map<sc_process_b*, shared_ptr<async_worker>> map_t;
    static map_t& all_workers() {
        static map_t workers;
        return workers;
    }

    static async_worker& lookup(sc_process_b* thread) {
        VCML_ERROR_ON(!thread, "invalid thread");

        auto& workers = all_workers();
        auto it = workers.find(thread);
        if (it != workers.end())
            return *it->second;

        size_t id = workers.size();
        auto worker = std::make_shared<async_worker>(id, thread);
        return *(workers[thread] = worker);
    }
};

void sc_async(function<void(void)> job) {
    auto thread = current_thread();
    VCML_ERROR_ON(!thread, "sc_async must be called from SC_THREAD");
    async_worker& worker = async_worker::lookup(thread);
    worker.run_async(job);
}

void sc_progress(const sc_time& delta) {
    VCML_ERROR_ON(!g_async, "no async thread to progress");
    g_async->progress += delta.value();
}

void sc_sync(function<void(void)> job) {
    if (thctl_is_sysc_thread()) {
        job();
    } else if (g_async != nullptr) {
        g_async->run_sync(std::move(job));
    } else {
        VCML_ERROR("not on systemc or async thread");
    }
}

void sc_join_async() {
    async_worker::all_workers().clear();
}

bool sc_is_async() {
    return g_async != nullptr;
}

sc_time async_time_stamp() {
    if (sc_is_async())
        return g_async->timestamp();
    return sc_time_stamp();
}

sc_time async_time_offset() {
    return async_time_stamp() - sc_time_stamp();
}

bool is_thread(sc_process_b* proc) {
    if (!thctl_is_sysc_thread())
        return false;
    if (proc == nullptr)
        proc = current_process();
    if (proc == nullptr)
        return false;
    return proc->proc_kind() == sc_core::SC_THREAD_PROC_;
}

bool is_method(sc_process_b* proc) {
    if (!thctl_is_sysc_thread())
        return false;
    if (proc == nullptr)
        proc = current_process();
    if (proc == nullptr)
        return false;
    return proc->proc_kind() == sc_core::SC_METHOD_PROC_;
}

sc_process_b* current_process() {
    if (g_async)
        return g_async->process;
    if (!thctl_is_sysc_thread())
        return nullptr;
    return sc_core::sc_get_current_process_b();
}

sc_process_b* current_thread() {
    sc_process_b* proc = current_process();
    if (proc == nullptr || proc->proc_kind() != sc_core::SC_THREAD_PROC_)
        return nullptr;
    return proc;
}

sc_process_b* current_method() {
    sc_process_b* proc = current_process();
    if (proc == nullptr || proc->proc_kind() != sc_core::SC_METHOD_PROC_)
        return nullptr;
    return proc;
}

bool is_stop_requested() {
    return sc_core::sc_get_simulator_status() == sc_core::SC_SIM_USER_STOP;
}

void request_stop() {
    if (!is_stop_requested())
        sc_stop();
}

bool sim_running() {
    return g_helper.sim_running;
}

string call_origin() {
    if (!thctl_is_sysc_thread()) {
        string name = mwr::get_thread_name();
        return mkstr("thread '%s'", name.c_str());
    }

    sc_core::sc_simcontext* simc = sc_core::sc_get_curr_simcontext();
    if (simc) {
        sc_process_b* proc = current_process();
        if (proc) {
            sc_object* parent = proc->get_parent_object();
            return parent ? parent->name() : proc->name();
        }

        sc_module* module = hierarchy_top();
        if (module)
            return module->name();
    }

    return "";
}

} // namespace vcml

namespace sc_core {

std::istream& operator>>(std::istream& is, sc_time& t) {
    std::string str;
    is >> str;
    str = vcml::to_lower(str);

    char* endptr = nullptr;
    sc_dt::uint64 val = strtoul(str.c_str(), &endptr, 0);
    double float_val = (double)val;

    if (strcmp(endptr, "ps") == 0)
        t = sc_time(float_val, sc_core::SC_PS);
    else if (strcmp(endptr, "ns") == 0)
        t = sc_time(float_val, sc_core::SC_NS);
    else if (strcmp(endptr, "us") == 0)
        t = sc_time(float_val, sc_core::SC_US);
    else if (strcmp(endptr, "ms") == 0)
        t = sc_time(float_val, sc_core::SC_MS);
    else if (strcmp(endptr, "s") == 0)
        t = sc_time(float_val, sc_core::SC_SEC);
    else
        t = ::vcml::time_from_value(val);

    return is;
}

} // namespace sc_core

namespace tlm {

std::ostream& operator<<(std::ostream& os, tlm_response_status status) {
    os << ::vcml::tlm_response_to_str(status);
    return os;
}

std::ostream& operator<<(std::ostream& os,
                         const tlm_generic_payload& payload) {
    os << ::vcml::tlm_transaction_to_str(payload);
    return os;
}

} // namespace tlm
