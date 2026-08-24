"""Tests for esphome.platformio.toolchain path functions."""

# pylint: disable=protected-access

from collections.abc import Callable, Generator
from contextlib import contextmanager
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import threading
from types import SimpleNamespace
from unittest.mock import MagicMock, Mock, call, patch

import pytest

from esphome.const import KEY_CORE, KEY_TARGET_FRAMEWORK, KEY_TARGET_PLATFORM
from esphome.core import CORE, EsphomeError
from esphome.platformio import runner, toolchain
from esphome.util import ESP32_ARDUINO_ENV, FlashImage


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
    """Malformed JSON is the environment (garbage stdout), so it must
    surface as EsphomeError and get the recompile hint downstream.
    """
    config = {"name": "test"}
    mock_run_platformio_cli_run.return_value = '{"invalid": json"}'

    with pytest.raises(EsphomeError):
        toolchain._run_idedata(config)


def test_run_idedata_raises_on_launch_failure(
    setup_core: Path, mock_run_platformio_cli_run: Mock
) -> None:
    """A failed platformio launch returns its exit code as an int; that
    must surface as EsphomeError, not a TypeError from re.search.
    """
    config = {"name": "test"}
    mock_run_platformio_cli_run.return_value = 1

    with pytest.raises(EsphomeError):
        toolchain._run_idedata(config)


def test_idedata_missing_prog_path_raises_esphome_error(setup_core: Path) -> None:
    """A stale cached idedata JSON without prog_path is the build tree's
    fault; it must surface as EsphomeError, not a KeyError.
    """
    with pytest.raises(EsphomeError):
        _ = toolchain.IDEData({}).firmware_elf_path


def test_idedata_missing_flash_image_field_raises_esphome_error(
    setup_core: Path,
) -> None:
    """A cached idedata whose flash image entries lost a field must
    classify as an environment error too, not a raw KeyError.
    """
    idedata = toolchain.IDEData({"extra": {"flash_images": [{"offset": "0x1000"}]}})
    with pytest.raises(EsphomeError):
        _ = idedata.extra_flash_images


def test_idedata_null_section_raises_esphome_error(setup_core: Path) -> None:
    """A section that is null instead of absent must classify the same
    as a missing key instead of escaping as TypeError.
    """
    with pytest.raises(EsphomeError):
        _ = toolchain.IDEData({"extra": None}).extra_flash_images


@pytest.mark.parametrize(
    ("platform", "framework", "expected"),
    [
        ("esp32", "arduino", "1"),
        ("esp32", "esp-idf", None),
        ("esp8266", "arduino", None),
    ],
)
def test_run_platformio_cli_flags_an_esp32_arduino_build(
    setup_core: Path,
    mock_run_external_process: Mock,
    platform: str,
    framework: str,
    expected: str | None,
) -> None:
    """Only an ESP32 Arduino build is flagged, and an inherited one is cleared."""
    CORE.build_path = str(setup_core / "build" / "test")
    CORE.data[KEY_CORE] = {
        KEY_TARGET_PLATFORM: platform,
        KEY_TARGET_FRAMEWORK: framework,
    }

    with patch.dict(os.environ, {ESP32_ARDUINO_ENV: "1"}, clear=False):
        mock_run_external_process.return_value = 0
        toolchain.run_platformio_cli("test", "arg")

        env = mock_run_external_process.call_args[1]["env"]
        assert env.get(ESP32_ARDUINO_ENV) == expected
        # Only the subprocess env is touched; ours is left as it was.
        assert os.environ[ESP32_ARDUINO_ENV] == "1"


def test_run_platformio_cli_ignores_an_inherited_flag_without_core(
    setup_core: Path, mock_run_external_process: Mock
) -> None:
    """An inherited flag must not end up answering for CORE."""
    CORE.build_path = str(setup_core / "build" / "test")
    CORE.data.pop(KEY_CORE, None)

    with patch.dict(os.environ, {ESP32_ARDUINO_ENV: "1"}, clear=False):
        mock_run_external_process.return_value = 0
        toolchain.run_platformio_cli("test", "arg")

        env = mock_run_external_process.call_args[1]["env"]
        assert ESP32_ARDUINO_ENV not in env


def test_run_platformio_cli_raises_on_a_half_filled_core(
    setup_core: Path, mock_run_external_process: Mock
) -> None:
    """A CORE set up but left incomplete must surface, not fall back."""
    CORE.build_path = str(setup_core / "build" / "test")
    CORE.data[KEY_CORE] = {}

    with patch.dict(os.environ, {}, clear=False):
        mock_run_external_process.return_value = 0
        with pytest.raises(KeyError):
            toolchain.run_platformio_cli("test", "arg")


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
        # Caps git's upward search at the config dir so an uninitialized or
        # corrupt parent git repo can't break the framework's `git describe`.
        assert str(CORE.config_dir) in os.environ["GIT_CEILING_DIRECTORIES"].split(
            os.pathsep
        )

        # Check command was called correctly — runs PlatformIO as a subprocess
        # via the esphome.platformio.runner entry point.
        mock_run_external_process.assert_called_once()
        args = mock_run_external_process.call_args[0]
        assert "-m" in args
        assert "esphome.platformio.runner" in args
        assert "test" in args
        assert "arg" in args


