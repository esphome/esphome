"""Tests for esphome.components.nrf52 upload_program and run_compile."""

import asyncio
from contextlib import ExitStack
from pathlib import Path
import re
import socket
from types import SimpleNamespace
from unittest.mock import AsyncMock, MagicMock, patch

import pytest

from esphome.address_cache import AddressCache
from esphome.components.nrf52.const import (
    BOOTLOADER_ADAFRUIT_NRF52_SD132_V2,
    BOOTLOADER_ADAFRUIT_NRF52_SD132_V6,
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V7,
    BOOTLOADER_NRF,
)
from esphome.components.zephyr.const import (
    BOOTLOADER_MCUBOOT,
    KEY_BOARD,
    KEY_BOOTLOADER,
    KEY_EXTRA_BUILD_FILES,
    KEY_KCONFIG,
    KEY_OVERLAY,
    KEY_OVERLAY_BUILDER,
    KEY_PM_STATIC,
    KEY_PRJ_CONF,
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
        KEY_OVERLAY_BUILDER: [],
        KEY_EXTRA_BUILD_FILES: {},
        KEY_PM_STATIC: [],
        KEY_KCONFIG: "",
    }


def _set_sdk(version: cv.Version, mcumgr_ota: bool = False) -> None:
    """MCUboot is built with zephyr_mcumgr OTA, which upload reads from the config."""
    CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] = version
    CORE.config = {"ota": [{"platform": "zephyr_mcumgr"}]} if mcumgr_ota else {}


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
# JLINK upload path
# ---------------------------------------------------------------------------


class TestUploadProgramJlink:
    def test_jlink_assembles_west_command(
        self, setup_core: Path, tmp_path: Path
    ) -> None:
        """West flash must use the jlink runner with the J-Link Commander that was found."""
        from esphome.components.nrf52 import upload_program

        _setup_nrf52_core(build_path=tmp_path / "build")
        CORE.config_path = tmp_path / "test.yaml"
        paths = _make_paths(tmp_path)
        commander = tmp_path / "JLink" / "JLinkExe"

        with (
            patch(
                "esphome.components.nrf52.jlink.find_jlink_commander",
                return_value=commander,
            ),
            patch("esphome.components.nrf52.jlink.check_and_install"),
            patch("esphome.components.nrf52.jlink.get_build_paths", return_value=paths),
            patch("esphome.components.nrf52.jlink.get_build_env", return_value={}),
            patch(
                "esphome.components.nrf52.jlink.run_command_ok", return_value=True
            ) as mock_run,
        ):
            assert upload_program(config={}, args=None, host="JLINK") is True

        cmd = mock_run.call_args[0][0]
        assert cmd == [
            str(paths["python_executable"]),
            "-m",
            "west",
            "flash",
            "--runner",
            "jlink",
            "-d",
            str(CORE.relative_pioenvs_path(CORE.name)),
            "--commander",
            str(commander),
            "--reset",
        ]

    def test_jlink_names_the_nrf54l15_device(
        self, setup_core: Path, tmp_path: Path
    ) -> None:
        """The board's generic cortex-m33 device cannot program the nRF54L15's RRAM."""
        from esphome.components.nrf52 import upload_program
        from esphome.components.nrf52.const import VARIANT_NRF54L15
        from esphome.components.nrf52.framework import set_nrf52_variant

        _setup_nrf52_core(build_path=tmp_path / "build")
        set_nrf52_variant(VARIANT_NRF54L15)
        CORE.config_path = tmp_path / "test.yaml"

        with (
            patch(
                "esphome.components.nrf52.jlink.find_jlink_commander",
                return_value=tmp_path / "JLinkExe",
            ),
            patch("esphome.components.nrf52.jlink.check_and_install"),
            patch(
                "esphome.components.nrf52.jlink.get_build_paths",
                return_value=_make_paths(tmp_path),
            ),
            patch("esphome.components.nrf52.jlink.get_build_env", return_value={}),
            patch(
                "esphome.components.nrf52.jlink.run_command_ok", return_value=True
            ) as mock_run,
        ):
            assert upload_program(config={}, args=None, host="JLINK") is True

        assert mock_run.call_args[0][0][-2:] == ["--device", "nRF54L15_M33"]

    def test_jlink_not_installed_raises(self, setup_core: Path, tmp_path: Path) -> None:
        """Without SEGGER's software the upload stops before running west."""
        from esphome.components.nrf52 import upload_program

        _setup_nrf52_core(build_path=tmp_path / "build")

        with (
            patch(
                "esphome.components.nrf52.jlink.find_jlink_commander", return_value=None
            ),
            patch("esphome.components.nrf52.jlink.run_command_ok") as mock_run,
            pytest.raises(EsphomeError, match="J-Link software was not found"),
        ):
            upload_program(config={}, args=None, host="JLINK")
        mock_run.assert_not_called()

    def test_jlink_platformio_raises(self, setup_core: Path, tmp_path: Path) -> None:
        from esphome.components.nrf52 import upload_program

        _setup_nrf52_core(toolchain=Toolchain.PLATFORMIO, build_path=tmp_path / "build")
        with pytest.raises(EsphomeError, match="sdk-nrf"):
            upload_program(config={}, args=None, host="JLINK")

    def test_jlink_failure_raises(self, setup_core: Path, tmp_path: Path) -> None:
        from esphome.components.nrf52 import upload_program

        _setup_nrf52_core(build_path=tmp_path / "build")
        CORE.config_path = tmp_path / "test.yaml"

        with (
            patch(
                "esphome.components.nrf52.jlink.find_jlink_commander",
                return_value=tmp_path / "JLinkExe",
            ),
            patch("esphome.components.nrf52.jlink.check_and_install"),
            patch(
                "esphome.components.nrf52.jlink.get_build_paths",
                return_value=_make_paths(tmp_path),
            ),
            patch("esphome.components.nrf52.jlink.get_build_env", return_value={}),
            patch("esphome.components.nrf52.jlink.run_command_ok", return_value=False),
            pytest.raises(EsphomeError, match="J-Link could not flash the device"),
        ):
            upload_program(config={}, args=None, host="JLINK")


# ---------------------------------------------------------------------------
# PlatformIO toolchain paths
# ---------------------------------------------------------------------------


class TestRunCompilePlatformio:
    def test_prepares_python_env_and_delegates_to_platformio(
        self, setup_core: Path, tmp_path: Path
    ) -> None:
        """The PlatformIO toolchain prepares the env, then returns False so PlatformIO builds."""
        from esphome.components.nrf52 import run_compile

        _setup_nrf52_core(toolchain=Toolchain.PLATFORMIO, build_path=tmp_path / "build")

        with patch(
            "esphome.components.nrf52.setup_platformio_python_env"
        ) as mock_setup:
            assert run_compile(args=None, config={}) is False

        mock_setup.assert_called_once_with()


class TestUploadProgramSerialPlatformio:
    def _upload(self, host: str, tmp_path: Path, run_result: int) -> tuple:
        from esphome.components.nrf52 import upload_program
        from esphome.upload_targets import PortType

        _setup_nrf52_core(toolchain=Toolchain.PLATFORMIO, build_path=tmp_path / "build")
        CORE.config_path = tmp_path / "test.yaml"

        with (
            patch("esphome.upload_targets.get_port_type", return_value=PortType.SERIAL),
            patch("esphome.__main__.check_permissions"),
            patch("esphome.components.nrf52.setup_platformio_python_env") as mock_setup,
            patch(
                "esphome.platformio.toolchain.run_platformio_cli_run",
                return_value=run_result,
            ) as mock_run,
        ):
            result = upload_program(config={}, args=None, host=host)
        return result, mock_setup, mock_run

    def test_serial_upload_prepares_env_and_runs_platformio(
        self, setup_core: Path, tmp_path: Path
    ) -> None:
        """Serial upload with the PlatformIO toolchain runs pio with -t upload."""
        host = "/dev/ttyACM0"
        result, mock_setup, mock_run = self._upload(host, tmp_path, run_result=0)

        assert result is True
        mock_setup.assert_called_once_with()
        mock_run.assert_called_once()
        run_args = mock_run.call_args[0]
        assert "-t" in run_args
        assert "upload" in run_args
        assert "--upload-port" in run_args
        assert host in run_args

    def test_serial_upload_failure_raises(
        self, setup_core: Path, tmp_path: Path
    ) -> None:
        """A non-zero PlatformIO result must raise EsphomeError."""
        with pytest.raises(EsphomeError, match="Upload failed"):
            self._upload("/dev/ttyACM0", tmp_path, run_result=1)


