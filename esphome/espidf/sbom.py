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
from esphome.helpers import write_file_if_changed

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


def _esphome_commit() -> str | None:
    """Return the commit ESPHome was installed from, if installed from git."""
    try:
        direct_url = importlib.metadata.distribution("esphome").read_text(
            "direct_url.json"
        )
    except importlib.metadata.PackageNotFoundError:
        return None
    if not direct_url:
        return None
    try:
        return json.loads(direct_url).get("vcs_info", {}).get("commit_id")
    except (ValueError, AttributeError):
        return None


def _project_manifest() -> dict[str, Any] | None:
    if (project := CORE.config[CONF_ESPHOME].get(CONF_PROJECT)) is None:
        return None
    return {"name": project[CONF_NAME], "version": project[CONF_VERSION]}


def _external_manifest(source: ExternalSource, name: str) -> dict[str, Any]:
    manifest: dict[str, Any] = {"name": name}
    if source.url is None:
        manifest["description"] = "Local external component"
        return manifest
    location = f"{source.url}@{source.ref}" if source.ref else source.url
    manifest["description"] = f"External component from {location}"
    if source.commit:
        manifest["version"] = source.commit
    if _is_remote_url(source.url):
        manifest["repository"] = source.url
    if source.commit and (
        purl := _repo_purl(source.url, source.commit, f"{source.repo_subpath}/{name}")
    ):
        manifest["purl"] = purl
    return manifest


def _external_manifests() -> list[dict[str, Any]]:
    """Carve each loaded external component out of the ESPHome package."""
    sources = [(s, s.components_dir.resolve()) for s in get_sources()]
    found: dict[Path, ExternalSource] = {}
    for domain in CORE.loaded_integrations:
        if (manifest := loader.get_component(domain)) is None:
            continue
        if (module_file := getattr(manifest.module, "__file__", None)) is None:
            continue
        component_dir = Path(module_file).resolve().parent
        for source, components_dir in sources:
            if component_dir.parent == components_dir:
                found[component_dir] = source
                break
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
        "repository": ESPHOME_REPOSITORY,
    }
    if commit := _esphome_commit():
        manifest["purl"] = _repo_purl(ESPHOME_REPOSITORY, commit)
    elif "dev" not in ESPHOME_VERSION:
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
