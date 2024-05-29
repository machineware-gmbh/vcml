#!/usr/bin/env python3

##############################################################################
#                                                                            #
# Copyright (C) 2024 MachineWare GmbH                                        #
# All Rights Reserved                                                        #
#                                                                            #
# This is work is licensed under the terms described in the LICENSE file     #
# found in the root directory of this source tree.                           #
#                                                                            #
##############################################################################

import os
import sys
import subprocess
import platform
import tempfile
from pathlib import Path

home = Path(__file__).resolve().parents[1]
ext = '.exe' if platform.system() == 'Windows' else ''
arch = platform.machine().lower()
formatter = home / 'cmake' / 'Tools' / platform.system()\
            / f'clang-format-18.{arch}{ext}'
srcdirs = [home / 'src', home / 'include', home / 'test']

with tempfile.NamedTemporaryFile(mode='w+', delete=False) as temp:
    temp_path = temp.name
    for dir in srcdirs:
        for ext in ('*.h', '*.cpp'):
            for file in dir.rglob(ext):
                if 'googletest' not in str(file):
                    temp.write(f"{str(file)}\n")
    temp.close()
    r = subprocess.run([formatter, '--Wno-error=unknown', '--Werror',\
                        f'--files={temp_path}'] + sys.argv[1:])
    os.remove(temp_path)
    sys.exit(r.returncode)
