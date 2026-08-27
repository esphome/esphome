"""Parallel prefetch and install of the packages a PlatformIO run needs.

Downloads the archives concurrently into PlatformIO's own download cache
(identical ``compute_download_path`` keys), then installs them through
PlatformIO's own ``_install`` with one worker per usable core, so
extraction (the serial, single-core half of a cold install) parallelizes
too and ``pio run`` finds every package already installed. Runs in a
subprocess like all PlatformIO execution: loading a platform executes
its code (pioarduino's penv setup rewrites ``sys.path``). A sentinel in
the build dir lets warm builds skip the spawn. Best-effort: any failure
logs and PlatformIO downloads and installs as before. Across processes
sharing a core dir every download destination is serialized by a file
lock; checksum-less URL downloads additionally stage under a stable
name and promote with an atomic rename.
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
_DOWNLOAD_LOCK_TIMEOUT = 60

# Child exit for a handled, already-warned failure; 1 would collide with
# the interpreter's own import-failure exit
_EXIT_HANDLED = 3

# Short lock-acquire slices so a waiting worker still observes Ctrl-C
_URI_LOCK_POLL = 1

# Resolution errored (vs a clean skip); suppresses the warm sentinel
_RESOLVE_FAILED = object()


def _sweep_stale_sidecars(download_dir: Path, expire_seconds: int) -> None:
    """Prune resume sidecars pio's usage.db pruner cannot see.

    A version bump strands an aborted archive's sidecars forever. Lock
    files stay: a held lock can carry an ancient mtime (O_TRUNC keeps
    it), and unlinking one reopens the single-writer hole it guards.
    """
    cutoff = time.time() - expire_seconds
    try:
        for f in download_dir.iterdir():
            if f.suffix not in (".part", ".meta", ".prefetch"):
                continue
            try:
                if f.stat().st_mtime < cutoff:
                    f.unlink()
            except OSError as err:
                _LOGGER.debug("Could not remove %s: %s", f, err)
    except OSError:
        _LOGGER.debug("Could not sweep %s", download_dir, exc_info=True)


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
    except BaseException as err:
        # SIGKILL (subprocess.run's choice on interrupt) could land inside
        # a package-directory copy pio run would then trust; ask first
        _stop_child(proc)
        if isinstance(err, Exception):
            # An unexpected wait() failure must degrade, not fail the build
            _LOGGER.warning(
                "PlatformIO package prefetch skipped: %s", failure_reason(err)
            )
            return
        raise
    if returncode == _EXIT_HANDLED:
        # The child already warned with the reason; a second line is noise
        _LOGGER.debug("Prefetch child reported a handled failure")
    elif returncode != 0:
        # Exit 1 stays here: the interpreter exits 1 for import/module
        # failures before main() ever runs, a wiring break worth a warning
        _LOGGER.warning("PlatformIO package prefetch skipped (exit %d)", returncode)


def _stop_child(proc: subprocess.Popen) -> None:
    """Stop the child without cutting an in-flight package install short.

    Wait first (a terminal interrupt already unwinds the child), then
    SIGTERM for the clean unwind main() installs, then kill. On Windows
    terminate() cannot reach the handler, so its arm is a plain wait.
    """
    if proc.poll() is None:
        _LOGGER.info("Waiting for the prefetch child to finish its current install")
    try:
        with suppress(subprocess.TimeoutExpired):
            proc.wait(timeout=5)
            return
        if sys.platform != "win32":
            proc.terminate()
        with suppress(subprocess.TimeoutExpired):
            proc.wait(timeout=30)
            return
        proc.kill()
        proc.wait(timeout=5)
        # The kill can land mid-copy; the uncertainty must be visible
        _LOGGER.warning("Prefetch child killed; a package install may be incomplete")
    except KeyboardInterrupt:
        # Kill so an interrupted stop cannot orphan a still-writing child
        # (BaseException: a further interrupt must not skip the kill),
        # then re-raise so the build aborts
        with suppress(BaseException):
            proc.kill()
            proc.wait(timeout=5)
        if proc.poll() is None:
            _LOGGER.warning("The prefetch child could not be confirmed stopped")
        raise
    except Exception:  # noqa: BLE001  # pylint: disable=broad-exception-caught
        # A surviving child may still be writing packages pio run trusts
        _LOGGER.warning("The prefetch child could not be confirmed stopped")
        _LOGGER.debug("Stop detail", exc_info=True)


def _project_platform_and_config(ini: Path, env: str) -> tuple[str | None, Any]:
    """The env's platform spec and the ProjectConfig for the given ini."""
    from platformio import app
    from platformio.project.config import ProjectConfig

    # PlatformBase.config reads the default ProjectConfig; it must see
    # this ini's env options
    app.set_session_var("custom_project_conf", str(ini))
    config = ProjectConfig.get_instance(str(ini))
    return config.get(f"env:{env}", "platform", None), config


