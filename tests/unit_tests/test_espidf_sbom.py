"""Tests for the esp-idf-sbom manifests written into the build tree."""

import importlib.metadata
import json
import logging
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import MagicMock, patch

import pytest

from esphome import yaml_util
from esphome.components.external_components import ExternalSource, get_sources
from esphome.const import CONF_ESPHOME, CONF_NAME, CONF_PROJECT, CONF_VERSION
from esphome.core import CORE
from esphome.espidf import sbom
from esphome.git import GitCommandError


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


def _direct_url(url: str | None, commit: str | None) -> str:
    return json.dumps({"url": url, "vcs_info": {"vcs": "git", "commit_id": commit}})


@pytest.mark.parametrize(
    ("direct_url", "expected"),
    [
        (
            _direct_url("https://github.com/esphome/esphome", "abc"),
            ("https://github.com/esphome/esphome", "abc"),
        ),
        (
            _direct_url("https://user:token@github.com/someone/esphome", "abc"),
            ("https://github.com/someone/esphome", "abc"),
        ),
        (_direct_url(None, "abc"), None),
        (_direct_url("https://github.com/esphome/esphome", None), None),
        (json.dumps({"dir_info": {"editable": True}}), None),
        (json.dumps([]), None),
        (None, None),
        ("not json", None),
    ],
)
def test_esphome_vcs(direct_url: str | None, expected: tuple[str, str] | None) -> None:
    dist = MagicMock()
    dist.read_text.return_value = direct_url
    with patch("importlib.metadata.distribution", return_value=dist):
        assert sbom._esphome_vcs() == expected


def test_esphome_manifest_release(build_tree: Path) -> None:
    with (
        patch.object(sbom, "ESPHOME_VERSION", "2026.10.1"),
        patch.object(sbom, "_esphome_vcs", return_value=None),
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
        patch.object(
            sbom,
            "_esphome_vcs",
            return_value=("https://github.com/esphome/esphome.git", "abc"),
        ),
    ):
        sbom.write_manifests()
    assert (
        _read(build_tree / "src" / "sbom.yml")["purl"]
        == "pkg:github/esphome/esphome@abc"
    )

    with (
        patch.object(sbom, "ESPHOME_VERSION", "2026.11.0-dev"),
        patch.object(sbom, "_esphome_vcs", return_value=None),
    ):
        sbom.write_manifests()
    assert "purl" not in _read(build_tree / "src" / "sbom.yml")


@pytest.mark.parametrize(
    ("url", "expected"),
    [
        (
            "https://github.com/someone/esphome",
            {
                "repository": "https://github.com/someone/esphome",
                "purl": "pkg:github/someone/esphome@abc",
            },
        ),
        ("file:///src/esphome", {}),
    ],
)
def test_esphome_manifest_fork(
    build_tree: Path, url: str, expected: dict[str, str]
) -> None:
    """A git install from a fork or a local checkout names that source, not upstream."""
    with patch.object(sbom, "_esphome_vcs", return_value=(url, "abc")):
        sbom.write_manifests()
    manifest = _read(build_tree / "src" / "sbom.yml")
    assert {k: manifest[k] for k in ("repository", "purl") if k in manifest} == expected


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
                repo_dir=tmp_path / "repo",
            ),
            ExternalSource(components_dir=local_dir),
        ]
    )
    _add_external_component(git_dir, "widget")
    _add_external_component(local_dir, "gadget")
    # Provided by the source but not used by this configuration
    (git_dir / "unused").mkdir()

    with patch.object(sbom, "run_git_command", return_value="abc") as run_git:
        sbom.write_manifests()

    run_git.assert_called_once_with(
        ["git", "rev-parse", "HEAD"], git_dir=tmp_path / "repo"
    )

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
        components_dir=Path("/x"), url="git@github.com:o/r.git", repo_dir=Path("/x")
    )
    with patch.object(sbom, "run_git_command", return_value="abc"):
        manifest = sbom._external_manifest(source, "widget")
    assert manifest == {
        "name": "widget",
        "description": "External component from git@github.com:o/r.git",
        "version": "abc",
    }


