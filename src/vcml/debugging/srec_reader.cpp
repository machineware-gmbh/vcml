/******************************************************************************
 *                                                                            *
 * Copyright 2022 Jan Henrik Weinstock                                        *
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

#include "vcml/debugging/srec_reader.h"

namespace vcml {
namespace debugging {

enum record_type : char {
    SREC_HEADER = '0',
    SREC_DATA16 = '1',
    SREC_DATA24 = '2',
    SREC_DATA32 = '3',
    SREC_DATA64 = '4',
    SREC_TEXT32 = '7',
    SREC_TEXT24 = '8',
    SREC_TEXT16 = '9',
};

static u8 srec_byte(const string& line, size_t off) {
    VCML_ERROR_ON(off + 1 >= line.length(), "reading beyond srec line");
    return from_hex_ascii(line[off]) << 4 | from_hex_ascii(line[off + 1]);
}

srec_reader::srec_reader(const string& filename):
    m_entry(), m_header(), m_records() {
    ifstream file(filename);
    VCML_ERROR_ON(!file, "cannot open srec file '%s'", filename.c_str());

    string line;
    while (getline(file, line)) {
        line = trim(line);
        if (line.length() < 2 || line[0] != 'S')
            continue;

        size_t delim = 0;
        switch (line[1]) {
        case SREC_HEADER:
        case SREC_DATA16:
        case SREC_TEXT16:
            delim = 8;
            break;
        case SREC_DATA24:
        case SREC_TEXT24:
            delim = 10;
            break;
        case SREC_DATA32:
        case SREC_TEXT32:
            delim = 12;
            break;
        case SREC_DATA64:
            delim = 20;
            break;
        default:
            continue;
        }

        record rec = {};

        for (size_t pos = 4; pos < delim; pos += 2)
            rec.addr = rec.addr << 8 | srec_byte(line, pos);

        for (size_t pos = delim; pos < line.length() - 2; pos += 2)
            rec.data.push_back(srec_byte(line, pos));

        switch (line[1]) {
        case SREC_HEADER:
            m_header = trim(string(rec.data.begin(), rec.data.end()));
            break;
        case SREC_TEXT16:
        case SREC_TEXT24:
        case SREC_TEXT32:
            m_entry = rec.addr;
            break;
        case SREC_DATA16:
        case SREC_DATA24:
        case SREC_DATA32:
        case SREC_DATA64:
            m_records.push_back(std::move(rec));
            break;
        }
    }
}

} // namespace debugging
} // namespace vcml
