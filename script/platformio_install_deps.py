#!/usr/bin/env python3
# This script is used to preinstall
# all platformio libraries in the global storage

import argparse
from concurrent.futures import ThreadPoolExecutor
import configparser
import os
import subprocess
import threading

config = configparser.ConfigParser(inline_comment_prefixes=(";",))

parser = argparse.ArgumentParser(description="")
parser.add_argument("file", help="Path to platformio.ini", nargs=1)
parser.add_argument("-l", "--libraries", help="Install libraries", action="store_true")
parser.add_argument("-p", "--platforms", help="Install platforms", action="store_true")
parser.add_argument("-t", "--tools", help="Install tools", action="store_true")

args = parser.parse_args()

config.read(args.file)


libs = []
tools = []
platforms = []
# Extract from every lib_deps key in all sections
for section in config.sections():
    conf = config[section]
    if "lib_deps" in conf and args.libraries:
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
    if "platform" in conf and args.platforms:
        platforms.append(conf["platform"])
    if "platform_packages" in conf and args.tools:
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
            tools.append(tool)


def parallel_install(manager_cls, specs: list[str]) -> None:
    """Best-effort parallel top-level install, one extraction worker per core.

    PlatformIO's own installer downloads and unpacks one package at a time
    on one core. Dependencies are skipped (two packages sharing one must
    not extract into the same directory from two threads) and any failure
    is left alone: the stock ``pkg install`` pass below installs whatever
    is missing and is the authority on the final state.
    """
    manager = manager_cls(None)
    pending = [spec for spec in specs if not manager.get_package(spec)]
    if not pending:
        return
    local = threading.local()

    def install_one(spec: str) -> None:
        try:
            if (mgr := getattr(local, "mgr", None)) is None:
                mgr = local.mgr = manager_cls(None)
            mgr._install(spec, skip_dependencies=True)  # noqa: SLF001
        except Exception:  # noqa: BLE001
            pass

    cpus = getattr(os, "process_cpu_count", os.cpu_count)() or 4
    manager.lock()
    try:
        with ThreadPoolExecutor(max_workers=min(cpus, len(pending))) as ex:
            list(ex.map(install_one, pending))
    finally:
        manager.unlock()


if libs or platforms or tools:
    from platformio.package.manager.library import LibraryPackageManager
    from platformio.package.manager.platform import PlatformPackageManager
    from platformio.package.manager.tool import ToolPackageManager

    for manager_cls, specs in (
        (PlatformPackageManager, platforms),
        (ToolPackageManager, tools),
        (LibraryPackageManager, libs),
    ):
        if specs:
            parallel_install(manager_cls, specs)

cli_args = []
for flag, specs in (("-l", libs), ("-p", platforms), ("-t", tools)):
    for spec in specs:
        cli_args.append(flag)
        cli_args.append(spec)

subprocess.check_call(
    ["platformio", "pkg", "install", "-g", *cli_args], close_fds=False
)
