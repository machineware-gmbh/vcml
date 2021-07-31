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

#ifndef VCML_LOG_FILE_H
#define VCML_LOG_FILE_H

#include "vcml/common/types.h"
#include "vcml/common/strings.h"
#include "vcml/logging/logger.h"

namespace vcml {

    class log_file: public logger
    {
    private:
        ofstream m_file;

    public:
        log_file(const string& filename);
        virtual ~log_file();

        virtual void write_log(const logmsg& msg) override;
    };

}

#endif
