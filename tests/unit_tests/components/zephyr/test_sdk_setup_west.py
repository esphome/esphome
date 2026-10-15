"""Unit tests for esphome.components.zephyr.sdk_setup_west."""

from __future__ import annotations

from collections.abc import Iterator
from pathlib import Path
import sys
from typing import Any
from unittest.mock import MagicMock, patch

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


def _fake_extract(archive: Path, extract_dir: Path, **kwargs: Any) -> None:
    """The real minimal archive ships setup.sh, which check_and_install() requires."""
    Path(extract_dir).mkdir(parents=True, exist_ok=True)
    (Path(extract_dir) / "setup.sh").touch()


def _make_complete_base(sdk_path: Path) -> None:
    sdk_path.mkdir(parents=True)
    (sdk_path / "setup.sh").touch()
    (sdk_path / ".esphome_base_complete").touch()


@pytest.fixture
def linux_host(monkeypatch: pytest.MonkeyPatch) -> None:
    """The SDK archive name is picked from sys.platform, which is unsupported on Windows."""
    monkeypatch.setattr(sys, "platform", "linux")


@pytest.fixture
def mock_sdk_download_ops(linux_host: None) -> Iterator[tuple[MagicMock, MagicMock]]:
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
    mock_sdk_download_ops: tuple[MagicMock, MagicMock],
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
    assert (sdk_path / ".esphome_base_complete").exists()
    assert (sdk_path / ".esphome_complete_host").exists()


def test_check_and_install_skips_download_when_sentinel_exists(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    mock_sdk_download_ops: tuple[MagicMock, MagicMock],
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
    mock_sdk_download_ops: tuple[MagicMock, MagicMock],
) -> None:
    monkeypatch.setenv("ESPHOME_SDK_ZEPHYR_PREFIX", str(tmp_path / "cache"))
    mock_download, mock_extract = mock_sdk_download_ops
    framework = _make_framework(tmp_path)
    sdk_path = _sdk_install_dir("0.17.4")
    _make_complete_base(sdk_path)
    (sdk_path / ".esphome_complete_host").touch()

    with patch("subprocess.run") as mock_subprocess_run:
        mock_subprocess_run.return_value.returncode = 0
        check_and_install(framework, toolchain="riscv64-zephyr-elf")

    mock_download.assert_not_called()
    mock_extract.assert_not_called()
    assert (sdk_path / ".esphome_complete_riscv64-zephyr-elf").exists()


def test_check_and_install_reextracts_partial_sdk_path(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    mock_sdk_download_ops: tuple[MagicMock, MagicMock],
) -> None:
    monkeypatch.setenv("ESPHOME_SDK_ZEPHYR_PREFIX", str(tmp_path / "cache"))
    mock_download, mock_extract = mock_sdk_download_ops
    framework = _make_framework(tmp_path)
    sdk_path = _sdk_install_dir("0.17.4")
    sdk_path.mkdir(parents=True)
    (sdk_path / "partial_leftover").touch()

    with patch("subprocess.run") as mock_subprocess_run:
        mock_subprocess_run.return_value.returncode = 0
        check_and_install(framework, toolchain="riscv64-zephyr-elf")

    mock_download.assert_called_once()
    mock_extract.assert_called_once()
    assert not (sdk_path / "partial_leftover").exists()
    assert (sdk_path / ".esphome_base_complete").exists()
    assert (sdk_path / ".esphome_complete_riscv64-zephyr-elf").exists()


def test_check_and_install_raises_without_setup_script(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    mock_sdk_download_ops: tuple[MagicMock, MagicMock],
) -> None:
    monkeypatch.setenv("ESPHOME_SDK_ZEPHYR_PREFIX", str(tmp_path / "cache"))
    framework = _make_framework(tmp_path)
    sdk_path = _sdk_install_dir("0.17.4")
    _make_complete_base(sdk_path)
    (sdk_path / "setup.sh").unlink()

    with pytest.raises(RuntimeError, match="no setup.sh"):
        check_and_install(framework, toolchain="riscv64-zephyr-elf")

    assert not (sdk_path / ".esphome_complete_riscv64-zephyr-elf").exists()


