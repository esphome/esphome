"""Tests for the esp-idf-sbom manifests written into the build tree."""

from collections.abc import Generator
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
from esphome.git import GitCommandError


@pytest.fixture(name="build_tree")
def fixture_build_tree(tmp_path: Path) -> Generator[Path]:
    CORE.config_path = tmp_path / "test.yaml"
    CORE.build_path = tmp_path / "build"
    CORE.relative_src_path().mkdir(parents=True)
    CORE.config = {CONF_ESPHOME: {}}
    # Independent of how the test environment installed ESPHome
    with patch.object(sbom, "_esphome_vcs", return_value=None):
        yield CORE.build_path


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
        # Present but not understood: the source is unknown, not the release
        (_direct_url(None, "abc"), (None, None)),
        (_direct_url("https://github.com/esphome/esphome", None), (None, None)),
        (json.dumps({"dir_info": {"editable": True}}), (None, None)),
        (
            json.dumps(
                {
                    "url": "https://github.com/someone/esphome/archive/fix.tar.gz",
                    "archive_info": {},
                }
            ),
            (None, None),
        ),
        (json.dumps([]), (None, None)),
        ("not json", (None, None)),
        # No direct_url.json: installed from a package index
        (None, None),
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
            return_value=sbom._Source("https://github.com/esphome/esphome.git", "abc"),
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
    with patch.object(sbom, "_esphome_vcs", return_value=sbom._Source(url, "abc")):
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


def test_project_manifest_sbom(build_tree: Path) -> None:
    """Product details become the supplier and the document's manufacturer."""
    CORE.config[CONF_ESPHOME][CONF_PROJECT] = {
        CONF_NAME: "acme.thermostat",
        CONF_VERSION: "1.2.3",
        "sbom": {
            "supplier": "Acme Corp",
            "contact": "psirt@acme.example",
            "url": "https://acme.example/security",
        },
    }
    sbom.write_manifests()
    assert _read(build_tree / "sbom.yml") == {
        "name": "acme.thermostat",
        "version": "1.2.3",
        "supplier": "Organization: Acme Corp (psirt@acme.example)",
        "document": {
            "manufacturer": {
                "name": "Organization: Acme Corp",
                "contact": "psirt@acme.example",
                "url": "https://acme.example/security",
            }
        },
    }


def test_project_manifest_sbom_url_only(build_tree: Path) -> None:
    CORE.config[CONF_ESPHOME][CONF_PROJECT] = {
        CONF_NAME: "acme.thermostat",
        CONF_VERSION: "1.2.3",
        "sbom": {"supplier": "Acme Corp", "url": "https://acme.example"},
    }
    sbom.write_manifests()
    manifest = _read(build_tree / "sbom.yml")
    assert manifest["supplier"] == "Organization: Acme Corp"
    assert manifest["document"] == {
        "manufacturer": {
            "name": "Organization: Acme Corp",
            "url": "https://acme.example",
        }
    }


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
        components_dir=Path("/x"), url="git@example.com:o/r.git", repo_dir=Path("/x")
    )
    with patch.object(sbom, "run_git_command", return_value="abc"):
        manifest = sbom._external_manifest(source, "widget")
    assert manifest == {
        "name": "widget",
        "description": "External component from git@example.com:o/r.git",
        "version": "abc",
    }


def test_external_component_ssh_url(tmp_path: Path) -> None:
    """An SSH GitHub URL keeps its repository and PURL in https form."""
    source = ExternalSource(
        components_dir=tmp_path / "components",
        url="git@github.com:acme/parts.git",
        repo_dir=tmp_path,
    )
    with patch.object(sbom, "run_git_command", return_value="abc"):
        manifest = sbom._external_manifest(source, "widget")
    assert manifest["repository"] == "https://github.com/acme/parts.git"
    assert manifest["purl"] == "pkg:github/acme/parts@abc#components/widget"


@pytest.mark.parametrize(
    ("url", "expected"),
    [
        ("git@github.com:o/r.git", "https://github.com/o/r.git"),
        ("github.com:o/r", "https://github.com/o/r"),
        ("ssh://git@gitlab.com:2222/o/r.git", "https://gitlab.com/o/r.git"),
        ("git+ssh://git@github.com/o/r", "https://github.com/o/r"),
        ("git@example.com:o/r.git", "git@example.com:o/r.git"),
        ("ssh://git@example.com/o/r", "ssh://git@example.com/o/r"),
        ("https://github.com/o/r", "https://github.com/o/r"),
        ("/src/esphome", "/src/esphome"),
        ("C:\\src\\esphome", "C:\\src\\esphome"),
    ],
)
def test_https_repo_url(url: str, expected: str) -> None:
    assert sbom._https_repo_url(url) == expected


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


