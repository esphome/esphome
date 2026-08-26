"""Parallel prefetch of the packages a PlatformIO run would install.

Downloads the archives concurrently into PlatformIO's own download cache
(identical ``compute_download_path`` keys) so the serial installer finds
them already cached. Runs in a subprocess like all PlatformIO execution:
loading a platform executes its code (pioarduino's penv setup rewrites
``sys.path``). A sentinel in the build dir lets warm builds skip the
spawn. Best-effort: any failure logs and PlatformIO downloads as before.
Across processes sharing a core dir, registry downloads share the cache
key and rely on sha256 verification; URL downloads carry no checksum, so
they are serialized by a file lock and promoted with an atomic rename.
"""

from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import logging
import os
from pathlib import Path
import subprocess
import sys
import threading
from typing import Any

from esphome.framework_helpers import (
    content_length,
    failure_reason,
    resume_fetch_job,
    run_batch_downloads,
    warn_prefetch_failures,
)

_LOGGER = logging.getLogger(__name__)

# Concurrent registry resolutions / HEAD probes (each is network-bound)
_RESOLVE_WORKERS = 8

# A hung child must not block the build; downloads resume on the next run
_PREFETCH_TIMEOUT = 20 * 60

# Waiting on another process's URL download; past this, leave it to pio
_URI_LOCK_TIMEOUT = 60

# Resolution errored (vs a clean skip); suppresses the warm sentinel
_RESOLVE_FAILED = object()

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
    cmd = [
        sys.executable,
        "-m",
        "esphome.platformio.prefetch",
        str(build_dir),
        CORE.name,
    ]
    try:
        proc = subprocess.run(cmd, env=env, check=False, timeout=_PREFETCH_TIMEOUT)
    except subprocess.TimeoutExpired:
        _LOGGER.warning("PlatformIO package prefetch timed out; continuing without it")
        return
    except Exception as err:  # noqa: BLE001  # pylint: disable=broad-exception-caught
        # The prefetch must never become a new way for the build to fail
        _LOGGER.warning("PlatformIO package prefetch skipped: %s", failure_reason(err))
        _LOGGER.debug("Prefetch failure detail", exc_info=True)
        return
    if proc.returncode != 0:
        _LOGGER.warning(
            "PlatformIO package prefetch skipped (exit %d)", proc.returncode
        )


def _project_platform_and_config(ini: Path, env: str) -> tuple[str | None, Any]:
    """The env's platform spec and the ProjectConfig for the given ini."""
    from platformio import app
    from platformio.project.config import ProjectConfig

    # PlatformBase.config reads the default ProjectConfig; it must see
    # this ini's env options
    app.set_session_var("custom_project_conf", str(ini))
    config = ProjectConfig.get_instance(str(ini))
    return config.get(f"env:{env}", "platform", None), config


def _registry_jobs(
    manager, specs, seen: set[str]
) -> tuple[list[tuple[str, int, Any]], int]:
    """Resolve registry specs to ``(name, size, fetch)`` batch jobs.

    Mirrors PlatformIO's install path: best version, systype file, first
    mirror, and the same sha1(url + checksum) download-cache key. Also
    returns how many resolutions errored (a clean skip is not an error).
    """
    from platformio.registry.mirror import RegistryFileMirrorIterator

    local = threading.local()

    def _resolve(spec) -> tuple[str, int, str, Path, str] | object | None:
        # One manager (and registry HTTP session) per worker thread;
        # installed-state was already checked on the shared manager
        if (mgr := getattr(local, "mgr", None)) is None:
            mgr = local.mgr = manager.__class__()
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
            if dl_path.is_file():
                return None  # cached from an earlier run
            size = pkgfile.get("size")
            if not size:
                return None  # no size, no bar share; PlatformIO fetches it
            return f"{package['name']}@{version['name']}", size, url, dl_path, checksum
        except Exception:  # noqa: BLE001  # pylint: disable=broad-exception-caught
            # One flaky spec must not discard the rest of the batch
            _LOGGER.debug("Could not resolve %s", spec, exc_info=True)
            return _RESOLVE_FAILED

    # Serial disk lookups on the shared manager: a fully warm build
    # resolves nothing, and duplicate specs resolve once
    unique: dict[tuple[str | None, str, str], Any] = {}
    for s in specs:
        if not s.uri and not manager.get_package(s):
            unique.setdefault((s.owner, s.name, str(s.requirements)), s)
    pending = list(unique.values())
    if not pending:
        return [], 0
    # Serial resolutions (registry GET + mirror HEAD each) dominate
    with ThreadPoolExecutor(max_workers=min(_RESOLVE_WORKERS, len(pending))) as ex:
        results = list(ex.map(_resolve, pending))
    jobs: list[tuple[str, int, Any]] = []
    failed = 0
    for res in results:
        if res is None:
            continue
        if res is _RESOLVE_FAILED:
            failed += 1
            continue
        name, size, url, dl_path, checksum = res
        if str(dl_path) in seen:
            continue  # duplicate spec; two workers must not share a .part
        seen.add(str(dl_path))
        jobs.append(
            (name, size, resume_fetch_job(url, dl_path, sha256=checksum, size=size))
        )
    if failed:
        # Visible once per build; per-spec detail stays at debug
        _LOGGER.warning(
            "Could not resolve %d of %d PlatformIO package(s); "
            "PlatformIO will download them serially",
            failed,
            len(pending),
        )
    return jobs, failed


