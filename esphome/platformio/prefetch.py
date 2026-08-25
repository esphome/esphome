"""Parallel prefetch of the packages a PlatformIO run would install.

PlatformIO resolves and downloads platform packages, toolchains, frameworks,
and libraries one at a time; on a cold cache (every CI run) the downloads
dominate the install. This prefetch resolves the same registry metadata
PlatformIO would, downloads the archives concurrently into PlatformIO's own
download cache (``compute_download_path`` keyed identically), and lets the
subsequent ``pio run`` find every file already cached.

The prefetch runs in a subprocess (``python -m esphome.platformio.prefetch``):
resolving a platform executes its code, and platform setup may rewrite the
running interpreter's state (pioarduino's penv setup replaces ``sys.path``
with its own virtualenv's, breaking every later import in the process).
All other PlatformIO execution is already subprocess-isolated the same way
(see ``run_platformio_cli``).

Strictly best-effort: any failure logs and returns, leaving PlatformIO to
download whatever is missing exactly as before.
"""

from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor
import logging
import os
from pathlib import Path
import subprocess
import sys
from typing import Any

from esphome.framework_helpers import (
    _content_length,
    resume_fetch_job,
    run_batch_downloads,
    warn_prefetch_failures,
)

_LOGGER = logging.getLogger(__name__)


def prefetch_platformio_packages() -> None:
    """Warm PlatformIO's download cache for the current project, in parallel."""
    from esphome.core import CORE

    env = dict(os.environ)
    # pio run later resolves installed libraries against this same dir
    # (run_platformio_cli sets it); the prefetch must agree or it re-resolves
    # every library on every warm build
    env.setdefault(
        "PLATFORMIO_LIBDEPS_DIR", str(CORE.relative_piolibdeps_path().absolute())
    )
    cmd = [
        sys.executable,
        "-m",
        "esphome.platformio.prefetch",
        str(CORE.build_path),
        CORE.name,
    ]
    try:
        proc = subprocess.run(cmd, env=env, check=False)
    except Exception as err:  # noqa: BLE001  # pylint: disable=broad-exception-caught
        # PlatformIO installs anything missing itself; the prefetch must
        # never become a new way for the build to fail
        _LOGGER.warning("PlatformIO package prefetch skipped: %s", err)
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

    # Make this ini the default ProjectConfig: PlatformBase.config (which
    # the package configuration reads) must see the same env options the
    # later pio run will
    app.set_session_var("custom_project_conf", str(ini))
    config = ProjectConfig.get_instance(str(ini))
    return config.get(f"env:{env}", "platform", None), config


def _registry_jobs(manager, specs, seen: set[str]) -> list[tuple[str, int, Any]]:
    """Resolve registry specs to ``(name, size, fetch)`` batch jobs.

    Resolution mirrors PlatformIO's install path exactly: best registry
    version, systype-compatible file, first mirror (whose HEAD result
    PlatformIO's own run then reuses from its content cache), and the same
    sha1(url + checksum) download-cache key.
    """
    from platformio.registry.mirror import RegistryFileMirrorIterator

    def _resolve(spec) -> tuple[str, int, str, Path, str] | None:
        # Fresh manager per task: the registry client's HTTP session is not
        # shared across threads (installed-state was already checked on the
        # shared manager, whose package dir a bare constructor may not match)
        mgr = manager.__class__()
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

    # Installed packages need no network at all; check them serially on the
    # shared manager (a disk lookup) so a fully warm build resolves nothing
    pending = [s for s in specs if not s.uri and not manager.get_package(s)]
    if not pending:
        return []
    # Each resolution is a registry GET plus a mirror HEAD; serially they
    # dominate the whole prefetch
    with ThreadPoolExecutor(max_workers=min(8, len(pending))) as ex:
        results = list(ex.map(_resolve, pending))
    jobs: list[tuple[str, int, Any]] = []
    for res in results:
        if res is None:
            continue
        name, size, url, dl_path, checksum = res
        if str(dl_path) in seen:
            continue  # duplicate spec; two workers must not share a .part
        seen.add(str(dl_path))
        jobs.append(
            (name, size, resume_fetch_job(url, dl_path, sha256=checksum, size=size))
        )
    return jobs


