"""Unit tests for esphome.components.zephyr.framework_west."""

from __future__ import annotations

from pathlib import Path
from unittest.mock import MagicMock, patch

import platformdirs
import pytest
import yaml

from esphome.components.zephyr.const import KEY_SDK_SOURCE_RESOLVED_REF
from esphome.components.zephyr.framework_west import (
    _generate_synthetic_manifest,
    _source_cache_key,
    _tools_path,
    check_and_install,
)
from esphome.components.zephyr.variants import ZephyrModule, ZephyrSDK

_FAKE_SDK = ZephyrSDK(manifest_url="https://example.invalid/zephyr")
_CACHE_KEY = "v4.4.1-my-branch-00000000"


class _Calls(list):
    """run_command_ok() commands; cwds[i] is the cwd= that call i was given."""

    def __init__(self) -> None:
        super().__init__()
        self.cwds: list[str | None] = []


def _make_fake_run_command_ok(tmp_path: Path, manifest_path: str = "zephyr"):
    """Record every run_command_ok() call; a `west init` also creates the
    framework and .west/config it would leave on disk (manifest.path is where west put
    the manifest repository), since check_and_install() depends on them existing."""
    calls = _Calls()

    def fake_run_command_ok(cmd, **kwargs):
        calls.append(cmd)
        calls.cwds.append(kwargs.get("cwd"))
        if cmd[2:4] == ["west", "init"]:
            west_dir = tmp_path / "sdk-zephyr" / "frameworks" / _CACHE_KEY / ".west"
            west_dir.mkdir(parents=True, exist_ok=True)
            (west_dir / "config").write_text(f"[manifest]\npath = {manifest_path}\n")
        return True

    return calls, fake_run_command_ok


def _fake_create_venv(root: Path, msg: str | None = None) -> None:
    python = Path(root) / "bin" / "python"
    python.parent.mkdir(parents=True, exist_ok=True)
    python.touch()


def _run_check_and_install(
    tmp_path: Path,
    source: dict,
    modules: list[ZephyrModule] | None = None,
    manifest_path: str = "zephyr",
) -> tuple[_Calls, MagicMock]:
    """Return the run_command_ok() calls and the mocked subprocess.run."""
    calls, fake_run_command_ok = _make_fake_run_command_ok(tmp_path, manifest_path)
    with (
        patch(
            "esphome.components.zephyr.framework_west._tools_path",
            return_value=tmp_path / "sdk-zephyr",
        ),
        patch(
            "esphome.components.zephyr.framework_west._source_cache_key",
            return_value=_CACHE_KEY,
        ),
        patch(
            "esphome.components.zephyr.framework_west.run_command_ok",
            side_effect=fake_run_command_ok,
        ),
        patch(
            "esphome.components.zephyr.framework_west.create_venv",
            side_effect=_fake_create_venv,
        ),
        patch("subprocess.run") as mock_subprocess_run,
    ):
        mock_subprocess_run.return_value.returncode = 0
        check_and_install(
            sdk=_FAKE_SDK, version="4.4.1", source=source, modules=modules
        )
    return calls, mock_subprocess_run


def _git_source(resolved_sha: str) -> dict:
    return {
        "type": "git",
        "url": "https://example.invalid/fork.git",
        "ref": "my-branch",
        KEY_SDK_SOURCE_RESOLVED_REF: resolved_sha,
    }


def _manifest_revision(framework: Path) -> str:
    manifest = yaml.safe_load((framework / "esphome-manifest" / "west.yml").read_text())
    return manifest["manifest"]["projects"][0]["revision"]


def _git_calls(calls: list[list[str]]) -> list[list[str]]:
    return [c for c in calls if c[0] == "git"]


def test_git_source_west_init_uses_ref_then_pins_manifest_repo_to_sha(
    tmp_path: Path,
) -> None:
    """A git sdk_source: is pinned to one resolved commit SHA before
    check_and_install() ever runs (dts_fetch.resolve_sdk_source_version()). `west init
    --mr` can't take a SHA (git clone --branch rejects it), so it inits at the source's
    own ref and the manifest repository is then pinned to the SHA with git."""
    resolved_sha = "a" * 40
    calls, _ = _run_check_and_install(tmp_path, _git_source(resolved_sha))

    init_calls = [c for c in calls if c[2:4] == ["west", "init"]]
    assert len(init_calls) == 1
    assert init_calls[0][init_calls[0].index("--mr") + 1] == "my-branch"
    assert resolved_sha not in init_calls[0]
    assert _git_calls(calls) == [
        ["git", "fetch", "--depth=1", "--", "origin", resolved_sha],
        ["git", "reset", "--hard", "FETCH_HEAD"],
    ]
    framework = tmp_path / "sdk-zephyr" / "frameworks" / _CACHE_KEY
    assert (framework / ".resolved_ref").read_text() == resolved_sha


