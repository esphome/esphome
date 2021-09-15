#!/usr/bin/env python3
# This script is used in the docker containers to preinstall
# all platformio libraries in the global storage

import configparser
import subprocess
import sys

config = configparser.ConfigParser(inline_comment_prefixes=(';', ))
config.read(sys.argv[1])
libs = [x for x in config['common']['lib_deps'].splitlines() if len(x) != 0]

subprocess.check_call(['platformio', 'lib', '-g', 'uninstall', *libs])
