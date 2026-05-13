"""Tests for esphome.platformio.toolchain path functions."""

# pylint: disable=protected-access

from contextlib import contextmanager
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import shutil
import threading
from types import SimpleNamespace
from unittest.mock import MagicMock, Mock, call, patch

import pytest

from esphome.core import CORE, EsphomeError
from esphome.platformio import runner, toolchain
from esphome.util import FlashImage


def test_idedata_firmware_elf_path(setup_core: Path) -> None:
    """Test IDEData.firmware_elf_path returns correct path."""
    CORE.build_path = setup_core / "build" / "test"
    CORE.name = "test"
    raw_data = {"prog_path": "/path/to/firmware.elf"}
    idedata = toolchain.IDEData(raw_data)

    assert idedata.firmware_elf_path == Path("/path/to/firmware.elf")


def test_idedata_firmware_bin_path(setup_core: Path) -> None:
    """Test IDEData.firmware_bin_path returns Path with .bin extension."""
    CORE.build_path = setup_core / "build" / "test"
    CORE.name = "test"
    prog_path = str(Path("/path/to/firmware.elf"))
    raw_data = {"prog_path": prog_path}
    idedata = toolchain.IDEData(raw_data)

    result = idedata.firmware_bin_path
    assert isinstance(result, Path)
    expected = Path("/path/to/firmware.bin")
    assert result == expected
    assert str(result).endswith(".bin")


def test_idedata_firmware_bin_path_preserves_directory(setup_core: Path) -> None:
    """Test firmware_bin_path preserves the directory structure."""
    CORE.build_path = setup_core / "build" / "test"
    CORE.name = "test"
    prog_path = str(Path("/complex/path/to/build/firmware.elf"))
    raw_data = {"prog_path": prog_path}
    idedata = toolchain.IDEData(raw_data)

    result = idedata.firmware_bin_path
    expected = Path("/complex/path/to/build/firmware.bin")
    assert result == expected


def test_idedata_extra_flash_images(setup_core: Path) -> None:
    """Test IDEData.extra_flash_images returns list of FlashImage objects."""
    CORE.build_path = setup_core / "build" / "test"
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
    idedata = toolchain.IDEData(raw_data)

    images = idedata.extra_flash_images
    assert len(images) == 2
    assert all(isinstance(img, FlashImage) for img in images)
    assert images[0].path == Path("/path/to/bootloader.bin")
    assert images[0].offset == "0x1000"
    assert images[1].path == Path("/path/to/partition.bin")
    assert images[1].offset == "0x8000"


def test_idedata_extra_flash_images_empty(setup_core: Path) -> None:
    """Test extra_flash_images returns empty list when no extra images."""
    CORE.build_path = setup_core / "build" / "test"
    CORE.name = "test"
    raw_data = {"prog_path": "/path/to/firmware.elf", "extra": {"flash_images": []}}
    idedata = toolchain.IDEData(raw_data)

    images = idedata.extra_flash_images
    assert images == []


def test_idedata_cc_path(setup_core: Path) -> None:
    """Test IDEData.cc_path returns compiler path."""
    CORE.build_path = setup_core / "build" / "test"
    CORE.name = "test"
    raw_data = {
        "prog_path": "/path/to/firmware.elf",
        "cc_path": "/Users/test/.platformio/packages/toolchain-xtensa32/bin/xtensa-esp32-elf-gcc",
    }
    idedata = toolchain.IDEData(raw_data)

    assert (
        idedata.cc_path
        == "/Users/test/.platformio/packages/toolchain-xtensa32/bin/xtensa-esp32-elf-gcc"
    )


def test_flash_image_dataclass() -> None:
    """Test FlashImage dataclass stores path and offset correctly."""
    image = FlashImage(path=Path("/path/to/image.bin"), offset="0x10000")

    assert image.path == Path("/path/to/image.bin")
    assert image.offset == "0x10000"


def test_load_idedata_returns_dict(
    setup_core: Path, mock_run_platformio_cli_run
) -> None:
    """Test _load_idedata returns parsed idedata dict when successful."""
    CORE.build_path = setup_core / "build" / "test"
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
    result = toolchain._load_idedata(config)

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
    result = toolchain._load_idedata(config)

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
    result = toolchain._load_idedata(config)

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
    result = toolchain._load_idedata(config)

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

    result = toolchain._run_idedata(config)

    assert result == expected_data


