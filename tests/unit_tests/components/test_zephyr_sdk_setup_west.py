"""Unit tests for esphome.components.zephyr.sdk_setup_west."""

from __future__ import annotations

from pathlib import Path
from unittest.mock import patch

import platformdirs
import pytest

from esphome.components.zephyr.sdk_setup_west import _sdk_install_dir, check_and_install

# ---------------------------------------------------------------------------
# _sdk_install_dir -- machine-global cache location
# ---------------------------------------------------------------------------


def test_sdk_install_dir_env_override(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    override = tmp_path / "custom" / "sdk-zephyr"
    monkeypatch.setenv("ESPHOME_SDK_ZEPHYR_PREFIX", str(override))
    assert (
        _sdk_install_dir("0.17.4")
        == override.resolve() / "toolchains" / "zephyr-sdk-0.17.4"
    )


def test_sdk_install_dir_default_is_global_cache(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.delenv("ESPHOME_SDK_ZEPHYR_PREFIX", raising=False)
    expected = (
        Path(platformdirs.user_cache_dir("esphome", appauthor=False))
        / "sdk-zephyr"
        / "toolchains"
        / "zephyr-sdk-0.17.4"
    ).resolve()
    assert _sdk_install_dir("0.17.4") == expected


# ---------------------------------------------------------------------------
# check_and_install -- download/extract reliability (issue #133)
# ---------------------------------------------------------------------------


def _make_framework(tmp_path: Path, sdk_version: str = "0.17.4") -> Path:
    framework = tmp_path / "framework"
    (framework / "zephyr").mkdir(parents=True)
    (framework / "zephyr" / "SDK_VERSION").write_text(sdk_version)
    return framework


def _fake_extract(archive, extract_dir, **kwargs) -> None:
    """Stand-in for archive_extract_all(): real extraction creates extract_dir
    (sdk_path) on disk, which sentinel.touch() then depends on."""
    Path(extract_dir).mkdir(parents=True, exist_ok=True)


@pytest.fixture
def mock_sdk_download_ops():
    """Patch the download/extract seams -- download_and_extract() resolves its
    internals in framework_helpers, matching how nrf52's own check_and_install
    tests are patched."""
    with (
        patch(
            "esphome.framework_helpers.download_from_mirrors",
            return_value="https://example.com/zephyr-sdk-0.17.4_linux-x86_64_minimal.tar.xz",
        ) as mock_download,
        patch(
            "esphome.framework_helpers.archive_extract_all", side_effect=_fake_extract
        ) as mock_extract,
    ):
        yield mock_download, mock_extract


def test_check_and_install_downloads_when_sdk_path_missing(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    mock_sdk_download_ops,
) -> None:
    monkeypatch.setenv("ESPHOME_SDK_ZEPHYR_PREFIX", str(tmp_path / "cache"))
    mock_download, mock_extract = mock_sdk_download_ops
    framework = _make_framework(tmp_path)
    sdk_path = _sdk_install_dir("0.17.4")

    with patch("subprocess.run") as mock_subprocess_run:
        mock_subprocess_run.return_value.returncode = 0
        result = check_and_install(framework, toolchain=None)

    mock_download.assert_called_once()
    mock_extract.assert_called_once()
    assert result == sdk_path
    assert (sdk_path / ".esphome_complete_host").exists()


def test_check_and_install_skips_download_when_sentinel_exists(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    mock_sdk_download_ops,
) -> None:
    monkeypatch.setenv("ESPHOME_SDK_ZEPHYR_PREFIX", str(tmp_path / "cache"))
    mock_download, mock_extract = mock_sdk_download_ops
    framework = _make_framework(tmp_path)
    sdk_path = _sdk_install_dir("0.17.4")
    sdk_path.mkdir(parents=True)
    (sdk_path / ".esphome_complete_host").touch()

    check_and_install(framework, toolchain=None)

    mock_download.assert_not_called()
    mock_extract.assert_not_called()


def test_check_and_install_reuses_sdk_path_for_a_second_toolchain(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    mock_sdk_download_ops,
) -> None:
    """sdk_path already present (host tools installed) + a different toolchain's
    sentinel missing -> the minimal archive isn't re-downloaded, only setup.sh runs."""
    monkeypatch.setenv("ESPHOME_SDK_ZEPHYR_PREFIX", str(tmp_path / "cache"))
    mock_download, mock_extract = mock_sdk_download_ops
    framework = _make_framework(tmp_path)
    sdk_path = _sdk_install_dir("0.17.4")
    sdk_path.mkdir(parents=True)
    (sdk_path / ".esphome_complete_host").touch()

    with patch("subprocess.run") as mock_subprocess_run:
        mock_subprocess_run.return_value.returncode = 0
        check_and_install(framework, toolchain="riscv64-zephyr-elf")

    mock_download.assert_not_called()
    mock_extract.assert_not_called()
    assert (sdk_path / ".esphome_complete_riscv64-zephyr-elf").exists()


def test_check_and_install_cleans_up_sdk_path_on_extract_failure(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    # A failed/partial extraction must not leave sdk_path looking complete to
    # the next run's `if not sdk_path.exists()` check -- sdk_path is otherwise
    # never wiped on the happy path since other toolchains may share it.
    monkeypatch.setenv("ESPHOME_SDK_ZEPHYR_PREFIX", str(tmp_path / "cache"))
    framework = _make_framework(tmp_path)
    sdk_path = _sdk_install_dir("0.17.4")

    with (
        patch(
            "esphome.framework_helpers.download_from_mirrors",
            return_value="https://example.com/x.tar.xz",
        ),
        patch(
            "esphome.framework_helpers.archive_extract_all",
            side_effect=ValueError("Unsupported archive format"),
        ),
        pytest.raises(ValueError, match="Unsupported archive format"),
    ):
        check_and_install(framework, toolchain=None)

    assert not sdk_path.exists()
