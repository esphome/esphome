"""Tests for the UART wake-on-RX define."""

from collections.abc import Callable
from pathlib import Path

from esphome.core import CORE

WAKE_ON_RX = "USE_UART_WAKE_LOOP_ON_RX"


def test_esp32_without_network_enables_wake_on_rx(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """An ESP32 without a network component enables wake-on-RX."""
    generate_main(component_config_path("wake_on_rx_esp32.yaml"))

    assert WAKE_ON_RX in {d.name for d in CORE.defines}


def test_esp8266_without_network_enables_wake_on_rx(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """An ESP8266 without a network component enables wake-on-RX."""
    generate_main(component_config_path("wake_on_rx_esp8266.yaml"))

    assert WAKE_ON_RX in {d.name for d in CORE.defines}


def test_rp2_leaves_wake_on_rx_off(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Only the ESP32 and ESP8266 UART code can wake on RX, so RP2 leaves it off."""
    generate_main(component_config_path("wake_on_rx_rp2.yaml"))

    assert WAKE_ON_RX not in {d.name for d in CORE.defines}