def test_run_idedata_raises_on_no_json(
    setup_core: Path, mock_run_platformio_cli_run: Mock
) -> None:
    """Test _run_idedata raises EsphomeError when no JSON found."""
    config = {"name": "test"}

    mock_run_platformio_cli_run.return_value = "No JSON in this output"

    with pytest.raises(EsphomeError):
        toolchain._run_idedata(config)


def test_run_idedata_raises_on_invalid_json(
    setup_core: Path, mock_run_platformio_cli_run: Mock
) -> None:
    """Test _run_idedata raises on malformed JSON."""
    config = {"name": "test"}
    mock_run_platformio_cli_run.return_value = '{"invalid": json"}'

    # The ValueError from json.loads is re-raised
    with pytest.raises(ValueError):
        toolchain._run_idedata(config)


def test_run_platformio_cli_sets_environment_variables(
    setup_core: Path, mock_run_external_process: Mock
) -> None:
    """Test run_platformio_cli sets correct environment variables."""
    CORE.build_path = str(setup_core / "build" / "test")

    with patch.dict(os.environ, {}, clear=False):
        mock_run_external_process.return_value = 0
        toolchain.run_platformio_cli("test", "arg")

        # Check environment variables were set
        assert os.environ["PLATFORMIO_FORCE_COLOR"] == "true"
        assert (
            setup_core / "build" / "test"
            in Path(os.environ["PLATFORMIO_BUILD_DIR"]).parents
            or Path(os.environ["PLATFORMIO_BUILD_DIR"]) == setup_core / "build" / "test"
        )
        assert "PLATFORMIO_LIBDEPS_DIR" in os.environ
        assert "PYTHONWARNINGS" in os.environ

        # Check command was called correctly — runs PlatformIO as a subprocess
        # via the esphome.platformio.runner entry point.
        mock_run_external_process.assert_called_once()
        args = mock_run_external_process.call_args[0]
        assert "-m" in args
        assert "esphome.platformio.runner" in args
        assert "test" in args
        assert "arg" in args


@pytest.mark.parametrize(
    ("platform", "input_path", "expected"),
    [
        # win32: drive-letter extended-length prefix is stripped
        (
            "win32",
            "\\\\?\\C:\\Users\\jesse\\AppData\\Local\\ESPHome Builder\\python\\python.exe",
            "C:\\Users\\jesse\\AppData\\Local\\ESPHome Builder\\python\\python.exe",
        ),
        # win32: UNC extended-length prefix is translated to a regular UNC path
        (
            "win32",
            "\\\\?\\UNC\\server\\share\\python.exe",
            "\\\\server\\share\\python.exe",
        ),
        # win32: paths without the prefix are returned unchanged
        (
            "win32",
            "C:\\Users\\jesse\\AppData\\Local\\ESPHome Builder\\python\\python.exe",
            "C:\\Users\\jesse\\AppData\\Local\\ESPHome Builder\\python\\python.exe",
        ),
        # non-win32: prefix is left alone (no-op)
        ("linux", "\\\\?\\C:\\python.exe", "\\\\?\\C:\\python.exe"),
        ("darwin", "/usr/bin/python3", "/usr/bin/python3"),
    ],
)
def test_strip_win_long_path_prefix(
    platform: str, input_path: str, expected: str
) -> None:
    r"""``\\?\`` and ``\\?\UNC\`` prefixes are stripped only on win32."""
    with patch("esphome.platformio.toolchain.sys.platform", platform):
        assert toolchain._strip_win_long_path_prefix(input_path) == expected


def test_run_platformio_cli_strips_win_long_path_prefix(
    setup_core: Path, mock_run_external_process: Mock
) -> None:
    r"""Windows ``\\?\`` prefix on sys.executable does not leak into the subprocess.

    The NSIS-installed esphome.exe launcher starts Python with
    ``sys.executable`` already prefixed by the extended-length path marker.
    That prefix would otherwise propagate into PlatformIO's ``PYTHONEXE`` and
    break SCons-emitted command lines run through ``cmd.exe``.
    """
    CORE.build_path = str(setup_core / "build" / "test")
    prefixed_exe = (
        "\\\\?\\C:\\Users\\jesse\\AppData\\Local\\ESPHome Builder\\python\\python.exe"
    )
    stripped_exe = (
        "C:\\Users\\jesse\\AppData\\Local\\ESPHome Builder\\python\\python.exe"
    )

    with (
        patch.dict(os.environ, {}, clear=False),
        patch("esphome.platformio.toolchain.sys.platform", "win32"),
        patch("esphome.platformio.toolchain.sys.executable", prefixed_exe),
    ):
        # Pop any pre-existing PYTHONEXEPATH so the assertion below reflects
        # what run_platformio_cli set, not whatever the test runner's
        # environment happened to contain.
        os.environ.pop("PYTHONEXEPATH", None)
        mock_run_external_process.return_value = 0
        toolchain.run_platformio_cli("test", "arg")

        # The subprocess is invoked with the stripped executable path.
        mock_run_external_process.assert_called_once()
        args = mock_run_external_process.call_args[0]
        assert args[0] == stripped_exe
        # PYTHONEXEPATH is exported with the stripped path so PlatformIO's
        # get_pythonexe_path() picks it up in the subprocess.
        assert os.environ["PYTHONEXEPATH"] == stripped_exe


