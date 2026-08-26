"""Parallel prefetch and install of the packages a PlatformIO run needs.

Downloads the archives concurrently into PlatformIO's own download cache
(identical ``compute_download_path`` keys), then installs them through
PlatformIO's own ``_install`` with one worker per usable core, so
extraction (the serial, single-core half of a cold install) parallelizes
too and ``pio run`` finds every package already installed. Runs in a
subprocess like all PlatformIO execution: loading a platform executes its
code (pioarduino's penv setup rewrites ``sys.path``). A sentinel in the
build dir lets warm builds skip the spawn. Best-effort: any failure logs
and PlatformIO downloads and installs as before. Across processes sharing
a core dir, registry downloads share the cache key and rely on sha256
verification; URL downloads carry no checksum, so they are serialized by
a file lock and promoted with an atomic rename.
"""

from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor
from contextlib import suppress
import hashlib
import json
import logging
import os
from pathlib import Path
from queue import SimpleQueue
import signal
import subprocess
import sys
import threading
import time
from typing import Any, NamedTuple

from esphome.framework_helpers import (
    content_length,
    discard_partial_download,
    failure_reason,
    resume_fetch_job,
    run_batch_downloads,
    warn_prefetch_failures,
)
from esphome.helpers import get_bool_env, get_usable_cpu_count, rmtree

_LOGGER = logging.getLogger(__name__)

# Concurrent registry resolutions / HEAD probes (each is network-bound)
_RESOLVE_WORKERS = 8

# A hung child must not block the build; downloads resume on the next run
_PREFETCH_TIMEOUT = 20 * 60

# Waiting on another process's URL download; past this, leave it to pio
_URI_LOCK_TIMEOUT = 60

# Short lock-acquire slices so a waiting worker still observes Ctrl-C
_URI_LOCK_POLL = 1

# Resolution errored (vs a clean skip); suppresses the warm sentinel
_RESOLVE_FAILED = object()


class _Resolved(NamedTuple):
    """A registry spec resolved to its archive; ``cached`` skips the download."""

    spec: Any
    name: str
    size: int
    url: str
    dl_path: Path
    checksum: str
    cached: bool


# Child records a no-work run; the parent skips the next spawn while valid
_SENTINEL_NAME = ".esphome_prefetch.json"
_SENTINEL_SCHEMA = 1


def _ini_sha256(build_dir: Path) -> str:
    return hashlib.sha256((build_dir / "platformio.ini").read_bytes()).hexdigest()


def _sentinel_state(build_dir: Path) -> dict[str, Any]:
    """The environment fingerprint a sentinel must match to stay valid."""
    # Same fingerprint as the heal stamp: the sentinel's dirs die with its wipe
    from esphome.platformio.toolchain import current_python_minor

    return {
        "schema": _SENTINEL_SCHEMA,
        "ini_sha256": _ini_sha256(build_dir),
        "python": current_python_minor(),
        "core_dir_env": os.environ.get("PLATFORMIO_CORE_DIR", ""),
    }


def _prefetch_is_warm(build_dir: Path) -> bool:
    """Whether the last prefetch found nothing to do and nothing changed since."""
    try:
        data = json.loads((build_dir / _SENTINEL_NAME).read_text(encoding="utf-8"))
        dirs = data.pop("dirs")
        return (
            data == _sentinel_state(build_dir)
            and bool(dirs)
            and all(Path(d).is_dir() for d in dirs)
        )
    except FileNotFoundError:
        return False
    except (OSError, ValueError, KeyError, AttributeError, TypeError):
        _LOGGER.debug("Ignoring invalid prefetch sentinel", exc_info=True)
        return False


