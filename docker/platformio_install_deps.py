#!/usr/bin/env python3
# This script is used in the docker containers to preinstall
# all platformio libraries in the global storage

import configparser
import re
import subprocess
import sys

config = configparser.ConfigParser()
config.read(sys.argv[1])
libs = []
for line in config['common']['lib_deps'].splitlines():
    # Format: '1655@1.0.2  ; TinyGPSPlus (has name conflict)' (includes comment)
    m = re.search(r'([a-zA-Z0-9-_/]+@[0-9\.]+)', line)
    if m is None:
        continue
    libs.append(m.group(1))

subprocess.check_call(['platformio', 'lib', '-g', 'install', *libs])
