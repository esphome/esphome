"""Unit tests for esphome.components.zephyr.framework_west."""

from __future__ import annotations

import os
from pathlib import Path
import time
from unittest.mock import patch

import platformdirs
import pytest
import yaml

from esphome.components.zephyr.framework_west import (
    _effective_requirements,
    _generate_synthetic_manifest,
    _source_cache_key,
    _tools_path,
    check_and_install,
)
from esphome.components.zephyr.variants import ZephyrModule, ZephyrSDK
from esphome.core import TimePeriodSeconds
from esphome.framework_helpers import get_python_env_executable_path

_FAKE_SDK = ZephyrSDK(manifest_url="https://example.invalid/zephyr")
_CACHE_KEY = "v4.4.1-my-branch-00000000"


def _set_sentinel_age(sentinel: Path, age_seconds: float) -> None:
    sentinel.touch()
    stamp = time.time() - age_seconds
    os.utime(sentinel, (stamp, stamp))


def _prepare_ready_workspace(tmp_path: Path, *, manifest_sentinel_age: float) -> Path:
    """Set up an already-initialized venv and west workspace on disk, with the
    workspace's `.ready` sentinel aged by `manifest_sentinel_age` seconds, so only
    the refresh path (never venv install / west init) is exercised."""
    python_env = tmp_path / "sdk-zephyr" / "penvs" / "v4.4.1"
    python_bin = get_python_env_executable_path(python_env, "python")
    python_bin.parent.mkdir(parents=True)
    python_bin.touch()
    (python_env / ".ready").write_text(_effective_requirements({}))

    framework = tmp_path / "sdk-zephyr" / "frameworks" / _CACHE_KEY
    zephyr_dir = framework / "zephyr"
    (framework / ".west").mkdir(parents=True)
    zephyr_dir.mkdir(parents=True)
    _set_sentinel_age(framework / ".ready", manifest_sentinel_age)
    return zephyr_dir


def _run_check_and_install(tmp_path: Path, refresh: TimePeriodSeconds):
    source = {
        "type": "git",
        "url": "https://example.invalid/fork.git",
        "ref": "my-branch",
    }
    calls: list[tuple[list[str], str | None]] = []

    def fake_run_command_ok(cmd, **kwargs):
        calls.append((cmd, kwargs.get("cwd")))
        return True

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
        patch("subprocess.run") as mock_subprocess_run,
    ):
        mock_subprocess_run.return_value.returncode = 0
        check_and_install(
            sdk=_FAKE_SDK,
            version="4.4.1",
            source=source,
            refresh=refresh,
        )

    return calls, mock_subprocess_run


def test_git_source_refreshes_manifest_repo_before_west_update(
    tmp_path: Path,
) -> None:
    """Regression test: `west update` never touches the manifest repository (the
    zephyr checkout itself) -- for a sdk_source: type: git moving ref, the manifest
    repo must be fetched/reset explicitly, or a stale checkout is reused forever.
    """
    zephyr_dir = _prepare_ready_workspace(tmp_path, manifest_sentinel_age=3600)

    calls, mock_subprocess_run = _run_check_and_install(
        tmp_path, TimePeriodSeconds(seconds=60)
    )

    fetch_calls = [(c, cwd) for c, cwd in calls if c[:2] == ["git", "fetch"]]
    reset_calls = [(c, cwd) for c, cwd in calls if c[:2] == ["git", "reset"]]
    assert len(fetch_calls) == 1
    assert fetch_calls[0][0][-1] == "my-branch"
    assert len(reset_calls) == 1
    # Both git commands must run inside the manifest repository (zephyr/), not the
    # west workspace root -- that's exactly what `west update` never touches.
    assert fetch_calls[0][1] == str(zephyr_dir)
    assert reset_calls[0][1] == str(zephyr_dir)

    # `west update` still runs afterward to sync the modules.
    assert mock_subprocess_run.called


def test_git_source_skips_manifest_refresh_within_window(tmp_path: Path) -> None:
    _prepare_ready_workspace(tmp_path, manifest_sentinel_age=5)

    calls, mock_subprocess_run = _run_check_and_install(
        tmp_path, TimePeriodSeconds(seconds=3600)
    )

    assert not any(c[:2] == ["git", "fetch"] for c, _cwd in calls)
    assert not mock_subprocess_run.called


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
