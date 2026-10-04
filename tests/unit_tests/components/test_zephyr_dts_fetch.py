"""Unit tests for esphome.components.zephyr.dts_fetch."""

from __future__ import annotations

from collections.abc import Callable, Iterator
import hashlib
import os
from pathlib import Path
import subprocess
from typing import Any
from unittest.mock import MagicMock, patch

import pytest

from esphome.components.zephyr.const import KEY_SDK_SOURCE_RESOLVED_REF, KEY_ZEPHYR
from esphome.components.zephyr.dts_fetch import (
    _SPARSE_CHECKOUT_SCHEMA,
    _dts_cache_root,
    _framework_base_version,
    _manifest_revision_cache_root,
    _native_dts_path,
    _parse_zephyr_version_file,
    _resolve_boards_ref,
    _resolve_git_head,
    _sdk_source_cache_key,
    _sdk_source_version_cache_root,
    _sparse_clone_dts,
    _sparse_clone_dts_from_source,
    _sparse_clone_hal_modules,
    fetch_board_dts,
    resolve_sdk_source_version,
)
from esphome.components.zephyr.variants import MAINLINE, NCS, SILABS, ZephyrSDK
import esphome.config_validation as cv
from esphome.const import (
    CONF_PATH,
    CONF_REF,
    CONF_TYPE,
    CONF_URL,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    TYPE_GIT,
    TYPE_LOCAL,
)
from esphome.core import CORE, TimePeriodSeconds
from tests.unit_tests.components.zephyr_state import empty_zephyr_data


def _set_framework_version(major: int, minor: int, patch: int, extra: str = "") -> None:
    CORE.data[KEY_CORE] = {
        KEY_FRAMEWORK_VERSION: cv.Version(major, minor, patch, extra)
    }


# ---------------------------------------------------------------------------
# _framework_base_version
# ---------------------------------------------------------------------------


def test_framework_base_version_formats_major_minor_patch() -> None:
    _set_framework_version(4, 4, 1)
    assert _framework_base_version() == "4.4.1"


def test_framework_base_version_keeps_release_candidate_suffix() -> None:
    _set_framework_version(4, 5, 0, "rc1")
    assert _framework_base_version() == "4.5.0-rc1"


# ---------------------------------------------------------------------------
# _resolve_git_head / resolve_sdk_source_version -- single ref resolution
# (issue: a moving git sdk_source: ref was independently re-resolved by three
# different consumers; resolve_sdk_source_version() now resolves it once and
# stashes it onto source[KEY_SDK_SOURCE_RESOLVED_REF] for the others to reuse)
# ---------------------------------------------------------------------------


def test_resolve_git_head_returns_rev_parse_output(tmp_path: Path) -> None:
    with patch("subprocess.run") as mock_run:
        mock_run.return_value = subprocess.CompletedProcess(
            [], 0, stdout=f"{_FAKE_RESOLVED_SHA}\n", stderr=""
        )
        assert _resolve_git_head(tmp_path) == _FAKE_RESOLVED_SHA
    mock_run.assert_called_once_with(
        ["git", "-C", str(tmp_path), "rev-parse", "HEAD"],
        check=True,
        capture_output=True,
        text=True,
    )


def test_resolve_git_head_raises_cv_invalid_on_git_failure(tmp_path: Path) -> None:
    with (
        patch(
            "subprocess.run",
            side_effect=subprocess.CalledProcessError(
                1, ["git"], stderr="not a git repository"
            ),
        ),
        pytest.raises(cv.Invalid, match="Can't resolve sdk_source commit"),
    ):
        _resolve_git_head(tmp_path)


def test_resolve_sdk_source_version_sets_resolved_ref_on_cache_hit(
    tmp_path: Path,
) -> None:
    """Even when the cached VERSION file is fresh enough to skip re-cloning,
    the resolved commit must still be captured from the existing checkout --
    it's the single value the other two consumers pin to for this run."""
    source = {"type": "git", "url": "https://example.invalid/zephyr", "ref": "main"}
    dest = tmp_path / _sdk_source_cache_key(source["url"], source["ref"])
    dest.mkdir(parents=True)
    (dest / "VERSION").write_text(
        "VERSION_MAJOR = 4\nVERSION_MINOR = 4\nPATCHLEVEL = 1\n"
    )

    with (
        patch(
            "esphome.components.zephyr.dts_fetch._sdk_source_version_cache_root",
            return_value=tmp_path,
        ),
        patch("subprocess.run") as mock_run,
    ):
        mock_run.return_value = subprocess.CompletedProcess(
            [], 0, stdout=f"{_FAKE_RESOLVED_SHA}\n", stderr=""
        )
        version = resolve_sdk_source_version(source, refresh=None)

    assert version == "4.4.1"
    assert source[KEY_SDK_SOURCE_RESOLVED_REF] == _FAKE_RESOLVED_SHA
    # No clone attempted -- only the rev-parse for the resolved ref.
    mock_run.assert_called_once()
    assert mock_run.call_args.args[0][-2:] == ["rev-parse", "HEAD"]