def test_ccache_env_enabled_by_default(setup_core: Path) -> None:
    """Ccache is enabled when the binary is on PATH and no override is set."""
    CORE.build_path = setup_core / "build" / "test"

    with (
        patch.dict(os.environ, {}, clear=True),
        patch.object(toolchain.shutil, "which", return_value="/usr/bin/ccache"),
        patch.object(toolchain.subprocess, "run"),
    ):
        env = toolchain._ccache_env()

    assert env["ESPHOME_CCACHE_ENABLE"] == "1"
    assert env["ESPHOME_CCACHE_PATH"] == "/usr/bin/ccache"
    assert env["CCACHE_BASEDIR"] == str((setup_core / "build" / "test").resolve())
    assert env["CCACHE_DIR"].endswith("platformio-ccache")
    assert env["CCACHE_NOHASHDIR"] == "true"
    # Nothing may leak into os.environ: a later ESP-IDF build in the same
    # process would otherwise skip its own ccache defaults.
    assert "CCACHE_BASEDIR" not in os.environ
    assert "ESPHOME_CCACHE_ENABLE" not in os.environ


@pytest.mark.parametrize(
    ("env_vars", "expect_warning"),
    [
        pytest.param({}, False, id="default"),
        pytest.param({"ESPHOME_CCACHE_ENABLE": "1"}, True, id="forced-on"),
    ],
)
def test_ccache_env_disabled_without_binary(
    setup_core: Path,
    caplog: pytest.LogCaptureFixture,
    env_vars: dict[str, str],
    expect_warning: bool,
) -> None:
    """Ccache stays off when the binary is not on PATH, even when forced on.

    A deliberate opt-in that finds no binary is downgraded with a warning so
    the user can tell why it had no effect; the default path stays quiet.
    """
    CORE.build_path = setup_core / "build" / "test"

    with (
        patch.dict(os.environ, env_vars, clear=True),
        patch.object(toolchain.shutil, "which", return_value=None),
        caplog.at_level("WARNING"),
    ):
        env = toolchain._ccache_env()

    assert env == {"ESPHOME_CCACHE_ENABLE": "0"}
    assert ("no ccache binary is on PATH" in caplog.text) is expect_warning


@pytest.mark.parametrize(
    "probe_error",
    [
        pytest.param(OSError("not runnable"), id="oserror"),
        pytest.param(subprocess.CalledProcessError(1, "ccache"), id="nonzero-exit"),
        pytest.param(subprocess.TimeoutExpired("ccache", 15), id="timeout"),
    ],
)
def test_ccache_env_disabled_when_probe_fails(
    setup_core: Path, probe_error: Exception
) -> None:
    """A ccache that resolves on PATH but fails to run stays disabled."""
    CORE.build_path = setup_core / "build" / "test"

    with (
        patch.dict(os.environ, {}, clear=True),
        patch.object(toolchain.shutil, "which", return_value="/usr/bin/ccache"),
        patch.object(toolchain.subprocess, "run", side_effect=probe_error),
    ):
        env = toolchain._ccache_env()

    assert env == {"ESPHOME_CCACHE_ENABLE": "0"}


def test_ccache_env_forced_on_skips_probe(setup_core: Path) -> None:
    """An explicit ESPHOME_CCACHE_ENABLE=1 does not probe the binary."""
    CORE.build_path = setup_core / "build" / "test"

    with (
        patch.dict(os.environ, {"ESPHOME_CCACHE_ENABLE": "1"}, clear=True),
        patch.object(toolchain.shutil, "which", return_value="/usr/bin/ccache"),
        patch.object(toolchain.subprocess, "run") as mock_probe,
    ):
        env = toolchain._ccache_env()

    assert env["ESPHOME_CCACHE_ENABLE"] == "1"
    # The binary's location is still handed to the build script.
    assert env["ESPHOME_CCACHE_PATH"] == "/usr/bin/ccache"
    mock_probe.assert_not_called()