def prefetch_platformio_packages() -> None:
    """Warm PlatformIO's download cache for the current project, in parallel."""
    from esphome.core import CORE
    from esphome.platformio.toolchain import (
        default_libdeps_dir,
        heal_platformio_python_env,
    )

    # Heal first: its Python-version wipe would discard freshly warmed
    # caches and the sentinel's dirs (the later heal call is a no-op)
    heal_platformio_python_env()
    build_dir = Path(CORE.build_path)
    if _prefetch_is_warm(build_dir):
        return
    # The child is esphome itself: PYTHONPATH stays so it imports this
    # tree's esphome (tests/integration pins the source tree through it)
    env = dict(os.environ)
    # Must match run_platformio_cli's default or warm builds re-resolve
    # every library
    env.setdefault("PLATFORMIO_LIBDEPS_DIR", default_libdeps_dir())
    # -v/-vv must reach the child's debug logging or the swallowed
    # failure detail is undiagnosable in the field
    env["ESPHOME_PREFETCH_LOG_LEVEL"] = str(logging.getLogger().getEffectiveLevel())
    if CORE.dashboard:
        # The child's progress bar and log escaping key off CORE.dashboard
        env["ESPHOME_PREFETCH_DASHBOARD"] = "1"
    cmd = [
        sys.executable,
        "-m",
        "esphome.platformio.prefetch",
        str(build_dir),
        CORE.name,
    ]
    try:
        # Not a with-block: the lifetime spans the wait/terminate arms
        proc = subprocess.Popen(cmd, env=env)  # pylint: disable=consider-using-with
    except Exception as err:  # noqa: BLE001  # pylint: disable=broad-exception-caught
        # The prefetch must never become a new way for the build to fail
        _LOGGER.warning("PlatformIO package prefetch skipped: %s", failure_reason(err))
        _LOGGER.debug("Prefetch failure detail", exc_info=True)
        return
    try:
        returncode = proc.wait(timeout=_PREFETCH_TIMEOUT)
    except subprocess.TimeoutExpired:
        _stop_child(proc)
        _LOGGER.warning("PlatformIO package prefetch timed out; continuing without it")
        return
    except BaseException:
        # SIGKILL (subprocess.run's choice on interrupt) could land inside
        # a package-directory copy pio run would then trust; ask first
        _stop_child(proc)
        raise
    if returncode != 0:
        _LOGGER.warning("PlatformIO package prefetch skipped (exit %d)", returncode)


def _stop_child(proc: subprocess.Popen) -> None:
    """Stop the child without cutting an in-flight package install short.

    On a terminal interrupt the child shares the process group and is
    already unwinding from its own SIGINT, so wait briefly first. SIGTERM
    then triggers the handler main() installs (a clean unwind through the
    pool's cancellation); the grace period covers one large extraction.
    Kill only a child that will not stop, with a bounded reap, and never
    let a second interrupt escape and orphan the child mid-copy.
    """
    try:
        with suppress(subprocess.TimeoutExpired):
            proc.wait(timeout=5)
            return
        proc.terminate()
        with suppress(subprocess.TimeoutExpired):
            proc.wait(timeout=30)
            return
        proc.kill()
        proc.wait(timeout=5)
    except BaseException:  # noqa: BLE001  # pylint: disable=broad-exception-caught
        _LOGGER.debug("Stopping the prefetch child failed", exc_info=True)


def _project_platform_and_config(ini: Path, env: str) -> tuple[str | None, Any]:
    """The env's platform spec and the ProjectConfig for the given ini."""
    from platformio import app
    from platformio.project.config import ProjectConfig

    # PlatformBase.config reads the default ProjectConfig; it must see
    # this ini's env options
    app.set_session_var("custom_project_conf", str(ini))
    config = ProjectConfig.get_instance(str(ini))
    return config.get(f"env:{env}", "platform", None), config


def _manager_kwargs(manager: Any) -> dict[str, Any]:
    """Constructor kwargs that make a sibling manager equivalent to the shared one."""
    if compatibility := getattr(manager, "compatibility", None):
        return {"compatibility": compatibility}
    return {}


