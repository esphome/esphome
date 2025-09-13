"""Tests for platformio_api.py path functions."""

import json
import os
from pathlib import Path
import shutil
from types import SimpleNamespace
from unittest.mock import MagicMock, Mock, patch

import pytest

from esphome import platformio_api
from esphome.core import CORE, EsphomeError


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


def test_load_idedata_returns_dict(
    setup_core: Path, mock_run_platformio_cli_run
) -> None:
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

    mock_run_platformio_cli_run.return_value = '{"prog_path": "/test/firmware.elf"}'

    config = {"name": "test"}
    result = platformio_api._load_idedata(config)

    assert result is not None
    assert isinstance(result, dict)
    assert result["prog_path"] == "/test/firmware.elf"


def test_load_idedata_uses_cache_when_valid(
    setup_core: Path, mock_run_platformio_cli_run: Mock
) -> None:
    """Test _load_idedata uses cached data when unchanged."""
    CORE.build_path = str(setup_core / "build" / "test")
    CORE.name = "test"

    # Create platformio.ini
    platformio_ini = setup_core / "build" / "test" / "platformio.ini"
    platformio_ini.parent.mkdir(parents=True, exist_ok=True)
    platformio_ini.write_text("content")

    # Create idedata cache file that's newer
    idedata_path = setup_core / ".esphome" / "idedata" / "test.json"
    idedata_path.parent.mkdir(parents=True, exist_ok=True)
    idedata_path.write_text('{"prog_path": "/cached/firmware.elf"}')

    # Make idedata newer than platformio.ini
    platformio_ini_mtime = platformio_ini.stat().st_mtime
    os.utime(idedata_path, (platformio_ini_mtime + 1, platformio_ini_mtime + 1))

    config = {"name": "test"}
    result = platformio_api._load_idedata(config)

    # Should not call _run_idedata since cache is valid
    mock_run_platformio_cli_run.assert_not_called()

    assert result["prog_path"] == "/cached/firmware.elf"


def test_load_idedata_regenerates_when_platformio_ini_newer(
    setup_core: Path, mock_run_platformio_cli_run: Mock
) -> None:
    """Test _load_idedata regenerates when platformio.ini is newer."""
    CORE.build_path = str(setup_core / "build" / "test")
    CORE.name = "test"

    # Create idedata cache file first
    idedata_path = setup_core / ".esphome" / "idedata" / "test.json"
    idedata_path.parent.mkdir(parents=True, exist_ok=True)
    idedata_path.write_text('{"prog_path": "/old/firmware.elf"}')

    # Create platformio.ini that's newer
    idedata_mtime = idedata_path.stat().st_mtime
    platformio_ini = setup_core / "build" / "test" / "platformio.ini"
    platformio_ini.parent.mkdir(parents=True, exist_ok=True)
    platformio_ini.write_text("content")
    # Make platformio.ini newer than idedata
    os.utime(platformio_ini, (idedata_mtime + 1, idedata_mtime + 1))

    # Mock platformio to return new data
    new_data = {"prog_path": "/new/firmware.elf"}
    mock_run_platformio_cli_run.return_value = json.dumps(new_data)

    config = {"name": "test"}
    result = platformio_api._load_idedata(config)

    # Should call _run_idedata since platformio.ini is newer
    mock_run_platformio_cli_run.assert_called_once()

    assert result["prog_path"] == "/new/firmware.elf"


def test_load_idedata_regenerates_on_corrupted_cache(
    setup_core: Path, mock_run_platformio_cli_run: Mock
) -> None:
    """Test _load_idedata regenerates when cache file is corrupted."""
    CORE.build_path = str(setup_core / "build" / "test")
    CORE.name = "test"

    # Create platformio.ini
    platformio_ini = setup_core / "build" / "test" / "platformio.ini"
    platformio_ini.parent.mkdir(parents=True, exist_ok=True)
    platformio_ini.write_text("content")

    # Create corrupted idedata cache file
    idedata_path = setup_core / ".esphome" / "idedata" / "test.json"
    idedata_path.parent.mkdir(parents=True, exist_ok=True)
    idedata_path.write_text('{"prog_path": invalid json')

    # Make idedata newer so it would be used if valid
    platformio_ini_mtime = platformio_ini.stat().st_mtime
    os.utime(idedata_path, (platformio_ini_mtime + 1, platformio_ini_mtime + 1))

    # Mock platformio to return new data
    new_data = {"prog_path": "/new/firmware.elf"}
    mock_run_platformio_cli_run.return_value = json.dumps(new_data)

    config = {"name": "test"}
    result = platformio_api._load_idedata(config)

    # Should call _run_idedata since cache is corrupted
    mock_run_platformio_cli_run.assert_called_once()

    assert result["prog_path"] == "/new/firmware.elf"