def test_run_platformio_cli_does_not_set_pythonexepath_without_strip(
    setup_core: Path, mock_run_external_process: Mock
) -> None:
    r"""PYTHONEXEPATH is not touched when sys.executable has no ``\\?\`` prefix.

    Setting it unconditionally would clobber a user-provided value (or
    interfere with non-Windows tooling that has no prefix to strip).
    """
    CORE.build_path = str(setup_core / "build" / "test")
    plain_exe = "/usr/bin/python3"

    with (
        patch.dict(os.environ, {}, clear=False),
        patch("esphome.platformio.toolchain.sys.platform", "linux"),
        patch("esphome.platformio.toolchain.sys.executable", plain_exe),
    ):
        os.environ.pop("PYTHONEXEPATH", None)
        mock_run_external_process.return_value = 0
        toolchain.run_platformio_cli("test", "arg")

        mock_run_external_process.assert_called_once()
        args = mock_run_external_process.call_args[0]
        assert args[0] == plain_exe
        assert "PYTHONEXEPATH" not in os.environ


def test_run_platformio_cli_run_builds_command(
    setup_core: Path, mock_run_platformio_cli: Mock
) -> None:
    """Test run_platformio_cli_run builds correct command."""
    CORE.build_path = str(setup_core / "build" / "test")
    mock_run_platformio_cli.return_value = 0

    config = {"name": "test"}
    toolchain.run_platformio_cli_run(config, True, "extra", "args")

    mock_run_platformio_cli.assert_called_once_with(
        "run", "-d", CORE.build_path, "-v", "extra", "args"
    )


def test_run_compile(setup_core: Path, mock_run_platformio_cli_run: Mock) -> None:
    """Test run_compile with process limit."""
    from esphome.const import CONF_COMPILE_PROCESS_LIMIT, CONF_ESPHOME

    CORE.build_path = str(setup_core / "build" / "test")
    config = {CONF_ESPHOME: {CONF_COMPILE_PROCESS_LIMIT: 4}}
    mock_run_platformio_cli_run.return_value = 0

    toolchain.run_compile(config, verbose=True)

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
    result1 = toolchain.get_idedata(config)
    mock_run_platformio_cli_run.assert_called_once()

    # Second call should use cache from CORE.data
    result2 = toolchain.get_idedata(config)
    mock_run_platformio_cli_run.assert_called_once()  # Still only called once

    assert result1 is result2
    assert isinstance(result1, toolchain.IDEData)
    assert result1.firmware_elf_path == Path("/test/firmware.elf")


def test_idedata_addr2line_path_windows(setup_core: Path) -> None:
    """Test IDEData.addr2line_path on Windows."""
    raw_data = {"prog_path": "/path/to/firmware.elf", "cc_path": "C:\\tools\\gcc.exe"}
    idedata = toolchain.IDEData(raw_data)

    result = idedata.addr2line_path
    assert result == "C:\\tools\\addr2line.exe"


def test_idedata_addr2line_path_unix(setup_core: Path) -> None:
    """Test IDEData.addr2line_path on Unix."""
    raw_data = {"prog_path": "/path/to/firmware.elf", "cc_path": "/usr/bin/gcc"}
    idedata = toolchain.IDEData(raw_data)

    result = idedata.addr2line_path
    assert result == "/usr/bin/addr2line"


def test_idedata_objdump_path_windows(setup_core: Path) -> None:
    """Test IDEData.objdump_path on Windows."""
    raw_data = {"prog_path": "/path/to/firmware.elf", "cc_path": "C:\\tools\\gcc.exe"}
    idedata = toolchain.IDEData(raw_data)

    result = idedata.objdump_path
    assert result == "C:\\tools\\objdump.exe"