# ---------------------------------------------------------------------------
# Serial DFU upload path
# ---------------------------------------------------------------------------


def _prepare_serial_dfu(
    tmp_path: Path, bootloader: str = BOOTLOADER_ADAFRUIT_NRF52_SD140_V7
) -> tuple[dict, Path]:
    """Set up CORE for a serial DFU upload with a built firmware.zip; returns (paths, dfu_package)."""
    _setup_nrf52_core(bootloader=bootloader, build_path=tmp_path / "build")
    CORE.config_path = tmp_path / "test.yaml"
    dfu_package = CORE.relative_pioenvs_path(CORE.name) / "firmware.zip"
    dfu_package.parent.mkdir(parents=True, exist_ok=True)
    dfu_package.touch()
    return _make_paths(tmp_path), dfu_package


def _enter_serial_dfu_patches(
    stack: ExitStack,
    host: str,
    tmp_path: Path,
    paths: dict,
    usb_id: tuple[int, int] | None = None,
) -> MagicMock:
    """Enter all context managers needed for the serial DFU happy path.

    Returns the mock for ``run_command_ok`` so callers can inspect calls.
    comports() returns [] on the first call (port disappeared) and a list
    containing the host on every subsequent call (port reappeared). With
    ``usb_id`` set, the port is present with that vid/pid on every call, as it
    is for a bootloader that is entered by hand. Patches
    are applied directly on the real pyserial module attributes so they are
    visible to the deferred ``import serial[.tools.list_ports] as _x``
    statements inside upload_program.
    """
    import serial
    import serial.tools.list_ports

    from esphome.upload_targets import PortType

    _comports_calls = [0]

    def _comports(**_kwargs):
        _comports_calls[0] += 1
        if _comports_calls[0] == 1 and usb_id is None:
            return []  # port disappeared → disappear loop breaks
        # port back → reappear loop breaks
        vid, pid = usb_id or (None, None)
        return [MagicMock(device=host, vid=vid, pid=pid)]

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
    stack.enter_context(
        patch(
            "esphome.components.nrf52.ensure_nordic_nrfutil",
            return_value="/opt/nordic/nrfutil",
        )
    )
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

        paths, dfu_package = _prepare_serial_dfu(tmp_path)

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

    @pytest.mark.parametrize(
        ("bootloader", "single_bank"),
        [
            (BOOTLOADER_ADAFRUIT_NRF52_SD132_V6, True),
            (BOOTLOADER_ADAFRUIT_NRF52_SD132_V2, False),
        ],
    )
    def test_serial_dfu_without_native_usb_skips_the_touch(
        self, setup_core: Path, tmp_path: Path, bootloader: str, single_bank: bool
    ) -> None:
        """adafruit-nrfutil resets an nRF52832 through the USB serial chip's DTR line itself;
        only the dual bank bootloader is flashed without --singlebank."""
        import serial

        from esphome.components.nrf52 import upload_program
        from esphome.components.nrf52.const import VARIANT_NRF52832
        from esphome.components.nrf52.framework import set_nrf52_variant

        paths, dfu_package = _prepare_serial_dfu(tmp_path, bootloader)
        set_nrf52_variant(VARIANT_NRF52832)
        host = "/dev/ttyUSB0"
        with ExitStack() as stack:
            mock_run = _enter_serial_dfu_patches(stack, host, tmp_path, paths)
            assert upload_program(config={}, args=None, host=host) is True
            assert not serial.Serial.mock_calls  # pylint: disable=no-member

        cmd = mock_run.call_args[0][0]
        assert str(dfu_package) in cmd
        assert host in cmd
        assert ("--singlebank" in cmd) is single_bank

    def test_serial_upload_refuses_an_unlisted_bootloader(
        self, setup_core: Path, tmp_path: Path
    ) -> None:
        """A bootloader without a serial DFU path fails before any tool runs."""
        from esphome.components.nrf52 import upload_program

        paths, _ = _prepare_serial_dfu(tmp_path, "future")
        host = "/dev/ttyACM0"
        with ExitStack() as stack:
            mock_run = _enter_serial_dfu_patches(stack, host, tmp_path, paths)
            with pytest.raises(EsphomeError, match="not implemented for the future"):
                upload_program(config={}, args=None, host=host)
        mock_run.assert_not_called()

    def test_open_dfu_bootloader_skips_the_touch(
        self, setup_core: Path, tmp_path: Path
    ) -> None:
        """The factory Open DFU bootloader has no auto-entry, so no 1200 bps touch is sent."""
        import serial

        from esphome.components.nrf52 import OPEN_DFU_USB_ID, upload_program

        paths, dfu_package = _prepare_serial_dfu(tmp_path, BOOTLOADER_NRF)
        host = "/dev/ttyACM0"
        with ExitStack() as stack:
            mock_run = _enter_serial_dfu_patches(
                stack, host, tmp_path, paths, usb_id=OPEN_DFU_USB_ID
            )
            assert upload_program(config={}, args=None, host=host) is True
            assert not serial.Serial.mock_calls  # pylint: disable=no-member

        # Nordic's tool speaks the Secure DFU protocol of this bootloader
        cmd = mock_run.call_args[0][0]
        assert cmd[:4] == ["/opt/nordic/nrfutil", "nrf5sdk-tools", "dfu", "usb-serial"]
        assert str(dfu_package) in cmd
        assert host in cmd

    def test_open_dfu_application_port_is_touched_and_the_bootloader_port_flashed(
        self, setup_core: Path, tmp_path: Path
    ) -> None:
        """An application port gets the 1200 bps touch; the bootloader then appears as its own port."""
        import serial

        from esphome.components.nrf52 import OPEN_DFU_USB_ID, upload_program

        paths, dfu_package = _prepare_serial_dfu(tmp_path, BOOTLOADER_NRF)
        host = "/dev/ttyACM0"
        bootloader_port = "/dev/ttyACM1"
        calls = [0]

        def _comports(**_kwargs):
            calls[0] += 1
            app = MagicMock(device=host, vid=0x2FE3, pid=0x0100)
            if calls[0] == 1:
                return [app]  # still the application when the upload starts
            return [
                MagicMock(
                    device=bootloader_port,
                    vid=OPEN_DFU_USB_ID[0],
                    pid=OPEN_DFU_USB_ID[1],
                )
            ]

        with ExitStack() as stack:
            mock_run = _enter_serial_dfu_patches(
                stack, host, tmp_path, paths, usb_id=(0x2FE3, 0x0100)
            )
            stack.enter_context(
                patch.object(serial.tools.list_ports, "comports", side_effect=_comports)
            )
            assert upload_program(config={}, args=None, host=host) is True
            serial.Serial.assert_called_once_with(host, baudrate=1200, timeout=1)  # pylint: disable=no-member

        cmd = mock_run.call_args[0][0]
        assert cmd[cmd.index("-p") + 1] == bootloader_port
        assert str(dfu_package) in cmd

    def test_open_dfu_leaves_another_dongle_in_its_bootloader_alone(
        self, setup_core: Path, tmp_path: Path
    ) -> None:
        """Only a bootloader port that appears after the touch is the touched dongle's."""
        import serial

        from esphome.components.nrf52 import OPEN_DFU_USB_ID, upload_program

        paths, _ = _prepare_serial_dfu(tmp_path, BOOTLOADER_NRF)
        host = "/dev/ttyACM0"
        other = MagicMock(
            device="/dev/ttyACM2", vid=OPEN_DFU_USB_ID[0], pid=OPEN_DFU_USB_ID[1]
        )
        calls = [0]

        def _comports(**_kwargs):
            calls[0] += 1
            if calls[0] == 1:
                return [MagicMock(device=host, vid=0x2FE3, pid=0x0100), other]
            if calls[0] == 2:
                return [other]  # the touched dongle is rebooting
            return [
                other,
                MagicMock(
                    device="/dev/ttyACM1",
                    vid=OPEN_DFU_USB_ID[0],
                    pid=OPEN_DFU_USB_ID[1],
                ),
            ]

        with ExitStack() as stack:
            mock_run = _enter_serial_dfu_patches(
                stack, host, tmp_path, paths, usb_id=(0x2FE3, 0x0100)
            )
            stack.enter_context(
                patch.object(serial.tools.list_ports, "comports", side_effect=_comports)
            )
            assert upload_program(config={}, args=None, host=host) is True

        cmd = mock_run.call_args[0][0]
        assert cmd[cmd.index("-p") + 1] == "/dev/ttyACM1"

    def test_open_dfu_bootloader_that_never_appears_gets_the_reset_hint(
        self, setup_core: Path, tmp_path: Path
    ) -> None:
        """Without ``dfu:`` the touch does nothing; the hint names the RESET button."""
        from esphome.components.nrf52 import upload_program

        paths, _ = _prepare_serial_dfu(tmp_path, BOOTLOADER_NRF)
        host = "/dev/ttyACM0"
        clock = [0.0]

        def _monotonic() -> float:
            clock[0] += 1.0
            return clock[0]

        with ExitStack() as stack:
            mock_run = _enter_serial_dfu_patches(
                stack, host, tmp_path, paths, usb_id=(0x2FE3, 0x0100)
            )
            stack.enter_context(patch("time.monotonic", side_effect=_monotonic))
            with pytest.raises(EsphomeError, match="press the RESET button"):
                upload_program(config={}, args=None, host=host)
        mock_run.assert_not_called()

    def test_open_dfu_bootloader_needs_nordic_nrfutil(
        self, setup_core: Path, tmp_path: Path
    ) -> None:
        """When Nordic's nrfutil cannot be fetched the upload fails before trying adafruit-nrfutil."""
        from esphome.components.nrf52 import OPEN_DFU_USB_ID, upload_program

        paths, _ = _prepare_serial_dfu(tmp_path, BOOTLOADER_NRF)
        host = "/dev/ttyACM0"
        with ExitStack() as stack:
            mock_run = _enter_serial_dfu_patches(
                stack, host, tmp_path, paths, usb_id=OPEN_DFU_USB_ID
            )
            stack.enter_context(
                patch(
                    "esphome.components.nrf52.ensure_nordic_nrfutil",
                    side_effect=EsphomeError("Failed to download Nordic's nrfutil"),
                )
            )
            with pytest.raises(EsphomeError, match="download Nordic's nrfutil"):
                upload_program(config={}, args=None, host=host)
        mock_run.assert_not_called()

    def test_serial_dfu_failure_raises(self, setup_core: Path, tmp_path: Path) -> None:
        """A failed nordicsemi DFU must raise EsphomeError."""
        from esphome.components.nrf52 import upload_program

        paths, _ = _prepare_serial_dfu(tmp_path)

        host = "/dev/ttyACM0"
        with ExitStack() as stack:
            mock_run = _enter_serial_dfu_patches(stack, host, tmp_path, paths)
            mock_run.return_value = False
            with pytest.raises(EsphomeError, match="serial DFU upload failed"):
                upload_program(config={}, args=None, host=host)