def test_run_idedata_parses_json_from_output(
    setup_core: Path, mock_run_platformio_cli_run: Mock
) -> None:
    """Test _run_idedata extracts JSON from platformio output."""
    config = {"name": "test"}

    expected_data = {
        "prog_path": "/path/to/firmware.elf",
        "cc_path": "/path/to/gcc",
        "extra": {"flash_images": []},
    }

    # Simulate platformio output with JSON embedded
    mock_run_platformio_cli_run.return_value = (
        f"Some preamble\n{json.dumps(expected_data)}\nSome postamble"
    )

    result = platformio_api._run_idedata(config)

    assert result == expected_data


def test_run_idedata_raises_on_no_json(
    setup_core: Path, mock_run_platformio_cli_run: Mock
) -> None:
    """Test _run_idedata raises EsphomeError when no JSON found."""
    config = {"name": "test"}

    mock_run_platformio_cli_run.return_value = "No JSON in this output"

    with pytest.raises(EsphomeError):
        platformio_api._run_idedata(config)


def test_run_idedata_raises_on_invalid_json(
    setup_core: Path, mock_run_platformio_cli_run: Mock
) -> None:
    """Test _run_idedata raises on malformed JSON."""
    config = {"name": "test"}
    mock_run_platformio_cli_run.return_value = '{"invalid": json"}'

    # The ValueError from json.loads is re-raised
    with pytest.raises(ValueError):
        platformio_api._run_idedata(config)


def test_run_platformio_cli_sets_environment_variables(
    setup_core: Path, mock_run_external_command: Mock
) -> None:
    """Test run_platformio_cli sets correct environment variables."""
    CORE.build_path = str(setup_core / "build" / "test")

    with patch.dict(os.environ, {}, clear=False):
        mock_run_external_command.return_value = 0
        platformio_api.run_platformio_cli("test", "arg")

        # Check environment variables were set
        assert os.environ["PLATFORMIO_FORCE_COLOR"] == "true"
        assert (
            setup_core / "build" / "test"
            in Path(os.environ["PLATFORMIO_BUILD_DIR"]).parents
            or Path(os.environ["PLATFORMIO_BUILD_DIR"]) == setup_core / "build" / "test"
        )
        assert "PLATFORMIO_LIBDEPS_DIR" in os.environ
        assert "PYTHONWARNINGS" in os.environ

        # Check command was called correctly
        mock_run_external_command.assert_called_once()
        args = mock_run_external_command.call_args[0]
        assert "platformio" in args
        assert "test" in args
        assert "arg" in args


def test_run_platformio_cli_run_builds_command(
    setup_core: Path, mock_run_platformio_cli: Mock
) -> None:
    """Test run_platformio_cli_run builds correct command."""
    CORE.build_path = str(setup_core / "build" / "test")
    mock_run_platformio_cli.return_value = 0

    config = {"name": "test"}
    platformio_api.run_platformio_cli_run(config, True, "extra", "args")

    mock_run_platformio_cli.assert_called_once_with(
        "run", "-d", CORE.build_path, "-v", "extra", "args"
    )


def test_run_compile(setup_core: Path, mock_run_platformio_cli_run: Mock) -> None:
    """Test run_compile with process limit."""
    from esphome.const import CONF_COMPILE_PROCESS_LIMIT, CONF_ESPHOME

    CORE.build_path = str(setup_core / "build" / "test")
    config = {CONF_ESPHOME: {CONF_COMPILE_PROCESS_LIMIT: 4}}
    mock_run_platformio_cli_run.return_value = 0

    platformio_api.run_compile(config, verbose=True)

    mock_run_platformio_cli_run.assert_called_once_with(config, True, "-j4")


def test_get_idedata_caches_result(
    setup_core: Path, mock_run_platformio_cli_run: Mock
) -> None:
    """Test get_idedata caches result in CORE.data."""
    from esphome.const import KEY_CORE

    CORE.build_path = str(setup_core / "build" / "test")
    CORE.name = "test"
    CORE.data[KEY_CORE] = {}

    # Create platformio.ini to avoid regeneration
    platformio_ini = setup_core / "build" / "test" / "platformio.ini"
    platformio_ini.parent.mkdir(parents=True, exist_ok=True)
    platformio_ini.write_text("content")

    # Mock platformio to return data
    idedata = {"prog_path": "/test/firmware.elf"}
    mock_run_platformio_cli_run.return_value = json.dumps(idedata)

    config = {"name": "test"}

    # First call should load and cache
    result1 = platformio_api.get_idedata(config)
    mock_run_platformio_cli_run.assert_called_once()

    # Second call should use cache from CORE.data
    result2 = platformio_api.get_idedata(config)
    mock_run_platformio_cli_run.assert_called_once()  # Still only called once

    assert result1 is result2
    assert isinstance(result1, platformio_api.IDEData)
    assert result1.firmware_elf_path == "/test/firmware.elf"


