"""Unit tests for esphome.components.zephyr.variants.rpi_pico_family."""

from __future__ import annotations

from pathlib import Path
from types import SimpleNamespace
from unittest.mock import MagicMock, patch

import serial

from esphome.components.zephyr.variants.rpi_pico_family import touch_1200_baud_reboot

_TARGET = "/dev/ttyACM0"


def _port(device: str, vid: int | None, pid: int | None) -> SimpleNamespace:
    return SimpleNamespace(device=device, vid=vid, pid=pid)


def _run_touch(
    ports: list[SimpleNamespace],
    realpaths: dict[str, str] | None = None,
    *,
    picotool: Path | None = Path("/usr/bin/picotool"),
    serial_error: Exception | None = None,
    port_never_leaves: bool = False,
) -> tuple[bool, MagicMock]:
    """The target drops out of get_serial_ports() after the first poll, as a rebooting
    board does, unless port_never_leaves is set."""
    realpaths = realpaths or {}
    serial_mock = MagicMock(side_effect=serial_error)
    before = [SimpleNamespace(path=p.device) for p in ports]
    after = [p for p in before if p.path != _TARGET]
    if port_never_leaves:
        get_ports = patch("esphome.util.get_serial_ports", return_value=before)
        # deadline = 0 + timeout; two polls fit, the third check is past it.
        clock = patch("time.monotonic", side_effect=[0.0, 0.2, 0.4, 2.0])
    else:
        get_ports = patch(
            "esphome.util.get_serial_ports", side_effect=[before, after, after]
        )
        clock = patch("time.monotonic", side_effect=[0.0, 0.2, 0.4])
    with (
        patch(
            "esphome.components.zephyr.variants.rpi_pico_family.find_picotool",
            return_value=picotool,
        ),
        patch("serial.tools.list_ports.comports", return_value=ports),
        patch("os.path.realpath", side_effect=lambda p: realpaths.get(p, p)),
        patch("serial.Serial", serial_mock),
        get_ports,
        clock,
        patch("time.sleep"),
    ):
        result = touch_1200_baud_reboot(_TARGET, timeout=1.0)
    return result, serial_mock


def test_touch_ignores_unrelated_usb_serial_adapters() -> None:
    result, serial_mock = _run_touch(
        [_port(_TARGET, 0x2FE3, 0x0001), _port("/dev/ttyUSB0", 0x10C4, 0xEA60)]
    )
    assert result is True
    serial_mock.assert_called_once_with(_TARGET, baudrate=1200)


def test_touch_refuses_second_device_with_same_vid_pid() -> None:
    result, serial_mock = _run_touch(
        [_port(_TARGET, 0x2FE3, 0x0001), _port("/dev/ttyACM1", 0x2FE3, 0x0001)]
    )
    assert result is False
    serial_mock.assert_not_called()


def test_touch_does_not_double_count_symlinked_port() -> None:
    by_id = "/dev/serial/by-id/usb-ZEPHYR_pico-if00"
    result, serial_mock = _run_touch(
        [_port(_TARGET, 0x2FE3, 0x0001), _port(by_id, 0x2FE3, 0x0001)],
        realpaths={by_id: _TARGET},
    )
    assert result is True
    serial_mock.assert_called_once()


def test_touch_refuses_other_port_when_target_not_listed() -> None:
    result, serial_mock = _run_touch([_port("/dev/ttyUSB0", 0x10C4, 0xEA60)])
    assert result is False
    serial_mock.assert_not_called()


def test_touch_refuses_other_port_when_target_has_no_vid() -> None:
    result, serial_mock = _run_touch(
        [_port(_TARGET, None, None), _port("/dev/ttyUSB0", 0x10C4, 0xEA60)]
    )
    assert result is False
    serial_mock.assert_not_called()


def test_touch_allows_target_alone_without_vid() -> None:
    result, serial_mock = _run_touch([_port(_TARGET, None, None)])
    assert result is True
    serial_mock.assert_called_once()


def test_touch_returns_false_without_picotool() -> None:
    result, serial_mock = _run_touch([_port(_TARGET, 0x2FE3, 0x0001)], picotool=None)
    assert result is False
    serial_mock.assert_not_called()


def test_touch_returns_false_when_port_cannot_be_opened() -> None:
    result, serial_mock = _run_touch(
        [_port(_TARGET, 0x2FE3, 0x0001)], serial_error=serial.SerialException("busy")
    )
    assert result is False
    serial_mock.assert_called_once_with(_TARGET, baudrate=1200)


def test_touch_returns_false_when_port_never_disappears() -> None:
    result, serial_mock = _run_touch(
        [_port(_TARGET, 0x2FE3, 0x0001)], port_never_leaves=True
    )
    assert result is False
    serial_mock.assert_called_once_with(_TARGET, baudrate=1200)
