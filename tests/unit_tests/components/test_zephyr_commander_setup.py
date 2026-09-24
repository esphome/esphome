"""Unit tests for esphome.components.zephyr.commander_setup."""

from __future__ import annotations

from pathlib import Path
from unittest.mock import patch

import platformdirs
import pytest

from esphome.components.zephyr.commander_setup import (
    _RELEASES,
    _install_dir,
    check_and_install,
)

# ---------------------------------------------------------------------------
# _install_dir -- machine-global cache location
# ---------------------------------------------------------------------------


def test_install_dir_env_override(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    override = tmp_path / "custom" / "sdk-silabs"
    monkeypatch.setenv("ESPHOME_SDK_SILABS_PREFIX", str(override))
    assert _install_dir("1.24.1") == override.resolve() / "commander" / "1.24.1"


def test_install_dir_default_is_global_cache(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv("ESPHOME_SDK_SILABS_PREFIX", raising=False)
    expected = (
        Path(platformdirs.user_cache_dir("esphome", appauthor=False))
        / "sdk-silabs"
        / "commander"
        / "1.24.1"
    ).resolve()
    assert _install_dir("1.24.1") == expected


# ---------------------------------------------------------------------------
# check_and_install -- download/extract reliability (issue #133's class of bug,
# not itself named in the issue text)
# ---------------------------------------------------------------------------

_TEST_VERSION = "1.24.1"


def _fake_extract(archive, extract_dir, **kwargs) -> None:
    """Stand-in for archive_extract_all(): real extraction creates extract_dir
    (install_dir) on disk, which sentinel.touch() then depends on."""
    Path(extract_dir).mkdir(parents=True, exist_ok=True)


@pytest.fixture
def mock_commander_download_ops():
    """Patch the download/extract seams. Unlike sdk_setup_west.py (which calls
    the combined download_and_extract() and so is patched in framework_helpers,
    where that function resolves its own internals), commander_setup.py calls
    download_with_resume()/archive_extract_all() directly by their imported
    names, so the seams are patched on commander_setup's own module instead."""
    with (
        patch(
            "esphome.components.zephyr.commander_setup.download_with_resume"
        ) as mock_download,
        patch(
            "esphome.components.zephyr.commander_setup.archive_extract_all",
            side_effect=_fake_extract,
        ) as mock_extract,
    ):
        yield mock_download, mock_extract


def _patch_linux_x86_64():
    return (
        patch("sys.platform", "linux"),
        patch("platform.machine", return_value="x86_64"),
    )


def test_check_and_install_downloads_when_missing(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    mock_commander_download_ops,
) -> None:
    monkeypatch.setenv("ESPHOME_SDK_SILABS_PREFIX", str(tmp_path / "cache"))
    mock_download, mock_extract = mock_commander_download_ops
    install_dir = _install_dir(_TEST_VERSION)

    p1, p2 = _patch_linux_x86_64()
    with p1, p2:
        result = check_and_install(_TEST_VERSION)

    mock_download.assert_called_once()
    _url, dest = mock_download.call_args.args
    assert dest.name.endswith(".archive")
    assert (
        mock_download.call_args.kwargs["sha256"] == _RELEASES[_TEST_VERSION]["x86_64"]
    )
    mock_extract.assert_called_once()
    assert result == install_dir
    assert (install_dir / ".esphome_complete").exists()


def test_check_and_install_skips_download_when_sentinel_exists(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    mock_commander_download_ops,
) -> None:
    monkeypatch.setenv("ESPHOME_SDK_SILABS_PREFIX", str(tmp_path / "cache"))
    mock_download, mock_extract = mock_commander_download_ops
    install_dir = _install_dir(_TEST_VERSION)
    install_dir.mkdir(parents=True)
    (install_dir / ".esphome_complete").touch()

    p1, p2 = _patch_linux_x86_64()
    with p1, p2:
        check_and_install(_TEST_VERSION)

    mock_download.assert_not_called()
    mock_extract.assert_not_called()


def test_check_and_install_cleans_up_on_extract_failure(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setenv("ESPHOME_SDK_SILABS_PREFIX", str(tmp_path / "cache"))
    install_dir = _install_dir(_TEST_VERSION)

    p1, p2 = _patch_linux_x86_64()
    with (
        p1,
        p2,
        patch("esphome.components.zephyr.commander_setup.download_with_resume"),
        patch(
            "esphome.components.zephyr.commander_setup.archive_extract_all",
            side_effect=ValueError("Unsupported archive format"),
        ),
        pytest.raises(ValueError, match="Unsupported archive format"),
    ):
        check_and_install(_TEST_VERSION)

    assert not install_dir.exists()