def _registry_jobs(
    manager: Any, specs: list[Any], seen: set[str]
) -> tuple[list[tuple[str, int, Any]], int, list[tuple[str, Any]]]:
    """Resolve registry specs to ``(name, size, fetch)`` batch jobs.

    Mirrors PlatformIO's install path: best version, systype file, first
    mirror, and the same sha1(url + checksum) download-cache key. Also
    returns how many resolutions errored (a clean skip is not an error)
    and the ``(name, spec)`` pairs whose archives will be installable.
    """
    from platformio.registry.mirror import RegistryFileMirrorIterator

    local = threading.local()
    errors: list[str] = []

    def _resolve(spec) -> _Resolved | object | None:
        # One manager (and registry HTTP session) per worker thread;
        # installed-state was already checked on the shared manager
        if (mgr := getattr(local, "mgr", None)) is None:
            mgr = local.mgr = manager.__class__(
                manager.package_dir, **_manager_kwargs(manager)
            )
        try:
            packages = mgr.search_registry_packages(spec)
            if not packages:
                return None  # unknown to the registry; let PlatformIO report it
            package, version = mgr.find_best_registry_version(packages, spec)
            if not package or not version:
                return None
            pkgfile = mgr.pick_compatible_pkg_file(version["files"])
            if not pkgfile:
                return None
            url, checksum = next(RegistryFileMirrorIterator(pkgfile["download_url"]))
            checksum = checksum or pkgfile["checksum"]["sha256"]
            dl_path = Path(mgr.compute_download_path(url, checksum))
            cached = dl_path.is_file()  # fetched by an earlier run
            size = pkgfile.get("size")
            if not cached and not size:
                return None  # no size, no bar share; PlatformIO fetches it
            name = f"{package['name']}@{version['name']}"
            return _Resolved(spec, name, size or 0, url, dl_path, checksum, cached)
        except Exception as err:  # noqa: BLE001  # pylint: disable=broad-exception-caught
            # One flaky spec must not discard the rest of the batch
            _LOGGER.debug("Could not resolve %s", spec, exc_info=True)
            errors.append(failure_reason(err))
            return _RESOLVE_FAILED

    # Serial disk lookups on the shared manager: a fully warm build
    # resolves nothing, and duplicate specs resolve once
    unique: dict[tuple[str | None, str, str], Any] = {}
    for s in specs:
        if not s.uri and not manager.get_package(s):
            unique.setdefault((s.owner, s.name, str(s.requirements)), s)
    pending = list(unique.values())
    if not pending:
        return [], 0, []
    # Serial resolutions (registry GET + mirror HEAD each) dominate
    with ThreadPoolExecutor(max_workers=min(_RESOLVE_WORKERS, len(pending))) as ex:
        results = list(ex.map(_resolve, pending))
    jobs: list[tuple[str, int, Any]] = []
    installable: list[tuple[str, Any]] = []
    for res in results:
        if res is None or res is _RESOLVE_FAILED:
            continue
        installable.append((res.name, res.spec))
        if res.cached or str(res.dl_path) in seen:
            continue  # already fetched, or a duplicate must not share a .part
        seen.add(str(res.dl_path))
        jobs.append(
            (
                res.name,
                res.size,
                resume_fetch_job(
                    res.url, res.dl_path, sha256=res.checksum, size=res.size
                ),
            )
        )
    if failed := len(errors):
        # Visible once per build, naming a cause so an API break does not
        # read as an outage; per-spec detail stays at debug
        _LOGGER.warning(
            "Could not resolve %d of %d PlatformIO package(s) (%s); "
            "PlatformIO will download them serially",
            failed,
            len(pending),
            errors[0],
        )
    return jobs, failed, installable


