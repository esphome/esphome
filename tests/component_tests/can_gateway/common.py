"""Shared helpers for can_gateway config-validation tests."""

from __future__ import annotations

from typing import Any

from esphome.components.esp32.const import KEY_BOARD, KEY_VARIANT, VARIANT_ESP32C6
import esphome.config_validation as cv
from esphome.const import KEY_FRAMEWORK_VERSION, PlatformFramework

# Pins follow the reference board: TWAI0 TX GPIO2 / RX GPIO3,
# TWAI1 TX GPIO10 / RX GPIO11.
PORT_A = {
    "id": "port_a",
    "rx_pin": "GPIO3",
    "tx_pin": "GPIO2",
    "bit_rate": "125kbps",
}
PORT_B = {
    "id": "port_b",
    "rx_pin": "GPIO11",
    "tx_pin": "GPIO10",
    "bit_rate": "125kbps",
}


def setup_c6(set_core_config) -> None:
    """Set the core up as an ESP32-C6 / IDF target."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        core_data={KEY_FRAMEWORK_VERSION: cv.Version(5, 5, 4)},
        platform_data={
            KEY_BOARD: "esp32-c6-devkitc-1",
            KEY_VARIANT: VARIANT_ESP32C6,
        },
    )


def validate(config):
    """Run the component's CONFIG_SCHEMA (imported late, after setup_c6)."""
    from esphome.components.can_gateway import CONFIG_SCHEMA

    return CONFIG_SCHEMA(config)


def port(base: dict[str, Any], **overrides: Any) -> dict[str, Any]:
    """A copy of a port config with overrides applied."""
    return {**base, **overrides}


def gateway(
    routes: list[dict[str, Any]] | None = None,
    ports: list[dict[str, Any]] | None = None,
    **extra: Any,
) -> dict[str, Any]:
    """A gateway config with sensible defaults."""
    config = {
        "ports": ports if ports is not None else [dict(PORT_A), dict(PORT_B)],
        "routes": (
            routes if routes is not None else [{"from": "port_a", "to": "port_b"}]
        ),
    }
    config.update(extra)
    return config


def route(**extra: Any) -> dict[str, Any]:
    """A route config from port_a to port_b with overrides applied."""
    config = {"from": "port_a", "to": "port_b"}
    config.update(extra)
    return config
