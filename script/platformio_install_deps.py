#!/usr/bin/env python3
# This script is used to preinstall
# all platformio libraries in the global storage

import argparse
from concurrent.futures import ThreadPoolExecutor
import configparser
from contextlib import suppress
import os
from pathlib import Path
import queue
import subprocess
import threading
import traceback

# esphome is not installed at this docker layer; pio's fs.rmtree is the
# same chmod-on-readonly shape its own installer uses
try:
    from platformio import fs
    from platformio.cache import ContentCache
    from platformio.package.manager.base import BasePackageManager
    from platformio.package.manager.library import LibraryPackageManager
    from platformio.package.manager.tool import ToolPackageManager
    from platformio.package.meta import PackageCompatibility

    PARALLEL_AVAILABLE = True
except ImportError as err:  # pragma: no cover
    # A moved pio module must degrade to the serial pass, not kill the
    # image build; the tripwire test makes the drift loud in CI
    PARALLEL_AVAILABLE = False
    IMPORT_ERROR = repr(err)

# Network-bound downloads release the GIL, so the pool oversubscribes
# the cores. This bypasses pio's 500ms registry throttle and races its
# self-unlinking cache LockFiles; both are cache-only and self-healing.
MAX_WORKERS = 16


class CleanupError(RuntimeError):
    """A torn destination could not be removed; the serial pass would
    trust it, so the build must fail rather than bake a corrupt image."""


class LockReleaseError(RuntimeError):
    """The manager lock could not be released; the serial pass would
    block on it, so the build must fail with the cause named."""


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
    # Exact-string dedupe only: name-level dedupe would change which
    # version conflicts the pkg install pass reconciles
    return (
        list(dict.fromkeys(libs)),
        list(dict.fromkeys(platforms)),
        list(dict.fromkeys(tools)),
    )


def piopm_matches(package_dir: str, spec) -> list[Path]:
    """Dirs whose .piopm metadata names this spec; a positive match beats
    guessing the manifest-derived dirname from the registry name."""
    want = (BasePackageManager.ensure_spec(spec).name or "").lower()
    matches: list[Path] = []
    if not want:
        return matches
    try:
        entries = list(Path(package_dir).iterdir())
    except FileNotFoundError:
        return matches
    for d in entries:
        if not d.is_dir():
            continue  # pio's get_installed skips files and *.pio-link too
        try:
            meta = fs.load_json(str(d / ".piopm"))
        except FileNotFoundError:
            continue  # no metadata means pio does not trust it either
        except (OSError, ValueError):
            if d.name.lower() == want:
                # A corrupt .piopm under this spec's own name would crash
                # pio's whole storage scan; remove it
                matches.append(d)
            continue
        mspec = meta.get("spec") or {}
        if (mspec.get("name") or meta.get("name") or "").lower() == want:
            matches.append(d)
    return matches


def remove_dir(spec, dest: Path) -> None:
    # fs.rmtree never raises (errors go to a printing onexc handler);
    # only the destination's absence proves the cleanup worked
    fs.rmtree(str(dest))
    if dest.exists():
        # Failing the build beats baking a corrupt image
        raise CleanupError(
            f"could not remove the failed pre-install of {spec} at {dest}"
        )
    print(f"Removed torn destination {dest}", flush=True)


def cleanup_or_die(mgr, spec) -> None:
    """Cleanup that did not demonstrably succeed must fail the build."""
    try:
        clean_torn(mgr, spec)
    except CleanupError:
        raise
    except Exception as err:  # noqa: BLE001
        raise CleanupError(f"cleanup failed for {spec}: {err!r}") from err


def clean_torn(mgr, spec) -> None:
    """Remove a torn destination so the serial pass cannot trust it."""
    pkg = None
    with suppress(Exception):
        # get_package memoizes a pre-install snapshot; reset to see the
        # torn dir. It also recognizes manifest-only legacy dirs pio's
        # storage scan would trust, which the .piopm fallback cannot see.
        mgr.memcache_reset()
        pkg = mgr.get_package(spec)
    if pkg is not None:
        remove_dir(spec, Path(pkg.path))
    elif dests := piopm_matches(mgr.package_dir, spec):
        # A .piopm naming this spec is the exact shape the serial pass
        # trusts; a dir without one is overwritten by pio's own install
        for dest in dests:
            remove_dir(spec, dest)
    else:
        print(f"No resolvable destination to clean for {spec}", flush=True)


def spec_key(spec) -> str | None:
    """The destination identity of a spec: PlatformIO installs by package
    name, so two specs sharing a name share a directory. ``None`` means
    the name could not be derived; such a spec must stay out of the wave
    (a raw-string key would break the one-per-destination guarantee)."""
    name = BasePackageManager.ensure_spec(spec).name
    return name.lower() if name else None


