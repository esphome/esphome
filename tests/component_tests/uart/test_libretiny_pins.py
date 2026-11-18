"""Tests for picking a LibreTiny hardware UART from the board's pin lists."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config, yaml_util
from esphome.core import CORE

CONFIG = """
esphome:
  name: libretiny-uart-pins
rtl87xx:
  board: {board}
{framework}
uart:
  baud_rate: 9600
{pins}
"""


def test_port_with_pin_choices_is_routed(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The port is picked in codegen, and only a port with pin choices gets its pins passed."""
    main_cpp = generate_main(component_config_path("libretiny_pin_choices.yaml"))

    assert "routed_uart->set_hardware_port(1);" in main_cpp
    assert "default_uart->set_hardware_port(0);" in main_cpp
    assert any(
        define.name == "USE_UART_LIBRETINY_PIN_ROUTING" for define in CORE.defines
    )


@pytest.mark.parametrize(
    ("board", "framework", "pins", "error"),
    [
        pytest.param(
            "bw15", "", "  tx_pin: PA03\n  rx_pin: PA02", None, id="second_pair"
        ),
        pytest.param(
            "bw15", "", "  tx_pin: PA01\n  rx_pin: PA02", None, id="mixed_pair"
        ),
        pytest.param("bw15", "", "  tx_pin: PA14", None, id="default_pins_tx_only"),
        pytest.param(
            "bw15",
            "",
            "  tx_pin: PA03",
            "UART1 of board bw15 has no default pins",
            id="pin_choices_tx_only",
        ),
        pytest.param(
            "bw15",
            "",
            "  tx_pin: PA17\n  rx_pin: PA18",
            "UART1: TX=1/3, RX=0/2",
            id="no_uart_on_pins",
        ),
        pytest.param(
            "bw15",
            "  framework:\n    version: 1.11.0",
            "  tx_pin: PA03\n  rx_pin: PA02",
            "needs LibreTiny 1.12.0 or later",
            id="old_libretiny",
        ),
        # RTL8710B falls back to SoftwareSerial
        pytest.param(
            "generic-rtl8710bn-2mb-788k",
            "",
            "  tx_pin: PA05\n  rx_pin: PA12",
            None,
            id="software_serial",
        ),
    ],
)
def test_pins_are_checked_against_the_board(
    board: str, framework: str, pins: str, error: str | None, tmp_path: Path
) -> None:
    path = tmp_path / "uart.yaml"
    path.write_text(
        CONFIG.format(board=board, framework=framework, pins=pins), encoding="utf-8"
    )
    CORE.config_path = path
    result = config.validate_config(yaml_util.load_yaml(path), {})
    errors = [str(err) for err in result.errors]
    if error is None:
        assert errors == []
    else:
        assert any(error in err for err in errors), errors


def test_hardware_uarts_of_a_board_with_pin_choices(tmp_path: Path) -> None:
    """The libretiny component reports each UART's pins and whether they are choices."""
    from esphome.components.libretiny import HardwareUart, get_hardware_uarts

    path = tmp_path / "uart.yaml"
    path.write_text(
        CONFIG.format(board="bw15", framework="", pins="  tx_pin: PA14"),
        encoding="utf-8",
    )
    CORE.config_path = path
    config.validate_config(yaml_util.load_yaml(path), {})
    assert get_hardware_uarts() == [
        HardwareUart(0, (14,), (13,), False),
        HardwareUart(1, (1, 3), (0, 2), True),
        HardwareUart(2, (16,), (15,), False),
    ]