def test_git_source_pins_manifest_repo_where_west_put_it(tmp_path: Path) -> None:
    """The manifest repository isn't always framework/zephyr: NCS's manifest declares
    `self: path: nrf`, and west records wherever it put it in .west/config."""
    resolved_sha = "a" * 40
    calls, _ = _run_check_and_install(
        tmp_path, _git_source(resolved_sha), manifest_path="nrf"
    )

    framework = tmp_path / "sdk-zephyr" / "frameworks" / _CACHE_KEY
    fetch = ["git", "fetch", "--depth=1", "--", "origin", resolved_sha]
    assert calls.cwds[calls.index(fetch)] == str(framework / "nrf")
    reset = ["git", "reset", "--hard", "FETCH_HEAD"]
    assert calls.cwds[calls.index(reset)] == str(framework / "nrf")


def test_git_source_moved_ref_repins_manifest_repo_without_reinit(
    tmp_path: Path,
) -> None:
    """A branch that moved since the workspace was initialized is updated in place."""
    _run_check_and_install(tmp_path, _git_source("a" * 40))
    new_sha = "b" * 40
    calls, mock_run = _run_check_and_install(tmp_path, _git_source(new_sha))

    assert not [c for c in calls if c[2:4] == ["west", "init"]]
    assert _git_calls(calls)[0] == [
        "git",
        "fetch",
        "--depth=1",
        "--",
        "origin",
        new_sha,
    ]
    assert mock_run.call_args.args[0][2:4] == ["west", "update"]
    framework = tmp_path / "sdk-zephyr" / "frameworks" / _CACHE_KEY
    assert (framework / ".resolved_ref").read_text() == new_sha


def test_git_source_unchanged_ref_skips_update(tmp_path: Path) -> None:
    _run_check_and_install(tmp_path, _git_source("a" * 40))
    calls, mock_run = _run_check_and_install(tmp_path, _git_source("a" * 40))

    assert not calls
    mock_run.assert_not_called()


_MODULE = ZephyrModule(name="mod", manifest_url="https://example.invalid/mod.git")


def test_git_source_with_modules_pins_sha_in_generated_manifest(
    tmp_path: Path,
) -> None:
    """With modules the workspace is rooted at a generated manifest, so the SHA goes
    there (revision:), not into a git pin of the manifest repository."""
    resolved_sha = "a" * 40
    calls, _ = _run_check_and_install(
        tmp_path, _git_source(resolved_sha), modules=[_MODULE]
    )

    init_calls = [c for c in calls if c[2:4] == ["west", "init"]]
    assert init_calls[0][4:] == ["-l", "esphome-manifest"]
    assert ["git", "fetch", "--depth=1", "--", "origin", resolved_sha] not in calls
    framework = tmp_path / "sdk-zephyr" / "frameworks" / _CACHE_KEY
    assert _manifest_revision(framework) == resolved_sha


def test_git_source_with_modules_moved_ref_regenerates_manifest(
    tmp_path: Path,
) -> None:
    _run_check_and_install(tmp_path, _git_source("a" * 40), modules=[_MODULE])
    new_sha = "b" * 40
    calls, mock_run = _run_check_and_install(
        tmp_path, _git_source(new_sha), modules=[_MODULE]
    )

    assert not [c for c in calls if c[2:4] == ["west", "init"]]
    assert mock_run.call_args.args[0][2:4] == ["west", "update"]
    framework = tmp_path / "sdk-zephyr" / "frameworks" / _CACHE_KEY
    assert _manifest_revision(framework) == new_sha
    assert (framework / ".resolved_ref").read_text() == new_sha


def test_git_source_west_init_falls_back_to_raw_ref_when_unresolved(
    tmp_path: Path,
) -> None:
    """Defensive fallback: a source dict without KEY_SDK_SOURCE_RESOLVED_REF (e.g.
    constructed directly, bypassing resolve_sdk_source_version()) still works,
    using the raw ref as before."""
    source = {
        "type": "git",
        "url": "https://example.invalid/fork.git",
        "ref": "my-branch",
    }
    calls, fake_run_command_ok = _make_fake_run_command_ok(tmp_path)

    with (
        patch(
            "esphome.components.zephyr.framework_west._tools_path",
            return_value=tmp_path / "sdk-zephyr",
        ),
        patch(
            "esphome.components.zephyr.framework_west._source_cache_key",
            return_value=_CACHE_KEY,
        ),
        patch(
            "esphome.components.zephyr.framework_west.run_command_ok",
            side_effect=fake_run_command_ok,
        ),
        patch(
            "esphome.components.zephyr.framework_west.create_venv",
            side_effect=lambda root, msg=None: Path(root).mkdir(
                parents=True, exist_ok=True
            ),
        ),
        patch("subprocess.run") as mock_subprocess_run,
    ):
        mock_subprocess_run.return_value.returncode = 0
        check_and_install(sdk=_FAKE_SDK, version="4.4.1", source=source)

    init_calls = [c for c in calls if c[2:4] == ["west", "init"]]
    assert len(init_calls) == 1
    assert init_calls[0][init_calls[0].index("--mr") + 1] == "my-branch"


# ---------------------------------------------------------------------------
# _source_cache_key -- module set identity
# ---------------------------------------------------------------------------


