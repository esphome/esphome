"""``sbom.yml`` manifests describing the code ESPHome adds to an ESP-IDF build.

esp-idf-sbom (https://github.com/espressif/esp-idf-sbom) builds an SBOM from
``build/project_description.json`` and reads ``sbom.yml`` files from the
project and component directories. ESP-IDF and registry components already
ship theirs; these cover the project, ESPHome itself (the ``src`` component)
and the external components copied into it.

esp-idf-sbom aborts on a malformed manifest, so only values that pass its
validation are written.
"""

import contextlib
import importlib.metadata
import json
import logging
from pathlib import Path
import re
from typing import Any, NamedTuple
from urllib.parse import urlparse
from urllib.request import url2pathname

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

# scp-like SSH remote: [user@]host:path
_SCP_URL_RE = re.compile(r"^(?:[^@/:]+@)?(?P<host>[^/:]+):(?P<path>[^/].*)$")


class _Source(NamedTuple):
    """Where ESPHome's code came from; None where that is not known."""

    url: str | None
    # Only set when the commit can be fetched exactly as it was built
    commit: str | None


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


def _https_repo_url(url: str) -> str:
    """Return the https form of a GitHub or GitLab SSH URL, else ``url`` unchanged.

    esp-idf-sbom accepts only git/http/https URLs, and developer clones often use SSH.
    """
    if "://" in url:
        parsed = urlparse(url)
        if parsed.scheme not in ("ssh", "git+ssh"):
            return url
        host, path = parsed.hostname or "", parsed.path
    elif match := _SCP_URL_RE.match(url):
        host, path = match["host"], match["path"]
    else:
        return url
    if host not in _PURL_HOSTS:
        return url
    return f"https://{host}/{path.lstrip('/')}"


def _checkout_remote(path: Path) -> tuple[str, str] | None:
    """Return the name and URL of the remote the branch tracks (else origin), if any."""
    try:
        branch = run_git_command(
            ["git", "rev-parse", "--abbrev-ref", "HEAD"], git_dir=path
        )
        remote = run_git_command(
            ["git", "config", "--get", f"branch.{branch}.remote"], git_dir=path
        )
    except GitException:
        # Detached HEAD or a branch that tracks nothing
        remote = "origin"
    try:
        return remote, run_git_command(
            ["git", "remote", "get-url", remote], git_dir=path
        )
    except GitException as err:
        _LOGGER.debug("SBOM: no URL for remote '%s' in %s: %s", remote, path, err)
        return None


def _checkout_vcs(path: Path) -> _Source | None:
    """Describe the git checkout at ``path``; None if it is not a checkout."""
    if not (path / ".git").exists():
        return None
    try:
        commit = run_git_command(["git", "rev-parse", "HEAD"], git_dir=path)
        # Only the package is compiled in; new files there count, notes elsewhere don't
        modified = run_git_command(
            ["git", "status", "--porcelain", "--", "esphome"], git_dir=path
        )
    except GitException as err:
        _LOGGER.warning(
            "SBOM: cannot read the git checkout ESPHome runs from (%s), so its "
            "source is left out: %s",
            path,
            err,
        )
        return _Source(None, None)
    if (remote := _checkout_remote(path)) is None:
        # Local-only clone: nothing that can be fetched to name
        return _Source(None, None)
    name, url = remote
    try:
        # Only the remote the URL comes from counts, or the PURL could name a
        # commit that exists in another remote only
        pushed = run_git_command(
            ["git", "branch", "--remotes", "--contains", "HEAD", "--list", f"{name}/*"],
            git_dir=path,
        )
    except GitException as err:
        _LOGGER.warning(
            "SBOM: cannot check whether %s has HEAD, so no commit is named: %s",
            name,
            err,
        )
        pushed = ""
    if modified or not pushed:
        # A PURL would name code other than what was built, or that no one can fetch
        return _Source(strip_url_credentials(url), None)
    return _Source(strip_url_credentials(url), commit)


def _unknown_install(reason: str) -> _Source:
    _LOGGER.warning(
        "SBOM: cannot tell where ESPHome was installed from (%s), so its source "
        "is left out",
        reason,
    )
    return _Source(None, None)


def _esphome_vcs() -> _Source | None:
    """Return the repository URL and commit ESPHome was installed from, if known.

    None means a plain package install, described by its release version.
    """
    try:
        direct_url = importlib.metadata.distribution("esphome").read_text(
            "direct_url.json"
        )
    except importlib.metadata.PackageNotFoundError:
        return None
    if not direct_url:
        return None
    # Not installed from a package index, so the release version alone may not
    # describe it. The raw file is never logged: its URL can hold a token.
    try:
        info = json.loads(direct_url)
        commit = info.get("vcs_info", {}).get("commit_id")
        url = info.get("url")
    except (ValueError, AttributeError):
        return _unknown_install("unreadable direct_url.json")
    if not isinstance(url, str):
        return _unknown_install("direct_url.json has no URL")
    if commit:
        return _Source(strip_url_credentials(url), commit)
    if "dir_info" in info and url.startswith("file:"):
        # An editable or local install may be a fork or a patched clone. The
        # Docker image's tree has no .git, so it keeps the release description.
        return _checkout_vcs(Path(url2pathname(urlparse(url).path)))
    return _unknown_install("not a git or local folder install")


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
    except GitException as err:
        # Only SBOM metadata depends on it; never fail the build over it
        _LOGGER.warning("SBOM: cannot read commit in %s: %s", source.repo_dir, err)
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
    url = _https_repo_url(source.url)
    if _is_remote_url(url):
        manifest["repository"] = url
    if commit and source.repo_dir is not None:
        component_dir = (source.components_dir / name).resolve()
        repo_dir = source.repo_dir.resolve()
        if not component_dir.is_relative_to(repo_dir):
            # An absolute or ../ path: nothing in the repository to point at
            _LOGGER.warning(
                "SBOM: component '%s' is outside its repository %s, so it has no PURL",
                name,
                source.url,
            )
        elif purl := _repo_purl(
            url, commit, component_dir.relative_to(repo_dir).as_posix()
        ):
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
    if (source := _esphome_vcs()) is not None:
        # Installed from git, possibly a fork, or from a checkout
        url = _https_repo_url(source.url) if source.url else None
        if _is_remote_url(url):
            manifest["repository"] = url
            if source.commit and (purl := _repo_purl(url, source.commit)):
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
    """Write the project and ``src`` component manifests into the build tree.

    Runs on every ESP-IDF build, so a failure is logged and never stops it.
    """
    try:
        _write_manifests()
    except (OSError, ValueError) as err:
        _LOGGER.warning("SBOM: could not write sbom.yml manifests: %s", err)
        # Manifests from an earlier build would describe other firmware
        for path in (
            CORE.relative_build_path(SBOM_FILENAME),
            CORE.relative_src_path(SBOM_FILENAME),
        ):
            with contextlib.suppress(OSError):
                path.unlink(missing_ok=True)


def _write_manifests() -> None:
    project_path = CORE.relative_build_path(SBOM_FILENAME)
    if (project := _project_manifest()) is not None:
        write_file_if_changed(project_path, yaml_util.dump(project))
    elif project_path.is_file():
        project_path.unlink()
    write_file_if_changed(
        CORE.relative_src_path(SBOM_FILENAME), yaml_util.dump(_esphome_manifest())
    )
