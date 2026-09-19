"""Unit tests for esphome.components.zephyr.framework_west."""

from __future__ import annotations

from pathlib import Path
from unittest.mock import patch

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


def _make_fake_run_command_ok(tmp_path: Path):
    """Record every run_command_ok() call; a `west init` also creates the
    framework dir it would leave on disk, since check_and_install()'s own
    sentinel.touch() at the end depends on it existing."""
    calls: list[list[str]] = []

    def fake_run_command_ok(cmd, **kwargs):
        calls.append(cmd)
        if cmd[2:4] == ["west", "init"]:
            (tmp_path / "sdk-zephyr" / "frameworks" / _CACHE_KEY).mkdir(
                parents=True, exist_ok=True
            )
        return True

    return calls, fake_run_command_ok


def test_git_source_west_init_pins_to_resolved_ref_not_raw_branch(
    tmp_path: Path,
) -> None:
    """A git sdk_source: is pinned to one resolved commit SHA before
    check_and_install() ever runs (dts_fetch.resolve_sdk_source_version()) -- the
    first-time `west init --mr` must use that exact SHA, not let `west` itself
    independently re-resolve "whatever HEAD of the branch is right now"."""
    resolved_sha = "a" * 40
    source = {
        "type": "git",
        "url": "https://example.invalid/fork.git",
        "ref": "my-branch",
        KEY_SDK_SOURCE_RESOLVED_REF: resolved_sha,
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
    assert "--mr" in init_calls[0]
    assert init_calls[0][init_calls[0].index("--mr") + 1] == resolved_sha
    assert "my-branch" not in init_calls[0]


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
