"""Tests for esphome.build_gen.platformio module."""

from __future__ import annotations

from collections.abc import Generator
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest

from esphome.build_gen import platformio
from esphome.core import CORE


@pytest.fixture
def mock_update_storage_json() -> Generator[MagicMock]:
    """Mock update_storage_json for all tests."""
    with patch("esphome.build_gen.platformio.update_storage_json") as mock:
        yield mock


@pytest.fixture
def mock_write_file_if_changed() -> Generator[MagicMock]:
    """Mock write_file_if_changed for tests."""
    with patch("esphome.build_gen.platformio.write_file_if_changed") as mock:
        yield mock


def test_write_ini_creates_new_file(
    tmp_path: Path, mock_update_storage_json: MagicMock
) -> None:
    """Test write_ini creates a new platformio.ini file."""
    CORE.build_path = str(tmp_path)

    content = """
[env:test]
platform = espressif32
board = esp32dev
framework = arduino
"""

    platformio.write_ini(content)

    ini_file = tmp_path / "platformio.ini"
    assert ini_file.exists()

    file_content = ini_file.read_text()
    assert content in file_content
    assert platformio.INI_AUTO_GENERATE_BEGIN in file_content
    assert platformio.INI_AUTO_GENERATE_END in file_content


def test_write_ini_updates_existing_file(
    tmp_path: Path, mock_update_storage_json: MagicMock
) -> None:
    """Test write_ini updates existing platformio.ini file."""
    CORE.build_path = str(tmp_path)

    # Create existing file with custom content
    ini_file = tmp_path / "platformio.ini"
    existing_content = f"""
; Custom header
[platformio]
default_envs = test

{platformio.INI_AUTO_GENERATE_BEGIN}
; Old auto-generated content
[env:old]
platform = old
{platformio.INI_AUTO_GENERATE_END}

; Custom footer
"""
    ini_file.write_text(existing_content)

    # New content to write
    new_content = """
[env:test]
platform = espressif32
board = esp32dev
framework = arduino
"""

    platformio.write_ini(new_content)

    file_content = ini_file.read_text()

    # Check that custom parts are preserved
    assert "; Custom header" in file_content
    assert "[platformio]" in file_content
    assert "default_envs = test" in file_content
    assert "; Custom footer" in file_content

    # Check that new content replaced old auto-generated content
    assert new_content in file_content
    assert "[env:old]" not in file_content
    assert "platform = old" not in file_content


def test_write_ini_preserves_custom_sections(
    tmp_path: Path, mock_update_storage_json: MagicMock
) -> None:
    """Test write_ini preserves custom sections outside auto-generate markers."""
    CORE.build_path = str(tmp_path)

    # Create existing file with multiple custom sections
    ini_file = tmp_path / "platformio.ini"
    existing_content = f"""
[platformio]
src_dir = .
include_dir = .

[common]
lib_deps =
    Wire
    SPI

{platformio.INI_AUTO_GENERATE_BEGIN}
[env:old]
platform = old
{platformio.INI_AUTO_GENERATE_END}

[env:custom]
upload_speed = 921600
monitor_speed = 115200
"""
    ini_file.write_text(existing_content)

    new_content = "[env:auto]\nplatform = new"

    platformio.write_ini(new_content)

    file_content = ini_file.read_text()

    # All custom sections should be preserved
    assert "[platformio]" in file_content
    assert "src_dir = ." in file_content
    assert "[common]" in file_content
    assert "lib_deps" in file_content
    assert "[env:custom]" in file_content
    assert "upload_speed = 921600" in file_content

    # New auto-generated content should replace old
    assert "[env:auto]" in file_content
    assert "platform = new" in file_content
    assert "[env:old]" not in file_content


def test_write_ini_no_change_when_content_same(
    tmp_path: Path,
    mock_update_storage_json: MagicMock,
    mock_write_file_if_changed: MagicMock,
) -> None:
    """Test write_ini doesn't rewrite file when content is unchanged."""
    CORE.build_path = str(tmp_path)

    content = "[env:test]\nplatform = esp32"
    full_content = (
        f"{platformio.INI_BASE_FORMAT[0]}"
        f"{platformio.INI_AUTO_GENERATE_BEGIN}\n"
        f"{content}"
        f"{platformio.INI_AUTO_GENERATE_END}"
        f"{platformio.INI_BASE_FORMAT[1]}"
    )

    ini_file = tmp_path / "platformio.ini"
    ini_file.write_text(full_content)

    mock_write_file_if_changed.return_value = False  # Indicate no change
    platformio.write_ini(content)

    # write_file_if_changed should be called with the same content
    mock_write_file_if_changed.assert_called_once()
    call_args = mock_write_file_if_changed.call_args[0]
    assert call_args[0] == ini_file
    assert content in call_args[1]


def test_write_ini_calls_update_storage_json(
    tmp_path: Path, mock_update_storage_json: MagicMock
) -> None:
    """Test write_ini calls update_storage_json."""
    CORE.build_path = str(tmp_path)

    content = "[env:test]\nplatform = esp32"

    platformio.write_ini(content)
    mock_update_storage_json.assert_called_once()
