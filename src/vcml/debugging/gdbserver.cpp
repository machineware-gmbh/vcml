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

#include <signal.h> // for SIGTRAP
#include "vcml/debugging/gdbserver.h"

namespace vcml { namespace debugging {

    union gdb_u64 {
        u64 val;
        u8 ptr[8];

        gdb_u64(): val() {}
    };

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

    void gdbserver::update_status(gdb_status status) {
        if (m_status == status)
            return;

        m_status = status;
        resume();
    }

    bool gdbserver::is_suspend_requested() const {
        if (!m_sync)
            return false;
        return m_status == GDB_STOPPED;
    }

    const cpureg* gdbserver::lookup_cpureg(unsigned int gdbno) {
        auto it = m_regmap.find(gdbno);
        if (it == m_regmap.end())
            return nullptr;
        return it->second;
    }

    string gdbserver::handle_unknown(const char* command) {
        return "";
    }

    string gdbserver::handle_step(const char* command) {
        int signal = 0;
        update_status(GDB_STEPPING);
        while (m_status == GDB_STEPPING) {
            if ((signal = recv_signal(100))) {
                log_debug("received signal 0x%x", signal);
                m_status = GDB_STOPPED;
                m_signal = GDBSIG_TRAP;
                wait_for_suspend();
            }
        }

        return mkstr("S%02x", m_signal);
    }

    string gdbserver::handle_continue(const char* command) {
        int signal = 0;
        update_status(GDB_RUNNING);
        while (m_status == GDB_RUNNING) {
            if ((signal = recv_signal(100))) {
                log_debug("received signal 0x%x", signal);
                m_status = GDB_STOPPED;
                m_signal = GDBSIG_TRAP;
                wait_for_suspend();
            }
        }

        return mkstr("S%02x", m_signal);
    }

    string gdbserver::handle_detach(const char* command) {
        disconnect();
        return "";
    }

    string gdbserver::handle_kill(const char* command) {
        disconnect();
        update_status(GDB_KILLED);
        sc_stop();
        return "";
    }

    string gdbserver::handle_query(const char* command) {
        if (strncmp(command, "qSupported", strlen("qSupported")) == 0)
            return mkstr("PacketSize=%zx", PACKET_SIZE);
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
        string response;
        if (!m_target->gdb_command(command, response))
            return ERR_COMMAND;
        return response;
    }

    string gdbserver::handle_reg_read(const char* command) {
        unsigned int regno;
        if (sscanf(command, "p%x", &regno) != 1) {
            log_warn("malformed command '%s'", command);
            return ERR_COMMAND;
        }

        const cpureg* reg = lookup_cpureg(regno);
        if (reg == nullptr || !reg->is_readable())
            return "xxxxxxxx"; // respond with "contents unknown"

        u64 val = reg->read();
        if (!m_target->is_host_endian())
            memswap(&val, reg->size);

        stringstream ss;
        ss << std::hex << std::setfill('0');
        for (u32 shift = 0; shift < reg->width(); shift += 8)
            ss << std::setw(2) << ((val >> shift) & 0xff);

        return ss.str();
    }

    string gdbserver::handle_reg_write(const char* command) {
        gdb_u64 val;
        unsigned int regno;

        if (sscanf(command, "P%x=", &regno) != 1) {
            log_warn("malformed command '%str'", command);
            return ERR_COMMAND;
        }

        const cpureg* reg = lookup_cpureg(regno);
        if (reg == nullptr) {
            log_warn("unknown register id: %u", regno);
            return "OK";
        }

        const char* str = strrchr(command, '=');
        if (str == nullptr || strlen(str + 1) != reg->size  * 2) {
            log_warn("malformed command '%s'", command);
            return ERR_COMMAND;
        }

        str++; // step beyond '='

        for (u64 i = 0; i < reg->size; i++, str+= 2) {
            if (sscanf(str, "%02hhx", val.ptr + i) != 1) {
                log_warn("error parsing register value near %s", str);
                return ERR_COMMAND;
            }
        }

        if (!m_target->is_host_endian())
            memswap(val.ptr, reg->size);

        reg->write(val.val);
        return "OK";
    }

