#!/usr/bin/env python3
# This script is used to preinstall
# all platformio libraries in the global storage

import argparse
from concurrent.futures import ThreadPoolExecutor
import configparser
import json
import os
from pathlib import Path
import queue
import stat
import subprocess
import threading
import time
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
    import semantic_version

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


# A spared other-version dir must carry one of pio's manifest files to
# count as intact rather than half-deleted
_MANIFEST_NAMES = (
    "package.json",
    "library.json",
    "library.properties",
    "platform.json",
    "module.json",
)


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


def manifest_present(d: Path) -> bool:
    """Whether the dir carries one of pio's manifest files. Path.is_file
    reads any OSError as "absent"; a stat error is unverifiable instead."""
    for m in _MANIFEST_NAMES:
        try:
            os.lstat(d / m)
        except FileNotFoundError:
            continue
        except OSError as err:
            raise CleanupError(
                f"could not check for a manifest in {d}: {err!r}"
            ) from err
        return True
    return False


def piopm_matches(package_dir: str, spec) -> list[Path]:
    """Dirs whose .piopm metadata names this spec; a positive match beats
    guessing the manifest-derived dirname from the registry name."""
    parsed = BasePackageManager.ensure_spec(spec)
    want = (parsed.name or "").lower()
    matches: list[Path] = []
    try:
        entries = list(Path(package_dir).iterdir())
    except FileNotFoundError:
        return matches
    except OSError as err:
        # Cannot look means cannot verify; the serial pass could trust
        # whatever is in here
        raise CleanupError(
            f"could not scan {package_dir} while cleaning up {spec}: {err!r}"
        ) from err
    for d in entries:
        try:
            entry_stat = os.lstat(d)
        except FileNotFoundError:
            continue  # vanished mid-scan; nothing left to clean
        except OSError as err:
            # Path.is_dir would read this as skippable; unverifiable
            # entries must fail the build like everywhere else
            raise CleanupError(
                f"could not stat {d} while cleaning up {spec}: {err!r}"
            ) from err
        if not stat.S_ISDIR(entry_stat.st_mode):
            continue  # pio's get_installed skips files and *.pio-link too
        meta = None
        last_err = None
        for attempt in range(3):
            try:
                meta = json.loads((d / ".piopm").read_text(encoding="utf-8"))
                last_err = None
                break
            except FileNotFoundError:
                last_err = None
                break  # no metadata means pio does not trust it either
            except (OSError, ValueError) as err:
                # Possibly another worker mid-copy of the tiny file
                last_err = err
                if attempt < 2:
                    time.sleep(0.1 * (2**attempt))
        if meta is None:
            if last_err is not None and d.name.lower() == want:
                # A corrupt .piopm crashes pio's own storage scan, so the
                # serial pass could not even run over it; remove it, but
                # only under this spec's own name (an innocent worker may
                # be mid-copy of another package's metadata)
                print(
                    f"Removing unreadable-metadata dir {d} ({last_err!r})", flush=True
                )
                matches.append(d)
            continue
        mspec = meta.get("spec") or {}
        name = (mspec.get("name") or meta.get("name") or "").lower()
        owner = (mspec.get("owner") or "").lower()
        if not want or name != want:
            continue
        if parsed.owner and owner != parsed.owner.lower():
            continue
        version = str(meta.get("version") or "")
        if parsed.requirements and version:
            try:
                other_version = (
                    semantic_version.Version.coerce(version) not in parsed.requirements
                )
            except ValueError:
                other_version = False  # unparsable version stays a candidate
            if other_version and manifest_present(d):
                # An intact other version this run never tried to write
                continue
            # An other-version dir without a manifest is a half-deleted
            # tree (an interrupted detach); it stays a candidate
        matches.append(d)
    return matches