def test_idedata_addr2line_path_windows(setup_core: Path) -> None:
    """Test IDEData.addr2line_path on Windows."""
    raw_data = {"prog_path": "/path/to/firmware.elf", "cc_path": "C:\\tools\\gcc.exe"}
    idedata = platformio_api.IDEData(raw_data)

    result = idedata.addr2line_path
    assert result == "C:\\tools\\addr2line.exe"


def test_idedata_addr2line_path_unix(setup_core: Path) -> None:
    """Test IDEData.addr2line_path on Unix."""
    raw_data = {"prog_path": "/path/to/firmware.elf", "cc_path": "/usr/bin/gcc"}
    idedata = platformio_api.IDEData(raw_data)

    result = idedata.addr2line_path
    assert result == "/usr/bin/addr2line"


def test_patch_structhash(setup_core: Path) -> None:
    """Test patch_structhash monkey patches platformio functions."""
    # Create simple namespace objects to act as modules
    mock_cli = SimpleNamespace()
    mock_helpers = SimpleNamespace()
    mock_run = SimpleNamespace(cli=mock_cli, helpers=mock_helpers)

    # Mock platformio modules
    with patch.dict(
        "sys.modules",
        {
            "platformio.run.cli": mock_cli,
            "platformio.run.helpers": mock_helpers,
            "platformio.run": mock_run,
            "platformio.project.helpers": MagicMock(),
            "platformio.fs": MagicMock(),
            "platformio": MagicMock(),
        },
    ):
        # Call patch_structhash
        platformio_api.patch_structhash()

        # Verify both modules had clean_build_dir patched
        # Check that clean_build_dir was set on both modules
        assert hasattr(mock_cli, "clean_build_dir")
        assert hasattr(mock_helpers, "clean_build_dir")

        # Verify they got the same function assigned
        assert mock_cli.clean_build_dir is mock_helpers.clean_build_dir

        # Verify it's a real function (not a Mock)
        assert callable(mock_cli.clean_build_dir)
        assert mock_cli.clean_build_dir.__name__ == "patched_clean_build_dir"


def test_patched_clean_build_dir_removes_outdated(setup_core: Path) -> None:
    """Test patched_clean_build_dir removes build dir when platformio.ini is newer."""
    build_dir = setup_core / "build"
    build_dir.mkdir()
    platformio_ini = setup_core / "platformio.ini"
    platformio_ini.write_text("config")

    # Make platformio.ini newer than build_dir
    build_mtime = build_dir.stat().st_mtime
    os.utime(platformio_ini, (build_mtime + 1, build_mtime + 1))

    # Track if directory was removed
    removed_paths: list[str] = []

    def track_rmtree(path: str) -> None:
        removed_paths.append(path)
        shutil.rmtree(path)

    # Create mock modules that patch_structhash expects
    mock_cli = SimpleNamespace()
    mock_helpers = SimpleNamespace()
    mock_project_helpers = MagicMock()
    mock_project_helpers.get_project_dir.return_value = str(setup_core)
    mock_fs = SimpleNamespace(rmtree=track_rmtree)

    with patch.dict(
        "sys.modules",
        {
            "platformio": SimpleNamespace(fs=mock_fs),
            "platformio.fs": mock_fs,
            "platformio.project.helpers": mock_project_helpers,
            "platformio.run": SimpleNamespace(cli=mock_cli, helpers=mock_helpers),
            "platformio.run.cli": mock_cli,
            "platformio.run.helpers": mock_helpers,
        },
    ):
        # Call patch_structhash to install the patched function
        platformio_api.patch_structhash()

        # Call the patched function
        mock_helpers.clean_build_dir(str(build_dir), [])

        # Verify directory was removed and recreated
        assert len(removed_paths) == 1
        assert removed_paths[0] == str(build_dir)
        assert build_dir.exists()  # makedirs recreated it


