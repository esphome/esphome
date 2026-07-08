"""Tests for esphome.components.nrf52 upload_program and run_compile."""

from contextlib import ExitStack
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest

from esphome.components.nrf52.const import BOOTLOADER_ADAFRUIT_NRF52_SD140_V7
from esphome.components.zephyr.const import (
    KEY_BOARD,
    KEY_BOOTLOADER,
    KEY_EXTRA_BUILD_FILES,
    KEY_KCONFIG,
    KEY_OVERLAY,
    KEY_PM_STATIC,
    KEY_PRJ_CONF,
    KEY_USER,
    KEY_ZEPHYR,
)
import esphome.config_validation as cv
from esphome.const import (
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PLATFORM_NRF52,
    Toolchain,
)
from esphome.core import CORE, EsphomeError

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _setup_nrf52_core(
    bootloader: str = BOOTLOADER_ADAFRUIT_NRF52_SD140_V7,
    toolchain: Toolchain = Toolchain.SDK_NRF,
    build_path: Path | None = None,
) -> None:
    CORE.name = "test_device"
    if build_path is not None:
        CORE.build_path = build_path
    CORE.data[KEY_CORE] = {
        KEY_TARGET_PLATFORM: PLATFORM_NRF52,
        KEY_TARGET_FRAMEWORK: KEY_ZEPHYR,
        KEY_FRAMEWORK_VERSION: cv.Version(2, 9, 2),
    }
    CORE.toolchain = toolchain
    CORE.data[KEY_ZEPHYR] = {
        KEY_BOARD: "adafruit_feather_nrf52840",
        KEY_BOOTLOADER: bootloader,
        KEY_PRJ_CONF: {},
        KEY_OVERLAY: {"": ""},
        KEY_EXTRA_BUILD_FILES: {},
        KEY_PM_STATIC: [],
        KEY_USER: {},
        KEY_KCONFIG: "",
    }


def _make_paths(tmp_path: Path) -> dict:
    return {
        "python_executable": tmp_path / "penv" / "python",
        "framework_path": tmp_path / "framework",
    }


# ---------------------------------------------------------------------------
# Config-reconstruction guard
# ---------------------------------------------------------------------------


class TestUploadProgramConfigGuard:
    def test_missing_platform_config_raises(self, setup_core: Path) -> None:
        """upload_program raises EsphomeError when the platform config section is absent."""
        from esphome.components.nrf52 import upload_program

        CORE.data[KEY_CORE] = {
            KEY_TARGET_PLATFORM: PLATFORM_NRF52,
            KEY_TARGET_FRAMEWORK: KEY_ZEPHYR,
        }
        # KEY_ZEPHYR absent → reconstruction branch is entered
        assert KEY_ZEPHYR not in CORE.data

        with pytest.raises(EsphomeError, match="platform configuration"):
            upload_program(config={}, args=None, host="PYOCD")


# ---------------------------------------------------------------------------
# PYOCD upload path
# ---------------------------------------------------------------------------


class TestUploadProgramPyocd:
    def test_pyocd_assembles_west_command(
        self, setup_core: Path, tmp_path: Path
    ) -> None:
        """West flash command must include --runner pyocd and the build dir."""
        from esphome.components.nrf52 import upload_program

        _setup_nrf52_core(build_path=tmp_path / "build")
        CORE.config_path = tmp_path / "test.yaml"
        paths = _make_paths(tmp_path)
        build_dir = CORE.relative_pioenvs_path(CORE.name)

        with (
            patch("esphome.components.nrf52.check_and_install"),
            patch("esphome.components.nrf52.get_build_paths", return_value=paths),
            patch("esphome.components.nrf52.get_build_env", return_value={}),
            patch(
                "esphome.components.nrf52.run_command_ok", return_value=True
            ) as mock_run,
        ):
            result = upload_program(config={}, args=None, host="PYOCD")

        assert result is True
        mock_run.assert_called_once()
        cmd = mock_run.call_args[0][0]
        assert str(paths["python_executable"]) == cmd[0]
        assert "west" in cmd
        assert "flash" in cmd
        assert "--runner" in cmd
        assert "pyocd" in cmd
        assert "-d" in cmd
        assert str(build_dir) in cmd

    def test_pyocd_failure_raises(self, setup_core: Path, tmp_path: Path) -> None:
        """A failed west flash must raise EsphomeError."""
        from esphome.components.nrf52 import upload_program

        _setup_nrf52_core(build_path=tmp_path / "build")
        CORE.config_path = tmp_path / "test.yaml"

        with (
            patch("esphome.components.nrf52.check_and_install"),
            patch(
                "esphome.components.nrf52.get_build_paths",
                return_value=_make_paths(tmp_path),
            ),
            patch("esphome.components.nrf52.get_build_env", return_value={}),
            patch("esphome.components.nrf52.run_command_ok", return_value=False),
            pytest.raises(EsphomeError, match="pyocd"),
        ):
            upload_program(config={}, args=None, host="PYOCD")


# ---------------------------------------------------------------------------
# Serial DFU upload path
# ---------------------------------------------------------------------------