def remove_dir(spec, dest: Path) -> None:
    # fs.rmtree never raises (errors go to a printing onexc handler), and
    # Path.exists suppresses stat errors; only a confirmed ENOENT proves
    # the cleanup worked
    fs.rmtree(str(dest))
    try:
        os.lstat(dest)
    except FileNotFoundError:
        print(f"Removed torn destination {dest}", flush=True)
        return
    except OSError as err:
        raise CleanupError(
            f"could not verify removal of {spec} at {dest} ({err!r})"
        ) from err
    # Failing the build beats baking a corrupt image
    raise CleanupError(f"could not remove the failed pre-install of {spec} at {dest}")


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
    scan_err = None
    pkg = None
    for attempt in range(5):
        try:
            # get_package memoizes a pre-install snapshot; without a
            # reset it cannot see the torn directory
            mgr.memcache_reset()
            pkg = mgr.get_package(spec)
            scan_err = None
            break
        except Exception as err:  # noqa: BLE001
            # Likely another worker's in-flight copy; back off and retry
            scan_err = err
            if attempt < 4:
                time.sleep(0.2 * (2**attempt))
    if pkg is not None:
        dest = Path(pkg.path)
        store = Path(os.path.realpath(mgr.package_dir))
        resolved = Path(os.path.realpath(dest))
        if resolved == store or not resolved.is_relative_to(store):
            # A symlinked (.pio-link) package resolves to its external
            # source dir; this script only removes strictly inside the
            # store, never the store itself
            print(f"Refusing to remove {dest} outside {store}", flush=True)
            return
        remove_dir(spec, dest)
        return
    # The scan verdict depends on the whole storage tree (or the manifest
    # is unparsable); judge by this spec's own footprint so an innocent
    # worker's in-flight copy cannot fail the build naming the wrong spec.
    # A .piopm naming this spec is the exact shape the serial pass trusts;
    # a dir without one is overwritten by pio's own install either way.
    if dests := piopm_matches(mgr.package_dir, spec):
        for dest in dests:
            remove_dir(spec, dest)
    elif scan_err is not None:
        print(
            f"Could not inspect failed pre-install of {spec} "
            f"({scan_err!r}); no destination to clean",
            flush=True,
        )
    else:
        print(f"No resolvable destination to clean for {spec}", flush=True)


def spec_key(spec) -> str | None:
    """The destination identity of a spec: PlatformIO installs by package
    name, so two specs sharing a name share a directory. ``None`` means
    the name could not be derived; such a spec must stay out of the wave
    (a raw-string key would break the one-per-destination guarantee)."""
    name = BasePackageManager.ensure_spec(spec).name
    return name.lower() if name else None


