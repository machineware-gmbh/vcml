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

#include "vcml/debugging/gdbserver.h"

namespace vcml { namespace debugging {

    static inline int char2int(char c) {
        return ((c >= 'a') && (c <= 'f')) ? c - 'a' + 10 :
               ((c >= 'A') && (c <= 'F')) ? c - 'A' + 10 :
               ((c >= '0') && (c <= '9')) ? c - '0' :
               (c == '\0') ? 0 : -1;
    }

    static inline u64 str2int(const char* s, int n) {
        u64 val = 0;
        for (const char* c = s + n - 1; c >= s; c--) {
            val <<= 4;
            val |= char2int(*c);
        }
        return val;
    }

    static inline u8 char_unescape(const char*& s) {
        u8 result = *s++;
        if (result == '}')
            result = *s++ ^ 0x20;
        return result;
    }

    void gdbserver::access_pmem(bool iswr, u64 addr, u8 buffer[], u64 size) {
        thctl_sysc_pause();

        try {
            bool result = iswr ? m_stub->async_write_mem(addr, buffer, size)
                               : m_stub->async_read_mem(addr, buffer, size);
            VCML_ERROR_ON(!result, "bus error");
        } catch (report& r) {
            log_warn("gdb cannot access %d bytes at address %x: %s",
                     size, addr, r.message());
            memset(buffer, 0xee, size);
        }

        thctl_sysc_resume();
    }

    void gdbserver::access_vmem(bool iswr, u64 addr, u8 buffer[], u64 size) {
        u64 page_size = 0;
        if (!m_stub->async_page_size(page_size)) {
            access_pmem(iswr, addr, buffer, size);
            return;
        }

        u64 end = addr + size;
        while (addr < end) {
            u64 pa = 0;
            u64 todo = min(end - addr, page_size - (addr % page_size));
            if (m_stub->async_virt_to_phys(addr, pa)) {
                access_pmem(iswr, pa, buffer, todo);
            } else {
                memset(buffer, 0xee, todo);
            }

            addr += todo;
            buffer += todo;
        }
    }

    string gdbserver::handle_unknown(const char* command) {
        return "";
    }

    string gdbserver::handle_step(const char* command) {
        int signal = 0;
        m_status = GDB_STEPPING;
        while (m_status == GDB_STEPPING) {
            if ((signal = recv_signal(100))) {
                log_debug("received signal 0x%x", signal);
                m_status = GDB_STOPPED;
                m_signal = GDBSIG_BREAKPOINT;
            }
        }

        return mkstr("S%02x", m_signal);
    }

    string gdbserver::handle_continue(const char* command) {
        int signal = 0;
        m_status = GDB_RUNNING;
        while (m_status == GDB_RUNNING) {
            if ((signal = recv_signal(100))) {
                log_debug("received signal 0x%x", signal);
                m_status = GDB_STOPPED;
                m_signal = GDBSIG_BREAKPOINT;
            }
        }

        return mkstr("S%02x", m_signal);
    }

    string gdbserver::handle_detach(const char* command) {
        m_status = m_default;
        disconnect();
        return "";
    }

    string gdbserver::handle_kill(const char* command) {
        m_status = m_default;
        disconnect();
        sc_core::sc_stop();
        return "";
    }

    string gdbserver::handle_query(const char* command) {
        if (strncmp(command, "qSupported", strlen("qSupported")) == 0)
            return mkstr("PacketSize=%x", VCML_RSP_MAX_PACKET_SIZE);
        else if (strncmp(command, "qAttached", strlen("qAttached")) == 0)
            return "1";
        else if (strncmp(command, "qOffsets", strlen("qOffsets")) == 0)
            return "Text=0;Data=0;Bss=0";
        else if (strncmp(command, "qRcmd", strlen("qRcmd")) == 0)
            return handle_rcmd(command);
        else
            return handle_unknown(command);
    }

    string gdbserver::handle_rcmd(const char* command) {
        return m_stub->async_handle_rcmd(command);
    }

    string gdbserver::handle_reg_read(const char* command) {
        unsigned int reg;
        if (sscanf(command, "p%x", &reg) != 1)
            VCML_ERROR("malformed command '%s'", command);

        u64 nregs = m_stub->async_num_registers();
        u64 regsz = m_stub->async_register_width();

        if (reg >= nregs)
            VCML_ERROR("malformed command '%s'", command);

        u8 buffer[VCML_GDBSERVER_BUFSIZE];
        if (!m_stub->async_read_reg(reg, buffer, regsz)) {
            memset(buffer, 'x', regsz);
            log_warn("gdb failed to read register %d", reg);
        }

        stringstream ss;
        ss << std::hex << std::setfill('0');
        for (unsigned int byte = 0; byte < regsz; byte++)
            ss << std::setw(2) << (int)buffer[byte];

        return ss.str();
    }

