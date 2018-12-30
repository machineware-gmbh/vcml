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

#include "vcml/spi.h"

namespace vcml {

    spi_initiator_socket::spi_initiator_socket():
        tlm::tlm_base_initiator_socket<1, spi_fw_transport_if,
                                          spi_bw_transport_if, 1,
                                          sc_core::SC_ONE_OR_MORE_BOUND>() {
    }

    spi_initiator_socket::spi_initiator_socket(const char* nm):
        tlm::tlm_base_initiator_socket<1, spi_fw_transport_if,
                                          spi_bw_transport_if, 1,
                                          sc_core::SC_ONE_OR_MORE_BOUND>(nm) {
    }

    sc_core::sc_type_index spi_initiator_socket::get_protocol_types() const {
        return typeid(spi_fw_transport_if);
    }

    spi_target_socket::spi_target_socket():
        tlm::tlm_base_target_socket<1, spi_fw_transport_if,
                                       spi_bw_transport_if, 1,
                                       sc_core::SC_ONE_OR_MORE_BOUND>() {
    }

    spi_target_socket::spi_target_socket(const char* nm):
        tlm::tlm_base_target_socket<1, spi_fw_transport_if,
                                       spi_bw_transport_if, 1,
                                       sc_core::SC_ONE_OR_MORE_BOUND>(nm) {
    }

    sc_core::sc_type_index spi_target_socket::get_protocol_types() const {
        return typeid(spi_bw_transport_if);
    }

    spi_initiator_stub::spi_initiator_stub(const sc_module_name& nm):
        sc_module(nm),
        spi_bw_transport_if(),
        SPI_OUT("SPI_OUT") {
        SPI_OUT.bind(*this);
    }

    spi_initiator_stub::~spi_initiator_stub() {
        /* nothing to do */
    }


    u8 spi_target_stub::spi_transport(u8 val) {
        log_debug("received 0x%02x", (unsigned int)val);
        return 0xff;
    }

    spi_target_stub::spi_target_stub(const sc_module_name& nm):
        component(nm),
        spi_fw_transport_if(),
        SPI_IN("SPI_IN") {
        SPI_IN.bind(*this);
    }

    spi_target_stub::~spi_target_stub() {
        /* nothing to do */
    }

}