def _sibling_manager(manager: Any) -> Any:
    """A same-store manager equivalent to the shared one."""
    # Hard read: a renamed attribute must fail loudly, not silently drop
    # the qualifiers wave-1 installs resolve with; is-not-None so a falsy
    # PackageCompatibility still propagates
    if (compatibility := manager.compatibility) is not None:
        return manager.__class__(manager.package_dir, compatibility=compatibility)
    return manager.__class__(manager.package_dir)


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
            mgr = local.mgr = _sibling_manager(manager)
        try:
            packages = mgr.search_registry_packages(spec)
            if not packages:
                _LOGGER.debug("%s is unknown to the registry", spec)
                return None  # let PlatformIO report it
            package, version = mgr.find_best_registry_version(packages, spec)
            if not package or not version:
                _LOGGER.debug("%s has no matching registry version", spec)
                return None
            pkgfile = mgr.pick_compatible_pkg_file(version["files"])
            if not pkgfile:
                _LOGGER.debug("%s has no file for this systype", spec)
                return None
            url, checksum = next(RegistryFileMirrorIterator(pkgfile["download_url"]))
            checksum = checksum or pkgfile["checksum"]["sha256"]
            dl_path = Path(mgr.compute_download_path(url, checksum))
            cached = dl_path.is_file()  # fetched by an earlier run
            size = pkgfile.get("size")
            if not cached and not size:
                _LOGGER.debug("%s has no size; PlatformIO fetches it", spec)
                return None  # no size, no bar share
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
                _registry_fetch_job(
                    manager, res.url, res.dl_path, res.checksum, res.size
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
            if spec.has_custom_name():
                # Only a custom name (Foo=https://...) is the destination
                # dir; a URI-derived name's destination comes from the
                # archive manifest, so its dedupe key could collide with
                # another name and race one directory. pio run installs it.
                installable.append((name, spec))  # fetched by an earlier run
            continue
        if str(dl_path) in seen:
            continue  # another spec already claimed this .part
        seen.add(str(dl_path))
        candidates.append((spec.name, url, dl_path, spec))

    errors: list[str] = []

    def _head_size(url: str) -> int:
        try:
            resp = fetch_with_retry(url, lambda: http_request("HEAD", url, timeout=30))
        except Exception as err:  # noqa: BLE001  # pylint: disable=broad-exception-caught
            _LOGGER.debug("HEAD %s failed", url, exc_info=True)
            errors.append(failure_reason(err))
            return -1
        if not resp.ok:
            # An error page's Content-Length is not a download size
            _LOGGER.debug("HEAD %s returned %s", url, resp.status_code)
            if resp.status_code in (401, 403, 408, 429) or resp.status_code >= 500:
                # 401/403 included: registries rate-limit with them
                errors.append(f"HTTP {resp.status_code}")
                return -1  # transient; must not be cached as warm
            # Permanent (405/501 HEAD-unsupported, 401/403/404): a clean
            # skip so the warm sentinel is not disabled forever; pio run
            # surfaces a genuinely broken URL when it downloads
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
            jobs.append((name, size, _uri_fetch_job(manager, url, dl_path, size)))
            if spec.has_custom_name():
                # See above: derived-name specs stay with pio run's installer
                installable.append((name, spec))
        else:
            # Missing or unusable Content-Length; visible under -v
            _LOGGER.debug("%s reports no usable length; PlatformIO fetches it", url)
    if failed:
        _LOGGER.warning(
            "Could not size %d of %d PlatformIO package URL(s) (%s); "
            "PlatformIO will download them serially",
            failed,
            len(candidates),
            errors[0],
        )
    return jobs, failed, installable


def _serialized_fetch_job(
    dl_path: Path, lock_path: str, body: Any, unlocked_ok: bool = True
) -> Any:
    """Wrap ``body`` so the shared destination is single-writer.

    Interleaved writers truncate each other's ``.part`` bytes (see
    registry.py). The bounded poll observes Ctrl-C via the tracker; a
    blown deadline is a clean skip (the holder's copy is what the build
    needs). On a lock-less filesystem a sha256-verified body runs
    unlocked with one warning; a checksum-less one
    (``unlocked_ok=False``) is a counted failure instead.
    """

    def run(tracker: Any) -> None:
        from filelock import FileLock, Timeout

        # fallback_to_soft would leave a stale marker on lock-less
        # filesystems that blocks every later build (see git.py)
        lock = FileLock(lock_path, fallback_to_soft=False)
        deadline = time.monotonic() + _DOWNLOAD_LOCK_TIMEOUT
        while True:
            try:
                lock.acquire(timeout=_URI_LOCK_POLL)
                break
            except Timeout:
                tracker(0)  # raises when the batch is cancelled
                if time.monotonic() >= deadline:
                    # Another process is fetching this same file; its copy
                    # is what the build needs (a large framework archive
                    # can hold the lock far longer than this deadline)
                    _LOGGER.debug("Leaving %s to its current downloader", dl_path.name)
                    return
            except OSError as err:
                if not unlocked_ok:
                    # A body with no checksum to catch interleaved corruption
                    raise
                lock = None
                _LOGGER.warning(
                    "Could not lock %s (%s); downloading unlocked",
                    dl_path.name,
                    err,
                )
                break
        try:
            if dl_path.is_file():
                return  # another process finished it while we waited
            body(tracker)
        finally:
            if lock is not None:
                lock.release()

    return run


# usage.db is a whole-file rewrite behind pio's self-unlinking LockFile;
# concurrent writers could reset every recorded entry
_REGISTER_LOCK = threading.Lock()


def _register_download(manager: Any, dl_path: Path) -> None:
    """Hand the archive to pio's usage.db pruner; an unregistered one is
    never expired (disk garbage, never a bad build)."""
    try:
        with _REGISTER_LOCK:
            manager.set_download_utime(str(dl_path))
    except Exception as err:  # noqa: BLE001  # pylint: disable=broad-exception-caught
        _LOGGER.debug("Could not register %s with pio's cache: %s", dl_path, err)


def _registry_fetch_job(
    manager: Any, url: str, dl_path: Path, checksum: str, size: int
) -> Any:
    """A locked fetch straight to the cache path; sha256 verifies it."""
    # .esphome.lock: pio's own LockFile(dl_path) owns <dl_path>.lock and
    # deletes it on release, which would unlink a held filelock
    fetch = _serialized_fetch_job(
        dl_path,
        f"{dl_path}.esphome.lock",
        resume_fetch_job(url, dl_path, sha256=checksum, size=size),
    )

    def run(tracker: Any) -> None:
        fetch(tracker)
        if dl_path.is_file():
            # The deadline skip can end with no archive landed
            _register_download(manager, dl_path)

    return run


def _uri_fetch_job(manager: Any, url: str, dl_path: Path, size: int) -> Any:
    """Fetch to a locked staging path, then rename into the cache.

    The stable staging name keeps resume working across interrupted
    runs; the rename makes the promotion atomic.
    """
    tmp = dl_path.with_name(f"{dl_path.name}.prefetch")
    # attempts=2: the size is only a HEAD probe's word, and a HEAD/GET
    # disagreement would otherwise re-download the archive five times
    fetch = resume_fetch_job(url, tmp, size=size, attempts=2)

    def promote(tracker: Any) -> None:
        fetch(tracker)
        if (actual := tmp.stat().st_size) != size:
            # A wrong-length checksum-less body must never be published
            discard_partial_download(tmp)
            raise ValueError(f"expected {size} bytes, fetched {actual}")
        tmp.replace(dl_path)

    def run(tracker: Any) -> None:
        _serialized_fetch_job(dl_path, f"{tmp}.lock", promote, unlocked_ok=False)(
            tracker
        )
        if dl_path.is_file():
            # Won or lost, the race is over; staging files left behind
            # are dead weight PlatformIO's cache never prunes
            discard_partial_download(tmp)
            _register_download(manager, dl_path)

    return run


# (name, spec) from wave 1, (name, spec, compatibility) from dep waves
_Entry = tuple[str, Any] | tuple[str, Any, Any]


def _dependency_entries(
    manager: Any, entries: list[_Entry], seen_names: set[str]
) -> list[_Entry]:
    """Registry dependencies of the installed entries, one per new name.

    Mostly local manifest reads; the builtin probe walks installed
    platforms (each may run platform code). Name-only platform libs stay
    with pio run.
    """

    # Hard read: losing this filter would pre-install incompatible
    # packages pio run then trusts
    compatibility = manager.compatibility
    # Tool managers have no builtin table; the contract test pins the name
    is_builtin = getattr(manager, "is_builtin_lib", None)
    deps: dict[str, Any] = {}
    skipped = 0
    for name, spec, *_ in entries:
        try:
            deps_of = _entry_dependencies(manager, spec, compatibility, is_builtin)
        except Exception:  # noqa: BLE001  # pylint: disable=broad-exception-caught
            # One unreadable manifest must not drop the group's whole wave
            _LOGGER.debug("Skipping dependencies of %s", name, exc_info=True)
            skipped += 1
            continue
        for key, entry in deps_of:
            if key not in seen_names:
                deps.setdefault(key, entry)
    if skipped:
        # Visible at default verbosity: a dropped subtree silently
        # degrades the wave; per-entry detail stays at debug
        _LOGGER.warning(
            "Could not read dependencies of %d of %d package(s)",
            skipped,
            len(entries),
        )
    return list(deps.values())


def _entry_dependencies(
    manager: Any, spec: Any, compatibility: Any, is_builtin: Any
) -> list[tuple[str, _Entry]]:
    from platformio.package.meta import PackageCompatibility

    out: list[tuple[str, _Entry]] = []
    if (pkg := manager.get_package(spec)) is None:
        # Only successful installs are walked, so this is a real anomaly
        # (stale memcache, name/dir mismatch, a pio API change); raising
        # folds it into the caller's aggregate dropped-subtree warning
        raise RuntimeError(f"just-installed {spec} is not resolvable")
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
        if not (key := (dspec.name or "").lower()):
            _LOGGER.debug("Dependency %r of %s has no name; left to pio run", dep, spec)
            continue
        if manager.get_package(dspec) is not None:
            continue  # already installed
        # Carry the dep's compatibility so _install searches the
        # registry qualified, exactly like pio's install_dependency
        out.append(
            (key, (dspec.name, dspec, PackageCompatibility.from_dependency(dep)))
        )
    return out


def _clean_failed_install(mgr: Any, name: str, spec: Any) -> None:
    # A post-copy failure leaves a package pio run would trust; remove it
    # so pio run genuinely reinstalls it
    try:
        mgr.memcache_reset()
        if (pkg := mgr.get_package(spec)) is not None:
            # Dropping the metadata is the invariant: pio's own install
            # overwrites a metadata-less dir, so a stuck tree cannot be
            # trusted. The rmtree is best-effort tidiness.
            (Path(pkg.path) / ".piopm").unlink(missing_ok=True)
            with suppress(OSError):
                rmtree(pkg.path)
        else:
            # Nothing was moved into place; the common failure shape
            _LOGGER.debug("No on-disk install of %s to remove", name)
    except Exception as cleanup_err:  # noqa: BLE001  # pylint: disable=broad-exception-caught
        _LOGGER.warning(
            "Could not remove the failed install of %s: %s",
            name,
            failure_reason(cleanup_err),
        )


def _preinstall(
    manager: Any, entries: list[_Entry], seen_names: set[str] | None = None
) -> None:
    """Install downloaded packages in parallel via pio's own ``_install``.

    ``entries`` are ``_Entry`` tuples, one per destination directory.
    The lock is held around each wave's pool, safe only because pio's
    private ``_install`` never re-acquires it (a same-process re-lock
    would hang, not fail). Waves skip dependencies; the installed
    manifests feed the next wave. Any failure falls back to pio run.
    """
    workers = min(get_usable_cpu_count(), len(entries))
    # One manager per worker (_install mutates instance state); built
    # serially because construction rewires the shared manager logger
    managers: SimpleQueue = SimpleQueue()
    for _ in range(workers):
        managers.put(_sibling_manager(manager))
    local = threading.local()

    def _install_one(entry) -> bool:
        # Wave-1 entries are (name, spec); dependency waves add compatibility
        name, spec, *rest = entry
        compat = rest[0] if rest else None
        if (mgr := getattr(local, "mgr", None)) is None:
            # at most `workers` pool threads, one dequeue each
            mgr = local.mgr = managers.get_nowait()
        try:
            mgr._install(  # pylint: disable=protected-access  # noqa: SLF001
                spec, skip_dependencies=True, compatibility=compat
            )
            return True
        except Exception as err:  # noqa: BLE001  # pylint: disable=broad-exception-caught
            _LOGGER.warning("Could not pre-install %s: %s", name, failure_reason(err))
            _LOGGER.debug("Pre-install failure detail", exc_info=True)
            _clean_failed_install(mgr, name, spec)
            return False
        except BaseException:
            # A SystemExit from a postinstall must not skip the cleanup
            # and leave a torn dir pio run trusts
            _clean_failed_install(mgr, name, spec)
            raise

    _LOGGER.info(
        "Installing %d PlatformIO package(s) with %d extraction worker(s): %s",
        len(entries),
        workers,
        ", ".join(name for name, *_ in entries),
    )
    # Postinstall scripts chdir process-globally; the cwd is restored
    # after the pool. Concurrent postinstalls can still race pio's
    # non-reentrant fs.cd mid-pool; that install fails, warns, and is
    # redone serially by pio run. Suppress interleaved progress bars.
    os.environ.setdefault("PLATFORMIO_DISABLE_PROGRESSBAR", "true")
    # get_tmp_dir/get_download_dir create without exist_ok; racing workers
    # would FileExistsError, so create them serially first. Concurrent
    # usage.db updates can drop download bookkeeping; never a bad build.
    manager.get_tmp_dir()
    manager.get_download_dir()
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
        # Each step runs even if an earlier one fails, and none may
        # displace the in-flight exception (SIGTERM's SystemExit
        # included) with a downgradeable one
        wave_ok = True
        for step, label in (
            (manager.memcache_reset, "reset the storage cache"),
            (manager.unlock, "release the manager lock"),
            (lambda: os.chdir(cwd), "restore the working dir"),
        ):
            try:
                step()
            except Exception:  # noqa: BLE001,PERF203  # pylint: disable=broad-exception-caught
                wave_ok = False
                _LOGGER.warning("Could not %s", label)
                _LOGGER.debug("Teardown detail", exc_info=True)
    if len(entries) > 1 and not any(results):
        # A systematic fault, not one bad archive; pio run installs serially
        _LOGGER.warning(
            "Could not pre-install any of %d PlatformIO package(s)", len(entries)
        )

    seen = seen_names if seen_names is not None else set()
    # All entries join seen (failures must not be re-queued); only
    # successful installs feed the dependency walk
    seen.update(name.split("@", 1)[0].lower() for name, *_ in entries)
    installed = [e for e, ok in zip(entries, results, strict=True) if ok]
    if not wave_ok:
        # A stale cache, an unknown lock state, or a lost cwd would
        # poison the next wave; pio run installs the rest cleanly
        _LOGGER.warning("Skipping the dependency wave")
        return
    # The builtin probe may construct platforms whose setup rewrites
    # sys.path (see _prefetch); restore it for later imports
    saved_sys_path = list(sys.path)
    try:
        next_entries = _dependency_entries(manager, installed, seen)
    finally:
        sys.path[:] = saved_sys_path
    if next_entries:
        # Terminates without a cap: every wave admits only never-seen
        # names, so a cycle yields an empty next wave
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
        # An env mismatch must not disable the feature with no trace
        _LOGGER.debug(
            "No platform for env %s in %s; nothing to prefetch", env, build_dir
        )
        return

    # The platform (manifest plus build scripts) installs first and
    # resolves the rest. Its setup may rewrite sys.path (pioarduino's penv
    # setup does); restore it so later imports here still resolve.
    saved_sys_path = list(sys.path)
    pm = PlatformPackageManager()
    _sweep_stale_sidecars(Path(pm.get_download_dir()), pm.DOWNLOAD_CACHE_EXPIRE)
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
    # A bare name is usually a framework built-in (WiFi, SPI); with no
    # lib builders here to tell built-in from registry, skip it. The only
    # cost is that an owner-less user library is not prefetched
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
    if jobs or groups:
        # Real work invalidates any previous no-work record
        sentinel.unlink(missing_ok=True)
    failed_names: set[str] = set()
    if jobs:
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
        # One install per destination: pio derives the directory from
        # the package name, so key on the name part
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
                    "Pre-install failed for the %s group: %s",
                    mgr.__class__.__name__,
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
    raw_level = os.environ.get("ESPHOME_PREFETCH_LOG_LEVEL")
    try:
        level = int(raw_level) if raw_level is not None else logging.INFO
    except ValueError:
        level = logging.INFO
    # Mirror the parent's log setup: warnings keep their level prefix and
    # color, and the download bar still draws under the dashboard
    CORE.dashboard = get_bool_env("ESPHOME_PREFETCH_DASHBOARD")
    setup_log(level)
    # pio's managers attach their own handler and still propagate; without
    # this every manager line also prints through the root handler. Their
    # construction re-pins the logger to INFO, so a logger-level filter
    # (which survives pio's handler reset) enforces a quiet level instead.
    for cls_name in (
        "ToolPackageManager",
        "LibraryPackageManager",
        "PlatformPackageManager",
    ):
        manager_logger = logging.getLogger(cls_name.replace("Package", " "))
        manager_logger.propagate = False
        manager_logger.addFilter(lambda record: record.levelno >= level)
    if len(argv) != 2:
        # A wiring bug, not a network failure; make it distinguishable
        _LOGGER.warning("prefetch usage: <build_dir> <env_name>")
        return 2
    build_dir, env = argv
    try:
        _prefetch(Path(build_dir), env)
    except KeyboardInterrupt:
        # Shared process group: exit quietly, no traceback on the terminal
        _LOGGER.debug("Prefetch interrupted", exc_info=True)
        return 130
    except Exception as err:  # noqa: BLE001  # pylint: disable=broad-exception-caught
        # The parent treats any exit as warn-and-continue, never a failure
        _LOGGER.warning("PlatformIO package prefetch skipped: %s", failure_reason(err))
        _LOGGER.debug("Prefetch failure detail", exc_info=True)
        return _EXIT_HANDLED
    return 0


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main(sys.argv[1:]))
