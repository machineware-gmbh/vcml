/******************************************************************************
 *                                                                            *
 * Copyright 2023 MachineWare GmbH                                            *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is unpublished proprietary work and may not be used or disclosed to   *
 * third parties, copied or duplicated in any form, in whole or in part,      *
 * without prior written permission of the authors.                           *
 *                                                                            *
 ******************************************************************************/

#include "testing.h"

class test_module : public module
{
public:
    class inner_test_module : public sc_module
    {
    public:
        class inner_object : public sc_object
        {
        public:
            inner_object(const char* n): sc_object(n) {}
            virtual ~inner_object() = default;
        };

        inner_object obj1;
        inner_object obj2;

        inner_test_module(const sc_module_name& nm):
            sc_module(nm), obj1("obj1"), obj2("obj2") {}
        virtual ~inner_test_module() = default;
    } mod1, mod2;

    test_module(const sc_module_name& nm):
        module(nm), mod1("mod1"), mod2("mod2") {}

    virtual ~test_module() = default;
};

TEST(hierarchy, find_child) {
    test_module main("main");

    sc_object* mod1 = find_child(main, "mod1");
    sc_object* mod2 = find_child(main, "mod2");
    sc_object* o1_1 = find_child(main, "mod1.obj1");
    sc_object* o1_2 = find_child(main, "mod1.obj2");
    sc_object* o2_1 = find_child(main, "mod2.obj1");
    sc_object* o2_2 = find_child(main, "mod2.obj2");

    EXPECT_EQ(mod1, &main.mod1);
    EXPECT_EQ(mod2, &main.mod2);
    EXPECT_EQ(o1_1, &main.mod1.obj1);
    EXPECT_EQ(o1_2, &main.mod1.obj2);
    EXPECT_EQ(o2_1, &main.mod2.obj1);
    EXPECT_EQ(o2_2, &main.mod2.obj2);

    EXPECT_EQ(mod1, main.find_child("mod1"));
    EXPECT_EQ(mod2, main.find_child("mod2"));
    EXPECT_EQ(o1_1, main.find_child("mod1.obj1"));
    EXPECT_EQ(o1_2, main.find_child("mod1.obj2"));
    EXPECT_EQ(o2_1, main.find_child("mod2.obj1"));
    EXPECT_EQ(o2_2, main.find_child("mod2.obj2"));

    EXPECT_EQ(find_child(main, "nothing"), nullptr);
    EXPECT_EQ(find_child(main, "..."), nullptr);
    EXPECT_EQ(find_child(main, "."), nullptr);
    EXPECT_EQ(find_child(main, ""), nullptr);
}
