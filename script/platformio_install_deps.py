#!/usr/bin/env python3
# This script is used to preinstall
# all platformio libraries in the global storage

import configparser
import subprocess
import sys

config = configparser.ConfigParser(inline_comment_prefixes=(";",))
config.read(sys.argv[1])

libs = []
tools = []
platforms = []
# Extract from every lib_deps key in all sections
for section in config.sections():
    conf = config[section]
    if "lib_deps" in conf:
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
            libs.append("-l")
            libs.append(lib_dep)
    if "platform" in conf:
        platforms.append("-p")
        platforms.append(conf["platform"])
    if "platform_packages" in conf:
        for tool in conf["platform_packages"].splitlines():
            if not tool:
                # Empty line or comment
                continue
            if tool.startswith("${"):
                # Extending from another section
                continue
            if tool.find("https://github.com") != -1:
                split = tool.find("@")
                tool = tool[split + 1 :]
            tools.append("-t")
            tools.append(tool)

subprocess.check_call(["platformio", "pkg", "install", "-g", *libs, *platforms, *tools])