def test_ccache_env_strips_win_long_path_prefix(setup_core: Path) -> None:
    r"""A ``\\?\`` ccache path from PATH is exported without the prefix.

    That is the shape ESPHome Desktop puts on PATH (#18399); see ``_ccache_env``.
    """
    CORE.build_path = setup_core / "build" / "test"
    prefixed = (
        "\\\\?\\C:\\Users\\jesse\\AppData\\Local\\ESPHome Device Builder"
        "\\ccache\\ccache.exe"
    )
    stripped = (
        "C:\\Users\\jesse\\AppData\\Local\\ESPHome Device Builder\\ccache\\ccache.exe"
    )

    with (
        patch.dict(os.environ, {}, clear=True),
        # shutil.which is patched, so the win32 code path of the real
        # implementation (which crashes on a POSIX host) is never reached.
        patch("esphome.platformio.toolchain.sys.platform", "win32"),
        patch.object(toolchain.shutil, "which", return_value=prefixed),
        patch.object(toolchain.subprocess, "run") as mock_probe,
    ):
        env = toolchain._ccache_env()

    assert env["ESPHOME_CCACHE_ENABLE"] == "1"
    assert env["ESPHOME_CCACHE_PATH"] == stripped
    # The probe validates the exact string the build will execute.
    assert mock_probe.call_args[0][0] == [stripped, "--version"]


def test_ccache_env_opt_out(setup_core: Path) -> None:
    """ESPHOME_CCACHE_ENABLE=0 disables ccache even with the binary present."""
    CORE.build_path = setup_core / "build" / "test"

    with (
        patch.dict(os.environ, {"ESPHOME_CCACHE_ENABLE": "0"}, clear=True),
        patch.object(toolchain.shutil, "which", return_value="/usr/bin/ccache"),
    ):
        env = toolchain._ccache_env()

    assert env == {"ESPHOME_CCACHE_ENABLE": "0"}


def test_ccache_env_normalizes_enable_value(setup_core: Path) -> None:
    """A truthy override value is normalized to "1" for the build scripts."""
    CORE.build_path = setup_core / "build" / "test"

    with (
        patch.dict(os.environ, {"ESPHOME_CCACHE_ENABLE": "yes"}, clear=True),
        patch.object(toolchain.shutil, "which", return_value="/usr/bin/ccache"),
    ):
        env = toolchain._ccache_env()

    assert env["ESPHOME_CCACHE_ENABLE"] == "1"


def test_ccache_env_respects_user_values_and_refreshes_basedir(
    setup_core: Path,
) -> None:
    """User CCACHE_* values win, but CCACHE_BASEDIR follows the build dir."""
    user_env = {
        "CCACHE_DIR": "/custom/cache",
        "CCACHE_BASEDIR": "/stale/other-device",
    }
    CORE.build_path = setup_core / "build" / "test"

    with (
        patch.dict(os.environ, user_env, clear=True),
        patch.object(toolchain.shutil, "which", return_value="/usr/bin/ccache"),
        patch.object(toolchain.subprocess, "run"),
    ):
        env = toolchain._ccache_env()

    # CCACHE_DIR is not returned, so the user's os.environ value applies in
    # the subprocess; CCACHE_BASEDIR is always refreshed to the build dir.
    assert "CCACHE_DIR" not in env
    assert env["CCACHE_BASEDIR"] == str((setup_core / "build" / "test").resolve())


def test_run_platformio_cli_passes_ccache_env_to_subprocess_only(
    setup_core: Path, mock_run_external_process: Mock
) -> None:
    """The ccache settings reach the subprocess env without touching os.environ."""
    CORE.build_path = str(setup_core / "build" / "test")

    with (
        patch.dict(os.environ, {}, clear=False),
        patch.object(toolchain.shutil, "which", return_value="/usr/bin/ccache"),
        patch.object(toolchain.subprocess, "run"),
    ):
        os.environ.pop("ESPHOME_CCACHE_ENABLE", None)
        mock_run_external_process.return_value = 0
        toolchain.run_platformio_cli("test", "arg")

        env = mock_run_external_process.call_args[1]["env"]
        assert env["ESPHOME_CCACHE_ENABLE"] == "1"
        assert env["ESPHOME_CCACHE_PATH"] == "/usr/bin/ccache"
        assert env["CCACHE_BASEDIR"] == str((setup_core / "build" / "test").resolve())
        assert "ESPHOME_CCACHE_ENABLE" not in os.environ
        assert "ESPHOME_CCACHE_PATH" not in os.environ
        assert "CCACHE_BASEDIR" not in os.environ


def test_ccache_env_requires_build_path(setup_core: Path) -> None:
    """Enabling ccache without a build path fails loudly."""
    CORE.build_path = None

    with (
        patch.dict(os.environ, {}, clear=True),
        patch.object(toolchain.shutil, "which", return_value="/usr/bin/ccache"),
        patch.object(toolchain.subprocess, "run"),
        pytest.raises(ValueError, match="CORE.build_path must be set"),
    ):
        toolchain._ccache_env()


def test_run_platformio_cli_merges_caller_env(
    setup_core: Path, mock_run_external_process: Mock
) -> None:
    """A caller-supplied env is the base and gains the ccache settings."""
    CORE.build_path = str(setup_core / "build" / "test")

    with (
        patch.object(toolchain.shutil, "which", return_value="/usr/bin/ccache"),
        patch.object(toolchain.subprocess, "run"),
    ):
        mock_run_external_process.return_value = 0
        toolchain.run_platformio_cli(
            "test", env={"CUSTOM_VAR": "1", "ESPHOME_CCACHE_ENABLE": "0"}
        )

    env = mock_run_external_process.call_args[1]["env"]
    assert env["CUSTOM_VAR"] == "1"
    # The normalized enable flag still lands in the subprocess env.
    assert "ESPHOME_CCACHE_ENABLE" in env


