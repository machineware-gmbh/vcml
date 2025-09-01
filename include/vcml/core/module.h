/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_MODULE_H
#define VCML_MODULE_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/command.h"

#include "vcml/logging/logger.h"
#include "vcml/tracing/tracer.h"
#include "vcml/properties/property.h"

namespace vcml {

class module : public sc_module, public hierarchy_element
{
private:
    std::map<string, command*> m_commands;

    bool cmd_clist(const vector<string>& args, ostream& os);
    bool cmd_cinfo(const vector<string>& args, ostream& os);
    bool cmd_abort(const vector<string>& args, ostream& os);
    bool cmd_version(const vector<string>& args, ostream& os);

public:
    property<bool> trace_all;
    property<bool> trace_errors;
    property<log_level> loglvl;

    logger log;

    module() = delete;
    module(const module&) = delete;
    module(const sc_module_name& nm);
    virtual ~module();
    VCML_KIND(module);
    virtual const char* version() const;

    sc_object* find_child(const string& name) const;

    virtual void session_suspend();
    virtual void session_resume();

    bool execute(const string& name, ostream& os);
    bool execute(const string& name, const vector<string>& args, ostream& os);

    void register_command(const string& name, unsigned int argc,
                          command_func func, const string& description);

    template <class T>
    void register_command(const string& name, unsigned int argc, T* host,
                          bool (T::*func)(const vector<string>&, ostream&),
                          const string& description);

    template <class T>
    void register_command(const string& name, unsigned int argc,
                          bool (T::*func)(const vector<string>&, ostream&),
                          const string& description);

    command* get_command(const string& name);
    vector<command*> get_commands() const;

    template <typename PAYLOAD>
    void record(trace_direction, const sc_object&, const PAYLOAD& tx,
                const sc_time& t = SC_ZERO_TIME) const;

    template <typename PAYLOAD>
    void trace_fw(const sc_object& port, const PAYLOAD& tx,
                  const sc_time& t = SC_ZERO_TIME) const;

    template <typename PAYLOAD>
    void trace_bw(const sc_object& port, const PAYLOAD& tx,
                  const sc_time& t = SC_ZERO_TIME) const;

    bool is_local_process(sc_process_b* proc = current_process()) const;

    using sc_object::get_hierarchy_scope;
};

inline sc_object* module::find_child(const string& name) const {
    return vcml::find_child(*this, name);
}

inline bool module::execute(const string& name, ostream& os) {
    return execute(name, std::vector<string>(), os);
}

inline void module::register_command(const string& cmdnm, unsigned int argc,
                                     command_func func, const string& desc) {
    if (stl_contains(m_commands, cmdnm)) {
        VCML_ERROR("module %s already has a command called %s", name(),
                   cmdnm.c_str());
    }

    m_commands[cmdnm] = new command(cmdnm, argc, std::move(func), desc);
}

template <class T>
void module::register_command(const string& cmdnm, unsigned int argc, T* host,
                              bool (T::*func)(const vector<string>&, ostream&),
                              const string& desc) {
    using std::placeholders::_1;
    using std::placeholders::_2;
    command_func cb = std::bind(func, host, _1, _2);
    register_command(cmdnm, argc, std::move(cb), desc);
}

template <class T>
void module::register_command(const string& cmdnm, unsigned int argc,
                              bool (T::*func)(const vector<string>&, ostream&),
                              const string& desc) {
    T* host = dynamic_cast<T*>(this);
    VCML_ERROR_ON(!host, "command host not found");
    register_command(cmdnm, argc, host, func, desc);
}

inline command* module::get_command(const string& name) {
    if (!stl_contains(m_commands, name))
        return nullptr;
    return m_commands[name];
}

template <typename PAYLOAD>
void module::record(trace_direction dir, const sc_object& port,
                    const PAYLOAD& tx, const sc_time& t) const {
    if (trace_all || (trace_errors && is_backward_trace(dir) && failed(tx)))
        tracer::record(dir, port, tx, t);
}

template <typename PAYLOAD>
void module::trace_fw(const sc_object& port, const PAYLOAD& tx,
                      const sc_time& t) const {
    record(TRACE_FW, port, tx, t);
}

template <typename PAYLOAD>
void module::trace_bw(const sc_object& port, const PAYLOAD& tx,
                      const sc_time& t) const {
    record(TRACE_BW, port, tx, t);
}

inline bool module::is_local_process(sc_process_b* proc) const {
    return proc && is_parent(this, proc);
}

} // namespace vcml

#endif