def _enter_serial_dfu_patches(
    stack: ExitStack, host: str, tmp_path: Path, paths: dict
) -> MagicMock:
    """Enter all context managers needed for the serial DFU happy path.

    Returns the mock for ``run_command_ok`` so callers can inspect calls.
    comports() returns [] on the first call (port disappeared) and a list
    containing the host on every subsequent call (port reappeared).  Patches
    are applied directly on the real pyserial module attributes so they are
    visible to the deferred ``import serial[.tools.list_ports] as _x``
    statements inside upload_program.
    """
    import serial
    import serial.tools.list_ports

    from esphome.upload_targets import PortType

    _comports_calls = [0]

    def _comports():
        _comports_calls[0] += 1
        if _comports_calls[0] == 1:
            return []  # port disappeared → disappear loop breaks
        return [MagicMock(device=host)]  # port back → reappear loop breaks

    stack.enter_context(
        patch("esphome.upload_targets.get_port_type", return_value=PortType.SERIAL)
    )
    stack.enter_context(patch("esphome.__main__.check_permissions"))
    stack.enter_context(patch("esphome.components.nrf52.check_and_install"))
    stack.enter_context(
        patch("esphome.components.nrf52.get_build_paths", return_value=paths)
    )
    stack.enter_context(
        patch("esphome.components.nrf52.get_build_env", return_value={})
    )
    stack.enter_context(patch("time.sleep"))
    # Patch directly on the real pyserial module so the deferred imports inside
    # upload_program see our mocks regardless of how sys.modules is cached.
    stack.enter_context(patch.object(serial, "Serial"))
    stack.enter_context(
        patch.object(serial.tools.list_ports, "comports", side_effect=_comports)
    )
    return stack.enter_context(
        patch("esphome.components.nrf52.run_command_ok", return_value=True)
    )


class TestUploadProgramSerialDfu:
    def test_unsupported_bootloader_raises(
        self, setup_core: Path, tmp_path: Path
    ) -> None:
        """An unknown bootloader must raise EsphomeError before touching the port."""
        from esphome.components.nrf52 import upload_program
        from esphome.upload_targets import PortType

        _setup_nrf52_core(
            bootloader="unknown_bootloader", build_path=tmp_path / "build"
        )
        CORE.config_path = tmp_path / "test.yaml"

        with (
            patch("esphome.upload_targets.get_port_type", return_value=PortType.SERIAL),
            patch("esphome.__main__.check_permissions"),
            pytest.raises(EsphomeError, match="Not implemented"),
        ):
            upload_program(config={}, args=None, host="/dev/ttyACM0")

    def test_missing_firmware_raises(self, setup_core: Path, tmp_path: Path) -> None:
        """Missing firmware.zip must raise EsphomeError before opening the serial port."""
        from esphome.components.nrf52 import upload_program
        from esphome.upload_targets import PortType

        _setup_nrf52_core(build_path=tmp_path / "build")
        CORE.config_path = tmp_path / "test.yaml"

        with (
            patch("esphome.upload_targets.get_port_type", return_value=PortType.SERIAL),
            patch("esphome.__main__.check_permissions"),
            patch("esphome.components.nrf52.check_and_install"),
            patch(
                "esphome.components.nrf52.get_build_paths",
                return_value=_make_paths(tmp_path),
            ),
            patch("esphome.components.nrf52.get_build_env", return_value={}),
            pytest.raises(EsphomeError, match="Firmware not found"),
        ):
            # firmware.zip does not exist on disk → is_file() returns False
            upload_program(config={}, args=None, host="/dev/ttyACM0")

    def test_serial_dfu_assembles_nordicsemi_command(
        self, setup_core: Path, tmp_path: Path
    ) -> None:
        """Nordicsemi DFU command must include pkg path, port, and --singlebank."""
        from esphome.components.nrf52 import upload_program

        _setup_nrf52_core(build_path=tmp_path / "build")
        CORE.config_path = tmp_path / "test.yaml"
        paths = _make_paths(tmp_path)
        build_dir = CORE.relative_pioenvs_path(CORE.name)
        dfu_package = build_dir / "firmware.zip"
        dfu_package.parent.mkdir(parents=True, exist_ok=True)
        dfu_package.touch()

        host = "/dev/ttyACM0"
        with ExitStack() as stack:
            mock_run = _enter_serial_dfu_patches(stack, host, tmp_path, paths)
            result = upload_program(config={}, args=None, host=host)

        assert result is True
        mock_run.assert_called_once()
        cmd = mock_run.call_args[0][0]
        assert "nordicsemi.__main__" in cmd
        assert "dfu" in cmd
        assert "serial" in cmd
        assert "-pkg" in cmd
        assert str(dfu_package) in cmd
        assert "-p" in cmd
        assert host in cmd
        assert "--singlebank" in cmd

    def test_serial_dfu_failure_raises(self, setup_core: Path, tmp_path: Path) -> None:
        """A failed nordicsemi DFU must raise EsphomeError."""
        from esphome.components.nrf52 import upload_program

        _setup_nrf52_core(build_path=tmp_path / "build")
        CORE.config_path = tmp_path / "test.yaml"
        paths = _make_paths(tmp_path)
        build_dir = CORE.relative_pioenvs_path(CORE.name)
        dfu_package = build_dir / "firmware.zip"
        dfu_package.parent.mkdir(parents=True, exist_ok=True)
        dfu_package.touch()

        host = "/dev/ttyACM0"
        with ExitStack() as stack:
            mock_run = _enter_serial_dfu_patches(stack, host, tmp_path, paths)
            mock_run.return_value = False
            with pytest.raises(EsphomeError, match="serial DFU upload failed"):
                upload_program(config={}, args=None, host=host)