def test_copy_ccache_script(setup_core: Path) -> None:
    """The shared ccache pre-script is copied into the build dir."""
    CORE.build_path = setup_core / "build" / "test"

    toolchain.copy_ccache_script()

    dest = setup_core / "build" / "test" / "ccache.py"
    source = Path(toolchain.__file__).parent / "ccache.py.script"
    assert dest.read_text() == source.read_text()


class _FakeSConsEnv(dict):
    """Just enough of a SCons construction environment for ccache.py."""

    def Replace(self, **kwargs: object) -> None:  # noqa: N802
        self.update(kwargs)


def _load_ccache_script(
    env_vars: dict[str, str], original_spawn: Callable[..., int] | None = None
) -> tuple[_FakeSConsEnv, Callable[..., int]]:
    """Run ccache.py.script against a fake SCons env and return (env, original SPAWN)."""
    if original_spawn is None:
        original_spawn = Mock(name="original_spawn", return_value=0)
    scons_env = _FakeSConsEnv(SPAWN=original_spawn)
    source = (Path(toolchain.__file__).parent / "ccache.py.script").read_text()
    with patch.dict(os.environ, env_vars, clear=True):
        exec(  # noqa: S102
            compile(source, "ccache.py", "exec"),
            {"Import": lambda *_names: None, "env": scons_env},
        )
    return scons_env, original_spawn


def _scons_win32_escape(x: str) -> str:
    """Copy of ``SCons.Platform.win32.escape``: quote, guarding a trailing backslash."""
    if x[-1] == "\\":
        x = x + "\\"
    return '"' + x + '"'


def test_ccache_script_wraps_compiles_with_exported_path() -> None:
    """The SCons script uses ESPHOME_CCACHE_PATH as given, without a PATH lookup."""
    ccache_path = "C:\\Users\\jesse\\ESPHome Device Builder\\ccache\\ccache.exe"
    scons_env, original_spawn = _load_ccache_script(
        {"ESPHOME_CCACHE_ENABLE": "1", "ESPHOME_CCACHE_PATH": ccache_path}
    )
    spawn = scons_env["SPAWN"]
    assert spawn is not original_spawn

    # A compile step is routed through ccache, with the same path used for
    # the program and (escaped) as the first argument.
    compile_args = ["xtensa-lx106-elf-g++", "-o", "main.o", "-c", "main.cpp"]
    spawn("cmd.exe", _scons_win32_escape, "xtensa-lx106-elf-g++", compile_args, {})
    original_spawn.assert_called_once_with(
        "cmd.exe",
        _scons_win32_escape,
        ccache_path,
        [_scons_win32_escape(ccache_path), *compile_args],
        {},
    )

    # Link steps pass through untouched.
    original_spawn.reset_mock()
    link_args = ["xtensa-lx106-elf-g++", "-o", "firmware.elf", "main.o"]
    spawn("cmd.exe", _scons_win32_escape, "xtensa-lx106-elf-g++", link_args, {})
    original_spawn.assert_called_once_with(
        "cmd.exe", _scons_win32_escape, "xtensa-lx106-elf-g++", link_args, {}
    )


@pytest.mark.parametrize(
    "env_vars",
    [
        pytest.param({"ESPHOME_CCACHE_ENABLE": "0"}, id="disabled"),
        pytest.param({"ESPHOME_CCACHE_ENABLE": "1"}, id="enabled-without-path"),
        pytest.param({}, id="unset"),
    ],
)
def test_ccache_script_leaves_spawn_alone_without_path(
    env_vars: dict[str, str],
) -> None:
    """Without both the enable flag and a path, SPAWN is not replaced."""
    scons_env, original_spawn = _load_ccache_script(env_vars)
    assert scons_env["SPAWN"] is original_spawn


def _scons_win32_spawn(
    sh: str, escape: Callable[[str], str], cmd: str, args: list[str], env: dict
) -> int:
    r"""Mirror of ``SCons.Platform.win32.spawn``: every command runs via ``cmd.exe /C``.

    SCons is not importable in the test environment (PlatformIO fetches it at
    build time), so the lines that matter are mirrored here. The command line
    SCons hands ``os.spawnve`` goes to ``CreateProcess`` via ``subprocess``
    instead (identical on Windows, where a string passes through untouched);
    ``spawnve`` itself crashes inside pytest.
    """
    return subprocess.run(
        " ".join([sh, "/C", escape(" ".join(args))]), env=env, check=False
    ).returncode


