"""Tests for esphome.build_gen.platformio module."""

from __future__ import annotations

from collections.abc import Generator
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest

from esphome.build_gen import platformio
from esphome.core import CORE


@pytest.fixture
def mock_write_file_if_changed() -> Generator[MagicMock]:
    """Mock write_file_if_changed for tests."""
    with patch("esphome.build_gen.platformio.write_file_if_changed") as mock:
        yield mock


def test_write_ini_creates_new_file(tmp_path: Path) -> None:
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


def test_write_ini_updates_existing_file(tmp_path: Path) -> None:
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


def test_write_ini_preserves_custom_sections(tmp_path: Path) -> None:
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


@pytest.fixture
def clean_core(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(CORE, "name", "test")
    monkeypatch.setattr(CORE, "platformio_options", {})
    monkeypatch.setattr(CORE, "platformio_libraries", {})
    monkeypatch.setattr(CORE, "build_flags", set())
    monkeypatch.setattr(CORE, "build_unflags", set())


def test_get_ini_content_pins_cpp_standard(
    clean_core: None, monkeypatch: pytest.MonkeyPatch
) -> None:
    """cg.set_cpp_standard() pins -std via build_flags and unflags every other
    known standard so the platform/framework default is stripped."""
    monkeypatch.setattr(CORE, "cpp_standard", "gnu++20")

    content = platformio.get_ini_content()

    flags_section = content.split("build_flags =")[1].split("build_unflags =")[0]
    unflags_section = content.split("build_unflags =")[1].split("extra_scripts")[0]
    assert "-std=gnu++20\n" in flags_section
    # Both the GNU and strict dialects of every other standard are stripped.
    for year in ("11", "14", "17", "23", "26", "2a", "2b", "2c"):
        assert f"-std=gnu++{year}\n" in unflags_section
        assert f"-std=c++{year}\n" in unflags_section
    assert "-std=c++20\n" in unflags_section
    # The selected standard must not unflag itself.
    assert "-std=gnu++20\n" not in unflags_section


def test_get_ini_content_no_cpp_standard(
    clean_core: None, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setattr(CORE, "cpp_standard", None)

    content = platformio.get_ini_content()

    assert "-std=" not in content


def test_write_cxx_flags_script_emits_registered_flags(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Flags registered via cg.add_cxx_build_flag() are emitted as CXXFLAGS,
    sorted, so they apply to C++ compiles only."""
    CORE.build_path = str(tmp_path)
    monkeypatch.setattr(CORE, "cxx_build_flags", {"-Wno-volatile", "-Wno-deprecated"})

    platformio.write_cxx_flags_script()

    content = (tmp_path / platformio.CXX_FLAGS_FILE_NAME).read_text()
    assert (
        'env.Append(CXXFLAGS=["-Wno-deprecated"])\n'
        'env.Append(CXXFLAGS=["-Wno-volatile"])\n'
    ) in content


def test_write_cxx_flags_script_no_flags(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    CORE.build_path = str(tmp_path)
    monkeypatch.setattr(CORE, "cxx_build_flags", set())

    platformio.write_cxx_flags_script()

    content = (tmp_path / platformio.CXX_FLAGS_FILE_NAME).read_text()
    assert "CXXFLAGS" not in content
