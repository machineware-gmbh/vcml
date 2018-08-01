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

#ifndef VCML_THCTL_H
#define VCML_THCTL_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"

namespace vcml {

    pthread_t thctl_sysc_thread();
    bool      thctl_is_sysc_thread();

    bool thctl_sysc_paused();
    void thctl_sysc_pause();
    void thctl_sysc_resume();
    void thctl_sysc_update();
    void thctl_sysc_yield();

    void thctl_sysc_set_paused(bool paused = true);

}


#endif
