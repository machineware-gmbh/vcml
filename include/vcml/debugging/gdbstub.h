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

#ifndef VCML_GDBSTUB_H
#define VCML_GDBSTUB_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/thctl.h"

#include "vcml/range.h"

namespace vcml { namespace debugging {

    class gdbstub
    {
    public:
        virtual ~gdbstub() {}

        virtual u64  gdb_num_registers() = 0;
        virtual u64  gdb_register_width(u64 idx) = 0;

        virtual bool gdb_read_reg(u64 idx, void* buffer, u64 size) = 0;
        virtual bool gdb_write_reg(u64 idx, const void* buffer, u64 size) = 0;

        virtual bool gdb_page_size(u64& size) = 0;
        virtual bool gdb_virt_to_phys(u64 vaddr, u64& paddr) = 0;

        virtual bool gdb_read_mem(u64 addr, void* buffer, u64 size) = 0;
        virtual bool gdb_write_mem(u64 addr, const void* buffer, u64 size) = 0;

        virtual bool gdb_insert_breakpoint(u64 addr) = 0;
        virtual bool gdb_remove_breakpoint(u64 addr) = 0;

        virtual bool gdb_insert_watchpoint(const range& m, vcml_access a) = 0;
        virtual bool gdb_remove_watchpoint(const range& m, vcml_access a) = 0;

        virtual string gdb_handle_rcmd(const string& command) = 0;

        virtual void gdb_simulate(unsigned int cycles) = 0;
        virtual void gdb_notify(int signal) = 0;

    public:
        u64  async_num_registers();
        u64  async_register_width(u64 idx);

        bool async_read_reg(u64 idx, void* buffer, u64 size);
        bool async_write_reg(u64 idx, const void* buffer, u64 size);

        bool async_page_size(u64& size);
        bool async_virt_to_phys(u64 vaddr, u64& paddr);

        bool async_read_mem(u64 addr, void* buffer, u64 size);
        bool async_write_mem(u64 addr, const void* buffer, u64 size);

        bool async_insert_breakpoint(u64 addr);
        bool async_remove_breakpoint(u64 addr);

        bool async_insert_watchpoint(const range& m, vcml_access a);
        bool async_remove_watchpoint(const range& m, vcml_access a);

        string async_handle_rcmd(const string& command);
    };

    inline u64 gdbstub::async_num_registers() {
        thctl_lock lock;
        return gdb_num_registers();
    }

    inline u64 gdbstub::async_register_width(u64 idx) {
        thctl_lock lock;
        return gdb_register_width(idx);
    }

    inline bool gdbstub::async_read_reg(u64 idx, void* buffer, u64 size) {
        thctl_lock lock;
        return gdb_read_reg(idx, buffer, size);
    }

    inline bool gdbstub::async_write_reg(u64 idx, const void* buffer, u64 sz) {
        thctl_lock lock;
        return gdb_write_reg(idx, buffer, sz);
    }

    inline bool gdbstub::async_page_size(u64& size) {
        thctl_lock lock;
        return gdb_page_size(size);
    }

    inline bool gdbstub::async_virt_to_phys(u64 vaddr, u64& paddr) {
        thctl_lock lock;
        return gdb_virt_to_phys(vaddr, paddr);
    }

    inline bool gdbstub::async_read_mem(u64 addr, void* buffer, u64 size) {
        thctl_lock lock;
        return gdb_read_mem(addr, buffer, size);
    }

    inline bool gdbstub::async_write_mem(u64 addr, const void* buf, u64 sz) {
        thctl_lock lock;
        return gdb_write_mem(addr, buf, sz);
    }

    inline bool gdbstub::async_insert_breakpoint(u64 addr) {
        thctl_lock lock;
        return gdb_insert_breakpoint(addr);
    }

    inline bool gdbstub::async_remove_breakpoint(u64 addr) {
        thctl_lock lock;
        return gdb_remove_breakpoint(addr);
    }

    inline bool gdbstub::async_insert_watchpoint(const range& address,
                                                 vcml_access acs) {
        thctl_lock lock;
        return gdb_insert_watchpoint(address, acs);
    }

    inline bool gdbstub::async_remove_watchpoint(const range& address,
                                                 vcml_access acs) {
        thctl_lock lock;
        return gdb_remove_watchpoint(address, acs);
    }

    inline string gdbstub::async_handle_rcmd(const string& command) {
        thctl_lock lock;
        return gdb_handle_rcmd(command);
    }

}}

#endif