def _uri_jobs(
    manager: Any, specs: list[Any], seen: set[str]
) -> tuple[list[tuple[str, int, Any]], int, list[tuple[str, Any]]]:
    """Jobs for direct-URL specs; a HEAD sizes each for the combined bar.

    Also returns how many HEAD probes errored (an absent length is not an
    error) and the ``(name, spec)`` pairs whose archives will be
    installable.
    """
    from esphome.net_retry import fetch_with_retry, http_request

    candidates: list[tuple[str, str, Path, Any]] = []
    installable: list[tuple[str, Any]] = []
    for spec in specs:
        url = spec.uri
        if not url or not url.startswith(("http://", "https://")):
            continue  # git+/file specs are cloned/copied, not downloaded
        if url.split("#", 1)[0].endswith(".git"):
            continue  # bare-URL VCS spec; PlatformIO clones it
        if manager.get_package(spec):
            continue
        name = spec.name or url.rsplit("/", 1)[-1]
        # PlatformIO downloads URL specs with no checksum
        dl_path = Path(manager.compute_download_path(url, ""))
        if dl_path.is_file():
            installable.append((name, spec))  # fetched by an earlier run
            continue
        if str(dl_path) in seen:
            continue  # another spec already claimed this .part
        seen.add(str(dl_path))
        candidates.append((name, url, dl_path, spec))

    def _head_size(url: str) -> int:
        try:
            resp = fetch_with_retry(url, lambda: http_request("HEAD", url, timeout=30))
        except Exception:  # noqa: BLE001  # pylint: disable=broad-exception-caught
            _LOGGER.debug("HEAD %s failed", url, exc_info=True)
            return -1
        if not resp.ok:
            # An error page's Content-Length is not a download size
            _LOGGER.debug("HEAD %s returned %s", url, resp.status_code)
            if resp.status_code in (408, 429) or resp.status_code >= 500:
                return -1  # transient; must not be cached as warm
            if resp.status_code in (405, 501):
                # HEAD unsupported but GET may work: a clean skip, or the
                # sentinel would never be written again
                return 0
            # A real URL problem (401/403/404): name it, but still a clean
            # skip so the warm sentinel is not disabled forever
            _LOGGER.warning("HEAD %s returned %s", url, resp.status_code)
            return 0
        return content_length(resp)

    if not candidates:
        return [], 0, installable
    with ThreadPoolExecutor(max_workers=min(_RESOLVE_WORKERS, len(candidates))) as ex:
        sizes = list(ex.map(_head_size, [url for _, url, _, _ in candidates]))
    jobs: list[tuple[str, int, Any]] = []
    failed = 0
    for (name, url, dl_path, spec), size in zip(candidates, sizes, strict=True):
        if size < 0:
            failed += 1
        elif size:
            jobs.append((name, size, _uri_fetch_job(url, dl_path, size)))
            installable.append((name, spec))
    if failed:
        _LOGGER.warning(
            "Could not size %d of %d PlatformIO package URL(s); "
            "PlatformIO will download them serially",
            failed,
            len(candidates),
        )
    return jobs, failed, installable


def _uri_fetch_job(url: str, dl_path: Path, size: int) -> Any:
    """Fetch to a locked staging path, then rename into the cache.

    URL specs carry no checksum, so a shared destination could let two
    processes interleave a same-length corrupt file into the cache. The
    lock makes the staging file single-writer (a stable name keeps the
    resume machinery working across interrupted runs) and the rename
    makes the promotion atomic.
    """
    tmp = dl_path.with_name(f"{dl_path.name}.prefetch")
    fetch = resume_fetch_job(url, tmp, size=size)

    def run(tracker: Any) -> None:
        from filelock import FileLock, Timeout

        # fallback_to_soft would leave a stale marker on lock-less
        # filesystems that blocks every later build (see git.py)
        lock = FileLock(f"{tmp}.lock", fallback_to_soft=False)
        deadline = time.monotonic() + _URI_LOCK_TIMEOUT
        while True:
            try:
                lock.acquire(timeout=_URI_LOCK_POLL)
                break
            except Timeout:
                # The tracker raises when the batch is cancelled, so a
                # parked worker still observes Ctrl-C; a raise past the
                # deadline makes the skip a counted, warned failure
                tracker(0)
                if time.monotonic() >= deadline:
                    raise TimeoutError(
                        "timed out waiting for another download of the same file"
                    ) from None
        try:
            if dl_path.is_file():
                # Another process finished it while we waited; the staging
                # files are dead weight PlatformIO's cache never prunes
                discard_partial_download(tmp)
                return
            fetch(tracker)
            tmp.replace(dl_path)
        finally:
            lock.release()

    return run