def _fake_git(outputs: dict[str, str | Exception]):
    """Answer git commands by their longest matching argument prefix."""

    def run(cmd: list[str], git_dir: Path) -> str:
        args = " ".join(cmd[1:])
        prefix = max((p for p in outputs if args.startswith(p)), key=len)
        if isinstance(result := outputs[prefix], Exception):
            raise result
        return result

    return run


def _checkout_outputs(**overrides: str | Exception) -> dict[str, str | Exception]:
    outputs: dict[str, str | Exception] = {
        "rev-parse HEAD": "abc",
        "rev-parse --abbrev-ref HEAD": "dev",
        "config --get branch.dev.remote": "origin",
        "remote get-url origin": "https://user:token@github.com/someone/esphome.git",
        "status": "",
        "branch --remotes": "  origin/dev",
    }
    outputs.update({k.replace("_", " "): v for k, v in overrides.items()})
    return outputs


def test_esphome_vcs_editable_checkout(tmp_path: Path) -> None:
    """An editable install from a git clone is described by that clone."""
    (tmp_path / ".git").mkdir()
    dist = MagicMock()
    dist.read_text.return_value = _editable_direct_url(tmp_path)
    with (
        patch("importlib.metadata.distribution", return_value=dist),
        patch.object(
            sbom, "run_git_command", side_effect=_fake_git(_checkout_outputs())
        ) as run_git,
    ):
        assert sbom._esphome_vcs() == ("https://github.com/someone/esphome.git", "abc")
    assert {call.kwargs["git_dir"] for call in run_git.call_args_list} == {tmp_path}


def test_checkout_vcs_tracked_remote(tmp_path: Path) -> None:
    """The URL and the pushed check both use the remote the branch tracks."""
    (tmp_path / ".git").mkdir()
    outputs = _checkout_outputs()
    outputs["config --get branch.dev.remote"] = "fork"
    outputs["remote get-url fork"] = "git@github.com:me/esphome.git"
    with patch.object(sbom, "run_git_command", side_effect=_fake_git(outputs)) as run:
        assert sbom._checkout_vcs(tmp_path) == ("git@github.com:me/esphome.git", "abc")
    assert ["git", "branch", "--remotes", "--contains", "HEAD", "--list", "fork/*"] in [
        call.args[0] for call in run.call_args_list
    ]


@pytest.mark.parametrize(
    ("overrides", "expected"),
    [
        # Detached HEAD: falls back to origin
        (
            {"config --get branch.dev.remote": GitCommandError("unset")},
            ("https://github.com/someone/esphome.git", "abc"),
        ),
        # No remote at all: nothing that can be fetched
        ({"remote get-url origin": GitCommandError("no such remote")}, (None, None)),
        # Pushed check fails: keep the repository, drop the commit
        (
            {"branch --remotes": GitCommandError("bad ref")},
            ("https://github.com/someone/esphome.git", None),
        ),
    ],
    ids=["detached", "no-remote", "pushed-check-fails"],
)
def test_checkout_vcs_remote_fallbacks(
    tmp_path: Path, overrides: dict[str, str | Exception], expected: tuple
) -> None:
    (tmp_path / ".git").mkdir()
    outputs = _checkout_outputs()
    outputs.update(overrides)
    with patch.object(sbom, "run_git_command", side_effect=_fake_git(outputs)):
        assert sbom._checkout_vcs(tmp_path) == expected


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


@pytest.mark.parametrize(
    ("status", "branch"),
    [(" M esphome/core/application.cpp", "  origin/dev"), ("", "")],
    ids=["modified", "unpushed"],
)
def test_esphome_vcs_editable_not_exact(
    tmp_path: Path, status: str, branch: str
) -> None:
    """Local edits or an unpushed commit keep the repository but drop the commit."""
    (tmp_path / ".git").mkdir()
    dist = MagicMock()
    dist.read_text.return_value = _editable_direct_url(tmp_path)
    outputs = _checkout_outputs()
    outputs["remote get-url origin"] = "git@github.com:someone/esphome.git"
    outputs["status"] = status
    outputs["branch --remotes"] = branch
    with (
        patch("importlib.metadata.distribution", return_value=dist),
        patch.object(sbom, "run_git_command", side_effect=_fake_git(outputs)),
    ):
        assert sbom._esphome_vcs() == ("git@github.com:someone/esphome.git", None)


