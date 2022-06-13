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

#include "vcml/core/version.h"

#ifndef VCML_VERSION_MAJOR
#error VCML_VERSION_MAJOR undefined
#endif

#ifndef VCML_VERSION_MINOR
#error VCML_VERSION_MINOR undefined
#endif

#ifndef VCML_VERSION_PATCH
#error VCML_VERSION_PATCH undefined
#endif

#ifndef VCML_GIT_REV
#error VCML_GIT_REV undefined
#endif

#ifndef VCML_GIT_REV_SHORT
#error VCML_GIT_REV_SHORT undefined
#endif

#ifndef VCML_VERSION
#error VCML_VERSION undefined
#endif

#ifndef VCML_VERSION_STRING
#error VCML_VERSION_STRING undefined
#endif

namespace vcml {

unsigned int library_version() {
    return VCML_VERSION;
}

} // namespace vcml
