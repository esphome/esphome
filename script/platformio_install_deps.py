#!/usr/bin/env python3
# This script is used to preinstall
# all platformio libraries in the global storage

import argparse
from concurrent.futures import ThreadPoolExecutor
import configparser
import shutil
import subprocess
import threading

from platformio.package.manager.library import LibraryPackageManager
from platformio.package.manager.tool import ToolPackageManager
from platformio.package.meta import PackageSpec

# Downloads are network bound and release the GIL, unpacks are CPU bound;
# a fixed pool well past the core count keeps the network busy while
# unpacks share the cores.
MAX_WORKERS = 16


def parse_specs(path: str, args: argparse.Namespace) -> tuple[list, list, list]:
    """Extract lib/platform/tool specs from every section of a platformio.ini."""
    config = configparser.ConfigParser(inline_comment_prefixes=(";",))
    config.read(path)
    libs = []
    tools = []
    platforms = []
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
    return libs, platforms, tools


def spec_key(spec: str) -> str:
    """The destination identity of a spec: PlatformIO installs by package
    name, so two specs sharing a name share a directory."""
    name = PackageSpec(spec).name
    return name.lower() if name else spec.strip()


def parallel_install(manager_cls, specs: list) -> None:
    """Best-effort parallel top-level install.

    PlatformIO's own installer downloads and unpacks one package at a time
    on one core. Dependencies are skipped (two packages sharing one must
    not extract into the same directory from two threads) and failures are
    only reported: the stock ``pkg install`` pass afterwards installs
    whatever is missing and is the authority on the final state.
    """
    if not specs:
        return
    manager = manager_cls(None)
    # One spec per destination: platformio.ini repeats specs across env
    # sections, and two threads must not extract into the same directory.
    # Another spec for the same name is left to the pkg install pass.
    unique = {spec_key(spec): spec for spec in specs}
    pending = [spec for spec in unique.values() if not manager.get_package(spec)]
    if not pending:
        return
    # One manager per worker thread: each construction rewires the
    # process-global manager logger, which drops lines under concurrency
    local = threading.local()

    def install_one(spec: str) -> bool:
        if (mgr := getattr(local, "mgr", None)) is None:
            mgr = local.mgr = manager_cls(None)
        try:
            mgr._install(spec, skip_dependencies=True)  # noqa: SLF001
            return True
        except Exception as err:  # noqa: BLE001
            print(f"Pre-install of {spec} failed ({err!r})", flush=True)
            # A torn copy into the destination can carry valid metadata the
            # pkg install pass would trust; remove it so that pass redoes it
            try:
                if (pkg := mgr.get_package(spec)) is not None:
                    shutil.rmtree(pkg.path, ignore_errors=True)
            except Exception:  # noqa: BLE001
                pass
            return False

    workers = min(len(pending), MAX_WORKERS)
    print(f"Preinstalling {len(pending)} package(s) with {workers} workers", flush=True)
    manager.lock()
    try:
        with ThreadPoolExecutor(max_workers=workers) as ex:
            results = list(ex.map(install_one, pending))
    finally:
        manager.unlock()
    if not any(results):
        # A systematic fault (e.g. a PlatformIO API change), not archive noise
        print(
            "Pre-install failed for every package; pkg install runs serially",
            flush=True,
        )


def build_cli_args(libs: list, platforms: list, tools: list) -> list:
    return [
        arg
        for flag, specs in (("-l", libs), ("-p", platforms), ("-t", tools))
        for spec in specs
        for arg in (flag, spec)
    ]


def main() -> None:
    parser = argparse.ArgumentParser(description="")
    parser.add_argument("file", help="Path to platformio.ini", nargs=1)
    parser.add_argument(
        "-l", "--libraries", help="Install libraries", action="store_true"
    )
    parser.add_argument(
        "-p", "--platforms", help="Install platforms", action="store_true"
    )
    parser.add_argument("-t", "--tools", help="Install tools", action="store_true")
    args = parser.parse_args()
    libs, platforms, tools = parse_specs(args.file, args)

    # Platforms stay serial: PlatformPackageManager.install runs an
    # on_installed hook the private _install path would skip
    for manager_cls, specs in (
        (ToolPackageManager, tools),
        (LibraryPackageManager, libs),
    ):
        try:
            parallel_install(manager_cls, specs)
        except Exception as err:  # noqa: BLE001
            # The wave is an optimization; it must never stop the real pass
            print(f"Parallel preinstall skipped ({err!r})", flush=True)

    subprocess.check_call(
        ["platformio", "pkg", "install", "-g", *build_cli_args(libs, platforms, tools)],
        close_fds=False,
    )


if __name__ == "__main__":
    main()