_VERSION_441 = "VERSION_MAJOR = 4\nVERSION_MINOR = 4\nPATCHLEVEL = 1\n"
_VERSION_442 = "VERSION_MAJOR = 4\nVERSION_MINOR = 4\nPATCHLEVEL = 2\n"
_SOURCE = {"type": "git", "url": "https://example.invalid/zephyr", "ref": "main"}


def _stale_cache(tmp_path: Path) -> Path:
    """A cached sdk_source clone whose VERSION is older than any refresh interval."""
    dest = tmp_path / _sdk_source_cache_key(_SOURCE["url"], _SOURCE["ref"])
    (dest / ".git").mkdir(parents=True)
    (dest / "VERSION").write_text(_VERSION_441)
    os.utime(dest / "VERSION", (0, 0))
    return dest


def _resolve_with(
    tmp_path: Path, git: Callable[[list[str]], None]
) -> tuple[str, list[list[str]]]:
    """Run resolve_sdk_source_version with `git(cmd)` handling every git call except
    rev-parse; return the version and the commands run."""
    calls: list[list[str]] = []

    def run(cmd: list[str], **kwargs: Any) -> subprocess.CompletedProcess[str]:
        calls.append(cmd)
        if "rev-parse" in cmd:
            return subprocess.CompletedProcess(
                cmd, 0, stdout=f"{_FAKE_RESOLVED_SHA}\n", stderr=""
            )
        git(cmd)
        return subprocess.CompletedProcess(cmd, 0, stdout="", stderr="")

    with (
        patch(
            "esphome.components.zephyr.dts_fetch._sdk_source_version_cache_root",
            return_value=tmp_path,
        ),
        patch("subprocess.run", side_effect=run),
    ):
        version = resolve_sdk_source_version(
            dict(_SOURCE), refresh=TimePeriodSeconds(seconds=1)
        )
    return version, calls


def _offline(cmd: list[str]) -> None:
    raise subprocess.CalledProcessError(
        128, cmd, stderr="Cloning...\nfatal: unable to access: Could not resolve host"
    )


def test_resolve_sdk_source_version_refresh_updates_cache_in_place(
    tmp_path: Path,
) -> None:
    dest = _stale_cache(tmp_path)

    def git(cmd: list[str]) -> None:
        if "reset" in cmd:
            (dest / "VERSION").write_text(_VERSION_442)

    version, calls = _resolve_with(tmp_path, git)

    assert version == "4.4.2"
    assert not [c for c in calls if "clone" in c]
    assert ["git", "-C", str(dest), "fetch", "--depth=1", "origin", "main"] in calls
    assert ["git", "-C", str(dest), "reset", "--hard", "FETCH_HEAD"] in calls
    # Refresh timer restarted even if VERSION's content didn't change.
    assert (dest / "VERSION").stat().st_mtime > 0


def test_resolve_sdk_source_version_failed_refresh_raises_and_keeps_cache(
    tmp_path: Path,
) -> None:
    dest = _stale_cache(tmp_path)

    with pytest.raises(cv.Invalid, match="Could not resolve host"):
        _resolve_with(tmp_path, _offline)
    assert (dest / ".git").is_dir()
    assert (dest / "VERSION").read_text() == _VERSION_441


def test_resolve_sdk_source_version_clones_when_not_cached(tmp_path: Path) -> None:
    def git(cmd: list[str]) -> None:
        dest = Path(cmd[-1])
        (dest / ".git").mkdir(parents=True)
        (dest / "VERSION").write_text(_VERSION_442)

    version, calls = _resolve_with(tmp_path, git)

    assert version == "4.4.2"
    assert calls[0][:2] == ["git", "clone"]


def test_resolve_sdk_source_version_failed_clone_leaves_no_partial_dir(
    tmp_path: Path,
) -> None:
    dest = tmp_path / _sdk_source_cache_key(_SOURCE["url"], _SOURCE["ref"])

    def git(cmd: list[str]) -> None:
        Path(cmd[-1]).mkdir(parents=True)
        _offline(cmd)

    with pytest.raises(cv.Invalid, match="Can't check sdk_source version"):
        _resolve_with(tmp_path, git)
    assert not dest.exists()


# ---------------------------------------------------------------------------
# _native_dts_path
# ---------------------------------------------------------------------------