    string gdbserver::handle_reg_write(const char* command) {
        unsigned int reg;
        if (sscanf(command, "P%x=", &reg) != 1)
            VCML_ERROR("malformed command '%s'", command);

        u64 nregs = m_stub->async_num_registers();
        u64 regsz = m_stub->async_register_width();

        if (reg >= nregs)
            VCML_ERROR("malformed command '%s'", command);

        const char* str = strchr(command, '=');
        if (str == NULL)
            VCML_ERROR("malformed command '%s'", command);
        str++;

        if (strlen(str) != regsz * 2)
            VCML_ERROR("malformed command '%s'", command);

        u8 buffer[VCML_GDBSERVER_BUFSIZE];
        for (unsigned int byte = 0; byte < regsz; byte++, str += 2)
            buffer[byte] = char2int(str[0]) << 4 || char2int(str[1]);

        if (!m_stub->async_write_reg(reg, buffer, regsz))
            log_warn("gdb failed to write register %d", reg);

        return "OK";
    }

    string gdbserver::handle_reg_read_all(const char* command) {
        u64 nregs = m_stub->async_num_registers();
        u64 regsz = m_stub->async_register_width();

        stringstream ss;
        ss << std::hex << std::setfill('0');

        u8 buffer[VCML_GDBSERVER_BUFSIZE];
        for (u64 reg = 0; reg < nregs; reg++) {
            if (!m_stub->async_read_reg(reg, buffer, regsz)) {
                log_warn("gdb cannot read register %d", reg);
                memset(buffer, 'x', regsz);
            }

            for (u64 byte = 0; byte < regsz; byte++)
                ss << std::setw(2) << (int)buffer[byte];
        }

        return ss.str();
    }

    string gdbserver::handle_reg_write_all(const char* command) {
        u64 nregs = m_stub->async_num_registers();
        u64 regsz = m_stub->async_register_width();

        u64 bufsz = nregs * regsz * 2;
        const char* str = command + 1;

        if (strlen(str) != bufsz)
            VCML_ERROR("malformed command '%s'", command);

        u8 buffer[VCML_GDBSERVER_BUFSIZE];
        for (u64 reg = 0; reg < nregs; reg++) {
            for (u64 byte = 0; byte < regsz; byte++, str += 2)
                buffer[byte] = char2int(str[0]) << 4 || char2int(str[1]);
            if (!m_stub->async_write_reg(reg, buffer, regsz))
                log_warn("gdb cannot write register %d", reg);
        }

        return "OK";
    }

    string gdbserver::handle_mem_read(const char* command) {
        unsigned int addr, size;
        if (sscanf(command, "m%x,%x", &addr, &size) != 2)
            VCML_ERROR("malformed command '%s'", command);

        if (size > VCML_GDBSERVER_BUFSIZE)
            VCML_ERROR("too much data requested: % bytes", size);

        stringstream ss;
        ss << std::hex << std::setfill('0');

        u8 buffer[VCML_GDBSERVER_BUFSIZE];
        access_vmem(false, addr, buffer, size);

        for (unsigned int i = 0; i < size; i++)
            ss << std::setw(2) << (int)buffer[i];

        return ss.str();
    }

    string gdbserver::handle_mem_write(const char* command) {
        unsigned int addr, size;
        if (sscanf(command, "M%x,%x", &addr, &size) != 2)
            VCML_ERROR("malformed command '%s'", command);

        if (size > VCML_GDBSERVER_BUFSIZE)
            VCML_ERROR("too much data requested: % bytes", size);

        const char* data = strchr(command, ':');
        if (data == NULL)
            VCML_ERROR("malformed command '%s'", command);

        data++;

        u8 buffer[VCML_GDBSERVER_BUFSIZE];
        for (unsigned int i = 0; i < size; i++)
            buffer[i] = str2int(data++, 2);

        access_vmem(true, addr, buffer, size);
        return "OK";
    }

