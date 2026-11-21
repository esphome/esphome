"""Tests for the mcu_watchdog action reset configuration validation."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components.tuya import (
    CONF_MCU_WATCHDOG,
    CONF_MISSED_HEARTBEATS,
    CONF_RESET_TIME,
    CONFIG_SCHEMA,
)
from esphome.const import CONF_ACTION, CONF_HEARTBEAT
from esphome.core import CORE

from .test_init import validation_errors

RESET_CONFIG = """
esphome:
  name: tuya-mcu-watchdog
bk72xx:
  board: generic-bk7231n-qfn32-tuya
uart:
  id: uart_tuya
  tx_pin: TX1
  rx_pin: RX1
  baud_rate: 9600
tuya:
  uart_id: uart_tuya
  mcu_watchdog:
    action: reset
"""

RESET_RP2040_CONFIG = """
esphome:
  name: tuya-mcu-watchdog
rp2040:
  board: rpipicow
uart:
  id: uart_tuya
  tx_pin: GPIO0
  rx_pin: GPIO1
  baud_rate: 9600
tuya:
  uart_id: uart_tuya
  mcu_watchdog:
    action: reset
"""

RESET_USB_UART_CONFIG = """
esphome:
  name: tuya-mcu-watchdog
esp32:
  variant: esp32s3
  framework:
    type: esp-idf
usb_uart:
  - type: ch340
    channels:
      - id: uart_tuya
        baud_rate: 9600
tuya:
  uart_id: uart_tuya
  mcu_watchdog:
    action: reset
"""


def test_reset_defaults() -> None:
    watchdog = CONFIG_SCHEMA({CONF_MCU_WATCHDOG: {CONF_ACTION: "reset"}})[
        CONF_MCU_WATCHDOG
    ]

    assert watchdog[CONF_MISSED_HEARTBEATS] == 6
    assert watchdog[CONF_RESET_TIME].total_milliseconds == 500


@pytest.mark.parametrize("value", ["49ms", "5001ms"])
def test_reset_time_range(value: str) -> None:
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA(
            {CONF_MCU_WATCHDOG: {CONF_ACTION: "reset", CONF_RESET_TIME: value}}
        )


@pytest.mark.parametrize(
    "watchdog",
    [
        {CONF_RESET_TIME: "500ms"},
        {CONF_ACTION: "reboot", CONF_RESET_TIME: "500ms"},
        {CONF_ACTION: "disable", CONF_RESET_TIME: "500ms"},
    ],
)
def test_reset_time_is_rejected_with_another_action(
    watchdog: dict[str, object],
) -> None:
    with pytest.raises(cv.Invalid, match="extra keys not allowed"):
        CONFIG_SCHEMA({CONF_MCU_WATCHDOG: watchdog})


def test_reset_requires_heartbeat() -> None:
    with pytest.raises(cv.Invalid, match="mcu_watchdog requires heartbeat"):
        CONFIG_SCHEMA(
            {CONF_HEARTBEAT: False, CONF_MCU_WATCHDOG: {CONF_ACTION: "reset"}}
        )


@pytest.mark.parametrize("yaml", [RESET_CONFIG, RESET_RP2040_CONFIG])
def test_reset_accepts_uart_with_tx_pin(yaml: str, tmp_path: Path) -> None:
    assert validation_errors(yaml, tmp_path) == []


@pytest.mark.parametrize(("pin", "name"), [("tx_pin", "TX1"), ("rx_pin", "RX1")])
def test_reset_requires_both_uart_pins(pin: str, name: str, tmp_path: Path) -> None:
    yaml = RESET_CONFIG.replace(f"  {pin}: {name}\n", "")
    assert pin not in yaml
    errors = validation_errors(yaml, tmp_path)
    assert any(f"requires a UART bus with a {pin}" in err for err in errors), errors


def test_reset_rejects_inverted_uart_tx_pin(tmp_path: Path) -> None:
    yaml = RESET_CONFIG.replace(
        "  tx_pin: TX1\n", "  tx_pin:\n    number: TX1\n    inverted: true\n"
    )
    assert "    inverted: true" in yaml
    errors = validation_errors(yaml, tmp_path)
    assert any("that is not inverted" in err for err in errors), errors


RESET_RTL8710B_CONFIG = """
esphome:
  name: tuya-mcu-watchdog
