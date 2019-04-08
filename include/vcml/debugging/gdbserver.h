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

#ifndef VCML_GDBSERVER_H
#define VCML_GDBSERVER_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"
#include "vcml/common/thctl.h"

#include "vcml/logging/logger.h"

#include "vcml/debugging/gdbstub.h"
#include "vcml/debugging/rspserver.h"

#define VCML_GDBSERVER_ERR_COMMAND  (1) // gdb rsp command was malformed
#define VCML_GDBSERVER_ERR_PARAM    (2) // command parameter has invalid value
#define VCML_GDBSERVER_ERR_INTERNAL (3) // internal error during execution
#define VCML_GDBSERVER_ERR_UNKNOWN  (4) // used for all other kinds of errors

#define VCML_GDBSERVER_BUFSIZE      (VCML_RSP_MAX_PACKET_SIZE >> 1)

namespace vcml { namespace debugging {

    enum gdb_status {
        GDB_STOPPED,
        GDB_STEPPING,
        GDB_RUNNING
    };

    enum gdb_signal {
        GDBSIG_TRAP = 5
    };

    class gdbserver: public rspserver
    {
    private:
        gdbstub*   m_stub;
        gdb_status m_status;
        gdb_status m_default;

        int  m_signal;
        bool m_sync;

        void access_vmem(bool iswr, u64 addr, u8 buffer[], u64 size);
        void access_pmem(bool iswr, u64 addr, u8 buffer[], u64 size);

        typedef string (gdbserver::*handler)(const char*);
        std::map<char, handler> m_handler;

        enum breakpoint_type {
             GDB_BREAKPOINT_SW     = 0,
             GDB_BREAKPOINT_HW     = 1,
             GDB_WATCHPOINT_WRITE  = 2,
             GDB_WATCHPOINT_READ   = 3,
             GDB_WATCHPOINT_ACCESS = 4
        };

        handler find_handler(const char* command);

        string handle_unknown(const char* command);

        string handle_query(const char* command);
        string handle_rcmd(const char* command);
        string handle_step(const char* command);
        string handle_continue(const char* command);
        string handle_detach(const char* command);
        string handle_kill(const char* command);

        string handle_reg_read(const char* command);
        string handle_reg_write(const char* command);
        string handle_reg_read_all(const char* command);
        string handle_reg_write_all(const char* command);
        string handle_mem_read(const char* command);
        string handle_mem_write(const char* command);
        string handle_mem_write_bin(const char* command);

        string handle_breakpoint_set(const char* command);
        string handle_breakpoint_delete(const char* command);

        string handle_exception(const char*);
        string handle_thread(const char*);
        string handle_vcont(const char*);

        // disabled
        gdbserver();
        gdbserver(const gdbserver&);

    public:
        gdb_status get_status() const { return m_status; }

        void sync(bool b = true) { m_sync = b; }

        gdbserver(u16 port, gdbstub* stub, gdb_status status = GDB_STOPPED);
        virtual ~gdbserver();

        void simulate(unsigned int& cycles);
        void notify(int signal);

        virtual string handle_command(const string& command) override;
        virtual void   handle_connect(const char* peer) override;
        virtual void   handle_disconnect() override;
    };

    inline void gdbserver::notify(int signal) {
        m_status = GDB_STOPPED;
        m_signal = signal;
    }

    inline gdbserver::handler gdbserver::find_handler(const char* command) {
        if (!stl_contains(m_handler, command[0]))
            return &gdbserver::handle_unknown;
        return m_handler.at(command[0]);
    }

}}

#endif