def test_idedata_objdump_path_unix(setup_core: Path) -> None:
    """Test IDEData.objdump_path on Unix."""
    raw_data = {"prog_path": "/path/to/firmware.elf", "cc_path": "/usr/bin/gcc"}
    idedata = toolchain.IDEData(raw_data)

    result = idedata.objdump_path
    assert result == "/usr/bin/objdump"


def test_idedata_readelf_path_windows(setup_core: Path) -> None:
    """Test IDEData.readelf_path on Windows."""
    raw_data = {"prog_path": "/path/to/firmware.elf", "cc_path": "C:\\tools\\gcc.exe"}
    idedata = toolchain.IDEData(raw_data)

    result = idedata.readelf_path
    assert result == "C:\\tools\\readelf.exe"


def test_idedata_readelf_path_unix(setup_core: Path) -> None:
    """Test IDEData.readelf_path on Unix."""
    raw_data = {"prog_path": "/path/to/firmware.elf", "cc_path": "/usr/bin/gcc"}
    idedata = toolchain.IDEData(raw_data)

    result = idedata.readelf_path
    assert result == "/usr/bin/readelf"


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
        runner.patch_structhash()

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
    removed_paths: list[Path] = []

    def track_rmtree(path: Path) -> None:
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
        runner.patch_structhash()

        # Call the patched function
        mock_helpers.clean_build_dir(str(build_dir), [])

        # Verify directory was removed and recreated
        assert len(removed_paths) == 1
        assert removed_paths[0] == build_dir
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
        runner.patch_structhash()

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
        runner.patch_structhash()

        # Call the patched function
        mock_helpers.clean_build_dir(str(build_dir), [])

        # Verify rmtree was NOT called
        assert len(removed_paths) == 0

        # Verify directory was created
        assert build_dir.exists()


def test_patch_file_downloader_succeeds_first_try() -> None:
    """Test patch_file_downloader succeeds on first attempt."""
    mock_exception_cls = type("PackageException", (Exception,), {})
    original_init = MagicMock()

    with patch.dict(
        "sys.modules",
        {
            "platformio": MagicMock(),
            "platformio.package": MagicMock(),
            "platformio.package.download": SimpleNamespace(
                FileDownloader=type("FileDownloader", (), {"__init__": original_init})
            ),
            "platformio.package.exception": SimpleNamespace(
                PackageException=mock_exception_cls
            ),
        },
    ):
        runner.patch_file_downloader()

        from platformio.package.download import FileDownloader

        instance = object.__new__(FileDownloader)
        FileDownloader.__init__(instance, "http://example.com/file.zip")

        original_init.assert_called_once()


def test_patch_file_downloader_retries_on_failure() -> None:
    """Test patch_file_downloader retries with backoff on PackageException."""
    mock_exception_cls = type("PackageException", (Exception,), {})
    call_count = 0

    def failing_init(self, *args, **kwargs):
        nonlocal call_count
        call_count += 1
        if call_count < 3:
            raise mock_exception_cls(f"502 error attempt {call_count}")

    with (
        patch.dict(
            "sys.modules",
            {
                "platformio": MagicMock(),
                "platformio.package": MagicMock(),
                "platformio.package.download": SimpleNamespace(
                    FileDownloader=type(
                        "FileDownloader", (), {"__init__": failing_init}
                    )
                ),
                "platformio.package.exception": SimpleNamespace(
                    PackageException=mock_exception_cls
                ),
            },
        ),
        patch("time.sleep") as mock_sleep,
    ):
        runner.patch_file_downloader()

        from platformio.package.download import FileDownloader

        instance = object.__new__(FileDownloader)
        FileDownloader.__init__(instance, "http://example.com/file.zip")

        # Should have been called 3 times (2 failures + 1 success)
        assert call_count == 3

        # Should have slept with exponential backoff: 2s, 4s
        assert mock_sleep.call_count == 2
        mock_sleep.assert_any_call(2)
        mock_sleep.assert_any_call(4)


