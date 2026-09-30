"""Local mirror of IDF component-registry packages.

The component manager contacts the registry on every cmake configure, even
with an unchanged ``dependencies.lock``, but checks
``IDF_COMPONENT_LOCAL_STORAGE_URL`` mirrors first and stops on a hit.
Mirroring the pinned components with its own ``registry sync`` removes all
registry traffic and makes builds work offline. A heavy transitive tree
can exceed the sync timeout and stay registry-served; accepted cost of
staying on the manager's CLI contract.
"""

from __future__ import annotations

from contextlib import suppress
import json
import logging
import os
from pathlib import Path
import subprocess
from typing import TYPE_CHECKING, NamedTuple

from esphome.build_helpers.tools_cache import IDF_TOOLS_CACHE, tools_cache_path
from esphome.core import EsphomeError
from esphome.framework_helpers import _rename_with_retry
from esphome.helpers import rmtree, write_file

if TYPE_CHECKING:
    from collections.abc import Callable, Iterator, Mapping

_LOGGER = logging.getLogger(__name__)

# Bump the name if the manager's storage layout ever changes incompatibly;
# the stale directory is removed with the tools cache on clean-all.
_MIRROR_DIR_NAME = "component_mirror"

_ENV_LOCAL_STORAGE_URL = "IDF_COMPONENT_LOCAL_STORAGE_URL"
_ENV_CHECK_NEW_VERSION = "IDF_COMPONENT_CHECK_NEW_VERSION"

_DEFAULT_REGISTRY_URL = "https://components.espressif.com"
# Everything a missing, unreadable or wrongly-shaped index file can raise.
_BAD_INDEX_ERRORS = (OSError, ValueError, TypeError, KeyError, AttributeError)
_SYNC_LOCK_NAME = ".sync.lock"
_STAGING_DIR_NAME = ".staging"
_SYNC_TIMEOUT_S = 120


class ServiceDep(NamedTuple):
    """A registry ("service") dependency pinned to an exact version."""

    namespace: str
    name: str
    version: str

    @property
    def spec(self) -> str:
        return f"{self.namespace}/{self.name}=={self.version}"


def get_mirror_path() -> Path:
    """The machine-global mirror directory."""
    return tools_cache_path(*IDF_TOOLS_CACHE) / _MIRROR_DIR_NAME


def component_mirror_env() -> dict[str, str]:
    """Environment additions that serve the mirror to the component manager.

    A user local storage list keeps precedence; the new-version check (an
    extra solve whose answer exact pins make moot) is off unless user-set.
    """
    mirror = get_mirror_path()
    try:
        mirror.mkdir(parents=True, exist_ok=True)
    except OSError as err:
        _LOGGER.debug("Component mirror unavailable at %s: %s", mirror, err)
        return {}
    _LOGGER.info("Serving IDF components from the local mirror at %s", mirror)
    local_storage = mirror.as_uri()
    if user_value := os.environ.get(_ENV_LOCAL_STORAGE_URL):
        local_storage = f"{user_value};{local_storage}"
    env = {_ENV_LOCAL_STORAGE_URL: local_storage}
    if _ENV_CHECK_NEW_VERSION not in os.environ:
        env[_ENV_CHECK_NEW_VERSION] = "0"
    return env


def _load_yaml_dict(path: Path) -> dict | None:
    """Read a small YAML mapping; missing file or bad content is None."""
    # Deferred: keeps pyyaml off the serial upload fast path.
    import yaml

    try:
        text = path.read_text(encoding="utf-8")
    except FileNotFoundError:
        return None
    except (OSError, UnicodeDecodeError) as err:
        _LOGGER.warning("Could not read %s: %s", path, err)
        return None
    try:
        data = yaml.safe_load(text)
    except yaml.YAMLError as err:
        _LOGGER.warning("Could not parse %s: %s", path, err)
        return None
    return data if isinstance(data, dict) else None


def _iter_deps(path: Path) -> Iterator[tuple[str, str, dict]]:
    """(namespace, name, entry) for each namespaced dependency in a YAML file."""
    data = _load_yaml_dict(path)
    deps = data.get("dependencies") if data else None
    if not isinstance(deps, dict):
        return
    for key, entry in deps.items():
        if isinstance(key, str) and "/" in key and isinstance(entry, dict):
            # The registry stores lowercase paths; match a mixed-case key.
            namespace, _, name = key.lower().partition("/")
            yield namespace, name, entry