def test_native_dts_path_none_when_tools_subdir_unset() -> None:
    assert _native_dts_path(ZephyrSDK(manifest_url="x")) is None


@pytest.mark.parametrize("sdk", [MAINLINE, NCS, SILABS])
def test_native_dts_path_matches_the_machine_global_cache_key(
    sdk: ZephyrSDK, tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    # Must land at the same directory framework_west._source_cache_key() would
    # produce for the plain official-source (no sdk_source:, no modules) case --
    # sdk.tools_subdir-prefixed, nested under the shared sdk-zephyr cache root.
    monkeypatch.setenv("ESPHOME_SDK_ZEPHYR_PREFIX", str(tmp_path))
    _set_framework_version(4, 4, 1)
    expected = tmp_path / "frameworks" / f"{sdk.tools_subdir}-v4.4.1" / "zephyr"
    expected.mkdir(parents=True)
    assert _native_dts_path(sdk) == expected


def test_native_dts_path_keeps_release_candidate_suffix(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setenv("ESPHOME_SDK_ZEPHYR_PREFIX", str(tmp_path))
    _set_framework_version(4, 5, 0, "rc1")
    expected = tmp_path / "frameworks" / "sdk-zephyr-v4.5.0-rc1" / "zephyr"
    expected.mkdir(parents=True)
    assert _native_dts_path(MAINLINE) == expected


def test_native_dts_path_none_when_not_on_disk(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setenv("ESPHOME_SDK_ZEPHYR_PREFIX", str(tmp_path))
    _set_framework_version(4, 4, 1)
    assert _native_dts_path(MAINLINE) is None


# ---------------------------------------------------------------------------
# _dts_cache_root / _sdk_source_version_cache_root / _manifest_revision_cache_root
# -- must nest under the machine-global sdk-zephyr root (ESPHOME_SDK_ZEPHYR_PREFIX),
# not a bare ~/.esphome/, so they're covered by the same override and by
# `esphome clean-all`.
# ---------------------------------------------------------------------------


def test_dts_cache_root_nests_under_sdk_zephyr_prefix(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setenv("ESPHOME_SDK_ZEPHYR_PREFIX", str(tmp_path))
    assert _dts_cache_root() == (tmp_path / "dts_cache").resolve()


def test_sdk_source_version_cache_root_nests_under_sdk_zephyr_prefix(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setenv("ESPHOME_SDK_ZEPHYR_PREFIX", str(tmp_path))
    assert (
        _sdk_source_version_cache_root()
        == (tmp_path / "sdk_source_version_cache").resolve()
    )


def test_manifest_revision_cache_root_nests_under_sdk_zephyr_prefix(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setenv("ESPHOME_SDK_ZEPHYR_PREFIX", str(tmp_path))
    assert (
        _manifest_revision_cache_root()
        == (tmp_path / "manifest_revision_cache").resolve()
    )


# ---------------------------------------------------------------------------
# _resolve_boards_ref
# ---------------------------------------------------------------------------


def test_resolve_boards_ref_prefixes_v_for_mainline_shaped_sdk() -> None:
    assert _resolve_boards_ref(MAINLINE, "4.4.1") == "v4.4.1"


def test_resolve_boards_ref_reads_manifest_revision_for_manifest_resolved_sdk(
    tmp_path: Path,
) -> None:
    """The NCS's boards_repo_url ref isn't derivable from version -- it must be read from
    manifest_url's own west.yml. Regression coverage for that discovery, not a guessed
    format string."""

    def fake_run(cmd: list[str], **kwargs: Any) -> subprocess.CompletedProcess[str]:
        # cmd ends with the clone destination directory.
        dest = Path(cmd[-1])
        dest.mkdir(parents=True, exist_ok=True)
        (dest / "west.yml").write_text(
            "manifest:\n"
            "  projects:\n"
            "    - name: zephyr\n"
            "      repo-path: sdk-zephyr\n"
            "      revision: ncs-v3.4.0\n"
        )
        return subprocess.CompletedProcess(cmd, 0, stdout="", stderr="")

    with (
        patch(
            "esphome.components.zephyr.dts_fetch._manifest_revision_cache_root",
            return_value=tmp_path / "zephyr_manifest_revision_cache",
        ),
        patch("subprocess.run", side_effect=fake_run),
    ):
        assert _resolve_boards_ref(NCS, "3.4.0") == "ncs-v3.4.0"


def test_resolve_boards_ref_keeps_release_candidate_suffix() -> None:
    assert _resolve_boards_ref(MAINLINE, "4.5.0-rc1") == "v4.5.0-rc1"


def test_resolve_boards_ref_clones_manifest_at_release_candidate_tag(
    tmp_path: Path,
) -> None:
    def fake_run(cmd: list[str], **kwargs: Any) -> subprocess.CompletedProcess[str]:
        dest = Path(cmd[-1])
        dest.mkdir(parents=True, exist_ok=True)
        (dest / "west.yml").write_text(
            "manifest:\n"
            "  projects:\n"
            "    - name: zephyr\n"
            "      repo-path: sdk-zephyr\n"
            "      revision: ncs-v3.4.0-rc1\n"
        )
        return subprocess.CompletedProcess(cmd, 0, stdout="", stderr="")

    with (
        patch(
            "esphome.components.zephyr.dts_fetch._manifest_revision_cache_root",
            return_value=tmp_path / "zephyr_manifest_revision_cache",
        ),
        patch("subprocess.run", side_effect=fake_run) as mock_run,
    ):
        assert _resolve_boards_ref(NCS, "3.4.0-rc1") == "ncs-v3.4.0-rc1"

    assert (
        mock_run.call_args.args[0][mock_run.call_args.args[0].index("--branch") + 1]
        == "v3.4.0-rc1"
    )


def test_resolve_boards_ref_returns_none_when_git_fails(tmp_path: Path) -> None:
    def fake_run(cmd: list[str], **kwargs: Any) -> subprocess.CompletedProcess[str]:
        raise subprocess.CalledProcessError(1, cmd, stderr="fatal: some git error")

    with (
        patch(
            "esphome.components.zephyr.dts_fetch._manifest_revision_cache_root",
            return_value=tmp_path / "zephyr_manifest_revision_cache",
        ),
        patch("subprocess.run", side_effect=fake_run),
    ):
        assert _resolve_boards_ref(NCS, "3.4.0") is None


def test_resolve_boards_ref_returns_cached_value_without_reinvoking_git(
    tmp_path: Path,
) -> None:
    cache_dir = tmp_path / "zephyr_manifest_revision_cache"
    cache_dir.mkdir(parents=True)
    cache_key = hashlib.sha1(f"{NCS.manifest_url}@v3.4.0".encode()).hexdigest()[:16]
    (cache_dir / cache_key).write_text("ncs-v3.4.0")

    with (
        patch(
            "esphome.components.zephyr.dts_fetch._manifest_revision_cache_root",
            return_value=cache_dir,
        ),
        patch("subprocess.run") as mock_run,
    ):
        result = _resolve_boards_ref(NCS, "3.4.0")

    mock_run.assert_not_called()
    assert result == "ncs-v3.4.0"


# ---------------------------------------------------------------------------
# _sparse_clone_dts -- regression coverage for the cone-mode VERSION bug
# ---------------------------------------------------------------------------


def test_sparse_clone_dts_sparse_checkout_never_lists_version_file(
    tmp_path: Path,
) -> None:
    """Regression test: cone-mode `git sparse-checkout set` only accepts directory
    patterns. VERSION is a top-level file in the zephyr repo -- passing it as a
    pattern fails the whole fetch ("'VERSION' is not a directory"). Cone mode
    already includes top-level files automatically, so it must never be listed.
    """
    _set_framework_version(4, 4, 1)

    calls: list[list[str]] = []

    def fake_run(cmd: list[str], **kwargs: Any) -> subprocess.CompletedProcess[str]:
        calls.append(cmd)
        result = subprocess.CompletedProcess(cmd, 0, stdout="", stderr="")
        if cmd[:2] == ["git", "clone"]:
            # Simulate the clone creating the destination directory tree so the
            # post-clone sparse-checkout step has somewhere to run "in".
            dest = Path(cmd[-1])
            (dest / "boards").mkdir(parents=True, exist_ok=True)
        return result

    with (
        patch(
            "esphome.components.zephyr.dts_fetch._dts_cache_root",
            return_value=tmp_path / "zephyr_dts_cache",
        ),
        patch("subprocess.run", side_effect=fake_run),
    ):
        result = _sparse_clone_dts("ESP32H2", "zephyr", MAINLINE)

    sparse_checkout_calls = [c for c in calls if "sparse-checkout" in c]
    assert len(sparse_checkout_calls) == 1
    assert "VERSION" not in sparse_checkout_calls[0]
    assert result is not None


def test_sparse_clone_dts_returns_none_and_cleans_up_on_git_failure(
    tmp_path: Path,
) -> None:
    _set_framework_version(4, 4, 1)

    def fake_run(cmd: list[str], **kwargs: Any) -> subprocess.CompletedProcess[str]:
        raise subprocess.CalledProcessError(1, cmd, stderr="fatal: some git error")

    dest = tmp_path / "zephyr_dts_cache" / "ESP32H2" / "zephyr" / "4.4.1"
    with (
        patch(
            "esphome.components.zephyr.dts_fetch._dts_cache_root",
            return_value=tmp_path / "zephyr_dts_cache",
        ),
        patch("subprocess.run", side_effect=fake_run),
    ):
        result = _sparse_clone_dts("ESP32H2", "zephyr", MAINLINE)

    assert result is None
    assert not dest.exists()  # cleaned up, not left in a half-cloned state


def test_sparse_clone_dts_returns_cached_path_without_reinvoking_git(
    tmp_path: Path,
) -> None:
    _set_framework_version(4, 4, 1)
    dest = tmp_path / "zephyr_dts_cache" / "ESP32H2" / "zephyr" / "4.4.1"
    (dest / "boards").mkdir(parents=True)
    (dest / ".resolved_ref").write_text(
        f"{_resolve_boards_ref(MAINLINE, '4.4.1')}#{_SPARSE_CHECKOUT_SCHEMA}"
    )

    with (
        patch(
            "esphome.components.zephyr.dts_fetch._dts_cache_root",
            return_value=tmp_path / "zephyr_dts_cache",
        ),
        patch("subprocess.run") as mock_run,
    ):
        result = _sparse_clone_dts("ESP32H2", "zephyr", MAINLINE)

    mock_run.assert_not_called()
    assert result == dest


def test_sparse_clone_dts_caches_release_candidate_separately(
    tmp_path: Path,
) -> None:
    _set_framework_version(4, 5, 0, "rc1")
    dest = tmp_path / "zephyr_dts_cache" / "ESP32" / "zephyr" / "4.5.0-rc1"
    (dest / "boards").mkdir(parents=True)
    (dest / ".resolved_ref").write_text(f"v4.5.0-rc1#{_SPARSE_CHECKOUT_SCHEMA}")

    with (
        patch(
            "esphome.components.zephyr.dts_fetch._dts_cache_root",
            return_value=tmp_path / "zephyr_dts_cache",
        ),
        patch("subprocess.run") as mock_run,
    ):
        result = _sparse_clone_dts("ESP32", "zephyr", MAINLINE)

    mock_run.assert_not_called()
    assert result == dest


def test_sparse_clone_dts_self_heals_when_resolved_ref_changed(
    tmp_path: Path,
) -> None:
    """Regression test for a cache dir left behind by an older, now-fixed ref
    resolution (e.g. before the ncs-zigbee two-hop fix). A stale marker must
    trigger a re-clone instead of being reused forever just because boards/
    happens to exist.
    """
    _set_framework_version(4, 4, 1)
    dest = tmp_path / "zephyr_dts_cache" / "ESP32H2" / "zephyr" / "4.4.1"
    (dest / "boards").mkdir(parents=True)
    (dest / "stale-marker-file").write_text("leftover from the old checkout")
    (dest / ".resolved_ref").write_text("v0.0.0-stale")

    def fake_run(cmd: list[str], **kwargs: Any) -> subprocess.CompletedProcess[str]:
        result = subprocess.CompletedProcess(cmd, 0, stdout="", stderr="")
        if cmd[:2] == ["git", "clone"]:
            new_dest = Path(cmd[-1])
            (new_dest / "boards").mkdir(parents=True, exist_ok=True)
        return result

    with (
        patch(
            "esphome.components.zephyr.dts_fetch._dts_cache_root",
            return_value=tmp_path / "zephyr_dts_cache",
        ),
        patch("subprocess.run", side_effect=fake_run) as mock_run,
    ):
        result = _sparse_clone_dts("ESP32H2", "zephyr", MAINLINE)

    assert mock_run.call_args_list  # re-fetched instead of trusting the stale cache
    assert not (dest / "stale-marker-file").exists()  # old checkout wiped, not merged
    assert result == dest
    assert (
        (dest / ".resolved_ref").read_text()
        == f"{_resolve_boards_ref(MAINLINE, '4.4.1')}#{_SPARSE_CHECKOUT_SCHEMA}"
    )


# ---------------------------------------------------------------------------
# _sparse_clone_dts_from_source -- schema-marker staleness guard (fix for the
# from-source path silently missing the snippets/ sparse-checkout addition)
# ---------------------------------------------------------------------------


_FAKE_RESOLVED_SHA = "a" * 40


def test_sparse_clone_dts_from_source_cache_hit_with_current_markers(
    tmp_path: Path,
) -> None:
    source = {
        "url": "https://example.invalid/zephyr",
        "ref": "my-branch",
        KEY_SDK_SOURCE_RESOLVED_REF: _FAKE_RESOLVED_SHA,
    }
    dest = (
        tmp_path
        / "zephyr_dts_cache"
        / _sdk_source_cache_key(source["url"], source["ref"])
    )
    (dest / "boards").mkdir(parents=True)
    (dest / ".sparse_schema").write_text(_SPARSE_CHECKOUT_SCHEMA)
    (dest / ".resolved_ref").write_text(_FAKE_RESOLVED_SHA)

    with (
        patch(
            "esphome.components.zephyr.dts_fetch._dts_cache_root",
            return_value=tmp_path / "zephyr_dts_cache",
        ),
        patch("subprocess.run") as mock_run,
    ):
        result = _sparse_clone_dts_from_source(source)

    mock_run.assert_not_called()
    assert result == dest


def test_sparse_clone_dts_from_source_refetches_when_schema_marker_missing(
    tmp_path: Path,
) -> None:
    """A cache dir from before the snippets/ sparse-checkout addition has no
    .sparse_schema marker at all -- must be treated as stale and re-fetched
    immediately, even though the resolved-ref marker (added below) matches."""
    source = {
        "url": "https://example.invalid/zephyr",
        "ref": "my-branch",
        KEY_SDK_SOURCE_RESOLVED_REF: _FAKE_RESOLVED_SHA,
    }
    dest = (
        tmp_path
        / "zephyr_dts_cache"
        / _sdk_source_cache_key(source["url"], source["ref"])
    )
    (dest / "boards").mkdir(parents=True)
    (dest / ".resolved_ref").write_text(_FAKE_RESOLVED_SHA)
    (dest / "stale-marker-file").write_text("leftover from the old checkout")

    def fake_run(cmd: list[str], **kwargs: Any) -> subprocess.CompletedProcess[str]:
        result = subprocess.CompletedProcess(cmd, 0, stdout="", stderr="")
        if cmd[:2] == ["git", "init"]:
            Path(cmd[-1], "boards").mkdir(parents=True, exist_ok=True)
        return result

    with (
        patch(
            "esphome.components.zephyr.dts_fetch._dts_cache_root",
            return_value=tmp_path / "zephyr_dts_cache",
        ),
        patch("subprocess.run", side_effect=fake_run) as mock_run,
    ):
        result = _sparse_clone_dts_from_source(source)

    assert mock_run.call_args_list  # re-fetched, not trusted as a cache hit
    assert not (dest / "stale-marker-file").exists()  # old checkout wiped
    assert result == dest


def test_sparse_clone_dts_from_source_refetches_when_resolved_ref_changed(
    tmp_path: Path,
) -> None:
    """A cache dir matching the current schema but a stale resolved_ref marker
    (upstream moved since the last resolution) must be re-fetched -- this
    replaces the old mtime/refresh: based staleness check with a direct
    comparison against the single ref resolve_sdk_source_version() resolved."""
    source = {
        "url": "https://example.invalid/zephyr",
        "ref": "my-branch",
        KEY_SDK_SOURCE_RESOLVED_REF: _FAKE_RESOLVED_SHA,
    }
    dest = (
        tmp_path
        / "zephyr_dts_cache"
        / _sdk_source_cache_key(source["url"], source["ref"])
    )
    (dest / "boards").mkdir(parents=True)
    (dest / ".sparse_schema").write_text(_SPARSE_CHECKOUT_SCHEMA)
    (dest / ".resolved_ref").write_text("b" * 40)  # a different, now-stale commit

    def fake_run(cmd: list[str], **kwargs: Any) -> subprocess.CompletedProcess[str]:
        result = subprocess.CompletedProcess(cmd, 0, stdout="", stderr="")
        if cmd[:2] == ["git", "init"]:
            Path(cmd[-1], "boards").mkdir(parents=True, exist_ok=True)
        return result

    with (
        patch(
            "esphome.components.zephyr.dts_fetch._dts_cache_root",
            return_value=tmp_path / "zephyr_dts_cache",
        ),
        patch("subprocess.run", side_effect=fake_run) as mock_run,
    ):
        result = _sparse_clone_dts_from_source(source)

    assert mock_run.call_args_list
    assert result == dest
    assert (dest / ".resolved_ref").read_text() == _FAKE_RESOLVED_SHA
    assert result == dest
    assert (dest / ".sparse_schema").read_text() == _SPARSE_CHECKOUT_SCHEMA


# ---------------------------------------------------------------------------
# _sparse_clone_hal_modules -- item 44 (hal_stm32 not fetched, breaking STM32 DTS)
# ---------------------------------------------------------------------------

_WEST_YML = """
manifest:
  defaults:
    remote: upstream
  remotes:
    - name: upstream
      url-base: https://github.com/zephyrproject-rtos
  projects:
    - name: hal_stm32
      revision: fc11896dd39cfca37bf9b4aeaaa2df8861b81875
      path: modules/hal/stm32
      groups: [hal]
"""


def _make_zephyr_dir(tmp_path: Path) -> Path:
    zephyr_dir = tmp_path / "zephyr"
    zephyr_dir.mkdir()
    (zephyr_dir / "west.yml").write_text(_WEST_YML)
    return zephyr_dir


def test_sparse_clone_hal_modules_noop_for_unlisted_family(tmp_path: Path) -> None:
    zephyr_dir = _make_zephyr_dir(tmp_path)
    with patch("subprocess.run") as mock_run:
        _sparse_clone_hal_modules(zephyr_dir, "esp32")
    mock_run.assert_not_called()


def test_sparse_clone_hal_modules_fetches_hal_stm32(tmp_path: Path) -> None:
    zephyr_dir = _make_zephyr_dir(tmp_path)
    calls: list[list[str]] = []

    def fake_run(cmd: list[str], **kwargs: Any) -> subprocess.CompletedProcess[str]:
        calls.append(cmd)
        return subprocess.CompletedProcess(cmd, 0, stdout="", stderr="")

    with patch("subprocess.run", side_effect=fake_run):
        _sparse_clone_hal_modules(zephyr_dir, "stm32")

    dest = zephyr_dir.parent / "modules" / "hal" / "stm32"
    fetch_call = next(c for c in calls if "remote" in c)
    assert fetch_call[fetch_call.index("origin") + 1] == (
        "https://github.com/zephyrproject-rtos/hal_stm32"
    )
    assert (
        dest / ".resolved_ref"
    ).read_text() == "fc11896dd39cfca37bf9b4aeaaa2df8861b81875"


def test_sparse_clone_hal_modules_skips_when_marker_matches(tmp_path: Path) -> None:
    zephyr_dir = _make_zephyr_dir(tmp_path)
    dest = zephyr_dir.parent / "modules" / "hal" / "stm32"
    dest.mkdir(parents=True)
    (dest / ".resolved_ref").write_text("fc11896dd39cfca37bf9b4aeaaa2df8861b81875")

    with patch("subprocess.run") as mock_run:
        _sparse_clone_hal_modules(zephyr_dir, "stm32")

    mock_run.assert_not_called()


def test_sparse_clone_hal_modules_refetches_when_marker_stale(tmp_path: Path) -> None:
    zephyr_dir = _make_zephyr_dir(tmp_path)
    dest = zephyr_dir.parent / "modules" / "hal" / "stm32"
    dest.mkdir(parents=True)
    (dest / ".resolved_ref").write_text("some-old-revision")
    (dest / "leftover-file").write_text("from an older hal_stm32 checkout")

    def fake_run(cmd: list[str], **kwargs: Any) -> subprocess.CompletedProcess[str]:
        return subprocess.CompletedProcess(cmd, 0, stdout="", stderr="")

    with patch("subprocess.run", side_effect=fake_run) as mock_run:
        _sparse_clone_hal_modules(zephyr_dir, "stm32")

    assert mock_run.call_args_list
    assert not (dest / "leftover-file").exists()
    assert (
        dest / ".resolved_ref"
    ).read_text() == "fc11896dd39cfca37bf9b4aeaaa2df8861b81875"


# ---------------------------------------------------------------------------
# _parse_zephyr_version_file
# ---------------------------------------------------------------------------


def test_parse_zephyr_version_file_reads_major_minor_patch(tmp_path: Path) -> None:
    version_file = tmp_path / "VERSION"
    version_file.write_text(
        "VERSION_MAJOR = 4\n"
        "VERSION_MINOR = 4\n"
        "PATCHLEVEL = 2\n"
        "VERSION_TWEAK = 0\n"
        "EXTRAVERSION =\n"
        "a line without an equals sign\n"
    )
    assert _parse_zephyr_version_file(version_file) == "4.4.2"


def test_parse_zephyr_version_file_rejects_missing_key(tmp_path: Path) -> None:
    version_file = tmp_path / "VERSION"
    version_file.write_text("VERSION_MAJOR = 4\nVERSION_MINOR = 4\n")
    with pytest.raises(cv.Invalid, match="PATCHLEVEL"):
        _parse_zephyr_version_file(version_file)


# ---------------------------------------------------------------------------
# fetch_board_dts -- one branch per way of finding the Zephyr tree
# ---------------------------------------------------------------------------

_FETCH = "esphome.components.zephyr.dts_fetch"


_FetchMocks = tuple[MagicMock, MagicMock, MagicMock]


@pytest.fixture
def fetch_mocks() -> Iterator[_FetchMocks]:
    """Patch every way fetch_board_dts() can find a tree; each starts as 'not found'."""
    with (
        patch(f"{_FETCH}._native_dts_path", return_value=None) as native,
        patch(f"{_FETCH}._sparse_clone_dts", return_value=None) as clone,
        patch(f"{_FETCH}._sparse_clone_dts_from_source", return_value=None) as source,
    ):
        yield native, clone, source


@pytest.mark.asyncio
async def test_fetch_board_dts_is_a_no_op_once_resolved(
    fetch_mocks: _FetchMocks,
) -> None:
    native, clone, source = fetch_mocks
    CORE.data[KEY_ZEPHYR] = empty_zephyr_data(dts_base_path="/already/there")
    await fetch_board_dts("ESP32H2", "zephyr", MAINLINE, None)
    assert CORE.data[KEY_ZEPHYR]["dts_base_path"] == "/already/there"
    native.assert_not_called()
    clone.assert_not_called()
    source.assert_not_called()


@pytest.mark.asyncio
async def test_fetch_board_dts_uses_local_source_directly(
    fetch_mocks: _FetchMocks, tmp_path: Path
) -> None:
    native, clone, source = fetch_mocks
    CORE.data[KEY_ZEPHYR] = empty_zephyr_data()
    local = {CONF_TYPE: TYPE_LOCAL, CONF_PATH: tmp_path}
    await fetch_board_dts("ESP32H2", "zephyr", MAINLINE, local)
    assert CORE.data[KEY_ZEPHYR]["dts_base_path"] == str(tmp_path)
    native.assert_not_called()
    clone.assert_not_called()
    source.assert_not_called()


@pytest.mark.asyncio
async def test_fetch_board_dts_clones_git_source(
    fetch_mocks: _FetchMocks, tmp_path: Path
) -> None:
    native, clone, source = fetch_mocks
    source.return_value = tmp_path
    CORE.data[KEY_ZEPHYR] = empty_zephyr_data()
    git = {CONF_TYPE: TYPE_GIT, CONF_URL: "https://example.invalid/z", CONF_REF: "x"}
    await fetch_board_dts("ESP32H2", "zephyr", MAINLINE, git, "esp32")
    assert CORE.data[KEY_ZEPHYR]["dts_base_path"] == str(tmp_path)
    source.assert_called_once_with(git, "esp32")
    native.assert_not_called()
    clone.assert_not_called()


@pytest.mark.asyncio
async def test_fetch_board_dts_warns_when_git_source_clone_fails(
    fetch_mocks: _FetchMocks, caplog: pytest.LogCaptureFixture
) -> None:
    native, clone, _source = fetch_mocks
    CORE.data[KEY_ZEPHYR] = empty_zephyr_data()
    git = {CONF_TYPE: TYPE_GIT, CONF_URL: "https://example.invalid/z"}
    await fetch_board_dts("ESP32H2", "zephyr", MAINLINE, git)
    assert CORE.data[KEY_ZEPHYR]["dts_base_path"] is None
    assert "Board DTS files unavailable for sdk_source" in caplog.text
    assert "default branch" in caplog.text
    # A failed source clone must not fall through to the stock SDK.
    native.assert_not_called()
    clone.assert_not_called()


@pytest.mark.asyncio
async def test_fetch_board_dts_prefers_native_sdk_install(
    fetch_mocks: _FetchMocks, tmp_path: Path
) -> None:
    native, clone, _source = fetch_mocks
    native.return_value = tmp_path
    CORE.data[KEY_ZEPHYR] = empty_zephyr_data()
    await fetch_board_dts("ESP32H2", "zephyr", MAINLINE, None)
    assert CORE.data[KEY_ZEPHYR]["dts_base_path"] == str(tmp_path)
    clone.assert_not_called()


@pytest.mark.asyncio
async def test_fetch_board_dts_falls_back_to_sparse_clone(
    fetch_mocks: _FetchMocks, tmp_path: Path
) -> None:
    _native, clone, _source = fetch_mocks
    clone.return_value = tmp_path
    CORE.data[KEY_ZEPHYR] = empty_zephyr_data()
    await fetch_board_dts("ESP32H2", "zephyr", MAINLINE, None, "esp32")
    assert CORE.data[KEY_ZEPHYR]["dts_base_path"] == str(tmp_path)
    clone.assert_called_once_with("ESP32H2", "zephyr", MAINLINE, "esp32")


@pytest.mark.asyncio
async def test_fetch_board_dts_warns_when_nothing_found(
    fetch_mocks: _FetchMocks, caplog: pytest.LogCaptureFixture
) -> None:
    _set_framework_version(4, 4, 2)
    CORE.data[KEY_ZEPHYR] = empty_zephyr_data()
    await fetch_board_dts("ESP32H2", "zephyr", MAINLINE, None)
    assert CORE.data[KEY_ZEPHYR]["dts_base_path"] is None
    assert "Board DTS files unavailable for ESP32H2 zephyr 4.4.2" in caplog.text