# ---------------------------------------------------------------------------
# BLE OTA upload path
# ---------------------------------------------------------------------------

BLE_MAC = "AA:BB:CC:DD:EE:FF"
DFU_PACKAGE = object()


class TestUploadProgramBle:
    """The BLE path uses the OTA protocol the firmware was built with."""

    def _run(
        self,
        tmp_path: Path,
        host: str = BLE_MAC,
        ota_platform: str = "adafruit_ble",
    ) -> tuple[AsyncMock, AsyncMock, AsyncMock, AsyncMock]:
        from esphome.components.nrf52 import upload_program

        _setup_nrf52_core(build_path=tmp_path / "build")
        CORE.config_path = tmp_path / "test.yaml"
        # The validated config cache skips validation, so nothing is loaded
        CORE.loaded_integrations = set()

        with (
            patch(
                "esphome.components.nrf52.ota.smpmgr_scan",
                new=AsyncMock(return_value=BLE_MAC),
            ) as mock_scan,
            patch(
                "esphome.components.adafruit_ble.dfu.enter_dfu_mode",
                new=AsyncMock(return_value="AA:BB:CC:DD:EE:00"),
            ) as mock_enter,
            patch(
                "esphome.components.adafruit_ble.dfu.load_firmware",
                return_value=DFU_PACKAGE,
            ),
            patch(
                "esphome.components.adafruit_ble.dfu.dfu_upload", new=AsyncMock()
            ) as mock_dfu,
            patch(
                "esphome.components.nrf52._smpmgr_upload", new=AsyncMock()
            ) as mock_smp,
        ):
            config = {"ota": [{"platform": ota_platform}]}
            assert upload_program(config=config, args=None, host=host) is True
        return mock_scan, mock_enter, mock_dfu, mock_smp

    @pytest.mark.parametrize("host", [BLE_MAC, "BLE"])
    def test_adafruit_ble_uploads_to_bootloader(
        self, setup_core: Path, tmp_path: Path, host: str
    ) -> None:
        """The application reboots into the bootloader, which receives the package.

        The choice comes from the config, so it holds on the validated config
        cache path where loaded_integrations stays empty.
        """
        mock_scan, mock_enter, mock_dfu, mock_smp = self._run(tmp_path, host)

        assert mock_scan.await_count == (host == "BLE")
        mock_enter.assert_awaited_once_with(BLE_MAC)
        mock_dfu.assert_awaited_once_with("AA:BB:CC:DD:EE:00", DFU_PACKAGE)
        mock_smp.assert_not_awaited()

    @pytest.mark.parametrize("host", [BLE_MAC, "BLE"])
    def test_adafruit_ble_checks_package_before_reboot(
        self, setup_core: Path, tmp_path: Path, host: str
    ) -> None:
        """A missing package fails before the device is asked to enter DFU mode."""
        from esphome.components.nrf52 import upload_program

        _setup_nrf52_core(build_path=tmp_path / "build")
        CORE.config_path = tmp_path / "test.yaml"
        with (
            patch(
                "esphome.components.nrf52.ota.smpmgr_scan",
                new=AsyncMock(return_value=BLE_MAC),
            ) as mock_scan,
            patch(
                "esphome.components.adafruit_ble.dfu.enter_dfu_mode", new=AsyncMock()
            ) as mock_enter,
            pytest.raises(EsphomeError, match="DFU package not found"),
        ):
            upload_program(
                config={"ota": [{"platform": "adafruit_ble"}]}, args=None, host=host
            )
        mock_scan.assert_not_awaited()
        mock_enter.assert_not_awaited()

    @pytest.mark.parametrize("host", [BLE_MAC, "BLE"])
    def test_mcumgr_uploads_over_smp(
        self, setup_core: Path, tmp_path: Path, host: str
    ) -> None:
        """Without adafruit_ble the image goes over MCUmgr, on either bootloader."""
        mock_scan, mock_enter, mock_dfu, mock_smp = self._run(
            tmp_path, host, "zephyr_mcumgr"
        )

        assert mock_scan.await_count == (host == "BLE")
        mock_smp.assert_awaited_once_with(BLE_MAC, None)
        mock_enter.assert_not_awaited()
        mock_dfu.assert_not_awaited()