def test_esphome_vcs_editable_git_error(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """An unreadable checkout is an unknown source, not the upstream release."""
    (tmp_path / ".git").mkdir()
    dist = MagicMock()
    dist.read_text.return_value = _editable_direct_url(tmp_path)
    with (
        patch("importlib.metadata.distribution", return_value=dist),
        patch.object(
            sbom, "run_git_command", side_effect=GitCommandError("broken repo")
        ),
    ):
        assert sbom._esphome_vcs() == (None, None)
    assert "cannot read the git checkout ESPHome runs from" in caplog.text


@pytest.mark.parametrize(
    ("source", "expected"),
    [
        (
            sbom._Source("git@github.com:someone/esphome.git", "abc"),
            {
                "repository": "https://github.com/someone/esphome.git",
                "purl": "pkg:github/someone/esphome@abc",
            },
        ),
        (
            sbom._Source("git@github.com:someone/esphome.git", None),
            {"repository": "https://github.com/someone/esphome.git"},
        ),
        (sbom._Source(None, None), {}),
    ],
    ids=["ssh", "not-exact", "unknown"],
)
def test_esphome_manifest_checkout(
    build_tree: Path, source: sbom._Source, expected: dict[str, str]
) -> None:
    with patch.object(sbom, "_esphome_vcs", return_value=source):
        sbom.write_manifests()
    manifest = _read(build_tree / "src" / "sbom.yml")
    assert {k: manifest[k] for k in ("repository", "purl") if k in manifest} == expected


def test_esphome_vcs_unknown_warns_without_url(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """An unknown install is reported without logging the URL, which may hold a token."""
    dist = MagicMock()
    dist.read_text.return_value = json.dumps(
        {"url": "https://user:secret@example.com/esphome.tar.gz", "archive_info": {}}
    )
    with patch("importlib.metadata.distribution", return_value=dist):
        assert sbom._esphome_vcs() == (None, None)
    assert "cannot tell where ESPHome was installed from" in caplog.text
    assert "secret" not in caplog.text


def test_write_manifests_never_fails_the_build(
    build_tree: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A failed run logs and removes the earlier build's manifests."""
    for path in (build_tree / "sbom.yml", build_tree / "src" / "sbom.yml"):
        path.write_text("name: stale\n")
    with patch.object(sbom, "_write_manifests", side_effect=OSError("disk full")):
        sbom.write_manifests()
    assert "could not write sbom.yml manifests: disk full" in caplog.text
    assert not (build_tree / "sbom.yml").exists()
    assert not (build_tree / "src" / "sbom.yml").exists()


def test_source_commit_failure_warns(caplog: pytest.LogCaptureFixture) -> None:
    source = ExternalSource(
        components_dir=Path("/x"), url="https://github.com/o/r", repo_dir=Path("/x")
    )
    with patch.object(
        sbom, "run_git_command", side_effect=GitCommandError("not a repo")
    ):
        assert sbom._source_commit(source) is None
    assert "cannot read commit in" in caplog.text


@pytest.mark.parametrize("absolute", [True, False], ids=["absolute", "parent"])
def test_external_component_outside_repo(
    build_tree: Path, tmp_path: Path, caplog: pytest.LogCaptureFixture, absolute: bool
) -> None:
    """A components path outside the repository loses only that component's PURL."""
    repo_dir = tmp_path / "repo"
    # What repo_dir / path gives for path: /abs/components and path: ../outside
    path = str(tmp_path / "elsewhere" / "components") if absolute else "../outside"
    components_dir = repo_dir / path
    get_sources().append(
        ExternalSource(
            components_dir=components_dir,
            url="https://github.com/acme/parts",
            repo_dir=repo_dir,
        )
    )
    _add_external_component(components_dir, "widget")

    with patch.object(sbom, "run_git_command", return_value="abc"):
        sbom.write_manifests()

    manifest = _read(build_tree / "src" / "sbom.yml")
    assert manifest["name"] == "esphome"
    widget = manifest["manifests"][0]["manifest"]
    assert widget["version"] == "abc"
    assert "purl" not in widget
    assert "component 'widget' is outside its repository" in caplog.text


def test_checkout_vcs_status_scope(tmp_path: Path) -> None:
    """Changes are looked for in the package only, untracked files included."""
    (tmp_path / ".git").mkdir()
    with patch.object(
        sbom, "run_git_command", side_effect=_fake_git(_checkout_outputs())
    ) as run:
        sbom._checkout_vcs(tmp_path)
    assert ["git", "status", "--porcelain", "--", "esphome"] in [
        call.args[0] for call in run.call_args_list
    ]


def test_checkout_vcs_pushed_check_failure_warns(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    (tmp_path / ".git").mkdir()
    outputs = _checkout_outputs()
    outputs["branch --remotes"] = GitCommandError("bad ref")
    with patch.object(sbom, "run_git_command", side_effect=_fake_git(outputs)):
        assert sbom._checkout_vcs(tmp_path)[1] is None
    assert "cannot check whether origin has HEAD" in caplog.text


def test_project_manifest_sbom_contact_only(build_tree: Path) -> None:
    CORE.config[CONF_ESPHOME][CONF_PROJECT] = {
        CONF_NAME: "acme.thermostat",
        CONF_VERSION: "1.2.3",
        "sbom": {"supplier": "Acme Corp", "contact": "psirt@acme.example"},
    }
    sbom.write_manifests()
    assert _read(build_tree / "sbom.yml")["document"] == {
        "manufacturer": {
            "name": "Organization: Acme Corp",
            "contact": "psirt@acme.example",
        }
    }