rtl87xx:
  board: generic-rtl8710bn-2mb-788k
uart:
  id: uart_tuya
  tx_pin: PA23
  rx_pin: PA18
  baud_rate: 9600
tuya:
  uart_id: uart_tuya
  mcu_watchdog:
    action: reset
"""


@pytest.mark.parametrize(
    ("pin", "flag"),
    [("tx_pin", "open_drain"), ("rx_pin", "pullup"), ("rx_pin", "pulldown")],
)
def test_reset_rejects_software_serial_pin_flags(
    pin: str, flag: str, tmp_path: Path
) -> None:
    number = {"tx_pin": "PA23", "rx_pin": "PA18"}[pin]
    mode = "output" if pin == "tx_pin" else "input"
    yaml = RESET_RTL8710B_CONFIG.replace(
        f"  {pin}: {number}\n",
        f"  {pin}:\n    number: {number}\n    mode:\n      {mode}: true\n      {flag}: true\n",
    )
    assert f"      {flag}: true" in yaml
    errors = validation_errors(yaml, tmp_path)
    assert any("requires a hardware UART" in err and pin in err for err in errors), (
        errors
    )


def test_reset_rejects_software_serial_pins(tmp_path: Path) -> None:
    # no hardware UART of the board has this pair: the bus runs on SoftwareSerial
    yaml = RESET_RTL8710B_CONFIG.replace("PA23", "PA05").replace("PA18", "PA12")
    errors = validation_errors(yaml, tmp_path)
    assert any("the board has none on these pins" in err for err in errors), errors


def test_reset_accepts_rtl8710b_hardware_uart(tmp_path: Path) -> None:
    assert validation_errors(RESET_RTL8710B_CONFIG, tmp_path) == []


def test_reset_keeps_pin_flags_without_software_serial(tmp_path: Path) -> None:
    # BK72xx has no SoftwareSerial: the flags are ignored and the bus stays a hardware UART
    yaml = RESET_CONFIG.replace(
        "  rx_pin: RX1\n",
        "  rx_pin:\n    number: RX1\n    mode:\n      input: true\n      pullup: true\n",
    )
    assert "      pullup: true" in yaml
    assert validation_errors(yaml, tmp_path) == []


def test_reset_rejects_another_component_on_the_uart(tmp_path: Path) -> None:
    # it would write while TX is held low
    yaml = RESET_CONFIG + "dfplayer:\n  uart_id: uart_tuya\n"
    errors = validation_errors(yaml, tmp_path)
    assert any("requires exclusive use of that UART" in err for err in errors), errors


def test_reset_rejects_the_port_of_the_logger(tmp_path: Path) -> None:
    # TX1/RX1 is UART1: the break is refused on the port the logger writes to
    yaml = RESET_CONFIG + "logger:\n  hardware_uart: UART1\n"
    errors = validation_errors(yaml, tmp_path)
    assert any("that the logger does not write to" in err for err in errors), errors


def test_reset_rejects_the_default_port_of_the_logger(tmp_path: Path) -> None:
    # the default port of a BK72xx board with UART2 is UART2
    yaml = RESET_CONFIG.replace("TX1", "TX2").replace("RX1", "RX2") + "logger:\n"
    errors = validation_errors(yaml, tmp_path)
    assert any("that the logger does not write to" in err for err in errors), errors


@pytest.mark.parametrize(
    "logger",
    ["logger:\n", "logger:\n  hardware_uart: UART1\n  baud_rate: 0\n"],
)
def test_reset_accepts_a_logger_on_another_port_or_off(
    logger: str, tmp_path: Path
) -> None:
    assert validation_errors(RESET_CONFIG + logger, tmp_path) == []


def test_reset_warns_for_a_board_without_pin_data(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, caplog: pytest.LogCaptureFixture
) -> None:
    monkeypatch.setattr("esphome.components.libretiny.get_hardware_uarts", list)
    assert validation_errors(RESET_RTL8710B_CONFIG, tmp_path) == []
    assert "has no pin data to check the pins of uart_tuya" in caplog.text
    assert "the logger does not write to" not in caplog.text


def test_reset_warns_for_a_logger_on_a_board_without_pin_data(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, caplog: pytest.LogCaptureFixture
) -> None:
    monkeypatch.setattr("esphome.components.libretiny.get_hardware_uarts", list)
    assert validation_errors(RESET_CONFIG + "logger:\n", tmp_path) == []
    assert "of uart_tuya on board" in caplog.text
    assert "cannot be told" in caplog.text


def _with_framework(yaml: str, framework: str) -> str:
    board = "  board: generic-bk7231n-qfn32-tuya\n"
    assert board in yaml
    return yaml.replace(board, board + "  framework:\n" + framework)


def test_reset_rejects_the_default_port_set_by_an_option(tmp_path: Path) -> None:
    # codegen lets the LT_UART_DEFAULT_PORT option go over uart_port
    framework = "    uart_port: 2\n    options:\n      LT_UART_DEFAULT_PORT: 1\n"
    yaml = _with_framework(RESET_CONFIG, framework) + "logger:\n"
    errors = validation_errors(yaml, tmp_path)
    assert any("that the logger does not write to" in err for err in errors), errors


def test_reset_warns_for_a_default_port_option_that_is_an_expression(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    framework = "    options:\n      LT_UART_DEFAULT_PORT: SOME_PORT\n"
    yaml = _with_framework(RESET_CONFIG, framework) + "logger:\n"
    assert validation_errors(yaml, tmp_path) == []
    assert "cannot be told" in caplog.text


def test_reset_rejects_the_default_port_behind_a_port_the_board_lacks(
    tmp_path: Path,
) -> None:
    # the board has UART0 and UART2: a logger on UART1 writes to the default port, UART2 on PA30/PA29
    yaml = RESET_RTL8710B_CONFIG.replace("PA23", "PA30").replace("PA18", "PA29")
    yaml += "logger:\n  hardware_uart: UART1\n"
    errors = validation_errors(yaml, tmp_path)
    assert any("that the logger does not write to" in err for err in errors), errors


RESET_ESP8266_CONFIG = """
esphome:
  name: tuya-mcu-watchdog