class TestUploadProgramUdp:
    """A network host goes over MCUmgr UDP only when zephyr_mcumgr enables it."""

    def _run(
        self,
        tmp_path: Path,
        host: str,
        transport: dict,
        args: object = None,
        network: dict | None = None,
    ) -> tuple[bool, AsyncMock]:
        from esphome.components.nrf52 import upload_program

        _setup_nrf52_core(build_path=tmp_path / "build")
        CORE.config_path = tmp_path / "test.yaml"
        config = {"ota": [{"platform": "zephyr_mcumgr", "transport": transport}]}
        if network is not None:
            config["network"] = network
        CORE.config = config
        with patch(
            "esphome.components.nrf52._smpmgr_upload", new=AsyncMock()
        ) as mock_smp:
            result = upload_program(config=config, args=args, host=host)
        return result, mock_smp

    @pytest.mark.parametrize(
        ("host", "network"),
        [
            ("fd00::1", None),
            ("fd00::1", {"enable_ipv4": False}),
            ("192.168.1.10", {"enable_ipv4": True}),
        ],
    )
    def test_udp_transport_uploads(
        self, setup_core: Path, tmp_path: Path, host: str, network: dict | None
    ) -> None:
        result, mock_smp = self._run(tmp_path, host, {"udp": True}, network=network)
        assert result is True
        mock_smp.assert_awaited_once_with(host, None, udp=True)

    def test_ipv4_address_without_ipv4_listener(
        self, setup_core: Path, tmp_path: Path
    ) -> None:
        with pytest.raises(EsphomeError, match="enable_ipv4: true"):
            self._run(tmp_path, "192.168.1.10", {"udp": True})

    def test_resolves_through_the_address_cache(
        self, setup_core: Path, tmp_path: Path
    ) -> None:
        CORE.address_cache = AddressCache(mdns_cache={"my-device.local": ["fd00::5"]})
        result, mock_smp = self._run(tmp_path, "my-device.local", {"udp": True})
        assert result is True
        mock_smp.assert_awaited_once_with("fd00::5", None, udp=True)

    def test_resolves_every_cached_address(
        self, setup_core: Path, tmp_path: Path
    ) -> None:
        """The CLI passes only the first cached address; the others are kept too."""
        from esphome.components.nrf52 import _resolve_smp_udp_hosts

        CORE.config = {"openthread": {"use_address": "my-device.local"}}
        CORE.address_cache = AddressCache(
            mdns_cache={"my-device.local": ["fd00::5", "fd00::6"]}
        )
        assert _resolve_smp_udp_hosts("fd00::5", False) == ["fd00::5", "fd00::6"]
        # An address the user named on its own is not widened
        assert _resolve_smp_udp_hosts("fd00::7", False) == ["fd00::7"]

    def test_uploads_the_requested_file(self, setup_core: Path, tmp_path: Path) -> None:
        args = SimpleNamespace(file="other.bin")
        result, mock_smp = self._run(tmp_path, "fd00::1", {"udp": True}, args)
        assert result is True
        mock_smp.assert_awaited_once_with("fd00::1", "other.bin", udp=True)

    def test_without_udp_transport_falls_through(
        self, setup_core: Path, tmp_path: Path
    ) -> None:
        result, mock_smp = self._run(tmp_path, "192.168.1.10", {"ble": True})
        assert result is False
        mock_smp.assert_not_awaited()


def test_resolve_keeps_every_address() -> None:
    """In resolve_ip_address order, IPv4 only when the network enables it."""
    from esphome.components.nrf52 import _resolve_smp_udp_hosts

    with patch(
        "esphome.helpers.resolve_ip_address",
        return_value=[
            (socket.AF_INET6, 0, 0, "", ("fd00::5", 1337, 0, 0)),
            (socket.AF_INET6, 0, 0, "", ("fd00::5", 1337, 0, 0)),
            (socket.AF_INET, 0, 0, "", ("192.168.1.10", 1337)),
            (socket.AF_INET6, 0, 0, "", ("fe80::1", 1337, 0, 7)),
        ],
    ):
        assert _resolve_smp_udp_hosts("my-device.local", True) == [
            "fd00::5",
            "192.168.1.10",
            "fe80::1%7",
        ]
        assert _resolve_smp_udp_hosts("my-device.local", False) == [
            "fd00::5",
            "fe80::1%7",
        ]


@pytest.mark.parametrize(
    ("outcomes", "tried", "error"),
    [
        # An address that does not answer is skipped
        (["unreachable", "ok"], 2, None),
        # A failure after the device answered is final
        (["stalled"], 1, "stalled"),
        (["unreachable", "unreachable"], 2, "no reply"),
    ],
)
def test_udp_upload_tries_the_next_address(
    setup_core: Path, outcomes: list[str], tried: int, error: str | None
) -> None:
    from esphome.components.nrf52 import _upload_over_udp
    from esphome.components.nrf52.ota_udp import SMPUnreachableError

    effects = {
        "ok": None,
        "unreachable": SMPUnreachableError("no reply"),
        "stalled": EsphomeError("Upload stalled"),
    }
    mock_smp = AsyncMock(side_effect=[effects[outcome] for outcome in outcomes])
    run = _upload_over_udp("my-device.local", ["fd00::5", "fd00::6"], None)
    with patch("esphome.components.nrf52._smpmgr_upload", new=mock_smp):
        if error is None:
            asyncio.run(run)
        else:
            with pytest.raises(EsphomeError, match=error):
                asyncio.run(run)
    assert [c.args[0] for c in mock_smp.await_args_list] == ["fd00::5", "fd00::6"][
        :tried
    ]


class TestUploadOverBle:
    def test_name_scan_miss_hints_at_bootloader_address(
        self, setup_core: Path, tmp_path: Path
    ) -> None:
        """A device stuck in its bootloader does not advertise its name."""
        from esphome.components.adafruit_ble import dfu
        from esphome.components.nrf52 import _upload_over_ble

        _setup_nrf52_core(build_path=tmp_path / "build")
        with (
            patch("esphome.components.adafruit_ble.dfu.load_firmware"),
            patch(
                "esphome.components.nrf52.ota.smpmgr_scan",
                AsyncMock(side_effect=EsphomeError("BLE device test_device not found")),
            ),
            pytest.raises(EsphomeError, match="test_device not found") as err,
        ):
            asyncio.run(
                _upload_over_ble({"ota": [{"platform": "adafruit_ble"}]}, "BLE")
            )
        assert str(err.value).endswith(dfu.BOOTLOADER_HINT)


@pytest.mark.parametrize("file", [None, "other.bin"])
def test_smpmgr_upload_sends_the_requested_file(
    setup_core: Path, tmp_path: Path, file: str | None
) -> None:
    """--file replaces the build's app_update.bin."""
    from esphome.components.nrf52 import _smpmgr_upload

    _setup_nrf52_core(build_path=tmp_path / "build")
    with patch(
        "esphome.components.nrf52.ota.smpmgr_upload", new=AsyncMock()
    ) as mock_upload:
        asyncio.run(_smpmgr_upload("fd00::1", file, udp=True))
    expected = (
        Path(file)
        if file
        else CORE.relative_pioenvs_path(CORE.name, "zephyr", "app_update.bin")
    )
    mock_upload.assert_awaited_once_with("fd00::1", expected.resolve(), True)


ITSYBITSY_PARTITIONS = """\
mcuboot_primary:
  address: 0xc000
  size: 0x61000
mcuboot_secondary:
  address: 0x6d000
  size: 0x61000
"""


SWAP_MOVE = "CONFIG_BOOT_SWAP_USING_MOVE=y\n"
MAX_SECTORS = "CONFIG_BOOT_MAX_IMG_SECTORS=128\n"


def _mcuboot_build(tmp_path: Path, image_size: int, config: str) -> Path:
    (tmp_path / "partitions.yml").write_text(ITSYBITSY_PARTITIONS)
    config_path = tmp_path / "mcuboot" / "zephyr" / ".config"
    config_path.parent.mkdir(parents=True)
    config_path.write_text(config)
    image = tmp_path / "zephyr.signed.bin"
    image.write_bytes(b"\0" * image_size)
    return image


@pytest.mark.parametrize(
    ("image_size", "config", "error"),
    [
        (389_120, SWAP_MOVE + MAX_SECTORS, None),
        (300_000, SWAP_MOVE + MAX_SECTORS, None),
        # The ItsyBitsy's 97 sector slot swaps in 95 sectors: 389,120 bytes
        (
            392_155,
            SWAP_MOVE + MAX_SECTORS,
            "392155 bytes, but MCUboot can only swap in 389120 bytes.*at least 3035",
        ),
        (392_155, "CONFIG_BOOT_UPGRADE_ONLY=y\n", None),
    ],
)
def test_mcuboot_swap_limit(
    tmp_path: Path, image_size: int, config: str, error: str | None
) -> None:
    from esphome.components.nrf52 import _check_mcuboot_swap_limit

    image = _mcuboot_build(tmp_path, image_size, config)
    if error is None:
        _check_mcuboot_swap_limit(tmp_path, image)
    else:
        with pytest.raises(EsphomeError, match=error):
            _check_mcuboot_swap_limit(tmp_path, image)


