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

#include "vcml/serial/terminal.h"
#include "vcml/serial/backend_term.h"
#include "vcml/debugging/suspender.h"

#include <termios.h>

namespace vcml {
namespace serial {

static terminal* g_owner = nullptr;
static struct termios g_term;

static void cleanup_term() {
    tcsetattr(STDIN_FILENO, TCSANOW, &g_term);
}

static void setup_term() {
    tcgetattr(STDIN_FILENO, &g_term);

    termios attr = g_term;
    attr.c_lflag &= ~(ICANON | ECHO | ISIG);
    attr.c_cc[VMIN]  = 1;
    attr.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &attr);

    atexit(cleanup_term);
}

void backend_term::terminate() {
    cleanup_term();

    if (m_exit_requested || !sim_running())
        exit(EXIT_SUCCESS);

    m_exit_requested = true;
    debugging::suspender::quit();
}

backend_term::backend_term(terminal* term):
    backend(term, "term"), m_exit_requested(false) {
    VCML_REPORT_ON(g_owner, "stdin already used by %s", g_owner->name());
    VCML_REPORT_ON(!isatty(STDIN_FILENO), "not a terminal");
    g_owner = term;
    setup_term();
}

backend_term::~backend_term() {
    cleanup_term();
    g_owner = nullptr;
}

enum keys : u8 {
    CTRL_A = 0x1,
    CTRL_C = 0x3,
    CTRL_X = 0x18,
};

bool backend_term::read(u8& value) {
    if (!fd_peek(STDIN_FILENO))
        return false;

    u8 ch;
    fd_read(STDIN_FILENO, &ch, sizeof(ch));

    if (ch == CTRL_A) { // ctrl-a
        fd_read(STDIN_FILENO, &ch, sizeof(ch));
        if (ch == 'x' || ch == CTRL_X || ch == g_term.c_cc[VINTR]) {
            terminate();
            return false;
        }

        if (ch == 'a')
            ch = CTRL_A;
    }

    value = ch;
    return true;
}

void backend_term::write(u8 val) {
    fd_write(STDOUT_FILENO, &val, sizeof(val));
}

backend* backend_term::create(terminal* term, const string& type) {
    return new backend_term(term);
}

} // namespace serial
} // namespace vcml