def test_patch_file_downloader_raises_after_max_retries() -> None:
    """Test patch_file_downloader raises after exhausting all retries."""
    mock_exception_cls = type("PackageException", (Exception,), {})

    def always_failing_init(self, *args, **kwargs):
        raise mock_exception_cls("502 error")

    with (
        patch.dict(
            "sys.modules",
            {
                "platformio": MagicMock(),
                "platformio.package": MagicMock(),
                "platformio.package.download": SimpleNamespace(
                    FileDownloader=type(
                        "FileDownloader", (), {"__init__": always_failing_init}
                    )
                ),
                "platformio.package.exception": SimpleNamespace(
                    PackageException=mock_exception_cls
                ),
            },
        ),
        patch("time.sleep") as mock_sleep,
    ):
        runner.patch_file_downloader()

        from platformio.package.download import FileDownloader

        instance = object.__new__(FileDownloader)
        with pytest.raises(mock_exception_cls, match="502 error"):
            FileDownloader.__init__(instance, "http://example.com/file.zip")

        # Should have slept 4 times (before attempts 2-5), not on final attempt
        assert mock_sleep.call_count == 4
        mock_sleep.assert_has_calls([call(2), call(4), call(8), call(16)])


def test_patch_file_downloader_closes_session_and_response_between_retries() -> None:
    """Test patch_file_downloader closes HTTP session and response between retries."""
    mock_exception_cls = type("PackageException", (Exception,), {})
    mock_session = MagicMock()
    mock_response = MagicMock()
    call_count = 0

    def failing_init_with_session(self, *args, **kwargs):
        nonlocal call_count
        call_count += 1
        self._http_session = mock_session
        self._http_response = mock_response
        if call_count < 2:
            raise mock_exception_cls("502 error")

    with (
        patch.dict(
            "sys.modules",
            {
                "platformio": MagicMock(),
                "platformio.package": MagicMock(),
                "platformio.package.download": SimpleNamespace(
                    FileDownloader=type(
                        "FileDownloader",
                        (),
                        {"__init__": failing_init_with_session},
                    )
                ),
                "platformio.package.exception": SimpleNamespace(
                    PackageException=mock_exception_cls
                ),
            },
        ),
        patch("time.sleep"),
    ):
        runner.patch_file_downloader()

        from platformio.package.download import FileDownloader

        instance = object.__new__(FileDownloader)
        FileDownloader.__init__(instance, "http://example.com/file.zip")

        # Both response and session should have been closed between retries
        mock_response.close.assert_called_once()
        mock_session.close.assert_called_once()


def test_patch_file_downloader_retries_on_connection_error() -> None:
    """Test patch_file_downloader retries on transport-layer errors (OSError subclasses).

    ``requests.exceptions.ConnectionError`` and ``ReadTimeout`` subclass
    ``OSError`` and are raised when the connection is aborted before any HTTP
    response is parsed -- e.g. ``RemoteDisconnected`` mid-download. These must
    retry too, not just ``PackageException``.
    """
    mock_exception_cls = type("PackageException", (Exception,), {})
    call_count = 0

    def failing_init(self, *args, **kwargs):
        nonlocal call_count
        call_count += 1
        if call_count < 3:
            raise ConnectionError(
                f"Connection aborted attempt {call_count}: RemoteDisconnected"
            )

    with (
        patch.dict(
            "sys.modules",
            {
                "platformio": MagicMock(),
                "platformio.package": MagicMock(),
                "platformio.package.download": SimpleNamespace(
                    FileDownloader=type(
                        "FileDownloader", (), {"__init__": failing_init}
                    )
                ),
                "platformio.package.exception": SimpleNamespace(
                    PackageException=mock_exception_cls
                ),
            },
        ),
        patch("time.sleep") as mock_sleep,
    ):
        runner.patch_file_downloader()

        from platformio.package.download import FileDownloader

        instance = object.__new__(FileDownloader)
        FileDownloader.__init__(instance, "http://example.com/file.zip")

        assert call_count == 3
        assert mock_sleep.call_count == 2
        mock_sleep.assert_any_call(2)
        mock_sleep.assert_any_call(4)


def test_patch_file_downloader_idempotent() -> None:
    """Test patch_file_downloader does not stack wrappers when called multiple times."""
    mock_exception_cls = type("PackageException", (Exception,), {})
    call_count = 0

    def counting_init(self, *args, **kwargs):
        nonlocal call_count
        call_count += 1

    with patch.dict(
        "sys.modules",
        {
            "platformio": MagicMock(),
            "platformio.package": MagicMock(),
            "platformio.package.download": SimpleNamespace(
                FileDownloader=type("FileDownloader", (), {"__init__": counting_init})
            ),
            "platformio.package.exception": SimpleNamespace(
                PackageException=mock_exception_cls
            ),
        },
    ):
        # Patch multiple times
        runner.patch_file_downloader()
        runner.patch_file_downloader()
        runner.patch_file_downloader()

        from platformio.package.download import FileDownloader

        instance = object.__new__(FileDownloader)
        FileDownloader.__init__(instance, "http://example.com/file.zip")

        # Should only be called once, not 3 times from stacked wrappers
        assert call_count == 1