def parse_lock_service_deps(lock_path: Path) -> list[ServiceDep]:
    """Pinned default-registry dependencies from a dependencies.lock.

    Git, local and idf sources cannot be mirrored; non-default registries
    are left to the manager.
    """
    deps: list[ServiceDep] = []
    for namespace, name, entry in _iter_deps(lock_path):
        source = entry.get("source")
        if not isinstance(source, dict) or source.get("type") != "service":
            continue
        registry_url = str(source.get("registry_url") or _DEFAULT_REGISTRY_URL)
        if registry_url.rstrip("/") != _DEFAULT_REGISTRY_URL:
            _LOGGER.debug("Not mirroring %s/%s: non-default registry", namespace, name)
            continue
        if isinstance(version := entry.get("version"), str):
            deps.append(ServiceDep(namespace, name, version))
    return deps


def parse_manifest_service_deps(manifest_path: Path) -> list[ServiceDep]:
    """Exactly-pinned registry dependencies from an idf_component.yml.

    The manifest exists before the first configure, so a fresh solve can
    install from the mirror; range specs are left to the solver.
    """
    deps: list[ServiceDep] = []
    for namespace, name, entry in _iter_deps(manifest_path):
        if entry.keys() != {"version"} or not isinstance(
            version := entry["version"], str
        ):
            continue
        # The YAML shorthand keeps its operator: "==1.2.3" is an exact pin.
        version = version.removeprefix("==")
        # Exact versions start with a digit ("1.12.0", "1.3.3~1"); range
        # operators are prefixes and wildcards contain "*".
        if version[:1].isdigit() and "*" not in version:
            deps.append(ServiceDep(namespace, name, version))
    return deps


def project_service_deps(lock_path: Path, manifest_path: Path) -> list[ServiceDep]:
    """The mirrorable dependencies of a build; the lock wins, the manifest
    covers the build whose lock has not been written yet."""
    deps = parse_lock_service_deps(lock_path)
    seen = {(dep.namespace, dep.name) for dep in deps}
    for dep in parse_manifest_service_deps(manifest_path):
        if (dep.namespace, dep.name) not in seen:
            deps.append(dep)
    return deps


def _mirror_has(mirror: Path, dep: ServiceDep) -> bool:
    """Whether the mirror holds the dependency's index entry and files.

    The manager fetches every file the entry names with no registry
    fallback once the version is found locally.
    """
    json_path = mirror / "components" / dep.namespace / f"{dep.name}.json"
    try:
        for entry in json.loads(json_path.read_text(encoding="utf-8"))["versions"]:
            if entry.get("version") == dep.version:
                checksums = entry.get("checksums")
                return (mirror / entry["url"]).is_file() and (
                    not checksums or (mirror / checksums).is_file()
                )
    except _BAD_INDEX_ERRORS:
        return False
    return False


def missing_deps(mirror: Path, deps: list[ServiceDep]) -> list[ServiceDep]:
    return [dep for dep in deps if not _mirror_has(mirror, dep)]


def _read_versions(path: Path) -> list[dict]:
    """The version entries of an index; [] when missing or unreadable,
    which lets publishing replace (heal) a broken live index."""
    try:
        entries = json.loads(path.read_text(encoding="utf-8"))["versions"]
    except FileNotFoundError:
        return []
    except _BAD_INDEX_ERRORS as err:
        _LOGGER.debug("Ignoring the unreadable index %s: %s", path, err)
        return []
    return [entry for entry in entries if isinstance(entry, dict)]


def _publish_index(src: Path, dst: Path) -> None:
    """Publish the staged index merged with the live one, atomically.

    The staged index lists only the versions this sync fetched; the
    atomic write keeps the live index intact when it fails, and raises.
    """
    staged = json.loads(src.read_text(encoding="utf-8"))
    versions = [entry for entry in staged["versions"] if isinstance(entry, dict)]
    known = {entry.get("version") for entry in versions}
    staged["versions"] = versions + [
        entry for entry in _read_versions(dst) if entry.get("version") not in known
    ]
    write_file(dst, json.dumps(staged))


