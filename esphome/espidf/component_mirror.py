"""Local mirror of IDF component-registry packages.

The component manager contacts the registry on every cmake configure, even
with an unchanged ``dependencies.lock``. It checks
``IDF_COMPONENT_LOCAL_STORAGE_URL`` mirrors first and stops on a hit, so a
mirror of the pinned components (filled with its own ``registry sync``)
removes all registry traffic and makes builds work offline.
"""

from __future__ import annotations

import json
import logging
import os
from pathlib import Path
import subprocess
from typing import TYPE_CHECKING, NamedTuple

import yaml

from esphome.build_helpers.tools_cache import IDF_TOOLS_CACHE, tools_cache_path
from esphome.core import EsphomeError

if TYPE_CHECKING:
    from collections.abc import Callable, Mapping

_LOGGER = logging.getLogger(__name__)

# Bump the name if the manager's storage layout ever changes incompatibly;
# the stale directory is removed with the tools cache on clean-all.
_MIRROR_DIR_NAME = "component_mirror"

_ENV_LOCAL_STORAGE_URL = "IDF_COMPONENT_LOCAL_STORAGE_URL"
_ENV_CHECK_NEW_VERSION = "IDF_COMPONENT_CHECK_NEW_VERSION"

_DEFAULT_REGISTRY_URL = "https://components.espressif.com"
_SYNC_LOCK_NAME = ".sync.lock"
_SYNC_TIMEOUT_S = 120


class ServiceDep(NamedTuple):
    """A registry ("service") dependency pinned to an exact version."""

    namespace: str
    name: str
    version: str


def get_mirror_path() -> Path:
    """The machine-global mirror directory; clean-all removes it with the tools cache."""
    return tools_cache_path(*IDF_TOOLS_CACHE) / _MIRROR_DIR_NAME


def component_mirror_env() -> dict[str, str]:
    """Environment additions that serve the mirror to the component manager.

    A user-supplied local storage list keeps precedence. The new-version
    check (an extra solve per configure) is disabled unless the user set
    it; ESPHome pins exact versions, so its answer is never actionable.
    """
    mirror = get_mirror_path()
    try:
        mirror.mkdir(parents=True, exist_ok=True)
    except OSError as err:
        _LOGGER.debug("Component mirror unavailable at %s: %s", mirror, err)
        return {}
    local_storage = mirror.as_uri()
    if user_value := os.environ.get(_ENV_LOCAL_STORAGE_URL):
        local_storage = f"{user_value};{local_storage}"
    env = {_ENV_LOCAL_STORAGE_URL: local_storage}
    if _ENV_CHECK_NEW_VERSION not in os.environ:
        env[_ENV_CHECK_NEW_VERSION] = "0"
    return env


def _load_yaml_dict(path: Path) -> dict | None:
    """Read a small YAML mapping; missing file or bad content is None."""
    try:
        text = path.read_text(encoding="utf-8")
    except FileNotFoundError:
        return None
    except OSError as err:
        _LOGGER.warning("Could not read %s: %s", path, err)
        return None
    try:
        data = yaml.safe_load(text)
    except yaml.YAMLError as err:
        _LOGGER.warning("Could not parse %s: %s", path, err)
        return None
    if not isinstance(data, dict):
        return None
    return data


def parse_lock_service_deps(lock_path: Path) -> list[ServiceDep]:
    """Pinned default-registry dependencies from a dependencies.lock.

    The lock pins every dependency, transitive ones included. Git, local
    and idf sources cannot be mirrored, and non-default registries (which
    ESPHome-generated manifests never use) are left to the manager.
    """
    data = _load_yaml_dict(lock_path)
    if data is None or not isinstance(data.get("dependencies"), dict):
        return []
    deps: list[ServiceDep] = []
    for key, entry in data["dependencies"].items():
        if not isinstance(key, str) or "/" not in key or not isinstance(entry, dict):
            continue
        source = entry.get("source")
        if not isinstance(source, dict) or source.get("type") != "service":
            continue
        registry_url = str(source.get("registry_url") or _DEFAULT_REGISTRY_URL)
        if registry_url.rstrip("/") != _DEFAULT_REGISTRY_URL:
            _LOGGER.debug("Not mirroring %s: non-default registry", key)
            continue
        version = entry.get("version")
        if not isinstance(version, str):
            continue
        namespace, _, name = key.partition("/")
        deps.append(ServiceDep(namespace, name, version))
    return deps


