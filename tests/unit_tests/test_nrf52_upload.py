"""Tests for esphome.components.nrf52 upload_program and run_compile."""

import asyncio
from contextlib import ExitStack
from pathlib import Path
import socket
from unittest.mock import AsyncMock, MagicMock, patch

import pytest

from esphome.address_cache import AddressCache
from esphome.components.nrf52.const import (
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V7,
    BOOTLOADER_NRF,
)
from esphome.components.zephyr.const import (
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
        mock_smp.assert_awaited_once_with(BLE_MAC)
        mock_enter.assert_not_awaited()
        mock_dfu.assert_not_awaited()


class TestUploadProgramUdp:
    """A network host goes over MCUmgr UDP only when zephyr_mcumgr enables it."""

    def _run(
        self, tmp_path: Path, host: str, transport: dict
    ) -> tuple[bool, AsyncMock]:
        from esphome.components.nrf52 import upload_program

        _setup_nrf52_core(build_path=tmp_path / "build")
        CORE.config_path = tmp_path / "test.yaml"
        config = {"ota": [{"platform": "zephyr_mcumgr", "transport": transport}]}
        with patch(
            "esphome.components.nrf52._smpmgr_upload", new=AsyncMock()
        ) as mock_smp:
            result = upload_program(config=config, args=None, host=host)
        return result, mock_smp

    @pytest.mark.parametrize("host", ["192.168.1.10", "fd00::1"])
    def test_udp_transport_uploads(
        self, setup_core: Path, tmp_path: Path, host: str
    ) -> None:
        result, mock_smp = self._run(tmp_path, host, {"udp": True})
        assert result is True
        mock_smp.assert_awaited_once_with(host, udp=True)

    def test_resolves_through_the_address_cache(
        self, setup_core: Path, tmp_path: Path
    ) -> None:
        CORE.address_cache = AddressCache(mdns_cache={"my-device.local": ["fd00::5"]})
        result, mock_smp = self._run(tmp_path, "my-device.local", {"udp": True})
        assert result is True
        mock_smp.assert_awaited_once_with("fd00::5", udp=True)

    def test_without_udp_transport_falls_through(
        self, setup_core: Path, tmp_path: Path
    ) -> None:
        result, mock_smp = self._run(tmp_path, "192.168.1.10", {"ble": True})
        assert result is False
        mock_smp.assert_not_awaited()


def test_resolve_keeps_the_ipv6_scope() -> None:
    from esphome.components.nrf52 import _resolve_smp_udp_host

    with patch(
        "esphome.helpers.resolve_ip_address",
        return_value=[(socket.AF_INET6, 0, 0, "", ("fe80::1", 1337, 0, 7))],
    ):
        assert _resolve_smp_udp_host("my-device.local") == "fe80::1%7"


def test_resolve_prefers_ipv6() -> None:
    """The device listens on IPv6, and on IPv4 only when the network enables it."""
    from esphome.components.nrf52 import _resolve_smp_udp_host

    with patch(
        "esphome.helpers.resolve_ip_address",
        return_value=[
            (socket.AF_INET, 0, 0, "", ("192.168.1.10", 1337)),
            (socket.AF_INET6, 0, 0, "", ("fd00::5", 1337, 0, 0)),
        ],
    ):
        assert _resolve_smp_udp_host("my-device.local") == "fd00::5"


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
