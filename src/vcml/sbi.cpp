/******************************************************************************
 *                                                                            *
 * Copyright 2019 Jan Henrik Weinstock                                        *
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

#include "vcml/sbi.h"

namespace vcml {

    tlm_extension_base* sbiext::clone() const {
        return new sbiext(*this);
    }

    void sbiext::copy_from(const tlm_extension_base& ext) {
        VCML_ERROR_ON(typeid(this) != typeid(ext), "cannot copy extension");
        const sbiext& other = (const sbiext&)ext;
        code = other.code;
    }

    void tx_set_sbi(tlm_generic_payload& tx, const sideband& info) {
        if (!tx_has_sbi(tx) && info == SBI_NONE)
            return;
        if (!tx_has_sbi(tx))
            tx.set_extension<sbiext>(new sbiext());
        sbiext* ext = tx.get_extension<sbiext>();
        ext->code = info.code;
    }

    void tx_set_cpuid(tlm_generic_payload& tx, int id) {
        sideband info; info.cpuid = id;
        VCML_ERROR_ON(info.cpuid != id, "coreid too large");
        tx_set_sbi(tx, SBI_NONE | info);
    }

    void tx_set_level(tlm_generic_payload& tx, int lvl) {
        sideband info; info.level = lvl;
        VCML_ERROR_ON(info.level != lvl, "privlvl too large");
        tx_set_sbi(tx, SBI_NONE | info);
    }

}
