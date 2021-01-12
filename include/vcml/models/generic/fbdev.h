/******************************************************************************
 *                                                                            *
 * Copyright 2020 Jan Henrik Weinstock                                        *
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

#ifndef VCML_GENERIC_FBDEV_H
#define VCML_GENERIC_FBDEV_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"

#include "vcml/component.h"
#include "vcml/master_socket.h"

#include "vcml/ui/display.h"

namespace vcml { namespace generic {

    class fbdev: public component
    {
    private:
        u64 m_stride;
        u64 m_size;
        u8* m_vptr;

        void update();

    public:
        u64 stride() const { return m_stride; }
        u64 size()   const { return m_size; }
        u8* vptr()   const { return m_vptr; }

        property<u64> addr;
        property<u32> resx;
        property<u32> resy;

        property<string> display;

        master_socket OUT;

        fbdev() = delete;
        fbdev(const sc_module_name& nm, u32 width = 1280, u32 height = 720);
        virtual ~fbdev();
        VCML_KIND(fbdev);

        virtual void reset() override;

    protected:
        void end_of_elaboration() override;
        void end_of_simulation() override;
    };

}}

#endif
