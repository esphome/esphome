"""Tests for the esp-idf-sbom manifests written into the build tree."""

import importlib.metadata
import json
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import MagicMock, patch

import pytest

from esphome import yaml_util
from esphome.components.external_components import ExternalSource, get_sources
from esphome.const import CONF_ESPHOME, CONF_NAME, CONF_PROJECT, CONF_VERSION
from esphome.core import CORE
from esphome.espidf import sbom


@pytest.fixture(name="build_tree")
def fixture_build_tree(tmp_path: Path) -> Path:
    CORE.config_path = tmp_path / "test.yaml"
    CORE.build_path = tmp_path / "build"
    CORE.relative_src_path().mkdir(parents=True)
    CORE.config = {CONF_ESPHOME: {}}
    return CORE.build_path


def _read(path: Path) -> dict:
    return yaml_util.parse_yaml(path, path.open(encoding="utf-8"))


def _add_external_component(components_dir: Path, name: str) -> None:
    """Make ``name`` resolve to a module in ``components_dir``, as a meta finder would."""
    module_file = components_dir / name / "__init__.py"
    module_file.parent.mkdir(parents=True)
    module_file.write_text("")
    CORE.loaded_integrations.add(name)
    sbom.loader._COMPONENT_CACHE[name] = SimpleNamespace(
        module=SimpleNamespace(__file__=str(module_file))
    )


@pytest.fixture(autouse=True)
def _clear_component_cache():
    yield
    sbom.loader._COMPONENT_CACHE.clear()


@pytest.mark.parametrize(
    ("url", "subpath", "expected"),
    [
        ("https://github.com/esphome/esphome", None, "pkg:github/esphome/esphome@v1"),
        (
            "https://github.com/o/r.git",
            "components/x",
            "pkg:github/o/r@v1#components/x",
        ),
        ("https://gitlab.com/o/r/", ".", "pkg:gitlab/o/r@v1"),
        ("https://gitlab.com/group/sub/r", None, None),
        ("https://example.com/o/r", None, None),
    ],
)
def test_repo_purl(url: str, subpath: str | None, expected: str | None) -> None:
    assert sbom._repo_purl(url, "v1", subpath) == expected


@pytest.mark.parametrize(
    ("url", "expected"),
    [
        ("https://github.com/o/r", True),
        ("git://host/r", True),
        ("git@github.com:o/r.git", False),
        ("/local/path", False),
        (None, False),
    ],
)
def test_is_remote_url(url: str | None, expected: bool) -> None:
    assert sbom._is_remote_url(url) is expected


@pytest.mark.parametrize(
    ("direct_url", "expected"),
    [
        (json.dumps({"vcs_info": {"vcs": "git", "commit_id": "abc"}}), "abc"),
        (json.dumps({"dir_info": {"editable": True}}), None),
        (None, None),
        ("not json", None),
    ],
)
def test_esphome_commit(direct_url: str | None, expected: str | None) -> None:
    dist = MagicMock()
    dist.read_text.return_value = direct_url
    with patch("importlib.metadata.distribution", return_value=dist):
        assert sbom._esphome_commit() == expected


def test_esphome_manifest_release(build_tree: Path) -> None:
    with (
        patch.object(sbom, "ESPHOME_VERSION", "2026.10.1"),
        patch.object(sbom, "_esphome_commit", return_value=None),
    ):
        sbom.write_manifests()

    assert _read(build_tree / "src" / "sbom.yml") == {
        "name": "esphome",
        "version": "2026.10.1",
        "description": "ESPHome firmware runtime and generated code",
        "supplier": sbom.ESPHOME_SUPPLIER,
        "license": "GPL-3.0-only",
        "repository": "https://github.com/esphome/esphome",
        "purl": "pkg:github/esphome/esphome@2026.10.1",
    }