_MARKER_ENV = "ESPHOME_TEST_CCACHE_MARKER"
# Stands in for a compile: the "ccache" is really the Python interpreter, and
# the compile "flags" make it write a marker file so the test can tell whether
# the wrapped command actually ran to completion.
_FAKE_COMPILE_ARGS = [
    "-c",
    f"import os, pathlib; pathlib.Path(os.environ['{_MARKER_ENV}']).write_text('compiled')",
]


def _spawn_fake_compile_via_cmd_exe(scons_env: _FakeSConsEnv, marker: Path) -> int:
    """Run one wrapped compile step the way SCons does on Windows."""
    child_env = {**os.environ, _MARKER_ENV: str(marker)}
    return scons_env["SPAWN"](
        os.environ.get("COMSPEC", "cmd.exe"),
        _scons_win32_escape,
        "xtensa-lx106-elf-gcc",
        [_scons_win32_escape(arg) if " " in arg else arg for arg in _FAKE_COMPILE_ARGS],
        child_env,
    )


_WINDOWS_ONLY = pytest.mark.skipif(
    sys.platform != "win32", reason="drives cmd.exe, which SCons uses only on Windows"
)


@_WINDOWS_ONLY
def test_ccache_env_real_probe_runs_stripped_path(setup_core: Path) -> None:
    r"""With a ``\\?\`` which result, the real probe runs the stripped binary.

    The probe therefore validates the exact string the build will execute
    through ``cmd.exe``; probing the verbatim path instead would pass even
    when the stripped path is unusable (``CreateProcess`` accepts
    extended-length paths, ``cmd.exe`` does not).
    """
    CORE.build_path = setup_core / "build" / "test"
    assert not sys.executable.startswith("\\\\?\\")

    with (
        patch.dict(os.environ, {}, clear=False),
        patch.object(
            toolchain.shutil, "which", return_value="\\\\?\\" + sys.executable
        ),
    ):
        os.environ.pop("ESPHOME_CCACHE_ENABLE", None)
        env = toolchain._ccache_env()

    assert env["ESPHOME_CCACHE_ENABLE"] == "1"
    assert env["ESPHOME_CCACHE_PATH"] == sys.executable


@_WINDOWS_ONLY
@pytest.mark.parametrize(
    ("prefix", "expect_ok"),
    [
        pytest.param("", True, id="stripped-path-compiles"),
        pytest.param("\\\\?\\", False, id="verbatim-path-fails"),
    ],
)
def test_ccache_wrapper_through_cmd_exe(
    tmp_path: Path, prefix: str, expect_ok: bool
) -> None:
    r"""End to end through ``cmd.exe``: the exported path works, a ``\\?\`` one does not.

    The interpreter stands in for ccache; the spawn mirrors SCons on Windows.
    The failing case is the mechanism behind #18399 ("The system cannot find
    the path specified." on every compile step); should it ever start passing,
    ``cmd.exe`` learned extended-length paths and the strip is no longer needed.
    """
    marker = tmp_path / "compiled.txt"
    scons_env, _ = _load_ccache_script(
        {"ESPHOME_CCACHE_ENABLE": "1", "ESPHOME_CCACHE_PATH": prefix + sys.executable},
        original_spawn=_scons_win32_spawn,
    )
    assert scons_env["SPAWN"] is not _scons_win32_spawn

    rc = _spawn_fake_compile_via_cmd_exe(scons_env, marker)
    assert (rc == 0) is expect_ok
    assert marker.exists() is expect_ok
    if expect_ok:
        assert marker.read_text() == "compiled"


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
        # Pin ccache off: patching sys.platform to win32 (sys is a singleton,
        # so the stdlib sees it too) would send shutil.which down the Windows
        # code path, which crashes on a POSIX host.
        patch.dict(os.environ, {"ESPHOME_CCACHE_ENABLE": "0"}, clear=False),
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


def test_run_compile_without_process_limit(
    setup_core: Path, mock_run_platformio_cli_run: Mock
) -> None:
    """When no compile_process_limit is set, run_compile passes no -j flag."""
    from esphome.const import CONF_ESPHOME

    CORE.build_path = str(setup_core / "build" / "test")
    config = {CONF_ESPHOME: {}}
    mock_run_platformio_cli_run.return_value = 0

    toolchain.run_compile(config, verbose=False)

    mock_run_platformio_cli_run.assert_called_once_with(config, False)


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


# ---------------------------------------------------------------------------
# PlatformIO python-version cache heal
# ---------------------------------------------------------------------------

_CURRENT_MINOR = f"{sys.version_info.major}.{sys.version_info.minor}"
# Captured before the autouse guard patches the name, so tests can exercise the
# real implementation.
_REAL_GET_PLATFORMIO_CONFIG = toolchain.get_platformio_config


@pytest.fixture(autouse=True)
def _guard_real_platformio() -> Generator[None, None, None]:
    """Default the PlatformIO config lookup to None so no test in this module
    touches a real ~/.platformio; the heal tests re-patch it at a temp dir."""
    with patch.object(toolchain, "get_platformio_config", return_value=None):
        yield