def _uri_jobs(manager, specs, seen: set[str]) -> list[tuple[str, int, Any]]:
    """Jobs for direct-URL package specs (e.g. pinned GitHub archives).

    No registry metadata exists, so a HEAD supplies the size for the
    combined bar; entries without a usable length are left to PlatformIO.
    """
    from esphome.net_retry import http_request

    candidates: list[tuple[str, str, Path]] = []
    for spec in specs:
        url = spec.uri
        if not url or not url.startswith(("http://", "https://")):
            continue  # git/file specs are cloned/copied, not downloaded
        if manager.get_package(spec):
            continue
        # PlatformIO downloads URL specs with no checksum
        dl_path = Path(manager.compute_download_path(url, ""))
        if dl_path.is_file() or str(dl_path) in seen:
            continue
        candidates.append((spec.name or url.rsplit("/", 1)[-1], url, dl_path))

    def _head_size(url: str) -> int:
        try:
            return _content_length(http_request("HEAD", url, timeout=30))
        except Exception:  # noqa: BLE001  # pylint: disable=broad-exception-caught
            return 0

    if not candidates:
        return []
    with ThreadPoolExecutor(max_workers=min(8, len(candidates))) as ex:
        sizes = list(ex.map(_head_size, [url for _, url, _ in candidates]))
    jobs: list[tuple[str, int, Any]] = []
    for (name, url, dl_path), size in zip(candidates, sizes, strict=True):
        if not size:
            continue
        seen.add(str(dl_path))
        jobs.append((name, size, resume_fetch_job(url, dl_path, size=size)))
    return jobs


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

    # The platform package itself resolves the rest, so it installs first
    # (small: manifest plus build scripts). project_env configures which
    # optional packages (frameworks, toolchains) this env requires. Platform
    # setup code may rewrite sys.path (pioarduino's penv setup does); put it
    # back so this process's own later imports still resolve.
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
    # PIO core's own build engine installs outside the platform's package
    # list, on first run; the other core dependencies (piohome, check
    # tools) are for commands a compile never touches. Some platforms list
    # tool-scons themselves; do not fetch it twice.
    if not any(s.name == "tool-scons" for s in specs):
        specs.append(
            PackageSpec(
                owner="platformio",
                name="tool-scons",
                requirements=get_core_dependencies()["tool-scons"],
            )
        )
    seen: set[str] = set()
    jobs = _registry_jobs(p.pm, specs, seen) + _uri_jobs(p.pm, specs, seen)

    lib_deps = config.get(f"env:{env}", "lib_deps", [])
    # Match pio run's storage dir for this env so already-installed
    # libraries are skipped by a disk lookup instead of re-resolved
    lm = LibraryPackageManager(str(Path(config.get("platformio", "libdeps_dir")) / env))
    lib_specs = [
        PackageSpec(dep) for dep in lib_deps if dep and not dep.startswith("$")
    ]
    jobs += _registry_jobs(lm, lib_specs, seen) + _uri_jobs(lm, lib_specs, seen)

    if not jobs:
        return
    _LOGGER.info(
        "Prefetching %d PlatformIO package(s): %s",
        len(jobs),
        ", ".join(name for name, _, _ in jobs),
    )
    # PlatformIO retries failed packages itself, without resume
    warn_prefetch_failures(run_batch_downloads("Downloading PlatformIO packages", jobs))


def main(argv: list[str]) -> int:
    """Subprocess entry point: ``prefetch <build_dir> <env_name>``."""
    logging.basicConfig(level=logging.INFO, format="%(message)s")
    try:
        build_dir, env = argv
        _prefetch(Path(build_dir), env)
    except Exception as err:  # noqa: BLE001  # pylint: disable=broad-exception-caught
        # Best-effort even here: exit 0 so the parent never treats a failed
        # prefetch as a failed build
        _LOGGER.warning("PlatformIO package prefetch skipped: %s", err)
        _LOGGER.debug("Prefetch failure detail", exc_info=True)
    return 0


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main(sys.argv[1:]))
