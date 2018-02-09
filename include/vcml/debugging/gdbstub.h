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

namespace vcml { namespace debugging {

    class gdbstub
    {
    public:
        virtual ~gdbstub() {}

        virtual u64  gdb_num_registers() = 0;
        virtual u64  gdb_register_width() = 0;

        virtual bool gdb_read_reg(u64 idx, void* buffer, u64 size) = 0;
        virtual bool gdb_write_reg(u64 idx, const void* buffer, u64 size) = 0;

        virtual bool gdb_page_size(u64& size) = 0;
        virtual bool gdb_virt_to_phys(u64 vaddr, u64& paddr) = 0;

        virtual bool gdb_read_mem(u64 addr, void* buffer, u64 size) = 0;
        virtual bool gdb_write_mem(u64 addr, const void* buffer, u64 size) = 0;

        virtual bool gdb_insert_breakpoint(u64 addr) = 0;
        virtual bool gdb_remove_breakpoint(u64 addr) = 0;

        virtual string gdb_handle_rcmd(const string& command) = 0;

        virtual void gdb_simulate(unsigned int& cycles) = 0;
        virtual void gdb_notify(int signal) = 0;

    public:
        u64  async_num_registers();
        u64  async_register_width();

        bool async_read_reg(u64 idx, void* buffer, u64 size);
        bool async_write_reg(u64 idx, const void* buffer, u64 size);

        bool async_page_size(u64& size);
        bool async_virt_to_phys(u64 vaddr, u64& paddr);

        bool async_read_mem(u64 addr, void* buffer, u64 size);
        bool async_write_mem(u64 addr, const void* buffer, u64 size);

        bool async_insert_breakpoint(u64 addr);
        bool async_remove_breakpoint(u64 addr);

        string async_handle_rcmd(const string& command);
    };

    inline u64 gdbstub::async_num_registers() {
        thctl_sysc_pause();
        u64 result = gdb_num_registers();
        thctl_sysc_resume();
        return result;
    }

    inline u64 gdbstub::async_register_width() {
        thctl_sysc_pause();
        u64 result = gdb_register_width();
        thctl_sysc_resume();
        return result;
    }

    inline bool gdbstub::async_read_reg(u64 idx, void* buffer, u64 size) {
        thctl_sysc_pause();
        bool result = gdb_read_reg(idx, buffer, size);
        thctl_sysc_resume();
        return result;
    }

    inline bool gdbstub::async_write_reg(u64 idx, const void* buffer, u64 sz) {
        thctl_sysc_pause();
        bool result = gdb_write_reg(idx, buffer, sz);
        thctl_sysc_resume();
        return result;
    }

    inline bool gdbstub::async_page_size(u64& size) {
        thctl_sysc_pause();
        bool result = gdb_page_size(size);
        thctl_sysc_resume();
        return result;
    }

    inline bool gdbstub::async_virt_to_phys(u64 vaddr, u64& paddr) {
        thctl_sysc_pause();
        bool result = gdb_virt_to_phys(vaddr, paddr);
        thctl_sysc_resume();
        return result;
    }

    inline bool gdbstub::async_read_mem(u64 addr, void* buffer, u64 size) {
        thctl_sysc_pause();
        bool result = gdb_read_mem(addr, buffer, size);
        thctl_sysc_resume();
        return result;
    }

    inline bool gdbstub::async_write_mem(u64 addr, const void* buf, u64 sz) {
        thctl_sysc_pause();
        bool result = gdb_write_mem(addr, buf, sz);
        thctl_sysc_resume();
        return result;
    }

    inline bool gdbstub::async_insert_breakpoint(u64 addr) {
        thctl_sysc_pause();
        bool result = gdb_insert_breakpoint(addr);
        thctl_sysc_resume();
        return result;
    }

    inline bool gdbstub::async_remove_breakpoint(u64 addr) {
        thctl_sysc_pause();
        bool result = gdb_remove_breakpoint(addr);
        thctl_sysc_resume();
        return result;
    }

    inline string gdbstub::async_handle_rcmd(const string& command) {
        thctl_sysc_pause();
        string response = gdb_handle_rcmd(command);
        thctl_sysc_resume();
        return response;
    }

}}

#endif