def _dependency_entries(
    manager: Any, entries: list[tuple[str, Any]], seen_names: set[str]
) -> list[tuple[str, Any]]:
    """Registry dependencies of the installed entries, one per new name.

    Local manifest reads only; name-only platform libs stay with pio run.
    """
    from platformio.package.meta import PackageCompatibility

    compatibility = getattr(manager, "compatibility", None)
    is_builtin = getattr(manager, "is_builtin_lib", None)
    deps: dict[str, Any] = {}
    for _name, spec in entries:
        if (pkg := manager.get_package(spec)) is None:
            continue
        for dep in manager.get_pkg_dependencies(pkg) or []:
            if not (dep.get("owner") or dep.get("version")):
                continue
            if compatibility and not PackageCompatibility.from_dependency(
                dep
            ).is_compatible(compatibility):
                continue  # pio's install_dependency would skip it too
            dspec = manager.dependency_to_spec(dep)
            if (
                is_builtin
                and not dspec.owner
                and not dspec.external
                and is_builtin(dspec.name)
            ):
                # pio's LibraryPackageManager.install_dependency skips
                # builtins; a registry copy would shadow the bundled one
                continue
            if (key := (dspec.name or "").lower()) and key not in seen_names:
                if manager.get_package(dspec) is not None:
                    continue  # already installed
                deps.setdefault(key, (dspec.name, dspec))
    return list(deps.values())


def _preinstall(
    manager: Any, entries: list[tuple[str, Any]], seen_names: set[str] | None = None
) -> None:
    """Install downloaded packages in parallel via PlatformIO's own ``_install``.

    ``entries`` are ``(name, spec)`` pairs, one per destination directory.
    The manager's lock is held once; each wave skips dependencies, then the
    installed manifests feed the next wave until nothing new appears. Any
    failure leaves that package to pio run's serial installer.
    """
    workers = min(get_usable_cpu_count() or 4, len(entries))
    # One manager per worker (_install mutates instance state); built
    # serially because construction rewires the shared manager logger
    managers: SimpleQueue = SimpleQueue()
    for _ in range(workers):
        managers.put(manager.__class__(manager.package_dir, **_manager_kwargs(manager)))
    local = threading.local()

    def _install_one(entry) -> bool:
        name, spec = entry
        if (mgr := getattr(local, "mgr", None)) is None:
            # at most `workers` pool threads, one dequeue each
            mgr = local.mgr = managers.get_nowait()
        try:
            mgr._install(spec, skip_dependencies=True)  # pylint: disable=protected-access  # noqa: SLF001
            return True
        except Exception as err:  # noqa: BLE001  # pylint: disable=broad-exception-caught
            _LOGGER.warning("Could not pre-install %s: %s", name, failure_reason(err))
            _LOGGER.debug("Pre-install failure detail", exc_info=True)
            # A post-copy failure leaves a package pio run would trust;
            # remove it so pio run genuinely reinstalls it
            try:
                mgr.memcache_reset()
                if (pkg := mgr.get_package(spec)) is not None:
                    rmtree(pkg.path)
                else:
                    _LOGGER.debug("No on-disk install of %s to remove", name)
            except Exception as cleanup_err:  # noqa: BLE001  # pylint: disable=broad-exception-caught
                _LOGGER.warning(
                    "Could not remove the failed install of %s: %s",
                    name,
                    failure_reason(cleanup_err),
                )
            return False

    _LOGGER.info(
        "Installing %d PlatformIO package(s) with %d extraction worker(s): %s",
        len(entries),
        workers,
        ", ".join(name for name, _ in entries),
    )
    # Postinstall scripts chdir process-globally; the cwd is restored
    # after the pool. Concurrent postinstalls can still race pio's
    # non-reentrant fs.cd mid-pool; that install fails, warns, and is
    # redone serially by pio run. Suppress interleaved progress bars.
    os.environ.setdefault("PLATFORMIO_DISABLE_PROGRESSBAR", "true")
    cwd = Path.cwd()
    manager.lock()
    try:
        with ThreadPoolExecutor(max_workers=workers) as ex:
            try:
                results = list(ex.map(_install_one, entries))
            except BaseException:
                # Drop queued installs; in-flight ones finish so no
                # package directory is left half copied
                ex.shutdown(wait=True, cancel_futures=True)
                raise
    finally:
        # Cleanup must not mask an in-flight exception or skip a step
        with suppress(Exception):
            manager.memcache_reset()
        try:
            manager.unlock()
        finally:
            os.chdir(cwd)
    if not any(results):
        # A systematic fault, not one bad archive; pio run installs serially
        _LOGGER.warning(
            "Could not pre-install any of %d PlatformIO package(s)", len(entries)
        )

    seen = seen_names if seen_names is not None else set()
    seen.update(name.split("@", 1)[0].lower() for name, _ in entries)
    next_entries = _dependency_entries(manager, entries, seen)
    if next_entries and len(seen) < 200:  # cycle backstop
        _preinstall(manager, next_entries, seen)