    string gdbserver::handle_reg_read_all(const char* command) {
        stringstream ss;
        ss << std::hex << std::setfill('0');
        u64 nregs = m_regmap.size();

        for (u64 regno = 0; regno < nregs; regno++) {
            const cpureg* reg = lookup_cpureg(regno);
            if (reg == nullptr || !reg->is_readable())
                continue;

            u64 val = reg->read();
            if (!m_target->is_host_endian())
                memswap(&val, reg->size);

            for (u32 shift = 0; shift < reg->width(); shift += 8)
                ss << std::setw(2) << ((val >> shift) & 0xff);
        }

        return ss.str();
    }

    string gdbserver::handle_reg_write_all(const char* command) {
        u64 nregs = m_regmap.size();
        const char* str = command + 1;

        for (u64 regno = 0; regno < nregs; regno++) {
            const cpureg* reg = m_target->find_cpureg(regno);
            if (reg == nullptr || !reg->is_writeable())
                continue;

            if (strlen(str) < reg->size * 2) {
                log_warn("malformed command '%s'", command);
                return ERR_COMMAND;
            }

            gdb_u64 val;
            for (u64 byte = 0; byte < reg->size; byte++, str += 2)
                sscanf(str, "%02hhx", val.ptr + byte);

            if (!m_target->is_host_endian())
                memswap(val.ptr, reg->size);

            reg->write(val.val);
        }

        return "OK";
    }

    string gdbserver::handle_mem_read(const char* command) {
        unsigned long long addr, size;
        if (sscanf(command, "m%llx,%llx", &addr, &size) != 2) {
            log_warn("malformed command '%s'", command);
            return ERR_COMMAND;
        }

        if (size > BUFFER_SIZE) {
            log_warn("too much data requested: %llu bytes", size);
            return ERR_PARAM;
        }

        stringstream ss;
        ss << std::hex << std::setfill('0');

        u8 buffer[BUFFER_SIZE];
        if (m_target->read_vmem_dbg(addr, buffer, size) != size)
            return ERR_UNKNOWN;

        for (unsigned int i = 0; i < size; i++)
            ss << std::setw(2) << (int)buffer[i];

        return ss.str();
    }

    string gdbserver::handle_mem_write(const char* command) {
        unsigned long long addr, size;
        if (sscanf(command, "M%llx,%llx", &addr, &size) != 2) {
            log_warn("malformed command '%s'", command);
            return ERR_COMMAND;
        }

        if (size > BUFFER_SIZE) {
            log_warn("too much data requested: %llu bytes", size);
            return ERR_PARAM;
        }

        const char* data = strchr(command, ':');
        if (data == NULL) {
            log_warn("malformed command '%s'", command);
            return ERR_COMMAND;
        }

        data++;

        u8 buffer[BUFFER_SIZE];
        for (unsigned int i = 0; i < size; i++)
            buffer[i] = str2int(data++, 2);

        if (m_target->write_vmem_dbg(addr, buffer, size) != size)
            return ERR_UNKNOWN;

        return "OK";
    }

    string gdbserver::handle_mem_write_bin(const char* command) {
        unsigned long long addr, size;
        if (sscanf(command, "X%llx,%llx:", &addr, &size) != 2) {
            log_warn("malformed command '%s'", command);
            return ERR_COMMAND;
        }

        if (size > BUFFER_SIZE) {
            log_warn("too much data requested: %llu bytes", size);
            return ERR_PARAM;
        }

        if (size == 0)
            return "OK"; // empty load to test if binary write is supported

        const char* data = strchr(command, ':');
        if (data == NULL) {
            log_warn("malformed command '%s'", command);
            return ERR_COMMAND;
        }

        data++;

        u8 buffer[BUFFER_SIZE];
        for (unsigned int i = 0; i < size; i++)
            buffer[i] = char_unescape(data);

        if (m_target->write_vmem_dbg(addr, buffer, size) != size)
            return ERR_UNKNOWN;

        return "OK";
    }