def dependency_specs(manager, specs: list, failed: set) -> list:
    """``(spec, compatibility)`` registry dependencies of installed packages.

    Local manifest reads only. Name-only dependencies (platform-bundled
    libs like SPI) are left to the ``pkg install`` pass, and a versioned
    owner-less name is skipped when the manager recognizes it as built-in,
    exactly like pio's install_dependency (built-ins only resolve once
    platforms are in the store, e.g. on a warm run). The compatibility
    qualifiers mirror install_dependency, so a qualified dep resolves to
    the same package the serial pass picks."""
    deps = []
    is_builtin = getattr(manager, "is_builtin_lib", None)
    for spec in specs:
        if (pkg := manager.get_package(spec)) is None:
            if spec_key(spec) not in failed:
                # Everything but a failed install resolved moments ago;
                # losing it now is an anomaly, and its subtree quietly
                # falls to the serial pass
                print(
                    f"Installed {spec} is not resolvable afterwards; its "
                    "dependencies are left to pkg install",
                    flush=True,
                )
            continue
        for dep in manager.get_pkg_dependencies(pkg) or []:
            if not dep.get("owner") and not dep.get("version"):
                continue  # bare platform-bundled names stay with pkg install
            dspec = manager.dependency_to_spec(dep)
            if (
                is_builtin is not None
                and not dspec.external
                and not dspec.owner
                and is_builtin(dspec.name)
            ):
                continue  # pio's install_dependency skips known built-ins
            deps.append((dspec, PackageCompatibility.from_dependency(dep)))
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
        _next_wave(manager_cls, manager, unique, [], [], seen_names)
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
        except (AttributeError, TypeError) as err:
            # A pio API break, not a flaky package; clean, then surface it
            print(f"Pre-install of {spec} hit an API break ({err!r})", flush=True)
            cleanup_or_die(mgr, spec)
            raise
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
    # pio creates these lazily without exist_ok; touch them serially so
    # cold-cache workers never race the creation
    for lazy_dir in (manager.get_download_dir(), manager.get_tmp_dir()):
        Path(lazy_dir).mkdir(parents=True, exist_ok=True)
    ContentCache("http")
    cwd = Path.cwd()
    unlock_error = None
    body_error: BaseException | None = None
    manager.lock()
    try:
        futures = []
        submit_error = None
        with ThreadPoolExecutor(max_workers=workers) as ex:
            try:
                for item in pending:
                    # An explicit append: every submitted future must be
                    # drained even when a later submit raises
                    futures.append(ex.submit(install_one, item))  # noqa: PERF401
            except BaseException as err:  # noqa: BLE001
                # Already-submitted workers run to completion in __exit__;
                # their CleanupErrors must still be drained below
                submit_error = err
        # The with-block joined every future; drain them all so a
        # concurrent CleanupError is never dropped
        errors = [err for f in futures if (err := f.exception()) is not None]
        for err in errors:
            # Every failure is on the record; the raised one is a summary
            print(f"Wave failure: {err!r}", flush=True)
        for err in errors:
            if isinstance(err, CleanupError):
                raise err
        if submit_error is not None:
            raise submit_error
        if errors:
            raise errors[0]
        results = [f.result() for f in futures]
    except BaseException as err:
        body_error = err
        raise
    finally:
        # Neither cleanup may replace an in-flight CleanupError
        try:
            manager.unlock()
        except Exception as err:  # noqa: BLE001
            unlock_error = err
            print(f"Could not release the manager lock ({err!r})", flush=True)
            if body_error is not None and not isinstance(
                body_error, (CleanupError, KeyboardInterrupt)
            ):
                # Any body error main() would swallow (SystemExit included)
                # falls back to a serial pass that blocks on the held
                # flock for its full timeout; the held lock is worse
                raise LockReleaseError(
                    f"could not release the manager lock ({err!r}) after "
                    f"a wave failure ({body_error!r})"
                ) from err
            if body_error is not None:
                # Build-fatal classes stay in flight; record the second fault
                body_error.add_note(
                    f"also: could not release the manager lock ({err!r})"
                )
        # Worker postinstall scripts chdir process-wide (pio's fs.cd);
        # restore between waves, not only for the final subprocess
        try:
            os.chdir(cwd)
        except OSError as chdir_err:
            print(f"Could not restore the working dir ({chdir_err!r})", flush=True)
            if body_error is None and unlock_error is None:
                # Further waves cannot run from an unknown cwd; the serial
                # pass is safe either way (its cwd is pinned)
                raise
            # A pending LockReleaseError must still win the raise
            (body_error or unlock_error).add_note(
                f"also: could not restore the working dir ({chdir_err!r})"
            )
    if unlock_error is not None:
        # A held flock would hang the serial pass in another process;
        # failing loudly beats an unexplained stuck docker build
        raise LockReleaseError(
            f"could not release the manager lock: {unlock_error!r}"
        ) from unlock_error
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
    _next_wave(manager_cls, manager, unique, pending, results, seen_names)


def _next_wave(
    manager_cls, manager, unique: dict, pending: list, results: list, seen_names: set
) -> None:
    """Queue the dependency wave for every requested spec, installed or
    freshly waved; a warm store can still be missing a transitive dep."""
    seen_names.update(unique)
    # The pre-wave get_package calls memoized an empty storage snapshot
    manager.memcache_reset()
    failed = {
        spec_key(spec) for (spec, _), ok in zip(pending, results, strict=True) if not ok
    }
    next_specs = [
        item
        for item in dependency_specs(
            manager, [spec for spec, _ in unique.values()], failed
        )
        if spec_key(item[0]) not in seen_names
    ]
    if not next_specs:
        return
    if len(seen_names) < 200:  # cycle backstop
        parallel_install(manager_cls, next_specs, seen_names)
    else:
        # Truncation must be visible, not an unexplained slow serial pass
        print(
            f"Dependency wave limit reached; {len(next_specs)} spec(s) "
            "left to pkg install",
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