@pytest.mark.usefixtures("linux_host")
def test_check_and_install_no_base_marker_on_extract_failure(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
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

    assert not (sdk_path / ".esphome_base_complete").exists()


@pytest.mark.parametrize(
    ("toolchain", "expected_flags"),
    [(None, ["-h"]), ("riscv64-zephyr-elf", ["-t", "riscv64-zephyr-elf"])],
)
def test_check_and_install_runs_setup_script_with_the_right_arguments(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    mock_sdk_download_ops: tuple[MagicMock, MagicMock],
    toolchain: str | None,
    expected_flags: list[str],
) -> None:
    monkeypatch.setenv("ESPHOME_SDK_ZEPHYR_PREFIX", str(tmp_path / "cache"))
    framework = _make_framework(tmp_path)
    sdk_path = _sdk_install_dir("0.17.4")
    _make_complete_base(sdk_path)

    with patch("subprocess.run") as mock_subprocess_run:
        mock_subprocess_run.return_value.returncode = 0
        check_and_install(framework, toolchain=toolchain)

    assert mock_subprocess_run.call_args.args[0] == [
        str(sdk_path / "setup.sh"),
        *expected_flags,
    ]
    assert mock_subprocess_run.call_args.kwargs["env"]["ZEPHYR_SDK_INSTALL_DIR"] == str(
        sdk_path
    )


def test_check_and_install_raises_when_setup_script_fails(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    mock_sdk_download_ops: tuple[MagicMock, MagicMock],
) -> None:
    monkeypatch.setenv("ESPHOME_SDK_ZEPHYR_PREFIX", str(tmp_path / "cache"))
    framework = _make_framework(tmp_path)
    sdk_path = _sdk_install_dir("0.17.4")
    _make_complete_base(sdk_path)

    with patch("subprocess.run") as mock_subprocess_run:
        mock_subprocess_run.return_value.returncode = 3
        with pytest.raises(RuntimeError, match="setup.sh failed with exit code 3"):
            check_and_install(framework, toolchain=None)

    assert not (sdk_path / ".esphome_complete_host").exists()


def test_check_and_install_rejects_unsupported_cpu(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    mock_sdk_download_ops: tuple[MagicMock, MagicMock],
) -> None:
    monkeypatch.setenv("ESPHOME_SDK_ZEPHYR_PREFIX", str(tmp_path / "cache"))
    framework = _make_framework(tmp_path)
    mock_download, _mock_extract = mock_sdk_download_ops

    with (
        patch("platform.machine", return_value="riscv64"),
        pytest.raises(RuntimeError, match="Unsupported CPU architecture"),
    ):
        check_and_install(framework, toolchain=None)

    mock_download.assert_not_called()


def test_check_and_install_rejects_unsupported_os(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    mock_sdk_download_ops: tuple[MagicMock, MagicMock],
) -> None:
    monkeypatch.setenv("ESPHOME_SDK_ZEPHYR_PREFIX", str(tmp_path / "cache"))
    monkeypatch.setattr(sys, "platform", "win32")
    framework = _make_framework(tmp_path)

    with pytest.raises(RuntimeError, match="Unsupported OS for Zephyr SDK"):
        check_and_install(framework, toolchain=None)


def test_check_and_install_picks_the_macos_arm_archive(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    mock_sdk_download_ops: tuple[MagicMock, MagicMock],
) -> None:
    monkeypatch.setenv("ESPHOME_SDK_ZEPHYR_PREFIX", str(tmp_path / "cache"))
    monkeypatch.setattr(sys, "platform", "darwin")
    framework = _make_framework(tmp_path)
    mock_download, _mock_extract = mock_sdk_download_ops

    with (
        patch("platform.machine", return_value="arm64"),
        patch("subprocess.run") as mock_subprocess_run,
    ):
        mock_subprocess_run.return_value.returncode = 0
        check_and_install(framework, toolchain=None)

    assert mock_download.call_args.args[1] == {
        "VERSION": "0.17.4",
        "sysname": "macos",
        "machine": "aarch64",
        "extension": "tar.xz",
    }
