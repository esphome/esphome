"""Tests for the esp32_camera capture_delay option and capture triggers."""

from collections.abc import Callable
from pathlib import Path
import textwrap

import pytest

from esphome.config import load_config
from esphome.core import CORE

CAPTURE_DEFINE = "USE_ESP32_CAMERA_CAPTURE"

BASE = """
esphome:
  name: test
esp32:
  board: esp32cam
logger:
psram:
i2c:
  id: camera_bus
  sda: 26
  scl: 27
esp32_camera:
  id: cam
  name: Camera
  data_pins: [5, 18, 19, 21, 36, 39, 34, 35]
  vsync_pin: 25
  href_pin: 23
  pixel_clock_pin: 22
  external_clock:
    pin: 0
    frequency: 20MHz
  i2c_id: camera_bus
  power_down_pin: 32
"""


@pytest.fixture(name="write_config")
def fixture_write_config(tmp_path: Path) -> Callable[[str], Path]:
    """Write a camera config with extra esp32_camera options."""

    def _write(options: str) -> Path:
        path = tmp_path / "test.yaml"
        path.write_text(
            BASE + textwrap.indent(textwrap.dedent(options).strip(), "  ") + "\n"
        )
        return path

    return _write


def test_no_capture_delay(
    generate_main: Callable[[str | Path], str],
    write_config: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(write_config(""))

    assert "set_capture_delay" not in main_cpp
    assert "add_on_capture_callback" not in main_cpp
    assert CAPTURE_DEFINE not in {d.name for d in CORE.defines}


def test_capture_delay_zero(
    generate_main: Callable[[str | Path], str],
    write_config: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(write_config("capture_delay: 0s"))

    assert "set_capture_delay" not in main_cpp
    assert CAPTURE_DEFINE not in {d.name for d in CORE.defines}


def test_capture_delay_only(
    generate_main: Callable[[str | Path], str],
    write_config: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(write_config("capture_delay: 2s"))

    assert "cam->set_capture_delay(2000);" in main_cpp
    assert "add_on_capture_callback" not in main_cpp
    assert CAPTURE_DEFINE in {d.name for d in CORE.defines}


def test_capture_delay_with_triggers(
    generate_main: Callable[[str | Path], str],
    write_config: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(
        write_config(
            """
            capture_delay: 500ms
            on_capture_start:
              - logger.log: capture start
            on_capture_end:
              - logger.log: capture end
            """
        )
    )

    assert "cam->set_capture_delay(500);" in main_cpp
    assert main_cpp.count("cam->add_on_capture_callback(TriggerOnTrueForwarder") == 1
    assert main_cpp.count("cam->add_on_capture_callback(TriggerOnFalseForwarder") == 1


@pytest.mark.parametrize(
    ("trigger", "forwarder"),
    [
        ("on_capture_start", "TriggerOnTrueForwarder"),
        ("on_capture_end", "TriggerOnFalseForwarder"),
    ],
)
def test_trigger_without_delay(
    generate_main: Callable[[str | Path], str],
    write_config: Callable[[str], Path],
    trigger: str,
    forwarder: str,
) -> None:
    main_cpp = generate_main(write_config(f"{trigger}:\n  - logger.log: capture"))

    assert "set_capture_delay" not in main_cpp
    assert main_cpp.count(f"cam->add_on_capture_callback({forwarder}") == 1
    assert CAPTURE_DEFINE in {d.name for d in CORE.defines}


@pytest.mark.parametrize(
    ("delay", "message"),
    [
        ("3001ms", "value must be at most 3s"),
        ("-1s", "value must be at least 0s"),
    ],
)
def test_capture_delay_invalid(
    write_config: Callable[[str], Path], delay: str, message: str
) -> None:
    CORE.config_path = write_config(f"capture_delay: {delay}")
    errors = [str(err) for err in load_config({}).errors]

    assert errors == [
        f"{message} for dictionary value @ data['esp32_camera']['capture_delay']"
    ]