def dependency_specs(manager, specs: list) -> list:
    """``(spec, compatibility)`` registry dependencies of installed
    packages, from local manifest reads. Name-only dependencies
    (platform-bundled libs like SPI) stay with the ``pkg install`` pass;
    the compatibility qualifiers mirror pio's install_dependency, so a
    qualified dep resolves to the same package the serial pass picks."""
    return [
        (manager.dependency_to_spec(dep), PackageCompatibility.from_dependency(dep))
        for spec in specs
        if (pkg := manager.get_package(spec)) is not None
        for dep in manager.get_pkg_dependencies(pkg) or []
        if dep.get("owner") or dep.get("version")
    ]


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
    # One spec per destination: two threads must not extract into the
    # same directory. Second versions of a name and URL specs (their dir
    # comes from the archive manifest) stay with the pkg install pass.
    seen_names: set = prior_names if prior_names is not None else set()
    # Wave-1 items are strings; dependency waves carry (spec, compatibility)
    pairs = [item if isinstance(item, tuple) else (item, None) for item in specs]
    unique = {}
    for spec, compat in pairs:
        # Normalize once: a dependency's URL version surfaces as spec.uri
        parsed = BasePackageManager.ensure_spec(spec)
        if parsed.uri:
            continue
        if (key := spec_key(parsed)) is None:
            # No name, no destination identity; leave it to the serial pass
            print(f"Skipping unresolvable spec {spec!r} in the wave", flush=True)
            continue
        unique.setdefault(key, (spec, compat))  # first-wins, like pio's walk
    pending = [
        (spec, compat)
        for spec, compat in unique.values()
        if not manager.get_package(spec)
    ]
    if not pending:
        # Nothing to install, but a warm store's dependencies must still
        # feed the next wave (a transitive dep may be missing)
        _next_wave(manager_cls, manager, unique, seen_names)
        return
    workers = min(len(pending), MAX_WORKERS)
    # One manager per worker (_install mutates instance state); built
    # serially because construction rewires the shared manager logger
    managers: queue.SimpleQueue = queue.SimpleQueue()
    for _ in range(workers):
        managers.put(manager_cls(None))
    local = threading.local()

    def install_one(item) -> bool:
        spec, compat = item
        if (mgr := getattr(local, "mgr", None)) is None:
            mgr = local.mgr = managers.get_nowait()
        try:
            mgr._install(  # noqa: SLF001
                spec, skip_dependencies=True, compatibility=compat
            )
            return True
        except Exception as err:  # noqa: BLE001
            print(f"Pre-install of {spec} failed ({err!r})", flush=True)
            cleanup_or_die(mgr, spec)
            return False
        except BaseException:
            # A worker SystemExit (main() guards against it) must not skip
            # the cleanup and leave a torn dir the serial pass trusts
            cleanup_or_die(mgr, spec)
            raise

    print(f"Preinstalling {len(pending)} package(s) with {workers} workers", flush=True)
    # The serial getter calls create pio's lazy dirs (made without
    # exist_ok) before cold-cache workers can race the creation
    manager.get_download_dir()
    manager.get_tmp_dir()
    ContentCache("http")
    cwd = Path.cwd()
    manager.lock()
    try:
        with ThreadPoolExecutor(max_workers=workers) as ex:
            futures = [ex.submit(install_one, item) for item in pending]
        # The with-block joined every future; drain them all so a
        # concurrent CleanupError is never dropped
        errors = [err for f in futures if (err := f.exception()) is not None]
        for err in errors:
            # Every failure is on the record; the raised one is a summary
            print(f"Wave failure: {err!r}", flush=True)
        if errors:
            raise next((e for e in errors if isinstance(e, CleanupError)), errors[0])
        results = [f.result() for f in futures]
    finally:
        try:
            manager.unlock()
        except Exception as unlock_err:  # noqa: BLE001
            # A held flock would hang the serial pass in another process;
            # failing loudly beats an unexplained stuck docker build. Any
            # in-flight error stays attached as the context.
            raise LockReleaseError(
                f"could not release the manager lock: {unlock_err!r}"
            ) from unlock_err
        # Worker postinstall scripts chdir process-wide (pio's fs.cd);
        # restore between waves. The serial pass pins its own cwd.
        with suppress(OSError):
            os.chdir(cwd)
    if failures := len(results) - sum(results):
        # The stock pass retries CLI specs and re-walks installed
        # packages' dependencies, so failed deps retry too
        print(
            f"Pre-install failed for {failures} of {len(results)} package(s); "
            "pkg install retries them serially",
            flush=True,
        )

    # Waves skip dependencies (a shared one must not extract from two
    # threads); the installed manifests feed the next wave
    _next_wave(manager_cls, manager, unique, seen_names)


def _next_wave(manager_cls, manager, unique: dict, seen_names: set) -> None:
    """Queue the dependency wave for every requested spec, installed or
    freshly waved; a warm store can still be missing a transitive dep.
    Terminates without a cap: each wave admits only never-seen names."""
    seen_names.update(unique)
    # The pre-wave get_package calls memoized an empty storage snapshot
    manager.memcache_reset()
    next_specs = [
        item
        for item in dependency_specs(manager, [spec for spec, _ in unique.values()])
        if spec_key(item[0]) not in seen_names
    ]
    if next_specs:
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
    start_cwd = Path.cwd()
    libs, platforms, tools = parse_specs(args.file[0], args)

    # Platforms stay serial: PlatformPackageManager.install runs an
    # on_installed hook the private _install path would skip
    if PARALLEL_AVAILABLE:
        wave_groups = [(ToolPackageManager, tools), (LibraryPackageManager, libs)]
    else:  # pragma: no cover
        wave_groups = []
        print(
            f"PlatformIO layout changed ({IMPORT_ERROR}); serial install only",
            flush=True,
        )
    for manager_cls, specs in wave_groups:
        try:
            parallel_install(manager_cls, specs)
        except (CleanupError, LockReleaseError, KeyboardInterrupt):
            # A torn package or a held lock must fail the build
            raise
        except BaseException:  # noqa: BLE001
            # BaseException: a worker postinstall's SystemExit must not
            # skip the authoritative serial pass (partial deps, exit 0)
            print("Parallel preinstall failed, falling back to serial", flush=True)
            traceback.print_exc()

    # Postinstall scripts chdir process-wide (pio's fs.cd captures its
    # restore path at construction); pin the authoritative pass's cwd
    subprocess.check_call(
        ["platformio", "pkg", "install", "-g", *build_cli_args(libs, platforms, tools)],
        close_fds=False,
        cwd=start_cwd,
    )


if __name__ == "__main__":
    main()