    string gdbserver::handle_mem_write_bin(const char* command) {
        unsigned int addr, size;
        if (sscanf(command, "X%x,%x:", &addr, &size) != 2)
            VCML_ERROR("malformed command '%s'", command);

        if (size > VCML_GDBSERVER_BUFSIZE)
            VCML_ERROR("too much data requested: % bytes", size);

        if (size == 0)
            return "OK"; // empty load to test if binary write is supported

        const char* data = strchr(command, ':');
        if (data == NULL)
            VCML_ERROR("malformed command '%s'", command);
        data++;

        u8 buffer[VCML_GDBSERVER_BUFSIZE];
        for (unsigned int i = 0; i < size; i++)
            buffer[i] = char_unescape(data);

        access_vmem(true, addr, buffer, size);
        return "OK";
    }

    string gdbserver::handle_breakpoint_set(const char* command) {
        unsigned int type, addr, length;
        if (sscanf(command, "Z%x,%x,%x", &type, &addr, &length) != 3)
            VCML_ERROR("malformed command '%s'", command);

        // ToDO: we assume type = r/w breakpoint and length = 4...
        m_stub->async_insert_breakpoint(addr);

        return "OK";
    }

    string gdbserver::handle_breakpoint_delete(const char* command) {
        unsigned int type, addr, length;
        if (sscanf(command, "z%x,%x,%x", &type, &addr, &length) != 3)
            VCML_ERROR("malformed command '%s'", command);

        // ToDO: we assume type = r/w breakpoint and length = 4...
        m_stub->async_remove_breakpoint(addr);

        return "OK";
    }

    string gdbserver::handle_exception(const char* command) {
        return mkstr("S%02u", SIGTRAP);
    }

    string gdbserver::handle_thread(const char* command) {
        return "OK";
    }

    string gdbserver::handle_vcont(const char* command) {
        return "";
    }

    gdbserver::gdbserver(u16 port, gdbstub* stub, gdb_status status):
        rspserver(port),
        m_stub(stub),
        m_status(status),
        m_default(status),
        m_signal(-1),
        m_sync(false),
        m_handler() {
        VCML_ERROR_ON(!stub, "no debug stub given");

        m_handler['q'] = &gdbserver::handle_query;

        m_handler['s'] = &gdbserver::handle_step;
        m_handler['c'] = &gdbserver::handle_continue;
        m_handler['D'] = &gdbserver::handle_detach;
        m_handler['k'] = &gdbserver::handle_kill;

        m_handler['p'] = &gdbserver::handle_reg_read;
        m_handler['P'] = &gdbserver::handle_reg_write;
        m_handler['g'] = &gdbserver::handle_reg_read_all;
        m_handler['G'] = &gdbserver::handle_reg_write_all;

        m_handler['m'] = &gdbserver::handle_mem_read;
        m_handler['M'] = &gdbserver::handle_mem_write;
        m_handler['X'] = &gdbserver::handle_mem_write_bin;

        m_handler['Z'] = &gdbserver::handle_breakpoint_set;
        m_handler['z'] = &gdbserver::handle_breakpoint_delete;

        m_handler['H'] = &gdbserver::handle_thread;
        m_handler['v'] = &gdbserver::handle_vcont;
        m_handler['?'] = &gdbserver::handle_exception;

        run_async();
    }

    gdbserver::~gdbserver() {
        /* nothing to do */
    }

    void gdbserver::simulate(unsigned int& cycles) {
        switch (m_status) {
        case GDB_STOPPED:
            if (m_sync)
                cycles = 0;
            return;

        case GDB_STEPPING: {
            unsigned int one = 1;
            m_stub->gdb_simulate(one);
            notify(GDBSIG_BREAKPOINT);
            if (m_sync)
                cycles = 1;
            return;
        }

        case GDB_RUNNING:
        default:
            m_stub->gdb_simulate(cycles);
            return;
        }
    }

    string gdbserver::handle_command(const string& command) {
        try {
            handler func = find_handler(command.c_str());
            return (this->*func)(command.c_str());
        } catch (report& rep) {
            vcml::logger::log(rep);
            return mkstr("E%02x", VCML_GDBSERVER_ERR_INTERNAL);
        } catch (std::exception& ex) {
            log_warn(ex.what());
            return mkstr("E%02x", VCML_GDBSERVER_ERR_INTERNAL);
        }
    }

    void gdbserver::handle_connect(const char* peer) {
        log_debug("gdb connected to %s", peer);
        m_status = GDB_STOPPED;
    }

    void gdbserver::handle_disconnect() {
        log_debug("gdb disconnected");
        m_status = m_default;
    }

}}
