"""SEGGER RTT channels as UART ports (RTT0..RTT2), shared by logger: and uart:."""

import esphome.config_validation as cv
from esphome.core import EsphomeError

from .const import SEGGER_RTT_UART

# Zephyr's RTT UART driver only binds nodes labelled rtt0..rtt3. Channel 3 also needs
# SEGGER's buffer counts raised, so it stays reachable only through "&rtt3".
RTT_PORTS = ("RTT0", "RTT1", "RTT2")


def rtt_channel(port: str) -> int | None:
    """Return the channel number of an RTT<n> port name, or None for any other port."""
    return int(port[3:]) if port in RTT_PORTS else None


def validate_rtt_port(port: str) -> str:
    """Reject an RTT<n> port on a family whose SoCs have no SEGGER RTT support."""
    from . import zephyr_family_uart_drivers, zephyr_variant, zephyr_variant_family

    if SEGGER_RTT_UART not in zephyr_family_uart_drivers():
        family = zephyr_variant_family() or zephyr_variant()
        raise cv.Invalid(f"{port} needs SEGGER RTT, which {family} does not support")
    return port


def enable_rtt_node(channel: int, owner: str) -> str:
    """Claim an RTT channel for owner and return its rtt<n> node label.

    Declares the node, or enables it when the board's devicetree already has one, since
    a second node with the same label fails the devicetree build.
    """
    from . import KEY_BOARD, zephyr_add_overlay, zephyr_add_prj_conf, zephyr_data
    from .dts_lookup import dts_node_label_exists

    claimed = zephyr_data()["rtt_channels"]
    if (other := claimed.get(channel)) is not None:
        raise EsphomeError(
            f"RTT{channel} is used by both {other} and {owner}; give each its own channel"
        )
    claimed[channel] = owner

    zephyr_add_prj_conf("USE_SEGGER_RTT", True)
    # Zephyr's own RTT backend would write native lines to RTT channel 0 as well,
    # alongside Logger's own output.
    zephyr_add_prj_conf("LOG_BACKEND_RTT", False, required=False)
    label = f"rtt{channel}"
    if dts_node_label_exists(zephyr_data()[KEY_BOARD], label):
        zephyr_add_overlay(f"""&{label} {{ status = "okay"; }};""")
    else:
        zephyr_add_overlay(
            f"""/ {{ {label}: {label} {{ compatible = "{SEGGER_RTT_UART}"; }}; }};"""
        )
    return label