def test_esphome_manifest_dev_commit(build_tree: Path) -> None:
    """A git install is identified by its commit; a dev version is not a tag."""
    with (
        patch.object(sbom, "ESPHOME_VERSION", "2026.11.0-dev"),
        patch.object(sbom, "_esphome_commit", return_value="abc"),
    ):
        sbom.write_manifests()
    assert (
        _read(build_tree / "src" / "sbom.yml")["purl"]
        == "pkg:github/esphome/esphome@abc"
    )

    with (
        patch.object(sbom, "ESPHOME_VERSION", "2026.11.0-dev"),
        patch.object(sbom, "_esphome_commit", return_value=None),
    ):
        sbom.write_manifests()
    assert "purl" not in _read(build_tree / "src" / "sbom.yml")


def test_project_manifest(build_tree: Path) -> None:
    """The project manifest follows esphome.project and is removed with it."""
    CORE.config[CONF_ESPHOME][CONF_PROJECT] = {
        CONF_NAME: "acme.thermostat",
        CONF_VERSION: "1.2.3",
    }
    sbom.write_manifests()
    assert _read(build_tree / "sbom.yml") == {
        "name": "acme.thermostat",
        "version": "1.2.3",
    }

    del CORE.config[CONF_ESPHOME][CONF_PROJECT]
    sbom.write_manifests()
    assert not (build_tree / "sbom.yml").exists()


def test_external_components(build_tree: Path, tmp_path: Path) -> None:
    """Each loaded external component becomes its own package inside src."""
    git_dir = tmp_path / "repo" / "components"
    local_dir = tmp_path / "local"
    get_sources().extend(
        [
            ExternalSource(
                components_dir=git_dir,
                url="https://github.com/acme/parts",
                ref="main",
                commit="abc",
                repo_subpath="components",
            ),
            ExternalSource(components_dir=local_dir),
        ]
    )
    _add_external_component(git_dir, "widget")
    _add_external_component(local_dir, "gadget")
    # Provided by the source but not used by this configuration
    (git_dir / "unused").mkdir()

    sbom.write_manifests()

    assert _read(build_tree / "src" / "sbom.yml")["manifests"] == [
        {
            "dest": "esphome/components/gadget",
            "manifest": {"name": "gadget", "description": "Local external component"},
        },
        {
            "dest": "esphome/components/widget",
            "manifest": {
                "name": "widget",
                "description": "External component from https://github.com/acme/parts@main",
                "version": "abc",
                "repository": "https://github.com/acme/parts",
                "purl": "pkg:github/acme/parts@abc#components/widget",
            },
        },
    ]


def test_external_component_unusual_url() -> None:
    """A URL esp-idf-sbom would reject is kept only in the description."""
    source = ExternalSource(
        components_dir=Path("/x"), url="git@github.com:o/r.git", commit="abc"
    )
    assert sbom._external_manifest(source, "widget") == {
        "name": "widget",
        "description": "External component from git@github.com:o/r.git",
        "version": "abc",
    }


def test_esphome_commit_not_installed() -> None:
    with patch(
        "importlib.metadata.distribution",
        side_effect=importlib.metadata.PackageNotFoundError,
    ):
        assert sbom._esphome_commit() is None


def test_external_component_without_commit() -> None:
    """Without a commit there is no version and no PURL to pin it."""
    source = ExternalSource(components_dir=Path("/x"), url="https://github.com/o/r")
    assert sbom._external_manifest(source, "widget") == {
        "name": "widget",
        "description": "External component from https://github.com/o/r",
        "repository": "https://github.com/o/r",
    }


def test_external_manifests_skip_other_components(tmp_path: Path) -> None:
    """Built-in, unresolvable and file-less components stay in the ESPHome package."""
    components_dir = tmp_path / "components"
    get_sources().append(ExternalSource(components_dir=components_dir))
    _add_external_component(components_dir, "widget")
    _add_external_component(tmp_path / "builtin", "wifi")
    CORE.loaded_integrations.update({"missing", "namespace"})
    sbom.loader._COMPONENT_CACHE["namespace"] = SimpleNamespace(
        module=SimpleNamespace()
    )

    with patch.object(
        sbom.loader,
        "get_component",
        side_effect=sbom.loader._COMPONENT_CACHE.get,
    ):
        manifests = sbom._external_manifests()

    assert [m["dest"] for m in manifests] == ["esphome/components/widget"]
