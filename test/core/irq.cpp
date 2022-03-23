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

TEST(irq, to_string) {
    irq_payload irq;
    irq.vector = 42;
    irq.active = true;

    // no formatting checks, just make sure it compiles
    std::cout << irq << std::endl;
}

class irq_test_harness : public test_base
{
public:
    unsigned int irq_no;
    unordered_map<irq_vector, bool> irq_state;
    unordered_set<unsigned int> irq_source;

    irq_initiator_socket out;
    irq_initiator_socket out2;
    irq_target_socket_array<> in;

    // for testing hierarchical binding
    irq_base_initiator_socket h_out;
    irq_base_target_socket h_in;

    // for adapter testing
    irq_initiator_socket a_out;
    irq_target_adapter ta;
    sc_signal<bool> signal;
    irq_initiator_adapter ia;

    irq_test_harness(const sc_module_name& nm):
        test_base(nm),
        irq_no(),
        irq_state(),
        irq_source(),
        out("out"),
        out2("out2"),
        in("in"),
        h_out("h_out"),
        h_in("h_in"),
        a_out("a_out"),
        ta("ta"),
        signal("signal"),
        ia("ia") {
        out.bind(in[0]);

        // check hierarchical binding: OUT -> H_OUT -> H_IN -> IN[1]
        out.bind(h_out);
        h_in.bind(in[1]);
        h_out.bind(h_in);

        // check stubbing
        out2.stub();
        in[2].stub();

        // check adapters
        a_out.bind(ta.irq_in);
        ta.irq_out.bind(signal);
        ia.irq_in.bind(signal);
        ia.irq_out.bind(in[3]);

        auto initiators = get_irq_initiator_sockets();
        auto targets    = get_irq_target_sockets();
        auto sockets    = get_irq_target_sockets(0);

        EXPECT_EQ(initiators.size(), 3) << "irq initiators did not register";
        EXPECT_EQ(targets.size(), 4) << "irq targets did not register";
        EXPECT_FALSE(sockets.empty()) << "irq targets in wrong address space";

        clk.stub(100 * MHz);
        rst.stub();
    }

    virtual void irq_transport(const irq_target_socket& socket,
                               irq_payload& irq) override {
        irq_state[irq.vector] = irq.active;
        size_t source         = in.index_of(socket);
        if (irq.active)
            irq_source.insert(source);
        else
            irq_source.erase(source);
    }

    virtual void run_test() override {
        // this also forces construction of IN[0]'s default event so that it
        // it can be triggered later on
        EXPECT_TRUE(in[0].default_event().name());

        enum test_vectors : irq_vector {
            VECTOR = 0x42,
        };

        EXPECT_FALSE(irq_state[VECTOR]);
        out[VECTOR] = true;
        EXPECT_TRUE(irq_state[VECTOR]);
        EXPECT_TRUE(irq_source.count(0));
        EXPECT_TRUE(irq_source.count(1));

        wait(in[0].default_event());

        EXPECT_TRUE(irq_state[VECTOR]);
        out[VECTOR] = false;
        EXPECT_FALSE(irq_state[VECTOR]);
        EXPECT_FALSE(irq_source.count(0));
        EXPECT_FALSE(irq_source.count(1));

        wait(in[0].default_event());

        EXPECT_FALSE(irq_state[IRQ_NO_VECTOR]);
        out = true;
        EXPECT_TRUE(irq_state[IRQ_NO_VECTOR]);
        EXPECT_TRUE(irq_source.count(0));
        EXPECT_TRUE(irq_source.count(1));

        wait(in[0].default_event());

        EXPECT_TRUE(irq_state[IRQ_NO_VECTOR]);
        out = false;
        EXPECT_FALSE(irq_state[IRQ_NO_VECTOR]);
        EXPECT_FALSE(irq_source.count(0));
        EXPECT_FALSE(irq_source.count(1));

        // test hierarchy binding
        EXPECT_FALSE(signal.read());
        a_out = true;
        wait(in[3].default_event());
        EXPECT_TRUE(in[3].read());
        a_out = false;
        wait(in[3].default_event());
        EXPECT_FALSE(in[3].read());
    }
};

TEST(irq, sockets) {
    broker_arg broker(sc_argc(), sc_argv());
    tracer_term tracer;
    irq_test_harness test("irq");
    sc_core::sc_start();
}
