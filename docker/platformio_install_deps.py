#!/usr/bin/env python3
# This script is used in the docker containers to preinstall
# all platformio libraries in the global storage

import configparser
import subprocess
import sys

config = configparser.ConfigParser(inline_comment_prefixes=(';', ))
config.read(sys.argv[1])

libs = []
# Extract from every lib_deps key in all sections
for section in config.sections():
    conf = config[section]
    if "lib_deps" not in conf:
        continue
    for lib_dep in conf["lib_deps"].splitlines():
        if not lib_dep:
            # Empty line or comment
            continue
        if lib_dep.startswith("${"):
            # Extending from another section
            continue
        if "@" not in lib_dep:
            # No version pinned, this is an internal lib
            continue
        libs.append(lib_dep)

subprocess.check_call(['platformio', 'lib', '-g', 'install', *libs])
