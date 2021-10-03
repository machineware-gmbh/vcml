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

class pci_harness: public test_base, public pci_initiator, public pci_target
{
private:
    /* PCI initiator interface */
    virtual void pci_bar_map(pci_initiator_socket& socket,
        const pci_bar& bar) override {
        ADD_FAILURE() << __FUNCTION__ << " should not be called";
    }

    virtual void pci_bar_unmap(pci_initiator_socket& socket,
        int barno) override {
        ADD_FAILURE() << __FUNCTION__ << " should not be called";
    }

    virtual void* pci_dma_ptr(pci_initiator_socket& socket,
        vcml_access rw, u64 addr, u64 size) override {
        ADD_FAILURE() << __FUNCTION__ << " should not be called";
        return nullptr;
    }

    virtual bool pci_dma_read(pci_initiator_socket& socket,
        u64 addr, u64 size, void* data) override {
        *(u32*)data = PCI_OUT.index_of(socket);
        return true;
    }

    virtual bool pci_dma_write(pci_initiator_socket& socket,
        u64 addr, u64 size, const void* data) override {
        ADD_FAILURE() << __FUNCTION__ << " should not be called";
        return false;
    }

    virtual void pci_interrupt(pci_initiator_socket& socket,
        pci_irq irq, bool state) override {
        ADD_FAILURE() << __FUNCTION__ << " should not be called";
    }

    /* PCI target interface */
    virtual void pci_transport(pci_target_socket& socket,
        pci_payload& pci) override {
        EXPECT_TRUE(pci.is_read());
        EXPECT_TRUE(pci.is_cfg());
        pci.data = PCI_IN.index_of(socket);
        pci.response = PCI_RESP_SUCCESS;
    }

public:
    pci_initiator_socket_array<> PCI_OUT;
    pci_target_socket_array<> PCI_IN;

    pci_harness(const sc_module_name& nm):
        test_base(nm), pci_initiator(), pci_target(),
        PCI_OUT("PCI_OUT"), PCI_IN("PCI_IN") {
        for (int i = 0; i < 4; i++)
            PCI_OUT[i].bind(PCI_IN[i]);
    }

    virtual void run_test() override {
        pci_payload pci;
        pci.command = PCI_READ;
        pci.space = PCI_AS_CFG;

        size_t ninit = 0;
        size_t ntgts = 0;

        for (auto& port : PCI_OUT) {
            (*port.second)->pci_transport(pci);
            EXPECT_SUCCESS(pci);
            EXPECT_EQ(pci.data, (u32)port.first);
            ninit++;
        }

        for (auto& port : PCI_IN) {
            u32 data = -1;
            EXPECT_TRUE((*port.second)->pci_dma_read(0, sizeof(data), &data));
            EXPECT_EQ(data, (u32)port.first);
            ntgts++;
        }

        EXPECT_EQ(ninit, PCI_OUT.count());
        EXPECT_EQ(ntgts, PCI_IN.count());
    }

};

TEST(spi, sockets) {
    pci_harness test("test");
    sc_core::sc_start();
}