def _prefetch(build_dir: Path, env: str) -> None:
    from platformio.dependencies import get_core_dependencies
    from platformio.package.manager.library import LibraryPackageManager
    from platformio.package.manager.platform import PlatformPackageManager
    from platformio.package.meta import PackageCompatibility, PackageSpec
    from platformio.platform.factory import PlatformFactory

    platform_spec, config = _project_platform_and_config(
        build_dir / "platformio.ini", env
    )
    if not platform_spec:
        return

    # The platform (manifest plus build scripts) installs first and
    # resolves the rest. Its setup may rewrite sys.path (pioarduino's penv
    # setup does); restore it so later imports here still resolve.
    saved_sys_path = list(sys.path)
    pm = PlatformPackageManager()
    pkg = pm.install(platform_spec, skip_dependencies=True)
    p = PlatformFactory.new(pkg)
    p.configure_project_packages(env, ["run"])
    sys.path[:] = saved_sys_path

    specs = [
        p.get_package_spec(name)
        for name, opts in p.packages.items()
        if not opts.get("optional")
    ]
    # PIO's build engine installs outside the platform package list;
    # skipped when the platform lists it itself
    if not any(s.name == "tool-scons" for s in specs):
        specs.append(
            PackageSpec(
                owner="platformio",
                name="tool-scons",
                requirements=get_core_dependencies()["tool-scons"],
            )
        )
    lib_deps = config.get(f"env:{env}", "lib_deps", [])
    # pio run's storage dir for this env, with its compatibility
    # qualifiers: an unqualified library install could land a different
    # owner's package pio run would then trust
    qualifiers: dict[str, Any] = {"platforms": [p.name]}
    if framework := config.get(f"env:{env}", "framework", None):
        qualifiers["frameworks"] = framework
    libdeps_dir = Path(config.get("platformio", "libdeps_dir")) / env
    lm = LibraryPackageManager(
        str(libdeps_dir), compatibility=PackageCompatibility(**qualifiers)
    )
    # pio run skips owner-less non-external lib_deps entirely (a bare
    # name like WiFi is a framework built-in or private library);
    # resolving one from the registry would shadow the bundled copy
    lib_specs = [
        spec
        for dep in lib_deps
        if dep and not dep.startswith("$")
        if (spec := PackageSpec(dep)).external or spec.owner
    ]

    seen: set[str] = set()
    jobs: list[tuple[str, int, Any]] = []
    groups: list[tuple[Any, list[tuple[str, Any]]]] = []
    unresolved = 0
    for mgr, batch in ((p.pm, specs), (lm, lib_specs)):
        entries: list[tuple[str, Any]] = []
        for build_jobs in (_registry_jobs, _uri_jobs):
            batch_jobs, failed, installable = build_jobs(mgr, batch, seen)
            jobs += batch_jobs
            unresolved += failed
            entries += installable
        if entries:
            groups.append((mgr, entries))

    sentinel = build_dir / _SENTINEL_NAME
    failed_names: set[str] = set()
    if jobs:
        sentinel.unlink(missing_ok=True)
        _LOGGER.info(
            "Prefetching %d PlatformIO package(s): %s",
            len(jobs),
            ", ".join(name for name, _, _ in jobs),
        )
        # PlatformIO retries failed packages itself, without resume
        failures = run_batch_downloads("Downloading PlatformIO packages", jobs)
        warn_prefetch_failures(failures)
        failed_names = {name for name, _ in failures}
    elif not groups and not unresolved:
        # Record the no-work run so the parent skips the next spawn.
        # A failed resolution is not "no work": a registry outage must
        # not be cached as warm.
        dirs = [config.get("platformio", "packages_dir")]
        if lib_specs:
            dirs.append(str(libdeps_dir))
        sentinel.write_text(
            json.dumps({**_sentinel_state(build_dir), "dirs": dirs}),
            encoding="utf-8",
        )

    for mgr, entries in groups:
        # One install per destination: pio derives the directory from the
        # package name, so key on the name part (a registry entry's display
        # name carries a version suffix)
        to_install = {
            name.split("@", 1)[0].lower(): (name, spec)
            for name, spec in entries
            if name not in failed_names
        }
        if to_install:
            try:
                _preinstall(mgr, list(to_install.values()))
            except Exception as err:  # noqa: BLE001  # pylint: disable=broad-exception-caught
                # Each group degrades independently; pio run installs
                # whatever this one did not
                _LOGGER.warning(
                    "Pre-install failed for a package group: %s",
                    failure_reason(err),
                )
                _LOGGER.debug("Pre-install group failure detail", exc_info=True)