@pytest.mark.parametrize(
    ("config", "remove"),
    [
        (SWAP_MOVE + MAX_SECTORS, "partitions.yml"),
        (SWAP_MOVE + MAX_SECTORS, "zephyr.signed.bin"),
        (SWAP_MOVE, None),
    ],
)
def test_mcuboot_swap_limit_warns_when_it_cannot_check(
    tmp_path: Path,
    caplog: pytest.LogCaptureFixture,
    config: str,
    remove: str | None,
) -> None:
    from esphome.components.nrf52 import _check_mcuboot_swap_limit

    image = _mcuboot_build(tmp_path, 392_155, config)
    if remove is not None:
        (tmp_path / remove).unlink()
    _check_mcuboot_swap_limit(tmp_path, image)
    assert "Skipping the MCUboot slot size check" in caplog.text


@pytest.mark.parametrize("ota_config", [[], [{"platform": "zephyr_mcumgr"}]])
def test_mcuboot_serial_upload_on_jlink_port_flashes_over_jlink(
    setup_core: Path, tmp_path: Path, ota_config: list
) -> None:
    """A DK's serial port belongs to its J-Link, which flashes even factory firmware."""
    from esphome.components.nrf52 import upload_program
    from esphome.upload_targets import PortType

    _setup_nrf52_core(bootloader=BOOTLOADER_MCUBOOT, build_path=tmp_path / "build")
    with (
        patch("esphome.upload_targets.get_port_type", return_value=PortType.SERIAL),
        patch("esphome.__main__.check_permissions"),
        patch(
            "esphome.components.nrf52.jlink_serial_number", return_value="1051234567"
        ),
        patch("esphome.components.nrf52.can_upload_using_jlink", return_value=True),
        patch("esphome.components.nrf52.upload_using_jlink") as mock_jlink,
        patch("esphome.components.nrf52._smpmgr_upload", new=AsyncMock()) as mock_smp,
    ):
        config = {"ota": ota_config}
        assert upload_program(config=config, args=None, host="/dev/ttyACM0")
    mock_jlink.assert_called_once_with("1051234567")
    mock_smp.assert_not_awaited()


