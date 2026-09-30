"""Local mirror of IDF component-registry packages.

The component manager contacts the registry on every cmake configure, even
with an unchanged ``dependencies.lock``. It checks
``IDF_COMPONENT_LOCAL_STORAGE_URL`` mirrors first and stops on a hit, so a
mirror of the pinned components (filled with its own ``registry sync``)
removes all registry traffic and makes builds work offline. ``registry
sync`` recurses into transitive version ranges, so a heavy tree can
exceed the sync timeout and stay registry-served; that is the accepted
cost of staying on the manager's CLI contract.
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
from esphome.helpers import rmtree

if TYPE_CHECKING:
    from collections.abc import Callable, Iterator, Mapping

_LOGGER = logging.getLogger(__name__)

# Bump the name if the manager's storage layout ever changes incompatibly;
# the stale directory is removed with the tools cache on clean-all.
_MIRROR_DIR_NAME = "component_mirror"

_ENV_LOCAL_STORAGE_URL = "IDF_COMPONENT_LOCAL_STORAGE_URL"
_ENV_CHECK_NEW_VERSION = "IDF_COMPONENT_CHECK_NEW_VERSION"

_DEFAULT_REGISTRY_URL = "https://components.espressif.com"
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
    # Deferred: keeps pyyaml off the serial upload fast path, which
    # imports this module through the espidf toolchain.
    import yaml

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

    The lock pins every dependency, transitive ones included. Git, local
    and idf sources cannot be mirrored, and non-default registries (which
    ESPHome-generated manifests never use) are left to the manager.
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

    The manifest exists before the first configure, so mirroring from it
    lets a fresh solve install from the mirror instead of downloading
    twice. Range specs are left to the solver; only an exact version can
    be checked against the mirror without one.
    """
    deps: list[ServiceDep] = []
    for namespace, name, entry in _iter_deps(manifest_path):
        if entry.keys() != {"version"}:
            continue
        version = entry.get("version")
        # Exact versions start with a digit ("1.12.0", "1.3.3~1"); range
        # operators are prefixes and wildcards contain "*".
        if isinstance(version, str) and version[:1].isdigit() and "*" not in version:
            deps.append(ServiceDep(namespace, name, version))
    return deps


def project_service_deps(lock_path: Path, manifest_path: Path) -> list[ServiceDep]:
    """The mirrorable dependencies of a build.

    The lock is authoritative; the manifest covers the fresh or
    just-changed build where the lock has not been written yet.
    """
    deps = parse_lock_service_deps(lock_path)
    seen = {(dep.namespace, dep.name) for dep in deps}
    for dep in parse_manifest_service_deps(manifest_path):
        if (dep.namespace, dep.name) not in seen:
            deps.append(dep)
    return deps


def _mirror_has(mirror: Path, dep: ServiceDep) -> bool:
    """Whether the mirror holds the dependency's metadata and archive."""
    json_path = mirror / "components" / dep.namespace / f"{dep.name}.json"
    try:
        doc = json.loads(json_path.read_text(encoding="utf-8"))
        url = next(
            (
                entry.get("url")
                for entry in doc["versions"]
                if entry.get("version") == dep.version
            ),
            None,
        )
        return bool(url) and (mirror / url).is_file()
    except (OSError, ValueError, TypeError, KeyError, AttributeError):
        return False


def missing_deps(mirror: Path, deps: list[ServiceDep]) -> list[ServiceDep]:
    return [dep for dep in deps if not _mirror_has(mirror, dep)]


def _merge_component_index(src: Path, dst: Path) -> None:
    """Fold the mirror's existing versions of a component into the staged index.

    The staged index lists only the versions this sync fetched; replacing
    the mirror's file outright would drop the versions it already had.
    """
    try:
        existing = json.loads(dst.read_text(encoding="utf-8"))["versions"]
        staged = json.loads(src.read_text(encoding="utf-8"))
        known = {entry.get("version") for entry in staged["versions"]}
        staged["versions"] += [
            entry for entry in existing if entry.get("version") not in known
        ]
        # Through a sibling file: a write that fails partway must not leave
        # src truncated, or the promotion would install a broken index.
        merged = src.with_name(f"{src.name}.merged")
        merged.write_text(json.dumps(staged), encoding="utf-8")
        Path(merged).replace(src)
    except (OSError, ValueError, TypeError, KeyError, AttributeError):
        # No usable existing index; the staged one stands alone. A partial
        # sibling must not reach the promotion pass.
        with suppress(OSError):
            src.with_name(f"{src.name}.merged").unlink(missing_ok=True)


def _is_component_index(rel: Path) -> bool:
    return (
        rel.parts[0] == "components" and len(rel.parts) == 3 and rel.suffix == ".json"
    )


def _promote(staging: Path, mirror: Path) -> None:
    """Move the synced files into the mirror, one atomic rename each.

    Indexes sort last, so a concurrent configure never reads a version
    entry whose archive has not landed yet.
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
            _merge_component_index(src, dst)
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
    # Sync into a staging directory: the manager writes files in place, so
    # a configure in another process could read a truncated file from the
    # live mirror. Promoting with renames keeps every read consistent.
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
        # An undeletable staging tree raises here: promoting stale files
        # from an earlier attempt would be worse than skipping the sync.
        rmtree(staging)
        result = subprocess.run(
            cmd,
            env=env,
            capture_output=True,
            text=True,
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
            # A partial ref or a key the registry spells differently can
            # sync clean yet cover nothing; retrying would loop forever.
            _LOGGER.warning(
                "Mirror sync left %d component(s) uncovered: %s",
                len(still),
                ", ".join(dep.spec for dep in still),
            )
            return False
    except (OSError, subprocess.SubprocessError) as err:
        _LOGGER.warning("Could not mirror IDF components: %s", err)
        return False
    finally:
        # Cleanup only; a leftover tree is removed by the next attempt.
        with suppress(OSError):
            rmtree(staging)
        lock.release()
    _LOGGER.info("Mirrored %d IDF component(s) for offline builds", len(to_sync))
    return True