def _sigterm(_signum, _frame) -> None:
    # Raised in the main thread: the pool's BaseException arm cancels
    # queued installs while in-flight copies finish, then finally runs
    raise SystemExit(143)


def main(argv: list[str]) -> int:
    """Subprocess entry point: ``prefetch <build_dir> <env_name>``."""
    from esphome.core import CORE
    from esphome.log import setup_log

    signal.signal(signal.SIGTERM, _sigterm)
    try:
        level = int(os.environ.get("ESPHOME_PREFETCH_LOG_LEVEL", logging.INFO))
    except ValueError:
        level = logging.INFO
    # Mirror the parent's log setup: warnings keep their level prefix and
    # color, and the download bar still draws under the dashboard
    CORE.dashboard = get_bool_env("ESPHOME_PREFETCH_DASHBOARD")
    setup_log(level)
    # pio's managers attach their own handler and still propagate; without
    # this every manager line also prints through the root handler
    for cls_name in (
        "ToolPackageManager",
        "LibraryPackageManager",
        "PlatformPackageManager",
    ):
        logging.getLogger(cls_name.replace("Package", " ")).propagate = False
    if len(argv) != 2:
        # A wiring bug, not a network failure; make it distinguishable
        _LOGGER.warning("prefetch usage: <build_dir> <env_name>")
        return 2
    build_dir, env = argv
    try:
        _prefetch(Path(build_dir), env)
    except Exception as err:  # noqa: BLE001  # pylint: disable=broad-exception-caught
        # Exit 0: the parent must not treat a failed prefetch as a failed build
        _LOGGER.warning("PlatformIO package prefetch skipped: %s", failure_reason(err))
        _LOGGER.debug("Prefetch failure detail", exc_info=True)
    return 0


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main(sys.argv[1:]))