def test_patched_clean_build_dir_keeps_updated(setup_core: Path) -> None:
    """Test patched_clean_build_dir keeps build dir when it's up to date."""
    build_dir = setup_core / "build"
    build_dir.mkdir()
    test_file = build_dir / "test.txt"
    test_file.write_text("test content")

    platformio_ini = setup_core / "platformio.ini"
    platformio_ini.write_text("config")

    # Make build_dir newer than platformio.ini
    ini_mtime = platformio_ini.stat().st_mtime
    os.utime(build_dir, (ini_mtime + 1, ini_mtime + 1))

    # Track if rmtree is called
    removed_paths: list[str] = []

    def track_rmtree(path: str) -> None:
        removed_paths.append(path)

    # Create mock modules
    mock_cli = SimpleNamespace()
    mock_helpers = SimpleNamespace()
    mock_project_helpers = MagicMock()
    mock_project_helpers.get_project_dir.return_value = str(setup_core)
    mock_fs = SimpleNamespace(rmtree=track_rmtree)

    with patch.dict(
        "sys.modules",
        {
            "platformio": SimpleNamespace(fs=mock_fs),
            "platformio.fs": mock_fs,
            "platformio.project.helpers": mock_project_helpers,
            "platformio.run": SimpleNamespace(cli=mock_cli, helpers=mock_helpers),
            "platformio.run.cli": mock_cli,
            "platformio.run.helpers": mock_helpers,
        },
    ):
        # Call patch_structhash to install the patched function
        platformio_api.patch_structhash()

        # Call the patched function
        mock_helpers.clean_build_dir(str(build_dir), [])

        # Verify rmtree was NOT called
        assert len(removed_paths) == 0

        # Verify directory and file still exist
        assert build_dir.exists()
        assert test_file.exists()
        assert test_file.read_text() == "test content"


def test_patched_clean_build_dir_creates_missing(setup_core: Path) -> None:
    """Test patched_clean_build_dir creates build dir when it doesn't exist."""
    build_dir = setup_core / "build"
    platformio_ini = setup_core / "platformio.ini"
    platformio_ini.write_text("config")

    # Ensure build_dir doesn't exist
    assert not build_dir.exists()

    # Track if rmtree is called
    removed_paths: list[str] = []

    def track_rmtree(path: str) -> None:
        removed_paths.append(path)

    # Create mock modules
    mock_cli = SimpleNamespace()
    mock_helpers = SimpleNamespace()
    mock_project_helpers = MagicMock()
    mock_project_helpers.get_project_dir.return_value = str(setup_core)
    mock_fs = SimpleNamespace(rmtree=track_rmtree)

    with patch.dict(
        "sys.modules",
        {
            "platformio": SimpleNamespace(fs=mock_fs),
            "platformio.fs": mock_fs,
            "platformio.project.helpers": mock_project_helpers,
            "platformio.run": SimpleNamespace(cli=mock_cli, helpers=mock_helpers),
            "platformio.run.cli": mock_cli,
            "platformio.run.helpers": mock_helpers,
        },
    ):
        # Call patch_structhash to install the patched function
        platformio_api.patch_structhash()

        # Call the patched function
        mock_helpers.clean_build_dir(str(build_dir), [])

        # Verify rmtree was NOT called
        assert len(removed_paths) == 0

        # Verify directory was created
        assert build_dir.exists()


def test_process_stacktrace_esp8266_exception(setup_core: Path, caplog) -> None:
    """Test process_stacktrace handles ESP8266 exceptions."""
    config = {"name": "test"}

    # Test exception type parsing
    line = "Exception (28):"
    backtrace_state = False

    result = platformio_api.process_stacktrace(config, line, backtrace_state)

    assert "Access to invalid address: LOAD (wild pointer?)" in caplog.text
    assert result is False


def test_process_stacktrace_esp8266_backtrace(
    setup_core: Path, mock_decode_pc: Mock
) -> None:
    """Test process_stacktrace handles ESP8266 multi-line backtrace."""
    config = {"name": "test"}

    # Start of backtrace
    line1 = ">>>stack>>>"
    state = platformio_api.process_stacktrace(config, line1, False)
    assert state is True

    # Backtrace content with addresses
    line2 = "40201234 40205678"
    state = platformio_api.process_stacktrace(config, line2, state)
    assert state is True
    assert mock_decode_pc.call_count == 2

    # End of backtrace
    line3 = "<<<stack<<<"
    state = platformio_api.process_stacktrace(config, line3, state)
    assert state is False


def test_process_stacktrace_esp32_backtrace(
    setup_core: Path, mock_decode_pc: Mock
) -> None:
    """Test process_stacktrace handles ESP32 single-line backtrace."""
    config = {"name": "test"}

    line = "Backtrace: 0x40081234:0x3ffb1234 0x40085678:0x3ffb5678"
    state = platformio_api.process_stacktrace(config, line, False)

    # Should decode both addresses
    assert mock_decode_pc.call_count == 2
    mock_decode_pc.assert_any_call(config, "40081234")
    mock_decode_pc.assert_any_call(config, "40085678")
    assert state is False


def test_process_stacktrace_bad_alloc(
    setup_core: Path, mock_decode_pc: Mock, caplog
) -> None:
    """Test process_stacktrace handles bad alloc messages."""
    config = {"name": "test"}

    line = "last failed alloc call: 40201234(512)"
    state = platformio_api.process_stacktrace(config, line, False)

    assert "Memory allocation of 512 bytes failed at 40201234" in caplog.text
    mock_decode_pc.assert_called_once_with(config, "40201234")
    assert state is False