@contextmanager
def _flaky_http_server(fail_first_n: int, fail_mode: str):
    """Local HTTP server that fails the first ``fail_first_n`` requests.

    ``fail_mode="drop"`` closes the TCP connection without responding, so
    the client raises ``RemoteDisconnected`` -- the exact CI failure mode.
    ``fail_mode="502"`` returns an HTTP 502, triggering ``PackageException``.
    """
    state = {"hits": 0}

    class _Handler(BaseHTTPRequestHandler):
        def handle_one_request(self) -> None:
            state["hits"] += 1
            if state["hits"] <= fail_first_n and fail_mode == "drop":
                return  # Skip read+respond → kernel sends FIN → RemoteDisconnected
            super().handle_one_request()

        def do_GET(self) -> None:  # noqa: N802
            if state["hits"] <= fail_first_n and fail_mode == "502":
                self.send_error(502)
                return
            body = b"esphome-test-payload"
            self.send_response(200)
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, format: str, *args: object) -> None:  # noqa: A002
            pass  # silence default stderr logging

    server = ThreadingHTTPServer(("127.0.0.1", 0), _Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        yield server.server_address[1], state
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=2)


@pytest.mark.parametrize("fail_mode", ["drop", "502"])
def test_patch_file_downloader_recovers_against_real_server(
    tmp_path: Path, fail_mode: str
) -> None:
    """End-to-end: real PlatformIO ``FileDownloader`` against a local server
    that fails twice then succeeds. Exercises the real
    requests/urllib3/http.client stack for both failure modes:

    - ``drop``: TCP close mid-request → ``RemoteDisconnected`` → caught as
      ``OSError`` by the retry patch (the CI failure path).
    - ``502``: HTTP error response → ``PackageException`` (the original path).
    """
    runner.patch_file_downloader()
    from platformio.package.download import FileDownloader

    with (
        _flaky_http_server(fail_first_n=2, fail_mode=fail_mode) as (port, state),
        patch("time.sleep"),
    ):
        fd = FileDownloader(f"http://127.0.0.1:{port}/payload.bin")
        fd.set_destination(str(tmp_path / "out.bin"))
        fd.start(with_progress=False, silent=True)

    assert state["hits"] == 3  # 2 failures + 1 success
    assert (tmp_path / "out.bin").read_bytes() == b"esphome-test-payload"


def _filter_through_redirect(line: str) -> str:
    """Write a line through RedirectText with FILTER_PLATFORMIO_LINES and return what passes."""
    import io

    from esphome.util import RedirectText

    captured = io.StringIO()
    redirect = RedirectText(captured, filter_lines=runner.FILTER_PLATFORMIO_LINES)
    redirect.write(line + "\n")
    return captured.getvalue()


@pytest.mark.parametrize(
    "msg",
    [
        "Verbose mode can be enabled via `-v, --verbose` option",
        "Found 5 compatible libraries",
        "Found 123 compatible libraries",
        "Building in release mode",
        "Building in debug mode",
        "Merged 2 ELF section",
        "esptool.py v4.7.0",
        "esptool v4.8.1",
        "PLATFORM: espressif32 @ 6.4.0",
        "Using cache: /path/to/cache",
        "Package configuration completed successfully",
        "Scanning dependencies...",
        "Installing dependencies",
        "Library Manager: Already installed, built-in library",
        "Memory Usage -> https://bit.ly/pio-memory-usage",
    ],
)
def test_filter_platformio_lines_blocks_noisy_messages(msg: str) -> None:
    """Test that noisy platformio output lines are filtered out by RedirectText."""
    assert _filter_through_redirect(msg) == ""


@pytest.mark.parametrize(
    "msg",
    [
        "Compiling .pio/build/test/src/main.cpp.o",
        "Linking .pio/build/test/firmware.elf",
        "Error: something went wrong",
        "warning: unused variable",
    ],
)
def test_filter_platformio_lines_allows_other_messages(msg: str) -> None:
    """Test that non-noisy platformio output lines pass through RedirectText."""
    assert _filter_through_redirect(msg) == msg + "\n"
