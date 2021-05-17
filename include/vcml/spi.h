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

#ifndef VCML_SPI_H
#define VCML_SPI_H

#include "vcml/common/types.h"
#include "vcml/common/systemc.h"
#include "vcml/component.h"

namespace vcml {

    class spi_fw_transport_if: public sc_core::sc_interface
    {
    public:
        virtual u8 spi_transport(u8 val) = 0;
    };

    class spi_bw_transport_if: public sc_core::sc_interface {
        /* empty interface */
    };

    class spi_initiator_socket:
        public tlm::tlm_base_initiator_socket<1, spi_fw_transport_if,
                                              spi_bw_transport_if, 1,
                                              sc_core::SC_ONE_OR_MORE_BOUND> {
    public:
        spi_initiator_socket();
        explicit spi_initiator_socket(const char* name);
        VCML_KIND(spi_initiator_socket);
        virtual sc_core::sc_type_index get_protocol_types() const override;
    };

    class spi_target_socket:
        public tlm::tlm_base_target_socket<1, spi_fw_transport_if,
                                              spi_bw_transport_if, 1,
                                              sc_core::SC_ONE_OR_MORE_BOUND> {
    public:
        spi_target_socket();
        explicit spi_target_socket(const char* name);
        VCML_KIND(spi_target_socket);
        virtual sc_core::sc_type_index get_protocol_types() const override;
    };

    class spi_initiator_stub: public module, protected spi_bw_transport_if
    {
    public:
        spi_initiator_socket SPI_OUT;
        spi_initiator_stub(const sc_module_name& name);
        virtual ~spi_initiator_stub();
        VCML_KIND(spi_initiator_stub);
    };

    class spi_target_stub: public module, protected spi_fw_transport_if
    {
    protected:
        virtual u8 spi_transport(u8 val) override;

    public:
        spi_target_socket SPI_IN;
        spi_target_stub(const sc_module_name& name);
        virtual ~spi_target_stub();
        VCML_KIND(spi_target_stub);
    };

}

#endif
