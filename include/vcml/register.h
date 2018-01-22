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

#ifndef VCML_REGISTER_H
#define VCML_REGISTER_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"

#include "vcml/logging/logger.h"
#include "vcml/backends/backend.h"
#include "vcml/properties/property.h"

#include "vcml/range.h"
#include "vcml/txext.h"
#include "vcml/dmi_cache.h"
#include "vcml/component.h"
#include "vcml/register.h"

namespace vcml {

    class peripheral;

    class reg_base: public sc_object
    {
    private:
        range       m_range;
        vcml_access m_access;
        peripheral* m_host;

    public:
        inline u64 get_address() const { return m_range.start; }
        inline u64 get_size() const { return m_range.length(); }
        inline const range& get_range() const { return m_range; }

        inline vcml_access get_access() const { return m_access; }
        inline void set_access(vcml_access a) { m_access = a; }

        inline bool is_read_only() const;
        inline bool is_write_only() const;

        inline bool is_readable() const { return is_read_allowed(m_access); }
        inline bool is_writeable() const { return is_write_allowed(m_access); }

        inline void allow_read() { m_access = VCML_ACCESS_READ; }
        inline void allow_write() { m_access = VCML_ACCESS_WRITE; }
        inline void allow_read_write() { m_access = VCML_ACCESS_READ_WRITE; }

        inline peripheral* get_host() { return m_host; }

        reg_base(const char* nm, u64 addr, u64 size, peripheral* host = NULL);
        virtual ~reg_base();

        VCML_KIND(reg_base);

        unsigned int receive(tlm_generic_payload& tx, int flags);

        virtual void do_read(const range& addr, int bank, void* ptr) = 0;
        virtual void do_write(const range& addr, int bank, const void* ptr) = 0;
    };

    inline bool reg_base::is_read_only() const {
        return m_access == VCML_ACCESS_READ;
    }

    inline bool reg_base::is_write_only() const {
        return m_access == VCML_ACCESS_WRITE;
    }

    template <class HOST, typename DATA>
    class reg: public reg_base,
               public property<DATA>
    {
    private:
        HOST* m_host;
        bool m_banked;
        std::map<int, DATA> m_banks;
        int m_current_bank;

        void load_bank(int bank);

    public:
        typedef DATA (HOST::*readfunc)  (void);
        typedef DATA (HOST::*writefunc) (DATA);

        readfunc read;
        writefunc write;

        typedef DATA (HOST::*tagged_readfunc)  (unsigned int);
        typedef DATA (HOST::*tagged_writefunc) (DATA, unsigned int);

        unsigned int tag;

        tagged_readfunc tagged_read;
        tagged_writefunc tagged_write;

        bool is_banked() const { return m_banked; }
        void set_banked(bool set = true) { m_banked = set; }

        reg(const char* nm, u64 addr, const DATA& init = DATA(),
            HOST* host = NULL);
        virtual ~reg();

        VCML_KIND(reg);

        virtual void do_read(const range& addr, int bank, void* ptr);
        virtual void do_read(const range& addr, DATA* ptr);

        virtual void do_write(const range& addr, int bank, const void* ptr);
        virtual void do_write(const range& addr, const DATA* ptr);

        template <typename T> reg<HOST, DATA>& operator =  (const T& value);
        template <typename T> reg<HOST, DATA>& operator |= (const T& value);
        template <typename T> reg<HOST, DATA>& operator &= (const T& value);
        template <typename T> reg<HOST, DATA>& operator += (const T& value);
        template <typename T> reg<HOST, DATA>& operator -= (const T& value);
        template <typename T> reg<HOST, DATA>& operator *= (const T& value);
        template <typename T> reg<HOST, DATA>& operator /= (const T& value);
    };

    template <class HOST, typename DATA>
    void reg<HOST, DATA>::load_bank(int bank) {
        if (!m_banked || bank == m_current_bank)
            return;

        if (!stl_contains(m_banks, bank))
            m_banks[bank] = property<DATA>::get_default();
        m_banks[m_current_bank] = property<DATA>::get();
        m_current_bank = bank;
        property<DATA>::set(m_banks[m_current_bank]);
    }

