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

    template <class HOST, typename DATA, const unsigned int N = 1>
    class reg: public reg_base,
               public property<DATA, N>
    {
    private:
        HOST* m_host;
        bool m_banked;
        std::map<int, DATA*> m_banks;
        int m_current_bank;

        void init_bank(int bank);
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

        inline bool is_banked() const { return m_banked; }
        inline void set_banked(bool set = true) { m_banked = set; }

        inline int current_bank() const { return m_current_bank; }

        const DATA& bank(int bank) const;
        DATA& bank(int bank);

        const DATA& bank(int bank, unsigned int idx) const;
        DATA& bank(int bank, unsigned int idx);

        const char* name() const { return sc_core::sc_object::name(); }

        reg(const char* nm, u64 addr, const DATA& init = DATA(),
            HOST* host = NULL);
        virtual ~reg();

        VCML_KIND(reg);

        virtual void do_read(const range& addr, int bank, void* ptr);
        virtual void do_read(const range& addr, DATA* ptr);

        virtual void do_write(const range& addr, int bank, const void* ptr);
        virtual void do_write(const range& addr, const DATA* ptr);

        template <typename T> reg<HOST, DATA, N>& operator =  (const T& value);
        template <typename T> reg<HOST, DATA, N>& operator |= (const T& value);
        template <typename T> reg<HOST, DATA, N>& operator &= (const T& value);
        template <typename T> reg<HOST, DATA, N>& operator ^= (const T& value);
        template <typename T> reg<HOST, DATA, N>& operator += (const T& value);
        template <typename T> reg<HOST, DATA, N>& operator -= (const T& value);
        template <typename T> reg<HOST, DATA, N>& operator *= (const T& value);
        template <typename T> reg<HOST, DATA, N>& operator /= (const T& value);

        template <typename T> bool operator == (const T& other);
        template <typename T> bool operator != (const T& other);
        template <typename T> bool operator <= (const T& other);
        template <typename T> bool operator >= (const T& other);
        template <typename T> bool operator  < (const T& other);
        template <typename T> bool operator  > (const T& other);
    };

    template <class HOST, typename DATA, const unsigned int N>
    void reg<HOST, DATA, N>::init_bank(int bank) {
        if (stl_contains(m_banks, bank))
            return;

        m_banks[bank] = new DATA[N];
        for (unsigned int i = 0; i < N; i++)
            m_banks[bank][i] = property<DATA, N>::get_default();
    }

    template <class HOST, typename DATA, const unsigned int N>
    void reg<HOST, DATA, N>::load_bank(int bank) {
        if (!m_banked || bank == m_current_bank)
            return;

        if (!stl_contains(m_banks, bank))
            init_bank(bank);

        for (unsigned int i = 0; i < N; i++) {
            m_banks[m_current_bank][i] = property<DATA, N>::get(i);
            property<DATA, N>::set(m_banks[bank][i]);
        }

        m_current_bank = bank;
    }

    template <class HOST, typename DATA, const unsigned int N>
    const DATA& reg<HOST, DATA, N>::bank(int bk) const {
        return bank(bk, 0);
    }

    template <class HOST, typename DATA, const unsigned int N>
    DATA& reg<HOST, DATA, N>::bank(int bk) {
        return bank(bk, 0);
    }

    template <class HOST, typename DATA, const unsigned int N>
    const DATA& reg<HOST, DATA, N>::bank(int bk,  unsigned int idx) const {
        VCML_ERROR_ON(!m_banked, "register %s is not banked", name());
        VCML_ERROR_ON(idx >= N, "index %d out of bounds", idx);
        if (!stl_contains(m_banks, bk))
            return property<DATA, N>::get_default();
        return m_banks.at(bk);
    }

    template <class HOST, typename DATA, const unsigned int N>
    DATA& reg<HOST, DATA, N>::bank(int bank, unsigned int idx) {
        VCML_ERROR_ON(!m_banked, "register %s is not banked", name());
        VCML_ERROR_ON(idx >= N, "index %d out of bounds", idx);
        if (!stl_contains(m_banks, bank))
            init_bank(bank);
        return m_banks[bank][idx];
    }

    template <class HOST, typename DATA, const unsigned int N>
    reg<HOST, DATA, N>::reg(const char* n, u64 addr, const DATA& def, HOST* h):
        reg_base(n, addr, N * sizeof(DATA), h),
        property<DATA, N>(n, def, h),
        m_host(h),
        m_banked(false),
        m_banks(),
        m_current_bank(ext_bank::NONE),
        read(NULL),
        write(NULL),
        tag(0),
        tagged_read(NULL),
        tagged_write(NULL) {
        m_banks[m_current_bank] = new DATA[N];
        for (unsigned int i = 0; i < N; i++)
            m_banks[m_current_bank][i] = property<DATA, N>::get(i);

        if (m_host == NULL)
            m_host = dynamic_cast<HOST*>(get_host());
        VCML_ERROR_ON(!m_host, "invalid host specified for register %s", n);
    }

    template <class HOST, typename DATA, const unsigned int N>
    reg<HOST, DATA, N>::~reg() {
        for (auto bank : m_banks)
            delete [] bank.second;
    }

    template <class HOST, typename DATA, const unsigned int N>
    void reg<HOST, DATA, N>::do_read(const range& addr, int bank, void* ptr) {
        load_bank(bank);
        do_read(addr, static_cast<DATA*>(ptr));
        load_bank(ext_bank::NONE);
    }

    template <class HOST, typename DATA, const unsigned int N>
    void reg<HOST, DATA, N>::do_write(const range& a, int b, const void* ptr) {
        load_bank(b);
        do_write(a, static_cast<const DATA*>(ptr));
        load_bank(ext_bank::NONE);
    }

    template <class HOST, typename DATA, const unsigned int N>
    void reg<HOST, DATA, N>::do_read(const range& txaddr, DATA* data) {
        range addr(txaddr);
        unsigned char* dest = (unsigned char*)data;

        while (addr.start <= addr.end) {
            u64 idx  = (addr.start - get_address()) / sizeof(DATA);
            u64 off  = (addr.start - get_address()) % sizeof(DATA);
            u64 size = min(addr.length(), (u64)sizeof(DATA));

            DATA val = property<DATA, N>::get(idx);

            if (tagged_read != NULL)
                val = (m_host->*tagged_read)(N > 1 ? idx : tag);
            else if (read != NULL)
                val = (m_host->*read)();

            property<DATA, N>::set(val, idx);

            unsigned char* ptr = (unsigned char*)&val + off;
            memcpy(dest, ptr, size);

            addr.start += size;
            dest += size;
        }
    }

    template <class HOST, typename DATA, const unsigned int N>
    void reg<HOST, DATA, N>::do_write(const range& txaddr, const DATA* data) {
        range addr(txaddr);
        const unsigned char* src = (const unsigned char*)data;

        while (addr.start <= addr.end) {
            u64 idx  = (addr.start - get_address()) / sizeof(DATA);
            u64 off  = (addr.start - get_address()) % sizeof(DATA);
            u64 size = min(addr.length(), (u64)sizeof(DATA));

            DATA val = property<DATA, N>::get(idx);

            unsigned char* ptr = (unsigned char*)&val + off;
            memcpy(ptr, src, size);

            if (tagged_write != NULL)
                val = (m_host->*tagged_write)(val, N > 1 ? idx : tag);
            else if (write != NULL)
                val = (m_host->*write)(val);

            property<DATA, N>::set(val, idx);
            addr.start += size;
            src += size;
        }
    }

    template <class HOST, typename DATA, const unsigned int N>
    template <typename T>
    reg<HOST, DATA, N>& reg<HOST, DATA, N>::operator = (const T& value) {
        property<DATA, N>::set(static_cast<DATA>(value));
        return *this;
    }

    template <class HOST, typename DATA, const unsigned int N>
    template <typename T>
    reg<HOST, DATA, N>& reg<HOST, DATA, N>::operator |= (const T& value) {
        property<DATA, N>::set(property<DATA, N>::get() | value);
        return *this;
    }

    template <class HOST, typename DATA, const unsigned int N>
    template <typename T>
    reg<HOST, DATA, N>& reg<HOST, DATA, N>::operator &= (const T& value) {
        property<DATA, N>::set(property<DATA, N>::get() & value);
        return *this;
    }

    template <class HOST, typename DATA, const unsigned int N>
    template <typename T>
    reg<HOST, DATA, N>& reg<HOST, DATA, N>::operator ^= (const T& value) {
        property<DATA, N>::set(property<DATA, N>::get() ^ value);
        return *this;
    }

    template <class HOST, typename DATA, const unsigned int N>
    template <typename T>
    reg<HOST, DATA, N>& reg<HOST, DATA, N>::operator += (const T& value) {
        property<DATA, N>::set(property<DATA, N>::get() + value);
        return *this;
    }

    template <class HOST, typename DATA, const unsigned int N>
    template <typename T>
    reg<HOST, DATA, N>& reg<HOST, DATA, N>::operator -= (const T& value) {
        property<DATA, N>::set(property<DATA, N>::get() - value);
        return *this;
    }

    template <class HOST, typename DATA, const unsigned int N>
    template <typename T>
    reg<HOST, DATA, N>& reg<HOST, DATA, N>::operator *= (const T& value) {
        property<DATA, N>::set(property<DATA, N>::get() * value);
        return *this;
    }

    template <class HOST, typename DATA, const unsigned int N>
    template <typename T>
    reg<HOST, DATA, N>& reg<HOST, DATA, N>::operator /= (const T& value) {
        property<DATA, N>::set(property<DATA, N>::get() / value);
        return *this;
    }

    template <class HOST, typename DATA, const unsigned int N>
    template <typename T>
    inline bool reg<HOST, DATA, N>::operator == (const T& other) {
        for (unsigned int i = 0; i < N; i++)
            if (property<DATA, N>::get(i) != other)
                return false;
        return true;
    }

    template <class HOST, typename DATA, const unsigned int N>
    template <typename T>
    inline bool reg<HOST, DATA, N>::operator < (const T& other) {
        for (unsigned int i = 0; i < N; i++)
            if (property<DATA, N>::get(i) >= other)
                return false;
        return true;
    }

    template <class HOST, typename DATA, const unsigned int N>
    template <typename T>
    inline bool reg<HOST, DATA, N>::operator > (const T& other) {
        for (unsigned int i = 0; i < N; i++)
            if (property<DATA, N>::get(i) <= other)
                return false;
        return true;
    }

    template <class HOST, typename DATA, const unsigned int N>
    template <typename T>
    inline bool reg<HOST, DATA, N>::operator != (const T& other) {
        return !operator == (other);
    }

    template <class HOST, typename DATA, const unsigned int N>
    template <typename T>
    inline bool reg<HOST, DATA, N>::operator <= (const T& other) {
        return !operator > (other);
    }

    template <class HOST, typename DATA, const unsigned int N>
    template <typename T>
    inline bool reg<HOST, DATA, N>::operator >= (const T& other) {
        return !operator < (other);
    }

}

#endif
