#!/usr/bin/env python3
# This script is used to preinstall
# all platformio libraries in the global storage

import argparse
from concurrent.futures import ThreadPoolExecutor
import configparser
import queue
import subprocess
import threading

# esphome is not installed at this docker layer, so its rmtree helper is
# out of reach; pio's fs.rmtree is the same chmod-on-readonly shape and
# is what pio's own installer uses on these directories
from platformio import fs
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
    if not config.read(path):
        # ConfigParser silently ignores unreadable files; an empty spec
        # list would build an image with no dependencies at all
        raise SystemExit(f"Could not read {path}")
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
    # Exact-string duplicates only: each costs the pkg install pass a
    # lock and rescan cycle. Name-level dedupe would change which version
    # conflicts the pass reconciles, so it stays out of scope here.
    return (
        list(dict.fromkeys(libs)),
        list(dict.fromkeys(platforms)),
        list(dict.fromkeys(tools)),
    )


def spec_key(spec) -> str:
    """The destination identity of a spec: PlatformIO installs by package
    name, so two specs sharing a name share a directory."""
    name = spec.name if isinstance(spec, PackageSpec) else PackageSpec(spec).name
    return name.lower() if name else str(spec).strip()


def dependency_specs(manager, specs) -> list:
    """The registry dependency specs of the given installed packages.

    Local manifest reads only. Name-only dependencies (platform-bundled
    libs like SPI) are left to the ``pkg install`` pass."""
    deps = []
    for spec in specs:
        if (pkg := manager.get_package(spec)) is None:
            continue
        deps.extend(
            manager.dependency_to_spec(dep)
            for dep in manager.get_pkg_dependencies(pkg) or []
            if dep.get("owner") or dep.get("version")
        )
    return deps


def parallel_install(manager_cls, specs: list, prior_names: set | None = None) -> None:
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
    # Another spec for the same name, and URL specs (which install into a
    # manifest-named dir the spec cannot predict), are left to the pkg
    # install pass; a dependency's URL version surfaces as spec.uri.
    seen_names: set = prior_names if prior_names is not None else set()
    unique = {
        spec_key(spec): spec
        for spec in specs
        if not (spec.uri if isinstance(spec, PackageSpec) else "://" in spec)
    }
    pending = [spec for spec in unique.values() if not manager.get_package(spec)]
    if not pending:
        return
    workers = min(len(pending), MAX_WORKERS)
    # One manager per worker thread: _install mutates per-instance state
    # (_MEMORY_CACHE, _INSTALL_HISTORY, the registry client). Built
    # serially, because every construction rewires the shared manager
    # logger and concurrent handler swaps drop log lines.
    managers: queue.SimpleQueue = queue.SimpleQueue()
    for _ in range(workers):
        managers.put(manager_cls(None))
    local = threading.local()

    def install_one(spec: str) -> bool:
        if (mgr := getattr(local, "mgr", None)) is None:
            mgr = local.mgr = managers.get_nowait()
        try:
            mgr._install(spec, skip_dependencies=True)  # noqa: SLF001
            return True
        except Exception as err:  # noqa: BLE001
            print(f"Pre-install of {spec} failed ({err!r})", flush=True)
            # A torn copy into the destination can carry valid metadata the
            # pkg install pass would trust; remove it so that pass redoes it
            try:
                # get_package memoizes a pre-install snapshot; without a
                # reset it cannot see the torn directory
                mgr.memcache_reset()
                if (pkg := mgr.get_package(spec)) is not None:
                    fs.rmtree(pkg.path)
            except Exception as cleanup_err:  # noqa: BLE001
                print(
                    f"Cleanup after failed pre-install of {spec} "
                    f"failed ({cleanup_err!r})",
                    flush=True,
                )
            return False

    print(f"Preinstalling {len(pending)} package(s) with {workers} workers", flush=True)
    manager.lock()
    try:
        with ThreadPoolExecutor(max_workers=workers) as ex:
            results = list(ex.map(install_one, pending))
    finally:
        manager.unlock()
    if failures := len(results) - sum(results):
        # Visible once per wave. The stock pass retries CLI specs and,
        # for already-installed packages, re-walks their dependencies
        # (_install without skip_dependencies), so failed deps retry too
        print(
            f"Pre-install failed for {failures} of {len(results)} package(s); "
            "pkg install retries them serially",
            flush=True,
        )

    # The wave skipped dependencies (a shared one must not extract from two
    # threads); collect them from the installed manifests, dedupe by name,
    # and run them as the next wave until nothing new appears
    seen_names.update(unique)
    # The pre-wave get_package calls memoized an empty storage snapshot
    manager.memcache_reset()
    next_specs = [
        dep
        for dep in dependency_specs(manager, pending)
        if spec_key(dep) not in seen_names
    ]
    if next_specs and len(seen_names) < 200:  # cycle backstop
        parallel_install(manager_cls, next_specs, seen_names)


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
    libs, platforms, tools = parse_specs(args.file[0], args)

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