def parse_manifest_service_deps(manifest_path: Path) -> list[ServiceDep]:
    """Exactly-pinned registry dependencies from an idf_component.yml.

    The manifest exists before the first configure, so mirroring from it
    lets a fresh solve install from the mirror instead of downloading
    twice. Range specs are left to the solver; only an exact version can
    be checked against the mirror without one.
    """
    data = _load_yaml_dict(manifest_path)
    if data is None or not isinstance(data.get("dependencies"), dict):
        return []
    deps: list[ServiceDep] = []
    for key, entry in data["dependencies"].items():
        if not isinstance(key, str) or "/" not in key or not isinstance(entry, dict):
            continue
        if not entry.keys() <= {"version"}:
            continue
        version = entry.get("version")
        # Exact versions start with a digit ("1.12.0", "1.3.3~1"); range
        # operators are prefixes and wildcards contain "*".
        if not isinstance(version, str) or not version[:1].isdigit() or "*" in version:
            continue
        namespace, _, name = key.partition("/")
        deps.append(ServiceDep(namespace, name, version))
    return deps


def project_service_deps(project_dir: Path) -> list[ServiceDep]:
    """The mirrorable dependencies of a build directory.

    The lock is authoritative; the manifest covers the fresh or
    just-changed build where the lock has not been written yet.
    """
    deps = parse_lock_service_deps(project_dir / "dependencies.lock")
    seen = {(dep.namespace, dep.name) for dep in deps}
    for dep in parse_manifest_service_deps(project_dir / "src" / "idf_component.yml"):
        if (dep.namespace, dep.name) not in seen:
            deps.append(dep)
    return deps


def _mirror_has(mirror: Path, dep: ServiceDep) -> bool:
    """Whether the mirror holds the dependency's metadata and archive."""
    json_path = mirror / "components" / dep.namespace / f"{dep.name}.json"
    try:
        doc = json.loads(json_path.read_text(encoding="utf-8"))
        for entry in doc["versions"]:
            if entry.get("version") == dep.version:
                url = entry.get("url")
                return bool(url) and (mirror / url).is_file()
    except (OSError, ValueError, TypeError, KeyError):
        return False
    return False


def missing_deps(mirror: Path, deps: list[ServiceDep]) -> list[ServiceDep]:
    return [dep for dep in deps if not _mirror_has(mirror, dep)]


def sync_component_mirror(
    project_dir: Path,
    get_python: Callable[[], str],
    get_env: Callable[[], Mapping[str, str]],
) -> bool:
    """Mirror any of the build's registry components the mirror lacks.

    Best effort: never raises for an expected failure, and returns False on
    a failed attempt so callers can skip retrying for the rest of the run.
    ``get_python``/``get_env`` are only called when a sync is needed, so
    the covered case resolves no environment.
    """
    mirror = get_mirror_path()
    to_sync = missing_deps(mirror, project_service_deps(project_dir))
    if not to_sync:
        return True
    try:
        python = get_python()
        env = dict(get_env())
        mirror.mkdir(parents=True, exist_ok=True)
    except (OSError, EsphomeError) as err:
        _LOGGER.warning("Could not mirror IDF components: %s", err)
        return False
    # Lazy import, as in git.py: keeps filelock off the CLI startup path.
    from filelock import FileLock, Timeout

    lock = FileLock(str(mirror / _SYNC_LOCK_NAME), fallback_to_soft=False)
    try:
        lock.acquire(blocking=False)
    except Timeout:
        # Another esphome process is already filling the shared mirror.
        _LOGGER.debug("Component mirror sync already running; skipping")
        return True
    except OSError as err:
        _LOGGER.debug("Could not lock component mirror: %s", err)
        return True
    try:
        cmd = [python, "-m", "idf_component_manager", "registry", "sync"]
        for dep in to_sync:
            cmd += ["--component", f"{dep.namespace}/{dep.name}=={dep.version}"]
        cmd.append(str(mirror))
        _LOGGER.info(
            "Mirroring %d IDF component(s) for offline builds...", len(to_sync)
        )
        result = subprocess.run(
            cmd,
            env=env,
            capture_output=True,
            text=True,
            timeout=_SYNC_TIMEOUT_S,
            check=False,
        )
    except (OSError, subprocess.SubprocessError) as err:
        _LOGGER.warning("Could not mirror IDF components: %s", err)
        return False
    finally:
        lock.release()
    if result.returncode != 0:
        tail = "\n".join(
            (result.stderr or result.stdout or "").strip().splitlines()[-5:]
        )
        _LOGGER.warning(
            "Could not mirror IDF components (exit %d):\n%s", result.returncode, tail
        )
        return False
    _LOGGER.info("Mirrored %d IDF component(s) for offline builds", len(to_sync))
    return True