    template <class HOST, typename DATA>
    reg<HOST, DATA>::reg(const char* nm, u64 addr, const DATA& init, HOST* h):
        reg_base(nm, addr, sizeof(DATA), h),
        property<DATA>(nm, init, h),
        m_host(h),
        m_banked(false),
        m_banks(),
        m_current_bank(ext_bank::NONE),
        read(NULL),
        write(NULL),
        tag(0),
        tagged_read(NULL),
        tagged_write(NULL) {
        m_banks[m_current_bank] = property<DATA>::get();
        if (m_host == NULL)
            m_host = dynamic_cast<HOST*>(get_host());
    }

    template <class HOST, typename DATA>
    reg<HOST, DATA>::~reg() {
        /* nothing to do */
    }

    template <class HOST, typename DATA>
    void reg<HOST, DATA>::do_read(const range& addr, int bank, void* ptr) {
        load_bank(bank);
        do_read(addr, static_cast<DATA*>(ptr));
    }

    template <class HOST, typename DATA>
    void reg<HOST, DATA>::do_write(const range& a, int b, const void* ptr) {
        load_bank(b);
        do_write(a, static_cast<const DATA*>(ptr));
    }

    template <class HOST, typename DATA>
    void reg<HOST, DATA>::do_read(const range& addr, DATA* data) {
        DATA val = property<DATA>::get();

        if (tagged_read != NULL)
            val = (m_host->*tagged_read)(tag);
        else if (read != NULL)
            val = (m_host->*read)();

        property<DATA>::set(val);

        unsigned char* ptr = (unsigned char*)&val + addr.start - get_address();
        memcpy(data, ptr, addr.length());
    }

    template <class HOST, typename DATA>
    void reg<HOST, DATA>::do_write(const range& addr, const DATA* data) {
        DATA val = property<DATA>::get();

        unsigned char* ptr = (unsigned char*)&val + addr.start - get_address();
        memcpy(ptr, data, addr.length());

        if (tagged_write != NULL)
            val = (m_host->*write)(val);
        else if (write != NULL)
            val = (m_host->*write)(val);

        property<DATA>::set(val);
    }

    template <class HOST, typename DATA> template <typename T>
    reg<HOST, DATA>& reg<HOST, DATA>::operator = (const T& value) {
        property<DATA>::set(static_cast<DATA>(value));
        return *this;
    }

    template <class HOST, typename DATA> template <typename T>
    reg<HOST, DATA>& reg<HOST, DATA>::operator |= (const T& value) {
        property<DATA>::set(property<DATA>::get() | static_cast<DATA>(value));
        return *this;
    }

    template <class HOST, typename DATA> template <typename T>
    reg<HOST, DATA>& reg<HOST, DATA>::operator &= (const T& value) {
        property<DATA>::set(property<DATA>::get() & static_cast<DATA>(value));
        return *this;
    }

    template <class HOST, typename DATA> template <typename T>
    reg<HOST, DATA>& reg<HOST, DATA>::operator += (const T& value) {
        property<DATA>::set(property<DATA>::get() + static_cast<DATA>(value));
        return *this;
    }

    template <class HOST, typename DATA> template <typename T>
    reg<HOST, DATA>& reg<HOST, DATA>::operator -= (const T& value) {
        property<DATA>::set(property<DATA>::get() - static_cast<DATA>(value));
        return *this;
    }

    template <class HOST, typename DATA> template <typename T>
    reg<HOST, DATA>& reg<HOST, DATA>::operator *= (const T& value) {
        property<DATA>::set(property<DATA>::get() * static_cast<DATA>(value));
        return *this;
    }

    template <class HOST, typename DATA> template <typename T>
    reg<HOST, DATA>& reg<HOST, DATA>::operator /= (const T& value) {
        property<DATA>::set(property<DATA>::get() / static_cast<DATA>(value));
        return *this;
    }

}

#endif