    string gdbserver::handle_breakpoint_set(const char* command) {
        unsigned long long type, addr, length;
        if (sscanf(command, "Z%llx,%llx,%llx", &type, &addr, &length) != 3) {
            log_warn("malformed command '%s'", command);
            return ERR_COMMAND;
        }

        const range mem(addr, addr + length - 1);
        switch (type) {
        case GDB_BREAKPOINT_SW:
        case GDB_BREAKPOINT_HW:
            if (!m_target->insert_breakpoint(addr))
                return ERR_INTERNAL;
            break;

        case GDB_WATCHPOINT_WRITE:
            if (!m_target->insert_watchpoint(mem, VCML_ACCESS_WRITE))
                return ERR_INTERNAL;
            break;

        case GDB_WATCHPOINT_READ:
            if (!m_target->insert_watchpoint(mem, VCML_ACCESS_READ))
                return ERR_INTERNAL;
            break;


        case GDB_WATCHPOINT_ACCESS:
            if (!m_target->insert_watchpoint(mem, VCML_ACCESS_READ_WRITE))
                return ERR_INTERNAL;
            break;

        default:
            log_warn("unknown breakpoint type %llu", type);
            return ERR_COMMAND;
        }

        return "OK";
    }

    string gdbserver::handle_breakpoint_delete(const char* command) {
        unsigned long long type, addr, length;
        if (sscanf(command, "z%llx,%llx,%llx", &type, &addr, &length) != 3) {
            log_warn("malformed command '%s'", command);
            return ERR_COMMAND;
        }

        const range mem(addr, addr + length - 1);
        switch (type) {
        case GDB_BREAKPOINT_SW:
        case GDB_BREAKPOINT_HW:
            if (!m_target->remove_breakpoint(addr))
                return ERR_INTERNAL;
            break;

        case GDB_WATCHPOINT_WRITE:
            if (!m_target->remove_watchpoint(mem, VCML_ACCESS_WRITE))
                return ERR_INTERNAL;
            break;

        case GDB_WATCHPOINT_READ:
            if (!m_target->remove_watchpoint(mem, VCML_ACCESS_READ))
                return ERR_INTERNAL;
            break;

        case GDB_WATCHPOINT_ACCESS:
            if (!m_target->remove_watchpoint(mem, VCML_ACCESS_READ_WRITE))
                return ERR_INTERNAL;
            break;

        default:
            log_warn("unknown breakpoint type %llu", type);
            return ERR_COMMAND;
        }

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

    gdbserver::gdbserver(u16 port, target* stub, gdb_status status):
        rspserver(port),
        suspender("gdbserver"),
        m_target(stub),
        m_status(status),
        m_default(status),
        m_regmap(),
        m_sync(true),
        m_signal(-1),
        m_handler() {
        VCML_ERROR_ON(!stub, "no debug stub given");

        vector<string> gdbregs;
        m_target->gdb_collect_regs(gdbregs);
        if (gdbregs.empty())
            VCML_ERROR("target does not define any gdb registers");

        for (auto gdbreg : gdbregs) {
            const cpureg* reg = m_target->find_cpureg(gdbreg);
            VCML_ERROR_ON(!reg, "register %s not found", gdbreg.c_str());
            m_regmap.insert({m_regmap.size(), reg});
        }

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

    void gdbserver::simulate(unsigned int cycles) {
        while (cycles > 0) {
            suspender::handle_requests();

            switch (m_status) {
            case GDB_KILLED:
                return;

            case GDB_STOPPED:
                return;

            case GDB_STEPPING:
                m_target->gdb_simulate(1);
                notify(GDBSIG_TRAP);
                cycles--;
                break;

            case GDB_RUNNING:
            default:
                m_target->gdb_simulate(cycles);
                cycles = 0;
                break;
            }
        }
    }

    string gdbserver::handle_command(const string& command) {
        try {
            handler func = find_handler(command.c_str());
            return (this->*func)(command.c_str());
        } catch (report& rep) {
            vcml::logger::log(rep);
            return ERR_INTERNAL;
        } catch (std::exception& ex) {
            log_warn("%s", ex.what());
            return ERR_INTERNAL;
        }
    }

    void gdbserver::handle_connect(const char* peer) {
        log_debug("gdb connected to %s", peer);
        update_status(GDB_STOPPED);
    }

    void gdbserver::handle_disconnect() {
        log_debug("gdb disconnected");
        update_status(m_default);
    }

}}