esp8266:
  board: d1_mini
uart:
  id: uart_tuya
  tx_pin: GPIO1
  rx_pin: GPIO3
  baud_rate: 9600
tuya:
  uart_id: uart_tuya
  mcu_watchdog:
    action: reset
"""


@pytest.mark.parametrize(
    ("pins", "logger"),
    [
        (("GPIO1", "GPIO3"), "logger:\n"),
        (("GPIO15", "GPIO13"), "logger:\n  hardware_uart: UART0_SWAP\n"),
    ],
)
def test_reset_rejects_the_uart_of_the_logger_on_esp8266(
    pins: tuple[str, str], logger: str, tmp_path: Path
) -> None:
    yaml = (
        RESET_ESP8266_CONFIG.replace("GPIO1\n", pins[0] + "\n").replace(
            "GPIO3\n", pins[1] + "\n"
        )
        + logger
    )
    errors = validation_errors(yaml, tmp_path)
    assert any("that the logger does not write to" in err for err in errors), errors


@pytest.mark.parametrize(
    "logger", ["", "logger:\n  baud_rate: 0\n", "logger:\n  hardware_uart: UART1\n"]
)
def test_reset_accepts_a_logger_off_the_uart_on_esp8266(
    logger: str, tmp_path: Path
) -> None:
    assert validation_errors(RESET_ESP8266_CONFIG + logger, tmp_path) == []


def test_reset_rejects_non_native_uart(tmp_path: Path) -> None:
    errors = validation_errors(RESET_USB_UART_CONFIG, tmp_path)
    assert any("UART bus of the microcontroller itself" in err for err in errors), (
        errors
    )


def test_reset_compiles_in_the_uart_break(
    tmp_path: Path, generate_main: Callable[[str | Path], str]
) -> None:
    path = tmp_path / "tuya.yaml"
    path.write_text(RESET_CONFIG, encoding="utf-8")
    main_cpp = generate_main(path)

    assert "->set_mcu_watchdog_reset(500);" in main_cpp
    defines = {d.name for d in CORE.defines}
    assert "USE_UART_TX_BREAK" in defines
    assert "USE_TUYA_MCU_WATCHDOG_RESET" in defines
