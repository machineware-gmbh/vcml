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

#ifndef VCML_TXEXT_H
#define VCML_TXEXT_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"

namespace vcml {

    class ext_bank: public tlm_extension<ext_bank>
    {
    private:
        int m_bank;

    public:
        static const int NONE = -1;

        inline int get_bank() const {
            return m_bank;
        }

        inline void set_bank(int bank) {
            m_bank = bank;
        }

        inline void clear_bank() {
            m_bank = NONE;
        }

        ext_bank(): m_bank(NONE) {
            // nothing to do
        }

        ext_bank(int bank): m_bank(bank) {
            // nothing to do
        }

        virtual tlm_extension_base* clone() const {
            return new ext_bank(m_bank);
        }

        virtual void copy_from(const tlm_extension_base& ext) {
            assert(typeid(this) == typeid(ext));
            //m_bank = ((ext_bank)ext).get_bank();
        }
    };

    static inline int tx_bank_id(const tlm_generic_payload& tx) {
        ext_bank* ext = tx.get_extension<ext_bank>();
        return ext ? ext->get_bank() : ext_bank::NONE;
    }

    class ext_exmem: public tlm::tlm_extension<ext_exmem>
    {
    private:
        int m_id;
        bool m_status;

    public:
        inline int get_id() const {
            return m_id;
        }

        inline void set_id(int id) {
            m_id = id;
        }

        inline bool get_status() const {
            return m_status;
        }

        inline void set_status(bool status) {
            m_status = status;
        }

        inline void reset() {
            m_status = false;
        }

        ext_exmem(): m_id(-1), m_status(false) {
            static int unique_id = 0;
            m_id = unique_id++;
        }

        ext_exmem(int id): m_id(id), m_status(false) {
            // nothing to do
        }

        virtual tlm_extension_base* clone() const {
            return new ext_exmem(m_id);
        }

        virtual void copy_from(const tlm_extension_base& ext) {
            assert(typeid(this) == typeid(ext));
            //m_id = ((ext_exmem)ext).get_id();
        }
    };

    static inline int tx_get_exid(const tlm_generic_payload& tx) {
        ext_exmem* ext = tx.get_extension<ext_exmem>();
        VCML_ERROR_ON(!ext, "attempt to get EXID from regular transaction");
        return ext->get_id();
    }

    static inline bool tx_is_excl(const tlm_generic_payload& tx) {
        return tx.get_extension<ext_exmem>() != nullptr;
    }

}

#endif