def _is_component_index(rel: Path) -> bool:
    return (
        rel.parts[0] == "components" and len(rel.parts) == 3 and rel.suffix == ".json"
    )


def _promote(staging: Path, mirror: Path) -> None:
    """Move the synced files into the mirror, atomically per file.

    Indexes go last, so a concurrent configure never reads a version
    entry whose files have not landed yet.
    """
    files = sorted(
        (path for path in staging.rglob("*") if path.is_file()),
        key=lambda path: (_is_component_index(path.relative_to(staging)), path),
    )
    for src in files:
        rel = src.relative_to(staging)
        dst = mirror / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        if _is_component_index(rel):
            _publish_index(src, dst)
        else:
            _rename_with_retry(src, dst, overwrite=True)


def sync_component_mirror(
    lock_path: Path,
    manifest_path: Path,
    get_python: Callable[[], str],
    get_env: Callable[[], Mapping[str, str]],
) -> bool:
    """Mirror any of the build's registry components the mirror lacks.

    Returns False on a failed attempt so callers can skip retrying this
    run; ``get_python``/``get_env`` are only called when a sync is needed.
    """
    mirror = get_mirror_path()
    to_sync = missing_deps(mirror, project_service_deps(lock_path, manifest_path))
    if not to_sync:
        return True
    try:
        python = get_python()
        env = dict(get_env())
        mirror.mkdir(parents=True, exist_ok=True)
    except (OSError, EsphomeError) as err:
        _LOGGER.warning("Could not mirror IDF components: %s", err)
        return False
    # Staged: the manager writes in place, and a configure in another
    # process must never read a truncated file from the live mirror.
    staging = mirror / _STAGING_DIR_NAME
    cmd = [python, "-m", "idf_component_manager", "registry", "sync"]
    for dep in to_sync:
        cmd += ["--component", dep.spec]
    cmd.append(str(staging))
    # Lazy import, as in git.py: keeps filelock off the CLI startup path.
    from filelock import FileLock, Timeout

    lock = FileLock(str(mirror / _SYNC_LOCK_NAME), fallback_to_soft=False)
    try:
        lock.acquire(blocking=False)
    except Timeout:
        # Another esphome process is already filling the shared mirror.
        _LOGGER.debug("Component mirror sync skipped: already in progress")
        return True
    except OSError as err:
        # A broken cache, not contention; the retry guard should apply.
        _LOGGER.warning("Could not lock the component mirror: %s", err)
        return False
    _LOGGER.info("Mirroring %d IDF component(s) for offline builds...", len(to_sync))
    try:
        # Raises on an undeletable tree; never promote stale files.
        rmtree(staging)
        result = subprocess.run(
            cmd,
            env=env,
            capture_output=True,
            # Not text=True: the locale codec can raise UnicodeDecodeError.
            encoding="utf-8",
            errors="replace",
            timeout=_SYNC_TIMEOUT_S,
            check=False,
        )
        if result.returncode != 0:
            tail = "\n".join((result.stderr or result.stdout).strip().splitlines()[-5:])
            _LOGGER.warning(
                "Could not mirror IDF components (exit %d):\n%s",
                result.returncode,
                tail,
            )
            return False
        _promote(staging, mirror)
        if still := missing_deps(mirror, to_sync):
            # A name the registry spells differently syncs clean yet
            # covers nothing; retrying would loop forever.
            _LOGGER.warning(
                "Mirror sync left %d component(s) uncovered: %s",
                len(still),
                ", ".join(dep.spec for dep in still),
            )
            return False
    except (*_BAD_INDEX_ERRORS, EsphomeError, subprocess.SubprocessError) as err:
        # Includes a failed index publish; the live index is intact.
        _LOGGER.warning("Could not mirror IDF components: %s", err)
        return False
    finally:
        # Cleanup only; a leftover tree is removed by the next attempt.
        with suppress(OSError):
            rmtree(staging)
        lock.release()
    _LOGGER.info("Mirrored %d IDF component(s) for offline builds", len(to_sync))
    return True