def test_source_cache_key_differs_when_module_url_changes_but_name_and_rev_match() -> (
    None
):
    # Same name+revision, different manifest_url (e.g. repointed at a fork on the
    # same branch name) -- must not collide, or a source change would silently
    # reuse a stale cached workspace.
    module_a = [ZephyrModule(name="mod", manifest_url="http://a", revision="main")]
    module_b = [ZephyrModule(name="mod", manifest_url="http://b", revision="main")]

    assert _source_cache_key(_FAKE_SDK, "v1.0.0", None, module_a) != _source_cache_key(
        _FAKE_SDK, "v1.0.0", None, module_b
    )


def test_source_cache_key_matches_for_identical_module_set() -> None:
    modules = [ZephyrModule(name="mod", manifest_url="http://a", revision="main")]

    assert _source_cache_key(_FAKE_SDK, "v1.0.0", None, modules) == _source_cache_key(
        _FAKE_SDK, "v1.0.0", None, modules
    )


def test_source_cache_key_differs_across_sdk_variants() -> None:
    # MAINLINE/NCS/SILABS share one machine-global cache root; a matching version:
    # string must not resolve to the same directory for different SDKs.
    mainline = ZephyrSDK(
        manifest_url="https://example.invalid/zephyr", tools_subdir="a"
    )
    ncs = ZephyrSDK(manifest_url="https://example.invalid/nrf", tools_subdir="b")

    assert _source_cache_key(mainline, "v1.0.0", None) != _source_cache_key(
        ncs, "v1.0.0", None
    )


# ---------------------------------------------------------------------------
# _generate_synthetic_manifest -- ref-less root source
# ---------------------------------------------------------------------------


def test_generate_synthetic_manifest_omits_null_revision(tmp_path: Path) -> None:
    # manifest_rev=None (a ref-less sdk_source: git:) must not become a literal
    # `revision: null` in the generated west.yml -- omitted means "track the
    # source's default branch," matching the plain `west init -m` path's own
    # `if manifest_rev: cmd += ["--mr", manifest_rev]` handling.
    with patch(
        "esphome.components.zephyr.framework_west.run_command_ok", return_value=True
    ):
        manifest_dir = _generate_synthetic_manifest(
            tmp_path, "https://example.invalid/root.git", None, []
        )

    manifest = yaml.safe_load((manifest_dir / "west.yml").read_text())
    root_project = manifest["manifest"]["projects"][0]
    assert "revision" not in root_project


def test_generate_synthetic_manifest_keeps_revision_when_given(tmp_path: Path) -> None:
    with patch(
        "esphome.components.zephyr.framework_west.run_command_ok", return_value=True
    ):
        manifest_dir = _generate_synthetic_manifest(
            tmp_path, "https://example.invalid/root.git", "v1.2.3", []
        )

    manifest = yaml.safe_load((manifest_dir / "west.yml").read_text())
    assert manifest["manifest"]["projects"][0]["revision"] == "v1.2.3"


def test_generate_synthetic_manifest_root_name_defaults_to_url_basename(
    tmp_path: Path,
) -> None:
    with patch(
        "esphome.components.zephyr.framework_west.run_command_ok", return_value=True
    ):
        manifest_dir = _generate_synthetic_manifest(
            tmp_path, "https://github.com/nrfconnect/sdk-nrf", "v3.4.0", []
        )

    manifest = yaml.safe_load((manifest_dir / "west.yml").read_text())
    assert manifest["manifest"]["projects"][0]["name"] == "sdk-nrf"


def test_generate_synthetic_manifest_root_name_override(tmp_path: Path) -> None:
    # NCS's own sysbuild/Kconfig scripts hardcode "nrf" as the project name --
    # regression coverage for the real bug this fixes (a mismatched name leaves
    # e.g. SYSBUILD_NRF_KCONFIG unset and breaks sysbuild's Kconfig configure step).
    with patch(
        "esphome.components.zephyr.framework_west.run_command_ok", return_value=True
    ):
        manifest_dir = _generate_synthetic_manifest(
            tmp_path,
            "https://github.com/nrfconnect/sdk-nrf",
            "v3.4.0",
            [],
            west_project_name="nrf",
        )

    manifest = yaml.safe_load((manifest_dir / "west.yml").read_text())
    assert manifest["manifest"]["projects"][0]["name"] == "nrf"


# ---------------------------------------------------------------------------
# _tools_path -- machine-global cache location
# ---------------------------------------------------------------------------


def test_tools_path_env_override(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    override = tmp_path / "custom" / "sdk-zephyr"
    monkeypatch.setenv("ESPHOME_SDK_ZEPHYR_PREFIX", str(override))
    assert _tools_path() == override.resolve()


def test_tools_path_default_is_global_cache(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv("ESPHOME_SDK_ZEPHYR_PREFIX", raising=False)
    expected = (
        Path(platformdirs.user_cache_dir("esphome", appauthor=False)) / "sdk-zephyr"
    ).resolve()
    assert _tools_path() == expected