def _uri_jobs(manager, specs, seen: set[str]) -> tuple[list[tuple[str, int, Any]], int]:
    """Jobs for direct-URL specs; a HEAD sizes each for the combined bar.

    Also returns how many HEAD probes errored (an absent length is not an
    error).
    """
    from esphome.net_retry import fetch_with_retry, http_request

    candidates: list[tuple[str, str, Path]] = []
    for spec in specs:
        url = spec.uri
        if not url or not url.startswith(("http://", "https://")):
            continue  # git+/file specs are cloned/copied, not downloaded
        if url.split("#", 1)[0].endswith(".git"):
            continue  # bare-URL VCS spec; PlatformIO clones it
        if manager.get_package(spec):
            continue
        # PlatformIO downloads URL specs with no checksum
        dl_path = Path(manager.compute_download_path(url, ""))
        if dl_path.is_file() or str(dl_path) in seen:
            continue  # cached, or another spec already claimed this .part
        seen.add(str(dl_path))
        candidates.append((spec.name or url.rsplit("/", 1)[-1], url, dl_path))

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
            # Permanent (e.g. a host that 405s HEAD but serves GET): a clean
            # skip, or the sentinel would never be written again
            return 0
        return content_length(resp)

    if not candidates:
        return [], 0
    with ThreadPoolExecutor(max_workers=min(_RESOLVE_WORKERS, len(candidates))) as ex:
        sizes = list(ex.map(_head_size, [url for _, url, _ in candidates]))
    jobs: list[tuple[str, int, Any]] = []
    failed = 0
    for (name, url, dl_path), size in zip(candidates, sizes, strict=True):
        if size < 0:
            failed += 1
        elif size:
            jobs.append((name, size, _uri_fetch_job(url, dl_path, size)))
    if failed:
        _LOGGER.warning(
            "Could not size %d of %d PlatformIO package URL(s); "
            "PlatformIO will download them serially",
            failed,
            len(candidates),
        )
    return jobs, failed


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
        # filesystems that blocks every later build (see git.py); a bounded
        # wait plus a skip keeps the job best-effort either way
        lock = FileLock(f"{tmp}.lock", fallback_to_soft=False)
        try:
            lock.acquire(timeout=_URI_LOCK_TIMEOUT)
        except (Timeout, OSError):
            _LOGGER.debug("Could not lock %s; leaving it to PlatformIO", dl_path)
            return
        try:
            if dl_path.is_file():
                return  # another process finished it while we waited
            fetch(tracker)
            tmp.replace(dl_path)
        finally:
            lock.release()

    return run


def _prefetch(build_dir: Path, env: str) -> None:
    from platformio.dependencies import get_core_dependencies
    from platformio.package.manager.library import LibraryPackageManager
    from platformio.package.manager.platform import PlatformPackageManager
    from platformio.package.meta import PackageSpec
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
    # pio run's storage dir for this env: installed libraries skip by
    # disk lookup
    libdeps_dir = Path(config.get("platformio", "libdeps_dir")) / env
    lm = LibraryPackageManager(str(libdeps_dir))
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
    unresolved = 0
    for mgr, batch in ((p.pm, specs), (lm, lib_specs)):
        for build_jobs in (_registry_jobs, _uri_jobs):
            batch_jobs, failed = build_jobs(mgr, batch, seen)
            jobs += batch_jobs
            unresolved += failed

    sentinel = build_dir / _SENTINEL_NAME
    if not jobs:
        if not unresolved:
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
        return
    sentinel.unlink(missing_ok=True)
    _LOGGER.info(
        "Prefetching %d PlatformIO package(s): %s",
        len(jobs),
        ", ".join(name for name, _, _ in jobs),
    )
    # PlatformIO retries failed packages itself, without resume
    warn_prefetch_failures(run_batch_downloads("Downloading PlatformIO packages", jobs))


def main(argv: list[str]) -> int:
    """Subprocess entry point: ``prefetch <build_dir> <env_name>``."""
    try:
        level = int(os.environ.get("ESPHOME_PREFETCH_LOG_LEVEL", logging.INFO))
    except ValueError:
        level = logging.INFO
    logging.basicConfig(level=level, format="%(message)s")
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
