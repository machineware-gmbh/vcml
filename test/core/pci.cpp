/******************************************************************************
 *                                                                            *
 * Copyright 2021 Jan Henrik Weinstock                                        *
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

#include "testing.h"

class pci_harness : public test_base, public pci_initiator, public pci_target
{
private:
    // PCI initiator interface
    MOCK_METHOD(void, pci_bar_map,
                (const pci_initiator_socket& socket, const pci_bar& bar));
    MOCK_METHOD(void, pci_bar_unmap,
                (const pci_initiator_socket& socket, int barno));
    MOCK_METHOD(void*, pci_dma_ptr,
                (const pci_initiator_socket& socket, vcml_access rw, u64 addr,
                 u64 size));
    MOCK_METHOD(bool, pci_dma_read,
                (const pci_initiator_socket& socket, u64 addr, u64 size,
                 void* data));
    MOCK_METHOD(bool, pci_dma_write,
                (const pci_initiator_socket& socket, u64 addr, u64 size,
                 const void* data));
    MOCK_METHOD(void, pci_interrupt,
                (const pci_initiator_socket& socket, pci_irq irq, bool state));

    // PCI target interface
    virtual void pci_transport(const pci_target_socket& socket,
                               pci_payload& pci) override {
        EXPECT_TRUE(pci.is_read());
        EXPECT_TRUE(pci.is_cfg());
        pci.data     = pci_in.index_of(socket);
        pci.response = PCI_RESP_SUCCESS;
    }

public:
    pci_initiator_socket_array<> pci_out;
    pci_target_socket_array<> pci_in;

    pci_base_initiator_socket_array<> pci_out_h;
    pci_base_target_socket_array<> pci_in_h;

    pci_initiator_socket pci_out_nocon;
    pci_target_socket pci_in_nocon;

    pci_harness(const sc_module_name& nm):
        test_base(nm),
        pci_initiator(),
        pci_target(),
        pci_out("pci_out"),
        pci_in("pci_in"),
        pci_out_h("pci_out_h"),
        pci_in_h("pci_in_h"),
        pci_out_nocon("pci_out_nocon"),
        pci_in_nocon("pci_in_nocon") {
        for (int i = 0; i < 4; i++) {
            pci_out[i].bind(pci_out_h[i]);
            pci_in_h[i].bind(pci_in[i]);
            pci_out_h[i].bind(pci_in_h[i]);
        }

        pci_out_nocon.stub();
        pci_in_nocon.stub();

        EXPECT_TRUE(find_object("pci.pci_out_nocon_stub"));
        EXPECT_TRUE(find_object("pci.pci_in_nocon_stub"));
    }

    virtual void run_test() override {
        pci_payload pci{};
        pci.command = PCI_READ;
        pci.space   = PCI_AS_CFG;
        pci.addr    = 0x12345678;
        pci.data    = 0xffffffff;
        pci.size    = 4;

        for (auto& port : pci_out) {
            pci.response = PCI_RESP_INCOMPLETE;
            (*port.second)->pci_transport(pci);
            EXPECT_SUCCESS(pci);
            EXPECT_EQ(pci.data, (u32)port.first);
        }

        for (auto& port : pci_in) {
            u32 data = -1;
            EXPECT_CALL(*this, pci_dma_read(_, 0, sizeof(data), &data))
                .Times(1)
                .WillOnce(Return(true));
            EXPECT_TRUE((*port.second)->pci_dma_read(0, sizeof(data), &data));
        }
    }
};

TEST(pci, sockets) {
    broker_arg broker(sc_argc(), sc_argv());
    tracer_term tracer;
    pci_harness test("pci");
    sc_core::sc_start();
}