def _pio_layout(core_dir: Path) -> dict[str, Path]:
    """Return the PlatformIO dir layout with cache/packages/platforms under core."""
    return {
        "core_dir": core_dir,
        "packages_dir": core_dir / "packages",
        "platforms_dir": core_dir / "platforms",
        "cache_dir": core_dir / ".cache",
    }


def _split_pio_layout(tmp_path: Path) -> dict[str, Path]:
    """Container-shape layout: caches on a persistent root, core_dir ephemeral."""
    persistent = tmp_path / "data" / "platformio"
    return {
        "core_dir": tmp_path / "root" / ".platformio",
        "platforms_dir": persistent / "platforms",
        "packages_dir": persistent / "packages",
        "cache_dir": persistent / "cache",
    }


def _seed_layout(layout: dict[str, Path]) -> None:
    """Populate each cache dir (and the core penv) with a marker file."""
    for key in ("platforms_dir", "packages_dir", "cache_dir"):
        layout[key].mkdir(parents=True, exist_ok=True)
        (layout[key] / "marker").write_text("x", encoding="utf-8")
    penv = layout["core_dir"] / "penv"
    penv.mkdir(parents=True, exist_ok=True)
    (penv / "marker").write_text("x", encoding="utf-8")


def _make_pio_config(layout: dict[str, Path] | Path) -> MagicMock:
    """A ProjectConfig stand-in resolving platformio dir options from *layout*."""
    resolved = _pio_layout(layout) if isinstance(layout, Path) else layout
    config = MagicMock()
    config.get.side_effect = lambda section, option: (
        str(resolved[option]) if section == "platformio" else ""
    )
    return config


@contextmanager
def _use_pio_config(layout: dict[str, Path] | Path) -> Generator[MagicMock, None, None]:
    """Point ``get_platformio_config`` at a temp layout for the block."""
    config = _make_pio_config(layout)
    with patch.object(toolchain, "get_platformio_config", return_value=config):
        yield config


def _stamp_version(core_dir: Path) -> str | None:
    """Read the python version recorded in the heal stamp under *core_dir*."""
    return toolchain._read_pio_stamp_python(core_dir / toolchain._PIO_PYTHON_STAMP_FILE)


def _cache_wiped(core_dir: Path) -> bool:
    """True when the seeded cache subdir markers are gone."""
    return not any(
        (core_dir / sub / "marker").exists()
        for sub in ("packages", "platforms", ".cache")
    )


@pytest.fixture
def pio_core_dir(tmp_path: Path) -> Path:
    """A populated PlatformIO core dir (packages/platforms/.cache/penv seeded)."""
    core = tmp_path / "dot-platformio"
    for sub in ("packages", "platforms", ".cache", "penv"):
        seeded = core / sub
        seeded.mkdir(parents=True)
        (seeded / "marker").write_text("x", encoding="utf-8")
    return core


def test_current_python_minor_matches_running_interpreter() -> None:
    """_current_python_minor returns major.minor of the running interpreter."""
    assert toolchain._current_python_minor() == _CURRENT_MINOR


def test_pio_stamp_round_trip(tmp_path: Path) -> None:
    """The stamp writer/reader round-trips and records the schema version."""
    stamp = tmp_path / toolchain._PIO_PYTHON_STAMP_FILE
    toolchain._write_pio_stamp_python(stamp, "3.13")
    assert toolchain._read_pio_stamp_python(stamp) == "3.13"
    assert json.loads(stamp.read_text()) == {
        "schema_version": toolchain._PIO_PYTHON_STAMP_SCHEMA,
        "python_version": "3.13",
    }


def test_read_pio_stamp_missing(tmp_path: Path) -> None:
    """A missing stamp file yields None."""
    assert toolchain._read_pio_stamp_python(tmp_path / "nope.json") is None


def test_read_pio_stamp_malformed(tmp_path: Path) -> None:
    """A corrupt stamp file yields None instead of raising."""
    stamp = tmp_path / "bad.json"
    stamp.write_text("{not json", encoding="utf-8")
    assert toolchain._read_pio_stamp_python(stamp) is None


