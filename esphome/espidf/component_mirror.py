"""Local mirror of IDF component-registry packages.

The IDF component manager contacts the registry during every cmake configure,
even when ``dependencies.lock`` is unchanged, which slows configures down and
makes builds fail or stall without good internet. The manager checks
``IDF_COMPONENT_LOCAL_STORAGE_URL`` mirrors first and stops on a hit, so
keeping the locked registry components mirrored under the IDF tools cache
(filled with the manager's own ``registry sync``) removes all registry
traffic from configures and lets a fresh dependency solve work offline.
"""

from __future__ import annotations

import json
import logging
import os
from pathlib import Path
import subprocess
import time
from typing import TYPE_CHECKING, NamedTuple

import yaml

from esphome.build_helpers.tools_cache import IDF_TOOLS_CACHE, tools_cache_path
from esphome.core import EsphomeError

if TYPE_CHECKING:
    from collections.abc import Callable, Mapping

_LOGGER = logging.getLogger(__name__)

# Bump the name if the manager's storage layout ever changes incompatibly;
# the stale directory is removed with the tools cache on clean-all.
MIRROR_DIR_NAME = "component_mirror"

ENV_LOCAL_STORAGE_URL = "IDF_COMPONENT_LOCAL_STORAGE_URL"
ENV_CHECK_NEW_VERSION = "IDF_COMPONENT_CHECK_NEW_VERSION"

_DEFAULT_REGISTRY_URL = "https://components.espressif.com"
_SYNC_LOCK_NAME = ".sync.lock"
_SYNC_LOCK_STALE_S = 600
_SYNC_TIMEOUT_S = 120


class ServiceDep(NamedTuple):
    """A registry ("service") dependency pinned in dependencies.lock."""

    namespace: str
    name: str
    version: str
    registry_url: str


def get_mirror_path() -> Path:
    """The machine-global mirror directory; clean-all removes it with the tools cache."""
    return tools_cache_path(*IDF_TOOLS_CACHE) / MIRROR_DIR_NAME


def component_mirror_env() -> dict[str, str]:
    """Environment additions that serve the mirror to the component manager.

    A user-supplied local storage list keeps precedence; the mirror is
    appended. The new-version check is disabled unless the user asked for
    it: ESPHome pins exact versions, so its answer is never actionable, and
    it is a whole extra dependency solve per configure.
    """
    mirror = get_mirror_path()
    try:
        mirror.mkdir(parents=True, exist_ok=True)
    except OSError as err:
        _LOGGER.debug("Component mirror unavailable at %s: %s", mirror, err)
        return {}
    local_storage = mirror.as_uri()
    if user_value := os.environ.get(ENV_LOCAL_STORAGE_URL):
        local_storage = f"{user_value};{local_storage}"
    env = {ENV_LOCAL_STORAGE_URL: local_storage}
    if ENV_CHECK_NEW_VERSION not in os.environ:
        env[ENV_CHECK_NEW_VERSION] = "0"
    return env


def parse_lock_service_deps(lock_path: Path) -> list[ServiceDep]:
    """Extract the pinned registry dependencies from a dependencies.lock.

    The lock pins every dependency, transitive ones included, each with its
    source type. Only "service" (registry) entries can be mirrored; git,
    local and idf sources are left to the manager as before.
    """
    try:
        text = lock_path.read_text(encoding="utf-8")
    except FileNotFoundError:
        return []
    except OSError as err:
        _LOGGER.warning("Could not read %s: %s", lock_path, err)
        return []
    try:
        data = yaml.safe_load(text)
    except yaml.YAMLError as err:
        _LOGGER.warning("Could not parse %s: %s", lock_path, err)
        return []
    if not isinstance(data, dict) or not isinstance(data.get("dependencies"), dict):
        return []
    deps: list[ServiceDep] = []
    for key, entry in data["dependencies"].items():
        if not isinstance(key, str) or "/" not in key or not isinstance(entry, dict):
            continue
        source = entry.get("source")
        if not isinstance(source, dict) or source.get("type") != "service":
            continue
        version = entry.get("version")
        if not isinstance(version, str):
            continue
        namespace, _, name = key.partition("/")
        registry_url = str(source.get("registry_url") or _DEFAULT_REGISTRY_URL)
        deps.append(ServiceDep(namespace, name, version, registry_url.rstrip("/")))
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


def _acquire_sync_lock(mirror: Path) -> bool:
    """Take the advisory sync lock, reclaiming it when stale.

    Held by another process means that process is already syncing the same
    machine-global mirror; skipping is the right outcome, not an error.
    """
    lock_path = mirror / _SYNC_LOCK_NAME
    for _ in range(2):
        try:
            os.close(os.open(lock_path, os.O_CREAT | os.O_EXCL | os.O_WRONLY))
        except FileExistsError:
            try:
                if time.time() - lock_path.stat().st_mtime <= _SYNC_LOCK_STALE_S:
                    return False
                lock_path.unlink(missing_ok=True)
            except OSError:
                return False
            continue
        except OSError:
            return False
        return True
    return False


def sync_component_mirror(
    lock_path: Path,
    get_python: Callable[[], str],
    get_env: Callable[[], Mapping[str, str]],
) -> bool:
    """Mirror any locked registry components the mirror lacks; best effort.

    Runs the version-matched component manager's own ``registry sync``,
    which downloads incrementally and merges metadata, so re-syncs only
    fetch what is missing. ``get_python``/``get_env`` are only called when a
    sync is actually needed, keeping the covered case free of environment
    resolution. Returns False when a sync attempt failed so callers can
    skip retrying in the same process.
    """
    mirror = get_mirror_path()
    to_sync = missing_deps(mirror, parse_lock_service_deps(lock_path))
    if skipped := [d for d in to_sync if d.registry_url != _DEFAULT_REGISTRY_URL]:
        # ESPHome-generated manifests only use the default registry; a
        # non-default dependency simply stays network-fetched as before.
        _LOGGER.debug(
            "Not mirroring %s: non-default registry",
            ", ".join(f"{d.namespace}/{d.name}" for d in skipped),
        )
        to_sync = [d for d in to_sync if d.registry_url == _DEFAULT_REGISTRY_URL]
    if not to_sync:
        return True
    try:
        python = get_python()
        env = dict(get_env())
        mirror.mkdir(parents=True, exist_ok=True)
    except (OSError, EsphomeError) as err:
        _LOGGER.warning("Could not mirror IDF components: %s", err)
        return False
    if not _acquire_sync_lock(mirror):
        _LOGGER.debug("Component mirror sync already running; skipping")
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
        (mirror / _SYNC_LOCK_NAME).unlink(missing_ok=True)
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
