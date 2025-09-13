"""Tests for platformio_api.py path functions."""

from pathlib import Path
from unittest.mock import patch

from esphome import platformio_api
from esphome.core import CORE


def test_idedata_firmware_elf_path(setup_core: Path) -> None:
    """Test IDEData.firmware_elf_path returns correct path."""
    CORE.build_path = str(setup_core / "build" / "test")
    CORE.name = "test"
    raw_data = {"prog_path": "/path/to/firmware.elf"}
    idedata = platformio_api.IDEData(raw_data)

    assert idedata.firmware_elf_path == "/path/to/firmware.elf"


def test_idedata_firmware_bin_path(setup_core: Path) -> None:
    """Test IDEData.firmware_bin_path returns Path with .bin extension."""
    CORE.build_path = str(setup_core / "build" / "test")
    CORE.name = "test"
    prog_path = str(Path("/path/to/firmware.elf"))
    raw_data = {"prog_path": prog_path}
    idedata = platformio_api.IDEData(raw_data)

    result = idedata.firmware_bin_path
    assert isinstance(result, str)
    expected = str(Path("/path/to/firmware.bin"))
    assert result == expected
    assert result.endswith(".bin")


def test_idedata_firmware_bin_path_preserves_directory(setup_core: Path) -> None:
    """Test firmware_bin_path preserves the directory structure."""
    CORE.build_path = str(setup_core / "build" / "test")
    CORE.name = "test"
    prog_path = str(Path("/complex/path/to/build/firmware.elf"))
    raw_data = {"prog_path": prog_path}
    idedata = platformio_api.IDEData(raw_data)

    result = idedata.firmware_bin_path
    expected = str(Path("/complex/path/to/build/firmware.bin"))
    assert result == expected


def test_idedata_extra_flash_images(setup_core: Path) -> None:
    """Test IDEData.extra_flash_images returns list of FlashImage objects."""
    CORE.build_path = str(setup_core / "build" / "test")
    CORE.name = "test"
    raw_data = {
        "prog_path": "/path/to/firmware.elf",
        "extra": {
            "flash_images": [
                {"path": "/path/to/bootloader.bin", "offset": "0x1000"},
                {"path": "/path/to/partition.bin", "offset": "0x8000"},
            ]
        },
    }
    idedata = platformio_api.IDEData(raw_data)

    images = idedata.extra_flash_images
    assert len(images) == 2
    assert all(isinstance(img, platformio_api.FlashImage) for img in images)
    assert images[0].path == "/path/to/bootloader.bin"
    assert images[0].offset == "0x1000"
    assert images[1].path == "/path/to/partition.bin"
    assert images[1].offset == "0x8000"


def test_idedata_extra_flash_images_empty(setup_core: Path) -> None:
    """Test extra_flash_images returns empty list when no extra images."""
    CORE.build_path = str(setup_core / "build" / "test")
    CORE.name = "test"
    raw_data = {"prog_path": "/path/to/firmware.elf", "extra": {"flash_images": []}}
    idedata = platformio_api.IDEData(raw_data)

    images = idedata.extra_flash_images
    assert images == []


def test_idedata_cc_path(setup_core: Path) -> None:
    """Test IDEData.cc_path returns compiler path."""
    CORE.build_path = str(setup_core / "build" / "test")
    CORE.name = "test"
    raw_data = {
        "prog_path": "/path/to/firmware.elf",
        "cc_path": "/Users/test/.platformio/packages/toolchain-xtensa32/bin/xtensa-esp32-elf-gcc",
    }
    idedata = platformio_api.IDEData(raw_data)

    assert (
        idedata.cc_path
        == "/Users/test/.platformio/packages/toolchain-xtensa32/bin/xtensa-esp32-elf-gcc"
    )


def test_flash_image_dataclass() -> None:
    """Test FlashImage dataclass stores path and offset correctly."""
    image = platformio_api.FlashImage(path="/path/to/image.bin", offset="0x10000")

    assert image.path == "/path/to/image.bin"
    assert image.offset == "0x10000"


def test_load_idedata_returns_dict(setup_core: Path) -> None:
    """Test _load_idedata returns parsed idedata dict when successful."""
    CORE.build_path = str(setup_core / "build" / "test")
    CORE.name = "test"

    # Create required files
    platformio_ini = setup_core / "build" / "test" / "platformio.ini"
    platformio_ini.parent.mkdir(parents=True, exist_ok=True)
    platformio_ini.touch()

    idedata_path = setup_core / ".esphome" / "idedata" / "test.json"
    idedata_path.parent.mkdir(parents=True, exist_ok=True)
    idedata_path.write_text('{"prog_path": "/test/firmware.elf"}')

    with patch("esphome.platformio_api.run_platformio_cli_run") as mock_run:
        mock_run.return_value = '{"prog_path": "/test/firmware.elf"}'

        config = {"name": "test"}
        result = platformio_api._load_idedata(config)

    assert result is not None
    assert isinstance(result, dict)
    assert result["prog_path"] == "/test/firmware.elf"