def test_read_pio_stamp_unreadable_logs_warning(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A present-but-unreadable stamp yields None and warns."""
    stamp = tmp_path / "stamp.json"
    stamp.mkdir()
    with caplog.at_level("WARNING"):
        assert toolchain._read_pio_stamp_python(stamp) is None
    assert "Could not read" in caplog.text


def test_read_pio_stamp_without_python_version(tmp_path: Path) -> None:
    """A stamp missing python_version yields None."""
    stamp = tmp_path / "s.json"
    stamp.write_text(json.dumps({"schema_version": "0"}), encoding="utf-8")
    assert toolchain._read_pio_stamp_python(stamp) is None


@pytest.mark.parametrize("payload", ["42", '"x"', "[1, 2]", "null"])
def test_read_pio_stamp_non_object_json(tmp_path: Path, payload: str) -> None:
    """Valid-but-non-object JSON in the stamp yields None, not a crash."""
    stamp = tmp_path / "s.json"
    stamp.write_text(payload, encoding="utf-8")
    assert toolchain._read_pio_stamp_python(stamp) is None


def test_clean_platformio_cache_none_config_is_noop() -> None:
    """clean_platformio_cache is a no-op when PlatformIO is unavailable."""
    with patch.object(toolchain, "get_platformio_config", return_value=None):
        toolchain.clean_platformio_cache()


def test_clean_platformio_cache_wipes_everything(pio_core_dir: Path) -> None:
    """clean_platformio_cache removes cache/packages/platforms and core_dir."""
    with _use_pio_config(pio_core_dir):
        toolchain.clean_platformio_cache()
    assert not pio_core_dir.exists()


def test_heal_none_config_is_noop() -> None:
    """Heal is a no-op (no error) when PlatformIO is unavailable."""
    with patch.object(toolchain, "get_platformio_config", return_value=None):
        toolchain.heal_platformio_python_env()


def test_heal_fresh_cache_stamps_without_wipe(tmp_path: Path) -> None:
    """A fresh core dir (no stamp, no penv) is stamped, not wiped."""
    core = tmp_path / "pio"
    with _use_pio_config(core):
        toolchain.heal_platformio_python_env()
    assert _stamp_version(core) == _CURRENT_MINOR


def test_heal_stamp_matches_current_no_wipe(pio_core_dir: Path) -> None:
    """A stamp matching the running interpreter leaves the cache untouched."""
    toolchain._write_pio_stamp_python(
        pio_core_dir / toolchain._PIO_PYTHON_STAMP_FILE, _CURRENT_MINOR
    )
    with _use_pio_config(pio_core_dir):
        toolchain.heal_platformio_python_env()
    assert not _cache_wiped(pio_core_dir)
    assert (pio_core_dir / "penv" / "marker").exists()


def test_heal_stale_stamp_wipes_and_restamps(pio_core_dir: Path) -> None:
    """A stamp from an older interpreter triggers a wipe + restamp; core_dir stays."""
    toolchain._write_pio_stamp_python(
        pio_core_dir / toolchain._PIO_PYTHON_STAMP_FILE, "2.7"
    )
    with _use_pio_config(pio_core_dir):
        toolchain.heal_platformio_python_env()
    assert _cache_wiped(pio_core_dir)
    assert not (pio_core_dir / "penv").exists()
    assert pio_core_dir.is_dir()
    assert _stamp_version(pio_core_dir) == _CURRENT_MINOR


def test_heal_no_stamp_existing_cache_wipes_once(
    pio_core_dir: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """An existing cache with no stamp is cleaned once and stamped."""
    with _use_pio_config(pio_core_dir), caplog.at_level("INFO"):
        toolchain.heal_platformio_python_env()
    assert _cache_wiped(pio_core_dir)
    assert not (pio_core_dir / "penv").exists()
    assert _stamp_version(pio_core_dir) == _CURRENT_MINOR
    assert "once" in caplog.text


def test_heal_no_stamp_penv_only_counts_as_cache(tmp_path: Path) -> None:
    """A core dir holding only a penv still triggers the one-time clean."""
    core = tmp_path / "pio"
    penv = core / "penv"
    penv.mkdir(parents=True)
    (penv / "marker").write_text("x", encoding="utf-8")
    with _use_pio_config(core):
        toolchain.heal_platformio_python_env()
    assert not penv.exists()
    assert _stamp_version(core) == _CURRENT_MINOR


def test_heal_oserror_is_nonfatal(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A filesystem failure during the check warns instead of aborting the build."""
    blocker = tmp_path / "pio"
    blocker.write_text("not a directory", encoding="utf-8")
    with _use_pio_config(blocker), caplog.at_level("WARNING"):
        toolchain.heal_platformio_python_env()
    assert "build environment check failed" in caplog.text


def test_heal_stamp_write_failure_is_nonfatal(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A failed stamp write (EsphomeError from write_file) warns, not aborts."""
    with (
        _use_pio_config(tmp_path / "pio"),
        patch.object(
            toolchain,
            "_write_pio_stamp_python",
            side_effect=EsphomeError("disk full"),
        ),
        caplog.at_level("WARNING"),
    ):
        toolchain.heal_platformio_python_env()
    assert "build environment check failed" in caplog.text


def test_heal_is_idempotent_across_runs(pio_core_dir: Path) -> None:
    """After a heal writes the stamp, a re-provisioned cache is not wiped again."""
    with _use_pio_config(pio_core_dir):
        toolchain.heal_platformio_python_env()
        repop = pio_core_dir / "packages"
        repop.mkdir(exist_ok=True)
        (repop / "marker").write_text("x", encoding="utf-8")
        toolchain.heal_platformio_python_env()
    assert (pio_core_dir / "packages" / "marker").exists()


def test_pio_stamp_dir_is_platforms_parent(tmp_path: Path) -> None:
    """The stamp home is the parent of platforms_dir, not core_dir."""
    layout = _split_pio_layout(tmp_path)
    config = _make_pio_config(layout)
    assert toolchain._pio_stamp_dir(config) == layout["platforms_dir"].parent
    nested = _make_pio_config(tmp_path / "pio")
    assert toolchain._pio_stamp_dir(nested) == tmp_path / "pio"


def test_heal_container_layout_stamps_persistent_root(tmp_path: Path) -> None:
    """Container shape: the stamp lands on the persistent cache root."""
    layout = _split_pio_layout(tmp_path)
    with _use_pio_config(layout):
        toolchain.heal_platformio_python_env()
    persistent = layout["platforms_dir"].parent
    assert _stamp_version(persistent) == _CURRENT_MINOR
    assert not (layout["core_dir"] / toolchain._PIO_PYTHON_STAMP_FILE).exists()


def test_heal_container_layout_stale_stamp_wipes_persistent_cache(
    tmp_path: Path,
) -> None:
    """Container shape: a stale stamp wipes the relocated persistent caches."""
    layout = _split_pio_layout(tmp_path)
    _seed_layout(layout)
    persistent = layout["platforms_dir"].parent
    toolchain._write_pio_stamp_python(
        persistent / toolchain._PIO_PYTHON_STAMP_FILE, "2.7"
    )
    with _use_pio_config(layout):
        toolchain.heal_platformio_python_env()
    for key in ("platforms_dir", "packages_dir", "cache_dir"):
        assert not layout[key].exists()
    assert not (layout["core_dir"] / "penv").exists()
    assert _stamp_version(persistent) == _CURRENT_MINOR


def test_heal_container_layout_survives_core_dir_wipe(tmp_path: Path) -> None:
    """A python change is still detected after an image update wiped core_dir."""
    layout = _split_pio_layout(tmp_path)
    _seed_layout(layout)
    shutil.rmtree(layout["core_dir"])
    persistent = layout["platforms_dir"].parent
    toolchain._write_pio_stamp_python(
        persistent / toolchain._PIO_PYTHON_STAMP_FILE, "2.7"
    )
    with _use_pio_config(layout):
        toolchain.heal_platformio_python_env()
    for key in ("platforms_dir", "packages_dir", "cache_dir"):
        assert not layout[key].exists()
    assert _stamp_version(persistent) == _CURRENT_MINOR


def test_get_platformio_config_returns_project_config() -> None:
    """The real lookup returns a usable ProjectConfig when PlatformIO is present."""
    config = _REAL_GET_PLATFORMIO_CONFIG()
    assert config is not None
    assert hasattr(config, "get")


def test_get_platformio_config_none_when_platformio_absent() -> None:
    """The lookup returns None when PlatformIO cannot be imported."""
    with patch.dict(sys.modules, {"platformio.project.config": None}):
        assert _REAL_GET_PLATFORMIO_CONFIG() is None


def test_delete_platformio_dirs_skips_missing(tmp_path: Path) -> None:
    """A named dir that does not exist is skipped without error."""
    (tmp_path / "packages").mkdir()
    (tmp_path / "packages" / "marker").write_text("x", encoding="utf-8")
    config = _make_pio_config(tmp_path)
    # platforms_dir does not exist; packages_dir does.
    toolchain._delete_platformio_dirs(config, ["packages_dir", "platforms_dir"])
    assert not (tmp_path / "packages").exists()


def test_heal_stale_stamp_wipes_when_penv_absent(pio_core_dir: Path) -> None:
    """The penv wipe is skipped cleanly when no penv exists."""
    shutil.rmtree(pio_core_dir / "penv")
    toolchain._write_pio_stamp_python(
        pio_core_dir / toolchain._PIO_PYTHON_STAMP_FILE, "2.7"
    )
    with _use_pio_config(pio_core_dir):
        toolchain.heal_platformio_python_env()
    assert _cache_wiped(pio_core_dir)
    assert _stamp_version(pio_core_dir) == _CURRENT_MINOR


def test_run_platformio_cli_invokes_heal(
    setup_core: Path, mock_run_external_process: Mock
) -> None:
    """run_platformio_cli runs the heal before spawning PlatformIO."""
    CORE.build_path = str(setup_core / "build" / "test")
    mock_run_external_process.return_value = 0
    with patch.object(toolchain, "heal_platformio_python_env") as mock_heal:
        toolchain.run_platformio_cli("test")
    mock_heal.assert_called_once()


def test_ccache_probe_spawns_with_close_fds_false() -> None:
    """The probe follows the repo-wide posix_spawn convention."""
    with patch("subprocess.run") as mock_run:
        assert toolchain._ccache_runs("/usr/bin/ccache") is True
    assert mock_run.call_args.kwargs["close_fds"] is False
