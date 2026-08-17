"""Unit tests for esphome.components.zephyr.dts_fetch."""

from __future__ import annotations

import hashlib
from pathlib import Path
import subprocess
from unittest.mock import patch

from esphome.components.zephyr.dts_fetch import (
    _SPARSE_CHECKOUT_SCHEMA,
    _framework_base_version,
    _resolve_boards_ref,
    _sdk_source_cache_key,
    _sparse_clone_dts,
    _sparse_clone_dts_from_source,
)
from esphome.components.zephyr.variants import MAINLINE, NCS
import esphome.config_validation as cv
from esphome.const import KEY_CORE, KEY_FRAMEWORK_VERSION
from esphome.core import CORE


def _set_framework_version(major: int, minor: int, patch: int) -> None:
    CORE.data[KEY_CORE] = {KEY_FRAMEWORK_VERSION: cv.Version(major, minor, patch)}


# ---------------------------------------------------------------------------
# _framework_base_version
# ---------------------------------------------------------------------------


def test_framework_base_version_formats_major_minor_patch() -> None:
    _set_framework_version(4, 4, 1)
    assert _framework_base_version() == "4.4.1"


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

    def fake_run(cmd, **kwargs):
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
            "esphome.components.zephyr.dts_fetch._MANIFEST_REVISION_CACHE",
            tmp_path / "zephyr_manifest_revision_cache",
        ),
        patch("subprocess.run", side_effect=fake_run),
    ):
        assert _resolve_boards_ref(NCS, "3.4.0") == "ncs-v3.4.0"


def test_resolve_boards_ref_returns_none_when_git_fails(tmp_path: Path) -> None:
    def fake_run(cmd, **kwargs):
        raise subprocess.CalledProcessError(1, cmd, stderr="fatal: some git error")

    with (
        patch(
            "esphome.components.zephyr.dts_fetch._MANIFEST_REVISION_CACHE",
            tmp_path / "zephyr_manifest_revision_cache",
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
            "esphome.components.zephyr.dts_fetch._MANIFEST_REVISION_CACHE", cache_dir
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

    def fake_run(cmd, **kwargs):
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
            "esphome.components.zephyr.dts_fetch._DTS_CACHE",
            tmp_path / "zephyr_dts_cache",
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

    def fake_run(cmd, **kwargs):
        raise subprocess.CalledProcessError(1, cmd, stderr="fatal: some git error")

    dest = tmp_path / "zephyr_dts_cache" / "ESP32H2" / "zephyr" / "4.4.1"
    with (
        patch(
            "esphome.components.zephyr.dts_fetch._DTS_CACHE",
            tmp_path / "zephyr_dts_cache",
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
            "esphome.components.zephyr.dts_fetch._DTS_CACHE",
            tmp_path / "zephyr_dts_cache",
        ),
        patch("subprocess.run") as mock_run,
    ):
        result = _sparse_clone_dts("ESP32H2", "zephyr", MAINLINE)

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

    def fake_run(cmd, **kwargs):
        result = subprocess.CompletedProcess(cmd, 0, stdout="", stderr="")
        if cmd[:2] == ["git", "clone"]:
            new_dest = Path(cmd[-1])
            (new_dest / "boards").mkdir(parents=True, exist_ok=True)
        return result

    with (
        patch(
            "esphome.components.zephyr.dts_fetch._DTS_CACHE",
            tmp_path / "zephyr_dts_cache",
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


def test_sparse_clone_dts_from_source_cache_hit_with_current_schema_marker(
    tmp_path: Path,
) -> None:
    source = {"url": "https://example.invalid/zephyr", "ref": "my-branch"}
    dest = (
        tmp_path
        / "zephyr_dts_cache"
        / _sdk_source_cache_key(source["url"], source["ref"])
    )
    (dest / "boards").mkdir(parents=True)
    (dest / ".sparse_schema").write_text(_SPARSE_CHECKOUT_SCHEMA)

    with (
        patch(
            "esphome.components.zephyr.dts_fetch._DTS_CACHE",
            tmp_path / "zephyr_dts_cache",
        ),
        patch("subprocess.run") as mock_run,
    ):
        result = _sparse_clone_dts_from_source(source, refresh=None)

    mock_run.assert_not_called()
    assert result == dest


def test_sparse_clone_dts_from_source_refetches_when_schema_marker_missing(
    tmp_path: Path,
) -> None:
    """A cache dir from before the snippets/ sparse-checkout addition has no
    .sparse_schema marker at all -- must be treated as stale and re-fetched
    immediately, independent of the refresh: window (refresh=None here)."""
    source = {"url": "https://example.invalid/zephyr", "ref": "my-branch"}
    dest = (
        tmp_path
        / "zephyr_dts_cache"
        / _sdk_source_cache_key(source["url"], source["ref"])
    )
    (dest / "boards").mkdir(parents=True)
    (dest / "stale-marker-file").write_text("leftover from the old checkout")

    def fake_run(cmd, **kwargs):
        result = subprocess.CompletedProcess(cmd, 0, stdout="", stderr="")
        if cmd[:2] == ["git", "clone"]:
            new_dest = Path(cmd[-1])
            (new_dest / "boards").mkdir(parents=True, exist_ok=True)
        return result

    with (
        patch(
            "esphome.components.zephyr.dts_fetch._DTS_CACHE",
            tmp_path / "zephyr_dts_cache",
        ),
        patch("subprocess.run", side_effect=fake_run) as mock_run,
    ):
        result = _sparse_clone_dts_from_source(source, refresh=None)

    assert mock_run.call_args_list  # re-fetched, not trusted as a cache hit
    assert not (dest / "stale-marker-file").exists()  # old checkout wiped
    assert result == dest
    assert (dest / ".sparse_schema").read_text() == _SPARSE_CHECKOUT_SCHEMA
