"""Stable classification of ``--device`` / port strings.

External tooling (the device-builder dashboard at
esphome/device-builder, and other consumers) needs to decide whether
a user-supplied port string names a local serial device, an OTA
network target, an MQTT magic string, or an RP2040 BOOTSEL upload.

This module is the single stable home for that classification. The
upstream CLI (``esphome.__main__``) re-exports ``PortType`` and
``get_port_type`` from here for its own use; external callers should
import directly from ``esphome.upload_targets`` so the surface stays
stable across releases (``esphome/__main__`` is a CLI entrypoint and
not a stable import path).

Please keep ``PortType`` member names / values and the
``get_port_type`` signature stable — see the docstrings on each for
the contract.
"""

from __future__ import annotations

from esphome.enum import StrEnum


class PortType(StrEnum):
    """Port classification returned by :func:`get_port_type`.

    Used by device-builder (esphome/device-builder) and other
    external tooling to route a user-supplied ``--device`` value to
    the right upload / log path. Member names and string values are
    part of the stable surface — adding new members is fine, but
    existing names / values must not be renamed or changed.
    """

    SERIAL = "SERIAL"
    NETWORK = "NETWORK"
    MQTT = "MQTT"
    MQTTIP = "MQTTIP"
    BOOTSEL = "BOOTSEL"


def get_port_type(port: str) -> PortType:
    """Determine the type of port/device identifier.

    Used by device-builder (esphome/device-builder)'s dashboard to
    decide whether a user-supplied ``--device`` value names a local
    serial port (must build / flash locally), an OTA network target
    (eligible for remote builds), an MQTT magic string, or an RP2040
    BOOTSEL upload. Please keep the signature stable.

    Returns:
        PortType.SERIAL for serial ports (/dev/ttyUSB0, COM1, etc.)
        PortType.BOOTSEL for RP2040 BOOTSEL upload via picotool
        PortType.MQTT for MQTT logging
        PortType.MQTTIP for MQTT IP lookup
        PortType.NETWORK for IP addresses, hostnames, or mDNS names
    """
    if port == "BOOTSEL":
        return PortType.BOOTSEL
    if port.startswith("/") or port.startswith("COM"):
        return PortType.SERIAL
    if port == "MQTT":
        return PortType.MQTT
    if port == "MQTTIP":
        return PortType.MQTTIP
    return PortType.NETWORK
