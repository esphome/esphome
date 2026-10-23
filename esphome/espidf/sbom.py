"""``sbom.yml`` manifests describing the code ESPHome adds to an ESP-IDF build.

esp-idf-sbom (https://github.com/espressif/esp-idf-sbom) builds an SBOM from
``build/project_description.json`` and reads ``sbom.yml`` files from the
project and component directories. ESP-IDF and registry components already
ship theirs; these cover the project, ESPHome itself (the ``src`` component)
and the external components copied into it.

esp-idf-sbom aborts on a malformed manifest, so only values that pass its
validation are written.
"""

import importlib.metadata
import json
import logging
from pathlib import Path
from typing import Any
from urllib.parse import urlparse

from esphome import loader, yaml_util
from esphome.components.external_components import ExternalSource, get_sources
from esphome.const import (
    CONF_ESPHOME,
    CONF_NAME,
    CONF_PROJECT,
    CONF_VERSION,
    __version__ as ESPHOME_VERSION,
)
from esphome.core import CORE
from esphome.git import GitException, run_git_command, strip_url_credentials
from esphome.helpers import write_file_if_changed

_LOGGER = logging.getLogger(__name__)

SBOM_FILENAME = "sbom.yml"

ESPHOME_REPOSITORY = "https://github.com/esphome/esphome"
# The firmware holds only ESPHome's C++ runtime, which is GPLv3 (see LICENSE)
ESPHOME_LICENSE = "GPL-3.0-only"
ESPHOME_SUPPLIER = "Organization: Open Home Foundation"

# PURL types esp-idf-sbom also derives itself; other hosts get no PURL
_PURL_HOSTS = {"github.com": "github", "gitlab.com": "gitlab"}


def _is_remote_url(url: str | None) -> bool:
    """Mirror esp-idf-sbom's URL check, which rejects anything else."""
    if not url:
        return False
    parsed = urlparse(url)
    return parsed.scheme in ("git", "http", "https") and bool(parsed.netloc)


def _repo_purl(url: str, version: str, subpath: str | None = None) -> str | None:
    """Return a github/gitlab PURL for a repository root URL, else None."""
    parsed = urlparse(url)
    purl_type = _PURL_HOSTS.get(parsed.hostname or "")
    parts = parsed.path.strip("/").removesuffix(".git").split("/")
    if purl_type is None or len(parts) != 2 or not all(parts):
        return None
    purl = f"pkg:{purl_type}/{parts[0]}/{parts[1]}@{version}"
    if subpath and subpath != ".":
        purl += f"#{subpath}"
    return purl


def _esphome_vcs() -> tuple[str, str] | None:
    """Return the repository URL and commit ESPHome was installed from, if from git."""
    try:
        direct_url = importlib.metadata.distribution("esphome").read_text(
            "direct_url.json"
        )
    except importlib.metadata.PackageNotFoundError:
        return None
    if not direct_url:
        return None
    try:
        info = json.loads(direct_url)
        commit = info.get("vcs_info", {}).get("commit_id")
        url = info.get("url")
    except (ValueError, AttributeError):
        _LOGGER.debug("SBOM: unreadable direct_url.json for esphome: %s", direct_url)
        return None
    if not commit or not isinstance(url, str):
        return None
    return strip_url_credentials(url), commit


def _project_manifest() -> dict[str, Any] | None:
    if (project := CORE.config[CONF_ESPHOME].get(CONF_PROJECT)) is None:
        return None
    return {"name": project[CONF_NAME], "version": project[CONF_VERSION]}


def _source_commit(source: ExternalSource) -> str | None:
    """Return the commit checked out for a git source, if it can be read."""
    if source.repo_dir is None:
        return None
    try:
        return run_git_command(["git", "rev-parse", "HEAD"], git_dir=source.repo_dir)
    except GitException:
        # Only SBOM metadata depends on it; never fail the build over it
        return None


def _external_manifest(source: ExternalSource, name: str) -> dict[str, Any]:
    manifest: dict[str, Any] = {"name": name}
    if source.url is None:
        manifest["description"] = "Local external component"
        return manifest
    location = f"{source.url}@{source.ref}" if source.ref else source.url
    manifest["description"] = f"External component from {location}"
    commit = _source_commit(source)
    if commit:
        manifest["version"] = commit
    if _is_remote_url(source.url):
        manifest["repository"] = source.url
    if commit and source.repo_dir is not None:
        subpath = (source.components_dir / name).relative_to(source.repo_dir)
        if purl := _repo_purl(source.url, commit, subpath.as_posix()):
            manifest["purl"] = purl
    return manifest


def _external_manifests() -> list[dict[str, Any]]:
    """Carve each loaded external component out of the ESPHome package."""
    sources = [(s, s.components_dir.resolve()) for s in get_sources()]
    found: dict[Path, ExternalSource] = {}
    for domain in CORE.loaded_integrations:
        manifest = loader.get_component(domain)
        module_file = getattr(getattr(manifest, "module", None), "__file__", None)
        if module_file is None:
            _LOGGER.warning(
                "SBOM: cannot locate component '%s'; it is listed as part of ESPHome",
                domain,
            )
            continue
        component_dir = Path(module_file).resolve().parent
        for source, components_dir in sources:
            if component_dir.parent == components_dir:
                found[component_dir] = source
                break
        else:
            if not component_dir.is_relative_to(loader.CORE_COMPONENTS_PATH):
                _LOGGER.warning(
                    "SBOM: component '%s' in %s matches no external component "
                    "source and is listed as part of ESPHome",
                    domain,
                    component_dir,
                )
    return [
        {
            "dest": f"esphome/components/{component_dir.name}",
            "manifest": _external_manifest(source, component_dir.name),
        }
        for component_dir, source in sorted(found.items(), key=lambda i: i[0].name)
    ]


def _esphome_manifest() -> dict[str, Any]:
    manifest: dict[str, Any] = {
        "name": "esphome",
        "version": ESPHOME_VERSION,
        "description": "ESPHome firmware runtime and generated code",
        "supplier": ESPHOME_SUPPLIER,
        "license": ESPHOME_LICENSE,
    }
    if (vcs := _esphome_vcs()) is not None:
        # Installed from git, which may be a fork
        url, commit = vcs
        if _is_remote_url(url):
            manifest["repository"] = url
        if purl := _repo_purl(url, commit):
            manifest["purl"] = purl
    else:
        manifest["repository"] = ESPHOME_REPOSITORY
        if "dev" not in ESPHOME_VERSION:
            # Release versions are also the git tags
            manifest["purl"] = _repo_purl(ESPHOME_REPOSITORY, ESPHOME_VERSION)
    if externals := _external_manifests():
        manifest["manifests"] = externals
    return manifest


def write_manifests() -> None:
    """Write the project and ``src`` component manifests into the build tree."""
    project_path = CORE.relative_build_path(SBOM_FILENAME)
    if (project := _project_manifest()) is not None:
        write_file_if_changed(project_path, yaml_util.dump(project))
    elif project_path.is_file():
        project_path.unlink()
    write_file_if_changed(
        CORE.relative_src_path(SBOM_FILENAME), yaml_util.dump(_esphome_manifest())
    )