def test_esphome_commit_not_installed() -> None:
    with patch(
        "importlib.metadata.distribution",
        side_effect=importlib.metadata.PackageNotFoundError,
    ):
        assert sbom._esphome_vcs() is None


def test_external_component_without_commit() -> None:
    """Without a readable commit there is no version and no PURL to pin it."""
    expected = {
        "name": "widget",
        "description": "External component from https://github.com/o/r",
        "repository": "https://github.com/o/r",
    }
    source = ExternalSource(components_dir=Path("/x"), url="https://github.com/o/r")
    assert sbom._external_manifest(source, "widget") == expected

    source.repo_dir = Path("/x")
    with patch.object(
        sbom, "run_git_command", side_effect=GitCommandError("not a repo")
    ):
        assert sbom._external_manifest(source, "widget") == expected


def test_external_manifests_skip_other_components(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """Only external components are carved out; unlocatable ones are reported."""
    components_dir = tmp_path / "components"
    get_sources().append(ExternalSource(components_dir=components_dir))
    _add_external_component(components_dir, "widget")
    _add_external_component(tmp_path / "builtin", "wifi")
    CORE.loaded_integrations.update({"missing", "namespace"})
    sbom.loader._COMPONENT_CACHE["namespace"] = SimpleNamespace(
        module=SimpleNamespace()
    )

    with (
        patch.object(
            sbom.loader,
            "get_component",
            side_effect=sbom.loader._COMPONENT_CACHE.get,
        ),
        patch.object(
            sbom.loader, "CORE_COMPONENTS_PATH", (tmp_path / "builtin").resolve()
        ),
    ):
        manifests = sbom._external_manifests()

    assert [m["dest"] for m in manifests] == ["esphome/components/widget"]
    assert "'wifi'" not in caplog.text
    assert "cannot locate component 'missing'" in caplog.text
    assert "cannot locate component 'namespace'" in caplog.text


def test_external_manifests_warn_unmatched(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A component from outside ESPHome that matches no source is not dropped silently."""
    _add_external_component(tmp_path / "stray", "widget")

    sbom._external_manifests()

    assert "SBOM: component 'widget'" in caplog.text
    assert "matches no external component source" in caplog.text


def _editable_direct_url(path: Path) -> str:
    return json.dumps({"url": path.as_uri(), "dir_info": {"editable": True}})


def test_esphome_vcs_editable_checkout(tmp_path: Path) -> None:
    """An editable install from a git clone is described by that clone."""
    (tmp_path / ".git").mkdir()
    dist = MagicMock()
    dist.read_text.return_value = _editable_direct_url(tmp_path)
    outputs = {
        "rev-parse": "abc",
        "ls-remote": "https://user:token@github.com/someone/esphome.git",
    }
    with (
        patch("importlib.metadata.distribution", return_value=dist),
        patch.object(
            sbom, "run_git_command", side_effect=lambda cmd, git_dir: outputs[cmd[1]]
        ) as run_git,
    ):
        assert sbom._esphome_vcs() == ("https://github.com/someone/esphome.git", "abc")
    assert {call.kwargs["git_dir"] for call in run_git.call_args_list} == {tmp_path}


def test_esphome_vcs_editable_without_git(tmp_path: Path) -> None:
    """A local tree without .git, like the Docker image's, keeps the release description."""
    dist = MagicMock()
    dist.read_text.return_value = _editable_direct_url(tmp_path)
    with (
        patch("importlib.metadata.distribution", return_value=dist),
        patch.object(sbom, "run_git_command") as run_git,
    ):
        assert sbom._esphome_vcs() is None
    run_git.assert_not_called()


def test_esphome_vcs_editable_git_error(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    (tmp_path / ".git").mkdir()
    dist = MagicMock()
    dist.read_text.return_value = _editable_direct_url(tmp_path)
    with (
        caplog.at_level(logging.DEBUG),
        patch("importlib.metadata.distribution", return_value=dist),
        patch.object(
            sbom, "run_git_command", side_effect=GitCommandError("broken repo")
        ),
    ):
        assert sbom._esphome_vcs() is None
    assert "cannot read git checkout" in caplog.text