def test_mcuboot_serial_upload_on_jlink_port_without_jlink_software_uses_mcumgr(
    setup_core: Path, tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """Without the J-Link software, a DK's serial port still takes an mcumgr upload."""
    from esphome.components.nrf52 import upload_program
    from esphome.upload_targets import PortType

    _setup_nrf52_core(bootloader=BOOTLOADER_MCUBOOT, build_path=tmp_path / "build")
    with (
        patch("esphome.upload_targets.get_port_type", return_value=PortType.SERIAL),
        patch("esphome.__main__.check_permissions"),
        patch(
            "esphome.components.nrf52.jlink_serial_number", return_value="1051234567"
        ),
        patch("esphome.components.nrf52.can_upload_using_jlink", return_value=False),
        patch("esphome.components.nrf52.cmsis_dap_unique_id", return_value=None),
        patch("esphome.components.nrf52.upload_using_jlink") as mock_jlink,
        patch("esphome.components.nrf52._smpmgr_upload", new=AsyncMock()) as mock_smp,
    ):
        config = {"ota": [{"platform": "zephyr_mcumgr"}]}
        assert upload_program(config=config, args=None, host="/dev/ttyACM0")
    mock_jlink.assert_not_called()
    mock_smp.assert_awaited_once_with("/dev/ttyACM0", None)
    assert "uploading with mcumgr instead" in caplog.text


@pytest.mark.parametrize(
    ("toolchain", "commander", "expected"),
    [
        (Toolchain.SDK_NRF, Path("JLinkExe"), True),
        (Toolchain.SDK_NRF, None, False),
        (Toolchain.PLATFORMIO, Path("JLinkExe"), False),
    ],
)
def test_can_upload_using_jlink(
    setup_core: Path, toolchain: Toolchain, commander: Path | None, expected: bool
) -> None:
    from esphome.components.nrf52.jlink import can_upload_using_jlink

    _setup_nrf52_core(toolchain=toolchain)
    with patch(
        "esphome.components.nrf52.jlink.find_jlink_commander", return_value=commander
    ):
        assert can_upload_using_jlink() is expected


@pytest.mark.parametrize(
    ("vid", "serial_number", "expected", "warns"),
    [
        (0x1366, "001051234567", "1051234567", False),
        (0x1366, None, None, True),
        (0x1366, "000000000000", None, True),
        (0x2FE3, "001051234567", None, False),
    ],
)
def test_jlink_serial_number(
    vid: int,
    serial_number: str | None,
    expected: str | None,
    warns: bool,
    caplog: pytest.LogCaptureFixture,
) -> None:
    import serial.tools.list_ports

    from esphome.components.nrf52.jlink import jlink_serial_number

    port = MagicMock(device="/dev/ttyACM0", vid=vid, serial_number=serial_number)
    with patch.object(serial.tools.list_ports, "comports", return_value=[port]):
        assert jlink_serial_number("/dev/ttyACM0") == expected
        assert jlink_serial_number("/dev/ttyACM1") is None
    # Without a serial number another probe could be flashed, so this one is not used
    assert ("reports no serial number" in caplog.text) is warns


def test_jlink_passes_the_probe_serial_number(setup_core: Path, tmp_path: Path) -> None:
    """With several probes connected, the one behind the chosen port is flashed."""
    from esphome.components.nrf52.const import VARIANT_NRF54LM20
    from esphome.components.nrf52.framework import set_nrf52_variant
    from esphome.components.nrf52.jlink import upload_using_jlink

    _setup_nrf52_core(build_path=tmp_path / "build")
    set_nrf52_variant(VARIANT_NRF54LM20)
    CORE.config_path = tmp_path / "test.yaml"
    with (
        patch(
            "esphome.components.nrf52.jlink.find_jlink_commander",
            return_value=tmp_path / "JLinkExe",
        ),
        patch("esphome.components.nrf52.jlink.check_and_install"),
        patch(
            "esphome.components.nrf52.jlink.get_build_paths",
            return_value=_make_paths(tmp_path),
        ),
        patch("esphome.components.nrf52.jlink.get_build_env", return_value={}),
        patch(
            "esphome.components.nrf52.jlink.run_command_ok", return_value=True
        ) as mock_run,
    ):
        upload_using_jlink("1051234567")
    assert mock_run.call_args[0][0][-4:] == [
        "--device",
        "nRF54LM20A_M33",
        "--dev-id",
        "1051234567",
    ]


def test_mcuboot_serial_upload_on_cmsis_dap_port_flashes_with_pyocd(
    setup_core: Path, tmp_path: Path
) -> None:
    """A Seeed XIAO nRF54's serial port belongs to its CMSIS-DAP probe."""
    from esphome.components.nrf52 import upload_program
    from esphome.upload_targets import PortType

    _setup_nrf52_core(bootloader=BOOTLOADER_MCUBOOT, build_path=tmp_path / "build")
    with (
        patch("esphome.upload_targets.get_port_type", return_value=PortType.SERIAL),
        patch("esphome.__main__.check_permissions"),
        patch("esphome.components.nrf52.jlink_serial_number", return_value=None),
        patch("esphome.components.nrf52.cmsis_dap_unique_id", return_value="CB1D0165"),
        patch("esphome.components.nrf52.upload_using_cmsis_dap") as mock_dap,
        patch("esphome.components.nrf52._smpmgr_upload", new=AsyncMock()) as mock_smp,
    ):
        assert upload_program(config={"ota": []}, args=None, host="/dev/ttyACM0")
    mock_dap.assert_called_once_with("CB1D0165")
    mock_smp.assert_not_awaited()


@pytest.mark.parametrize(
    ("vid", "product", "serial_number", "expected"),
    [
        (0x2886, "Seeed Studio XIAO nRF54LM20A CMSIS-DAP", "CB1D0165", "CB1D0165"),
        (0x2886, "Seeed Studio XIAO nRF54LM20A CMSIS-DAP", None, None),
        (0x2FE3, "ESPHome", "CB1D0165", None),
    ],
)
def test_cmsis_dap_unique_id(
    vid: int,
    product: str | None,
    serial_number: str | None,
    expected: str | None,
    caplog: pytest.LogCaptureFixture,
) -> None:
    import serial.tools.list_ports

    from esphome.components.nrf52.cmsis_dap import cmsis_dap_unique_id

    port = MagicMock(
        device="/dev/ttyACM0",
        vid=vid,
        pid=0x0001,
        product=product,
        interface=None,
        serial_number=serial_number,
    )
    with patch.object(serial.tools.list_ports, "comports", return_value=[port]):
        assert cmsis_dap_unique_id("/dev/ttyACM0") == expected
        assert cmsis_dap_unique_id("/dev/ttyACM1") is None
    assert ("reports no serial number" in caplog.text) == (
        vid == 0x2886 and serial_number is None
    )


def test_cmsis_dap_flashes_mcuboot_and_signed_app(
    setup_core: Path, tmp_path: Path
) -> None:
    from esphome.components.nrf52 import cmsis_dap
    from esphome.components.nrf52.cmsis_dap import upload_using_cmsis_dap
    from esphome.components.nrf52.const import VARIANT_NRF54LM20
    from esphome.components.nrf52.framework import set_nrf52_variant

    _setup_nrf52_core(bootloader=BOOTLOADER_MCUBOOT, build_path=tmp_path / "build")
    _set_sdk(cv.Version(3, 4, 0), mcumgr_ota=True)
    set_nrf52_variant(VARIANT_NRF54LM20)
    CORE.config_path = tmp_path / "test.yaml"
    build_dir = CORE.relative_pioenvs_path(CORE.name)
    mcuboot = build_dir / "mcuboot" / "zephyr" / "zephyr.hex"
    app = build_dir / "zephyr" / "zephyr" / "zephyr.signed.hex"
    # merged.hex is left by an older SDK build, zephyr.hex is the unsigned app
    for image in (mcuboot, app, app.with_name("zephyr.hex"), build_dir / "merged.hex"):
        image.parent.mkdir(parents=True, exist_ok=True)
        image.write_text("")
    paths = _make_paths(tmp_path)
    with (
        patch("esphome.components.nrf52.cmsis_dap.check_and_install"),
        patch("esphome.components.nrf52.cmsis_dap.get_build_paths", return_value=paths),
        patch("esphome.components.nrf52.cmsis_dap.get_build_env", return_value={}),
        patch(
            "esphome.components.nrf52.cmsis_dap.run_command_ok", return_value=True
        ) as mock_run,
    ):
        upload_using_cmsis_dap("CB1D0165")
    python = str(paths["python_executable"])
    mock_run.assert_called_once()
    assert mock_run.call_args[0][0] == [
        python,
        "-m",
        "pyocd",
        "flash",
        "--no-wait",
        "-t",
        "nrf54lm20a",
        "-u",
        "CB1D0165",
        "-e",
        "sector",
        # Stops the watchdog, so it cannot reset the chip while it is halted
        "--script",
        str(Path(cmsis_dap.__file__).with_name("pyocd_nrf54l_watchdog.py")),
        str(mcuboot),
        str(app),
    ]


def test_cmsis_dap_flashes_nrf52_with_pyocd_flash(
    setup_core: Path, tmp_path: Path
) -> None:
    """nRF52 flash works with pyOCD's own flash algorithm."""
    from esphome.components.nrf52.cmsis_dap import upload_using_cmsis_dap
    from esphome.components.nrf52.const import VARIANT_NRF52840
    from esphome.components.nrf52.framework import set_nrf52_variant

    _setup_nrf52_core(bootloader=BOOTLOADER_MCUBOOT, build_path=tmp_path / "build")
    _set_sdk(cv.Version(3, 4, 0))
    set_nrf52_variant(VARIANT_NRF52840)
    CORE.config_path = tmp_path / "test.yaml"
    app = CORE.relative_pioenvs_path(CORE.name) / "zephyr" / "zephyr" / "zephyr.hex"
    app.parent.mkdir(parents=True)
    app.write_text("")
    paths = _make_paths(tmp_path)
    with (
        patch("esphome.components.nrf52.cmsis_dap.check_and_install"),
        patch("esphome.components.nrf52.cmsis_dap.get_build_paths", return_value=paths),
        patch("esphome.components.nrf52.cmsis_dap.get_build_env", return_value={}),
        patch(
            "esphome.components.nrf52.cmsis_dap.run_command_ok", return_value=True
        ) as mock_run,
    ):
        upload_using_cmsis_dap("CB1D0165")
    mock_run.assert_called_once()
    assert mock_run.call_args[0][0] == [
        str(paths["python_executable"]),
        "-m",
        "pyocd",
        "flash",
        "--no-wait",
        "-t",
        "nrf52840",
        "-u",
        "CB1D0165",
        "-e",
        "sector",
        str(app),
    ]


@pytest.mark.parametrize(
    ("version", "mcumgr_ota", "present", "error"),
    [
        # A fresh build of these versions always has merged.hex
        (cv.Version(2, 9, 2), True, ["zephyr/zephyr/zephyr.signed.hex"], "merged.hex"),
        # With MCUboot built, both images are needed, not the app alone
        (
            cv.Version(3, 4, 0),
            True,
            ["zephyr/zephyr/zephyr.hex", "zephyr/zephyr/zephyr.signed.hex"],
            "mcuboot",
        ),
        (
            cv.Version(3, 4, 0),
            True,
            ["mcuboot/zephyr/zephyr.hex", "zephyr/zephyr/zephyr.hex"],
            "zephyr.signed.hex",
        ),
        (cv.Version(3, 4, 0), False, ["merged.hex"], "zephyr.hex"),
        (cv.Version(2, 6, 1), False, ["zephyr/zephyr.hex"], "2.9.2 or newer"),
    ],
)
def test_cmsis_dap_without_firmware_raises(
    setup_core: Path,
    tmp_path: Path,
    version: cv.Version,
    mcumgr_ota: bool,
    present: list[str],
    error: str,
) -> None:
    from esphome.components.nrf52.cmsis_dap import upload_using_cmsis_dap
    from esphome.components.nrf52.const import VARIANT_NRF54LM20
    from esphome.components.nrf52.framework import set_nrf52_variant

    _setup_nrf52_core(bootloader=BOOTLOADER_MCUBOOT, build_path=tmp_path / "build")
    _set_sdk(version, mcumgr_ota)
    set_nrf52_variant(VARIANT_NRF54LM20)
    CORE.config_path = tmp_path / "test.yaml"
    build_dir = CORE.relative_pioenvs_path(CORE.name)
    for name in present:
        (build_dir / name).parent.mkdir(parents=True, exist_ok=True)
        (build_dir / name).write_text("")
    with (
        patch("esphome.components.nrf52.cmsis_dap.run_command_ok") as mock_run,
        pytest.raises(EsphomeError, match=error),
    ):
        upload_using_cmsis_dap("CB1D0165")
    mock_run.assert_not_called()


def test_cmsis_dap_flashes_merged_hex_before_sdk_3_4_0(
    setup_core: Path, tmp_path: Path
) -> None:
    """SDK 2.9.2 to 3.3.x put MCUboot and the signed app together in merged.hex."""
    from esphome.components.nrf52.cmsis_dap import upload_using_cmsis_dap
    from esphome.components.nrf52.const import VARIANT_NRF52840
    from esphome.components.nrf52.framework import set_nrf52_variant

    _setup_nrf52_core(bootloader=BOOTLOADER_MCUBOOT, build_path=tmp_path / "build")
    _set_sdk(cv.Version(3, 2, 0), mcumgr_ota=True)
    set_nrf52_variant(VARIANT_NRF52840)
    CORE.config_path = tmp_path / "test.yaml"
    build_dir = CORE.relative_pioenvs_path(CORE.name)
    merged = build_dir / "merged.hex"
    app = build_dir / "zephyr" / "zephyr" / "zephyr.signed.hex"
    for image in (merged, app):
        image.parent.mkdir(parents=True, exist_ok=True)
        image.write_text("")
    with (
        patch("esphome.components.nrf52.cmsis_dap.check_and_install"),
        patch(
            "esphome.components.nrf52.cmsis_dap.get_build_paths",
            return_value=_make_paths(tmp_path),
        ),
        patch("esphome.components.nrf52.cmsis_dap.get_build_env", return_value={}),
        patch(
            "esphome.components.nrf52.cmsis_dap.run_command_ok", return_value=True
        ) as mock_run,
    ):
        upload_using_cmsis_dap("CB1D0165")
    assert mock_run.call_args[0][0][-1:] == [str(merged)]


@pytest.mark.parametrize(
    ("results", "succeeds"),
    [([False, True], True), ([False, False], False)],
)
def test_cmsis_dap_flashes_again_after_a_failure(
    setup_core: Path, tmp_path: Path, results: list[bool], succeeds: bool
) -> None:
    """Unlocking a locked chip fails that pyOCD run; a second run reaches the chip."""
    from esphome.components.nrf52.cmsis_dap import _command_line, upload_using_cmsis_dap
    from esphome.components.nrf52.const import VARIANT_NRF52840
    from esphome.components.nrf52.framework import set_nrf52_variant

    _setup_nrf52_core(bootloader=BOOTLOADER_MCUBOOT, build_path=tmp_path / "build")
    _set_sdk(cv.Version(3, 4, 0))
    set_nrf52_variant(VARIANT_NRF52840)
    CORE.config_path = tmp_path / "test.yaml"
    app = CORE.relative_pioenvs_path(CORE.name) / "zephyr" / "zephyr" / "zephyr.hex"
    app.parent.mkdir(parents=True)
    app.write_text("")
    with (
        patch("esphome.components.nrf52.cmsis_dap.check_and_install"),
        patch(
            "esphome.components.nrf52.cmsis_dap.get_build_paths",
            return_value=_make_paths(tmp_path),
        ),
        patch("esphome.components.nrf52.cmsis_dap.get_build_env", return_value={}),
        patch(
            "esphome.components.nrf52.cmsis_dap.run_command_ok", side_effect=results
        ) as mock_run,
    ):
        if succeeds:
            upload_using_cmsis_dap("CB1D0165")
        else:
            # The hint names the SDK's pyOCD and the probe that was picked
            python = _make_paths(tmp_path)["python_executable"]
            with pytest.raises(
                EsphomeError,
                match=re.escape(
                    "\n  "
                    + _command_line(
                        [str(python), "-m", "pyocd", "erase", "-t", "nrf52840"]
                        + ["-u", "CB1D0165", "--mass"]
                    )
                    + "\n"
                ),
            ):
                upload_using_cmsis_dap("CB1D0165")
    assert mock_run.call_count == 2
    assert mock_run.call_args_list[0] == mock_run.call_args_list[1]


@pytest.mark.parametrize("patched", [True, False])
def test_cmsis_dap_failure_names_an_unpatched_nrf54lm20a_flash_algo(
    setup_core: Path, tmp_path: Path, patched: bool
) -> None:
    """With pyOCD's original algorithm the flash faults; the error says so, not 'locked'."""
    from esphome.components.nrf52.cmsis_dap import upload_using_cmsis_dap
    from esphome.components.nrf52.const import VARIANT_NRF54LM20
    from esphome.components.nrf52.framework import set_nrf52_variant

    _setup_nrf52_core(bootloader=BOOTLOADER_MCUBOOT, build_path=tmp_path / "build")
    _set_sdk(cv.Version(3, 4, 0))
    set_nrf52_variant(VARIANT_NRF54LM20)
    CORE.config_path = tmp_path / "test.yaml"
    app = CORE.relative_pioenvs_path(CORE.name) / "zephyr" / "zephyr" / "zephyr.hex"
    app.parent.mkdir(parents=True)
    app.write_text("")
    target = tmp_path / "target_nRF54LM20A.py"
    with (
        patch("esphome.components.nrf52.cmsis_dap.check_and_install"),
        patch(
            "esphome.components.nrf52.cmsis_dap.get_build_paths",
            return_value=_make_paths(tmp_path),
        ),
        patch("esphome.components.nrf52.cmsis_dap.get_build_env", return_value={}),
        patch("esphome.components.nrf52.cmsis_dap.run_command_ok", return_value=False),
        patch(
            "esphome.components.nrf52.cmsis_dap.unpatched_pyocd_nrf54lm20a_target",
            return_value=None if patched else target,
        ),
        pytest.raises(
            EsphomeError,
            match="--mass" if patched else "fixed nRF54LM20A flash algorithm",
        ),
    ):
        upload_using_cmsis_dap("CB1D0165")


@pytest.mark.parametrize(
    ("vid", "pid", "expected"), [(0x2886, 0x0068, "CB1D0165"), (0x2886, 0x0001, None)]
)
def test_cmsis_dap_unique_id_by_usb_id(
    vid: int, pid: int, expected: str | None
) -> None:
    """On Windows pySerial names no product or interface, so a known probe is found by its USB ID."""
    import serial.tools.list_ports

    from esphome.components.nrf52.cmsis_dap import cmsis_dap_unique_id

    port = MagicMock(
        device="COM5",
        vid=vid,
        pid=pid,
        product=None,
        interface=None,
        serial_number="CB1D0165",
    )
    with patch.object(serial.tools.list_ports, "comports", return_value=[port]):
        assert cmsis_dap_unique_id("COM5") == expected


def test_probe_ports_match_through_links(tmp_path: Path) -> None:
    """A /dev/serial/by-id link names the same port pySerial lists by its device node."""
    import serial.tools.list_ports

    from esphome.components.nrf52.cmsis_dap import cmsis_dap_unique_id
    from esphome.components.nrf52.jlink import jlink_serial_number

    node = tmp_path / "ttyACM0"
    node.write_text("")
    link = tmp_path / "usb-probe-if00"
    link.symlink_to(node)
    dap = MagicMock(
        device=str(node),
        vid=0x2886,
        pid=0x0068,
        product=None,
        interface=None,
        serial_number="CB1D0165",
    )
    jlink = MagicMock(device=str(node), vid=0x1366, serial_number="001051234567")
    with patch.object(serial.tools.list_ports, "comports", return_value=[dap]):
        assert cmsis_dap_unique_id(str(link)) == "CB1D0165"
    with patch.object(serial.tools.list_ports, "comports", return_value=[jlink]):
        assert jlink_serial_number(str(link)) == "1051234567"


@pytest.mark.parametrize(
    ("toolchain", "version", "expected"),
    [
        (Toolchain.SDK_NRF, cv.Version(2, 9, 2), True),
        (Toolchain.SDK_NRF, cv.Version(2, 6, 1), False),
        (Toolchain.PLATFORMIO, cv.Version(2, 9, 2), False),
    ],
)
def test_can_upload_using_cmsis_dap(
    setup_core: Path, toolchain: Toolchain, version: cv.Version, expected: bool
) -> None:
    from esphome.components.nrf52.cmsis_dap import can_upload_using_cmsis_dap

    _setup_nrf52_core(toolchain=toolchain)
    _set_sdk(version)
    assert can_upload_using_cmsis_dap() is expected


def test_mcuboot_serial_upload_on_cmsis_dap_port_without_pyocd_support_uses_mcumgr(
    setup_core: Path, tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A build pyOCD cannot flash, such as a PlatformIO one, still takes an mcumgr upload."""
    from esphome.components.nrf52 import upload_program
    from esphome.upload_targets import PortType

    _setup_nrf52_core(bootloader=BOOTLOADER_MCUBOOT, build_path=tmp_path / "build")
    with (
        patch("esphome.upload_targets.get_port_type", return_value=PortType.SERIAL),
        patch("esphome.__main__.check_permissions"),
        patch("esphome.components.nrf52.jlink_serial_number", return_value=None),
        patch("esphome.components.nrf52.cmsis_dap_unique_id", return_value="CB1D0165"),
        patch(
            "esphome.components.nrf52.can_upload_using_cmsis_dap", return_value=False
        ),
        patch("esphome.components.nrf52.upload_using_cmsis_dap") as mock_dap,
        patch("esphome.components.nrf52._smpmgr_upload", new=AsyncMock()) as mock_smp,
    ):
        config = {"ota": [{"platform": "zephyr_mcumgr"}]}
        assert upload_program(config=config, args=None, host="/dev/ttyACM0")
    mock_dap.assert_not_called()
    mock_smp.assert_awaited_once_with("/dev/ttyACM0", None)
    assert "uploading with mcumgr instead" in caplog.text


def test_mcuboot_serial_upload_of_a_given_file_skips_the_probes(
    setup_core: Path, tmp_path: Path
) -> None:
    """A probe would flash the local build, so --file goes over mcumgr."""
    from esphome.components.nrf52 import upload_program
    from esphome.upload_targets import PortType

    _setup_nrf52_core(bootloader=BOOTLOADER_MCUBOOT, build_path=tmp_path / "build")
    with (
        patch("esphome.upload_targets.get_port_type", return_value=PortType.SERIAL),
        patch("esphome.__main__.check_permissions"),
        patch(
            "esphome.components.nrf52.jlink_serial_number", return_value="1051234567"
        ) as mock_jlink_serial,
        patch(
            "esphome.components.nrf52.cmsis_dap_unique_id", return_value="CB1D0165"
        ) as mock_dap_id,
        patch("esphome.components.nrf52._smpmgr_upload", new=AsyncMock()) as mock_smp,
    ):
        config = {"ota": [{"platform": "zephyr_mcumgr"}]}
        args = SimpleNamespace(file="other.bin")
        assert upload_program(config=config, args=args, host="/dev/ttyACM0")
    mock_jlink_serial.assert_not_called()
    mock_dap_id.assert_not_called()
    mock_smp.assert_awaited_once_with("/dev/ttyACM0", "other.bin")


def test_can_upload_using_cmsis_dap_needs_a_pyocd_target(setup_core: Path) -> None:
    """Without a pyOCD target for the chip, the shortcut is skipped rather than failing."""
    from esphome.components.nrf52.cmsis_dap import can_upload_using_cmsis_dap
    from esphome.components.nrf52.const import VARIANT_NRF54LM20, VARIANT_NRF51822
    from esphome.components.nrf52.framework import set_nrf52_variant

    _setup_nrf52_core()
    _set_sdk(cv.Version(3, 4, 0))
    set_nrf52_variant(VARIANT_NRF51822)
    assert not can_upload_using_cmsis_dap()
    set_nrf52_variant(VARIANT_NRF54LM20)
    # pyOCD releases the SDK requirements still allow lack the nRF54L targets
    with (
        patch("esphome.components.nrf52.cmsis_dap.check_and_install") as mock_install,
        patch(
            "esphome.components.nrf52.cmsis_dap.pyocd_has_builtin_target",
            return_value=False,
        ) as mock_has,
    ):
        assert not can_upload_using_cmsis_dap()
    # A cleared SDK cache is reinstalled first, so it is not mistaken for a missing target
    mock_install.assert_called_once_with()
    mock_has.assert_called_once_with("target_nRF54LM20A.py")
    with (
        patch("esphome.components.nrf52.cmsis_dap.check_and_install"),
        patch(
            "esphome.components.nrf52.cmsis_dap.pyocd_has_builtin_target",
            return_value=True,
        ),
    ):
        assert can_upload_using_cmsis_dap()


@pytest.mark.parametrize(
    ("platform", "expected"),
    [
        ("win32", r'"C:\SDK env\python" -m pyocd'),
        ("linux", "'/sdk env/python' -m pyocd"),
    ],
)
def test_command_line_quotes_for_the_users_shell(platform: str, expected: str) -> None:
    from esphome.components.nrf52 import cmsis_dap

    path = r"C:\SDK env\python" if platform == "win32" else "/sdk env/python"
    with patch.object(cmsis_dap.sys, "platform", platform):
        assert cmsis_dap._command_line([path, "-m", "pyocd"]) == expected


def test_mcuboot_serial_upload_on_a_probe_without_serial_number_uses_mcumgr(
    setup_core: Path, tmp_path: Path
) -> None:
    """A probe that cannot be told apart from another one is not flashed through."""
    import serial.tools.list_ports

    from esphome.components.nrf52 import upload_program
    from esphome.upload_targets import PortType

    _setup_nrf52_core(bootloader=BOOTLOADER_MCUBOOT, build_path=tmp_path / "build")
    jlink = MagicMock(device="/dev/ttyACM0", vid=0x1366, serial_number=None)
    dap = MagicMock(
        device="/dev/ttyACM0",
        vid=0x2886,
        pid=0x0068,
        product=None,
        interface=None,
        serial_number=None,
    )
    with (
        patch("esphome.upload_targets.get_port_type", return_value=PortType.SERIAL),
        patch("esphome.__main__.check_permissions"),
        patch.object(serial.tools.list_ports, "comports", return_value=[jlink, dap]),
        patch("esphome.components.nrf52.upload_using_jlink") as mock_jlink,
        patch("esphome.components.nrf52.upload_using_cmsis_dap") as mock_dap,
        patch("esphome.components.nrf52._smpmgr_upload", new=AsyncMock()) as mock_smp,
    ):
        config = {"ota": [{"platform": "zephyr_mcumgr"}]}
        assert upload_program(config=config, args=None, host="/dev/ttyACM0")
    mock_jlink.assert_not_called()
    mock_dap.assert_not_called()
    mock_smp.assert_awaited_once_with("/dev/ttyACM0", None)


@pytest.mark.parametrize("probe", ["jlink", "cmsis_dap"])
def test_probe_upload_does_not_check_serial_port_permissions(
    setup_core: Path, tmp_path: Path, probe: str
) -> None:
    """The probe uploads never open the serial port, so its permissions do not matter."""
    from esphome.components.nrf52 import upload_program
    from esphome.upload_targets import PortType

    _setup_nrf52_core(bootloader=BOOTLOADER_MCUBOOT, build_path=tmp_path / "build")
    with (
        patch("esphome.upload_targets.get_port_type", return_value=PortType.SERIAL),
        patch("esphome.__main__.check_permissions") as mock_permissions,
        patch(
            "esphome.components.nrf52.jlink_serial_number",
            return_value="1051234567" if probe == "jlink" else None,
        ),
        patch("esphome.components.nrf52.can_upload_using_jlink", return_value=True),
        patch("esphome.components.nrf52.cmsis_dap_unique_id", return_value="CB1D0165"),
        patch("esphome.components.nrf52.can_upload_using_cmsis_dap", return_value=True),
        patch("esphome.components.nrf52.upload_using_jlink") as mock_jlink,
        patch("esphome.components.nrf52.upload_using_cmsis_dap") as mock_dap,
    ):
        assert upload_program(config={"ota": []}, args=None, host="/dev/ttyACM0")
    mock_permissions.assert_not_called()
    assert (mock_jlink if probe == "jlink" else mock_dap).call_count == 1


def test_mcumgr_serial_upload_checks_serial_port_permissions(
    setup_core: Path, tmp_path: Path
) -> None:
    from esphome.components.nrf52 import upload_program
    from esphome.upload_targets import PortType

    _setup_nrf52_core(bootloader=BOOTLOADER_MCUBOOT, build_path=tmp_path / "build")
    with (
        patch("esphome.upload_targets.get_port_type", return_value=PortType.SERIAL),
        patch("esphome.__main__.check_permissions") as mock_permissions,
        patch("esphome.components.nrf52.jlink_serial_number", return_value=None),
        patch("esphome.components.nrf52.cmsis_dap_unique_id", return_value=None),
        patch("esphome.components.nrf52._smpmgr_upload", new=AsyncMock()),
    ):
        config = {"ota": [{"platform": "zephyr_mcumgr"}]}
        assert upload_program(config=config, args=None, host="/dev/ttyACM0")
    mock_permissions.assert_called_once_with("/dev/ttyACM0")
